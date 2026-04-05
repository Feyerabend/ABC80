// ------------------------------------------------------
// sd_device.c — ABC80 SD: device driver (main-Pico side)
//
// This file implements the C-side handlers for the fake "SD:" device
// that is injected into ABC80's enhetslista (device list) at boot.
//
// Architecture overview
// ---------------------
//
//   Main Pico 2W                         SD-Pico 2W
//   ─────────────────────────────────    ──────────────────────────
//   Z80 emulator (abc_pico)              SD-Pico firmware
//     │                                    │
//     │  UART1 (TX=GP4, RX=GP5)            │ UART0 (RX=GP1, TX=GP0)
//     ├────────────────────────────────────┤
//     │  ABC80 BASIC --> SAVE/LOAD SD:     ├── SPI --> SD card (FatFS)
//     │  PC trap --> handler here          │
//
// PC trap mechanism
// -----------------
//
//   abc80_step() calls step() (z80.c) once.  After the step, abc80.c
//   calls sd_device_dispatch(pc).  If pc matches any of our trap
//   addresses the corresponding handler runs and we simulate Z80 RET
//   by popping the return address from the emulated stack (sd_ret).
//   The actual HALT byte at the trap address is never executed because
//   we intercept before the next step().
//
// --------------------------------------------
// ABC80 enhetslista (device list) entry format
// --------------------------------------------
//
//   Each entry is 7 bytes at an arbitrary RAM address:
//
//     +0, +1   next_ptr  (uint16_t LE) — address of next entry, or original
//                          list head so we stay linked into the chain
//     +2..+4   name[3]   — device name in ABC80 character set (e.g. "SD ")
//     +5, +6   htable_ptr (uint16_t LE) — address of the 9-entry jump table
//
//   The ROM's boot code at 0x00B7 does:
//       LD HL, <original_head>       ; e.g. 0x018C
//       LD (0xFE0A), HL              ; store into system variable
//   We patch the LD HL immediate operand at 0x00B8/0x00B9 so the ROM
//   writes SD_ENTRY into 0xFE0A itself.  That way SD: is always the
//   first entry and the original head becomes our next_ptr — no extra
//   timing tricks needed.
//
//   We use 0xFF80 for SD_ENTRY (top 128 bytes of RAM are documented
//   as freely usable in "Avancerad programmering för ABC80").
//
// -----------------------------------------------------------------
// Handler jump table (9 × 3 bytes = 27 bytes of JP nn instructions)
// -----------------------------------------------------------------
//
//   Index   Mnemonic   Our address        Notes
//   -----   --------   ---------------    --------------------------------
//     0     OPEN       SD_TRAP_OPEN       open existing file for read
//     1     PREPARE    SD_TRAP_INIT       create/truncate file for write
//     2     CLOSE      SD_TRAP_CLOSE      close file, flush partial block
//     3     INPUT      SD_TRAP_INPUT      supply next BAS line (HL=dst, C=maxlen)
//     4     PRINT      ROM 0x001B         ROM blocking-print routine (no trap)
//     5     BL_IN      SD_TRAP_BLREAD     supply next block to ROM input
//     6     BL_UT      SD_TRAP_BLWRITE    flush full block from ROM output
//     7     (error)    SD_TRAP_ERR7       unused, returns NOT_READY
//     8     (error)    SD_TRAP_ERR8       unused, returns NOT_READY
//
//   Trap 4 (PRINT) jumps to the existing ROM routine 0x001B.
//   All other entries, including trap 3 (INPUT), are our own handlers.
//
// ---------------------------------------
// FCB (File Control Block) — IX register
// ---------------------------------------
//
//   When any driver routine is called, IX points to a 15-byte FCB that
//   the BASIC interpreter allocates on the stack.  Each open file has
//   its own FCB.  Routines MUST preserve IX and IY (save/restore if used).
//
//   Byte    Symbol     Meaning
//   ----    ------     -------
//   IX+0    next_lo    link to next open-file node (lo byte); 0 = last
//   IX+1    next_hi    link to next open-file node (hi byte)
//   IX+2    filnum     logical file number (the N in OPEN "SD:" AS FILE N)
//   IX+3    dev_lo     ptr to device node in enhetslistan (lo)
//   IX+4    dev_hi     ptr to device node in enhetslistan (hi)
//   IX+5    open_flag  0 = file closed,  1 = file open
//   IX+6    pos        current write position (managed by ROM 0x001B after BL_UT)
//   IX+7    maxpos     max line/block length; we set 0x84 = 132 = block flush point
//
//   -- Non-blocking use (not applicable to us, kept for reference) --
//   IX+8               reserved
//   IX+9..IX+14        driver-local variables
//
//   -- Blocking use (what we use) --
//   IX+8    buf_lo     buffer start address (lo); we write SD_BUF & 0xFF
//   IX+9    buf_hi     buffer start address (hi); we write SD_BUF >> 8
//   IX+10   cur_lo     current write ptr in buffer (lo); managed by ROM after BL_UT
//   IX+11   cur_hi     current write ptr in buffer (hi); managed by ROM after BL_UT
//   IX+12              free (unused)
//   IX+13   free_cnt   free bytes remaining in buffer; ROM 0x001B decrements this
//                      and calls BL_UT when it reaches zero; init to 252 (0xFC)
//   IX+14   status     bit 7: 1 = ASCII data has been written to buffer
//                      bit 0: 1 = current buffer has been flushed to device
//
// ------------------------------------------------------------
// Blocking buffer mechanism — BL_UT (write) calling convention
// ------------------------------------------------------------
//
//   The ROM's blocking print routine at 0x001B:
//     - Writes one character at IX+10,11 (current buffer ptr), increments IX+6 and IX+10,11
//     - Decrements IX+13 (free-bytes counter); when IX+13 reaches zero, calls BL_UT
//     - Also calls BL_UT when IX+6 reaches IX+7 (position == maxpos = 132)
//
//   On BL_UT entry:
//     - HL = current write pointer (= SD_BUF + bytes_written so far)
//     - Data to flush is at m[SD_BUF .. HL-1], i.e. data_len = HL - SD_BUF
//     - IX+8,9 still holds SD_BUF (buffer start)
//
//   BL_UT must:
//     1. Compute data_len = HL - IX+8,9
//     2. Send data_len bytes from m[SD_BUF] to SD-Pico (TODO)
//     3. Reset IX+13 = 252  <-- CRITICAL: without this, ROM never calls BL_UT
//                               via the IX+13 path again, causing runaway writes
//     4. Set   IX+14 |= 0x01    (mark buffer as flushed)
//     5. Set HL = SD_BUF        (return buffer start address)
//     6. Call sd_ok() --> A=0, CY=0, simulated RET
//
//   Reset IX+6, IX+10, IX+11 and DE in BL_UT.
//   The SAVE loop (ROM ~0x0DF5) uses DE as the write pointer and does NOT
//   reload it from IX+10,11 after BL_UT returns, so we must reset it here.
//   IX+6 and IX+10,11 must stay in sync with DE/HL.
//
// -----------------------------------------------------------
// Blocking buffer mechanism — BL_IN (read) calling convention
// -----------------------------------------------------------
//
//   The ROM's blocking input routine at 0x0015:
//     - Reads from the current buffer position (IX+10,11)
//     - When the buffer is exhausted, calls BL_IN
//
//   On BL_IN entry:
//     - No specific register convention; IX still set
//
//   BL_IN must:
//     1. Fill m[SD_BUF .. SD_BUF+N-1] with next N bytes from file
//     2. Set IX+10,11 = SD_BUF  (reset read ptr to buffer start)
//     3. Set IX+13    = N       (bytes available; normally 252)
//     4. Set HL = SD_BUF
//     5. Return CY=0 for more data, or A=0/CY=1 for EOF
//
// ----------------
// CLOSE convention
// ----------------
//
//   CLOSE is called after the last PRINT#/INPUT# before CLOSE statement.
//   If any data was written (IX+14 bit 7 is set after ROM 0x001B sets it,
//   or bit 0 from BL_UT), there may be a partial block remaining.
//   The remaining bytes are at m[SD_BUF .. IX+10,11 - 1].
//   We must flush that partial block and then send CLOSE to SD-Pico.
//
// -------------------------
// OPEN / PREPARE convention
// -------------------------
//
//   Both receive DE = address of 11-byte 8.3 filename in Z80 RAM.
//   The filename is space-padded, no dot:  "TEST    BAC" for TEST.BAC
//   PREPARE --> create/truncate for write
//   OPEN    --> open for read; return SD_ERR_NOT_FOUND (21) if missing
//
// ---------------
// Error reporting
// ---------------
//
//   On error: set A = error_code, set CY=1, then simulate RET (sd_ret).
//   The BASIC interpreter reads A and CY on return from every device call.
//
//   Alternatively, a driver can invoke the ROM error RST:
//       RST 10H
//       DEFB (error_code | 0x80)   <- high bit must be set
//   We use the A/CY mechanism from C.
//
//   Error codes:
//     21  hittar ej filen       file not found
//     34  filen slut            EOF
//     35  checksummafel         read/checksum error
//     37  felaktigt recordfmt   missing PREPARE (wrong record format)
//     38  blocknr utanför fil   block number out of range
//     42  enheten ej klar       device not ready / no SD card
//     43  skivan skrivskyddad   write-protected
//
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "z80.h"          // m[], pc, sp, a, b, c, d, e, h, l, ix, cf, ...
#include "sd_device.h"

// ---------------------------------------------------------------------------
// Memory map for SD: device structures
//
//   0xFF80  SD_ENTRY  — 7-byte enhetslistan entry (next_ptr + name + htable_ptr)
//   0xFF87  SD_HTABLE — 27-byte handler jump table (9 × JP nn)
//   0x7B00  SD_BUF    — 256-byte blocking I/O buffer (BL_UT write, BAS INPUT).
//                       0x7B00-0x7BFF sits below screen RAM (0x7C00) and below
//                       BOFA (0x8000), so it is never reached by BASIC programs
//                       or the display controller.

#define SD_ENTRY        0xFF80u
#define SD_HTABLE       0xFF87u
#define SD_BUF          0x7B00u   // below screen RAM (0x7C00) and below BOFA (0x8000)

// Trap addresses — these must be in the top-of-RAM region where we own the
// memory.  They hold a HALT (0x76) as a guard; our dispatch intercepts
// before the guard executes.

#define SD_TRAP_OPEN    0xFFA2u   // handler index 0: OPEN
#define SD_TRAP_INIT    0xFFA3u   // handler index 1: PREPARE
#define SD_TRAP_CLOSE   0xFFA4u   // handler index 2: CLOSE
#define SD_TRAP_BLREAD  0xFFA5u   // handler index 5: BL_IN
#define SD_TRAP_BLWRITE 0xFFA6u   // handler index 6: BL_UT
#define SD_TRAP_ERR7    0xFFA7u   // handler index 7: unused error stub
#define SD_TRAP_ERR8    0xFFA8u   // handler index 8: unused error stub
#define SD_TRAP_INPUT   0xFFA9u   // handler index 3: INPUT for BAS load

// ---------------------------------------------------------------------------
// WiFi protocol — TODO
//
// All data exchange with SD-Pico goes here once the protocol is defined.
// Each handler below marks exactly what must be sent / received.

// ---------------------------------------------------------------------------
// File type detection
//
// The 11-byte 8.3 filename at DE has the 3-byte extension at DE+8..DE+10.
// We distinguish BAC (tokenized binary) from BAS (plain ASCII text) because
// the save and load paths differ significantly between the two.

typedef enum { FILE_BAC, FILE_BAS, FILE_OTHER } file_type_t;

static file_type_t sd_detect_type(uint16_t de) {
    uint8_t e0 = m[(uint16_t)(de + 8u)];
    uint8_t e1 = m[(uint16_t)(de + 9u)];
    uint8_t e2 = m[(uint16_t)(de + 10u)];
    if (e0 == 'B' && e1 == 'A' && e2 == 'C') return FILE_BAC;
    if (e0 == 'B' && e1 == 'A' && e2 == 'S') return FILE_BAS;
    return FILE_OTHER;
}

// ---------------------------------------------------------------------------
// In-memory simulation buffer
//
// Both BAC and BAS files are held in sim_buf while WiFi to the SD-Pico is
// not yet implemented.
//
// BAC save: CLOSE captures m[BOFA..EOFA-1] directly.
// BAC load: CLOSE writes sim_buf back to m[BOFA] and fixes EOFA/HEAP.
//
// BAS save: the ROM drives text through BL_UT blocks; we accumulate them
//           into sim_buf; CLOSE flushes the final partial block.
// BAS load: the ROM calls our INPUT handler (SD_TRAP_INPUT) once per line;
//           each call reads the next CR-terminated line from sim_buf.
//
// 8 KB covers programs of several hundred lines.

#define SIM_BUF_SIZE 8192u
static uint8_t    sim_buf[SIM_BUF_SIZE];
static uint16_t   sim_len        = 0;
static uint16_t   sim_pos        = 0;
static bool       sim_is_writing = false;
static file_type_t sim_file_type = FILE_OTHER;

static void sd_ret(void);   // forward declaration

// ---------------------------------------------------------------------------
// BAS load — INPUT handler
//
// For BAS files the ROM LOAD command calls the device INPUT routine once per
// BASIC line.  Interface (from ABC80 device driver spec):
//   Entry: HL = Z80 address where the line text should be stored
//          C  = maximum allowed string length
//   Exit OK:  text written at m[HL], CR (0x0D) placed immediately after,
//             A=0, CY=0
//   Exit EOF: A=0, CY=1
//
// The ROM itself calls Radcompile on each line we supply; we do not touch
// Radcompile at all.
//
// State is initialised in sd_handle_open() when sim_file_type == FILE_BAS.

static uint16_t bas_src_pos = 0;   // read cursor in sim_buf
static int      bas_line_n  = 0;   // line counter (for debug)

// ---------------------------------------------------------------------------
// Handler: INPUT — supply next BAS line to the ROM's LOAD machinery
//
// Called by: ROM LOAD loop for BAS files (htab index 3 → SD_TRAP_INPUT)
// Entry:     HL = destination in Z80 RAM, C = max length
// Exit OK:   text at m[HL], m[HL+len] = 0x0D, A=0, CY=0
// Exit EOF:  A=0, CY=1

static void sd_handle_input(void) {
    if (bas_src_pos >= sim_len) {
        printf("SD: INPUT EOF after %d lines\n", bas_line_n);
        a  = 0;
        cf = 1;
        sd_ret();
        return;
    }

    uint16_t dst    = (uint16_t)((h << 8) | l);
    uint8_t  maxlen = c;
    uint8_t  i      = 0;

    while (bas_src_pos < sim_len && i < maxlen) {
        uint8_t ch = sim_buf[bas_src_pos++];
        if (ch == 0x0A) continue;       // skip bare LF
        if (ch == 0x0D) break;          // CR = end of line (consumed, not stored)
        m[(uint16_t)(dst + i++)] = ch;
    }
    m[(uint16_t)(dst + i)] = 0x0D;     // CR must follow the text immediately

    bas_line_n++;
    printf("SD: INPUT #%d [%.*s]\n", bas_line_n, (int)i, (char *)(m + dst));

    a  = 0;
    cf = 0;
    sd_ret();
}

// ---------------------------------------------------------------------------
// ABC80 error codes (set in A with CY=1 on error return)

#define SD_ERR_NOT_FOUND  21    // hittar ej filen
#define SD_ERR_EOF        34    // filen slut
#define SD_ERR_CHECKSUM   35    // checksummafel
#define SD_ERR_REC_FMT    37    // felaktigt record format (forgot PREPARE?)
#define SD_ERR_BLOCK      38    // blocknummer utanför fil
#define SD_ERR_NOT_READY  42    // enheten ej klar (no SD card)
#define SD_ERR_PROTECTED  43    // skivan skrivskyddad

// ---------------------------------------------------------------------------
// Z80 stack simulation helpers

// Simulate Z80 RET: pop 2-byte return address from emulated stack.
static void sd_ret(void) {
    uint16_t lo = m[sp];
    uint16_t hi = m[(uint16_t)(sp + 1u)];
    sp = (uint16_t)(sp + 2u);
    pc = lo | (hi << 8);
}

// Normal return: A=0, CY=0.
static void sd_ok(void) {
    a  = 0;
    cf = 0;
    sd_ret();
}

// Error return: A=code, CY=1.
static void sd_error(uint8_t code) {
    a  = code;
    cf = 1;
    sd_ret();
}

// ---------------------------------------------------------------------------
// Filename parsing
//
// The ROM passes DE = address of an 11-byte 8.3 filename in Z80 RAM.
// Format: 8 bytes name (space-padded) + 3 bytes extension (space-padded),
// no dot separator.  Example: "TEST    BAC" --> "TEST.BAC".
// Output buffer `out` must be at least 13 bytes.

static void sd_parse_filename(uint16_t de, char *out) {
    int p = 0;

    // Name part: trim trailing spaces.
    int name_end = 8;
    while (name_end > 0 && m[de + name_end - 1] == ' ')
        name_end--;
    for (int i = 0; i < name_end; i++)
        out[p++] = (char)m[de + i];

    // Extension part: trim trailing spaces, add dot if non-empty.
    int ext_end = 3;
    while (ext_end > 0 && m[de + 8 + ext_end - 1] == ' ')
        ext_end--;
    if (ext_end > 0) {
        out[p++] = '.';
        for (int i = 0; i < ext_end; i++)
            out[p++] = (char)m[de + 8 + i];
    }
    out[p] = '\0';
}

// ---------------------------------------------------------------------------
// FCB blocking-buffer initialisation (common to OPEN and PREPARE)
//
// Sets the FCB fields that control the blocking mechanism:
//   IX+7  = 0x84 = 132  — flush trigger (BL_UT called when IX+6 reaches this)
//   IX+8,9              — SD_BUF (buffer start address, lo/hi)
//   IX+10,11            — SD_BUF (current write ptr, starts at buffer start)
//   IX+13 = 252 (0xFC)  — free bytes counter; ROM 0x001B decrements this per char
//                         IMPORTANT: must be 252, not 253.  The ROM example
//                         uses `LD (IX+0DH), 0FCH`.  Off-by-one here causes the
//                         counter to wrap incorrectly after the first BL_UT.
//   IX+14 = 0x00        — status: nothing written, nothing flushed
//
//   Do NOT pre-write m[SD_BUF]: the ROM writes TO the buffer during SAVE/PRINT;
//   a sentinel byte there would corrupt the output and is not needed.
//
// OPEN and PREPARE both call this before their file-open TODO logic.

static void sd_setup_fcb_buf(void) {
    m[(uint16_t)(ix + 6)]  = 0;                          // current position = 0
    m[(uint16_t)(ix + 7)]  = 0x84;                       // max pos = 132 (flush trigger)
    m[(uint16_t)(ix + 8)]  = (uint8_t)(SD_BUF & 0xFF);   // buf start lo
    m[(uint16_t)(ix + 9)]  = (uint8_t)(SD_BUF >> 8);     // buf start hi
    m[(uint16_t)(ix + 10)] = (uint8_t)(SD_BUF & 0xFF);   // cur write ptr lo
    m[(uint16_t)(ix + 11)] = (uint8_t)(SD_BUF >> 8);     // cur write ptr hi
    m[(uint16_t)(ix + 13)] = 252;                        // 252 (0xFC) free bytes
    m[(uint16_t)(ix + 14)] = 0x00;                       // status: clean
}

// ---------------------------------------------------------------------------
// Handler: OPEN — open existing file for reading
//
// Called by: LOAD SD:file / RUN SD:file / OPEN "SD:file" AS FILE N
// Entry:     DE = address of 11-byte 8.3 filename in Z80 RAM
// Exit OK:   A=0, CY=0
// Exit ERR:  A=error_code, CY=1

static void sd_handle_open(void) {
    uint16_t de = (uint16_t)((d << 8) | e);
    char fname[13];
    sd_parse_filename(de, fname);
    sim_file_type = sd_detect_type(de);
    sim_is_writing = false;
    sim_pos = 0;
    printf("SD: OPEN  %s  type=%d\n", fname, (int)sim_file_type);

    // TODO (WiFi): send CMD_OPEN + fname to SD-Pico.
    //   On success: SD-Pico replies with file size (uint16 LE).
    //   On not-found: call sd_error(SD_ERR_NOT_FOUND) and return.

    sd_setup_fcb_buf();

    if (sim_file_type == FILE_BAS) {
        if (sim_len == 0) { sd_error(SD_ERR_NOT_FOUND); return; }
        // Initialise BAS-load state.  The ROM will call our INPUT handler
        // (SD_TRAP_INPUT) once per line; sd_handle_input() reads from sim_buf.
        bas_src_pos = 0;
        bas_line_n  = 0;
        printf("SD: OPEN BAS  sim_len=%u\n", (unsigned)sim_len);
    }

    sd_ok();
}

// ---------------------------------------------------------------------------
// Handler: PREPARE — create/truncate file for writing
//
// Called by: SAVE SD:file / LIST SD:file / PREPARE "SD:file" AS FILE N
// Entry:     DE = address of 11-byte 8.3 filename in Z80 RAM
// Exit OK:   A=0, CY=0
// Exit ERR:  A=error_code, CY=1

static void sd_handle_prepare(void) {
    uint16_t de = (uint16_t)((d << 8) | e);
    char fname[13];
    sd_parse_filename(de, fname);
    sim_file_type  = sd_detect_type(de);
    sim_is_writing = true;
    sim_len        = 0;
    printf("SD: PREP  %s  type=%d\n", fname, (int)sim_file_type);

    // TODO (WiFi): send CMD_PREPARE + fname to SD-Pico.
    //   SD-Pico creates / truncates the file and replies OK or error
    //   (e.g. SD_ERR_PROTECTED if write-protected).

    sd_setup_fcb_buf();
    sd_ok();
}

// ---------------------------------------------------------------------------
// Handler: CLOSE — flush partial buffer and close file
//
// Called by: CLOSE N  (after PREPARE/PRINT# sequence)
// Entry:     IX = FCB for the file being closed
// Exit OK:   A=0, CY=0
//
// If any data was written (IX+14 bit 7 set by ROM's 0x001B), there may be
// a partial block in SD_BUF that has not been sent via BL_UT yet.
// The remaining bytes are at m[SD_BUF .. IX+10,11 - 1].

static void sd_handle_close(void) {
    printf("SD: CLOSE  writing=%d  type=%d  IX+14=%02X\n",
           (int)sim_is_writing, (int)sim_file_type, m[(uint16_t)(ix + 14)]);

    if (sim_is_writing) {

        if (sim_file_type == FILE_BAS) {
            // -----------------------------------------------------------
            // BAS write path (after PREPARE / LIST SD:file.BAS)
            //
            // The ROM's blocking print routine (0x001B) has been emitting
            // the program as plain ASCII text, one line per BL_UT block.
            // BL_UT has already accumulated full blocks into sim_buf.
            // Flush the partial last block (IX+10,11 - IX+8,9 bytes) now.
            uint16_t buf_start = (uint16_t)(m[(uint16_t)(ix+8)]  | (m[(uint16_t)(ix+9)]  << 8));
            uint16_t buf_cur   = (uint16_t)(m[(uint16_t)(ix+10)] | (m[(uint16_t)(ix+11)] << 8));
            uint16_t remaining = (uint16_t)(buf_cur - buf_start);
            if (remaining > 0 && sim_len + remaining <= SIM_BUF_SIZE) {
                memcpy(sim_buf + sim_len, m + buf_start, remaining);
                sim_len += remaining;
            }
            printf("SD: CLOSE  BAS saved %u bytes\n", (unsigned)sim_len);
            // TODO (WiFi): send sim_buf[0..sim_len-1] to SD-Pico (if not
            //   already sent block-by-block in BL_UT), then CMD_CLOSE.

        } else {
            // -----------------------------------------------------------
            // BAC write path (after PREPARE / SAVE SD:file.BAC)
            //
            // The BL_UT data is cassette-tape format — do NOT use it.
            // The raw token bytes are in the program area directly:
            //   BOFA = m[0xFE1C/1D], EOFA = m[0xFE1E/1F]
            //
            // TODO (WiFi): send m[BOFA..EOFA-1] to SD-Pico, then CMD_CLOSE.
            uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
            uint16_t eofa = (uint16_t)(m[0xFE1Eu] | ((uint16_t)m[0xFE1Fu] << 8));
            printf("SD: CLOSE  BAC write  BOFA=%04X  EOFA=%04X  len=%u\n",
                   (unsigned)bofa, (unsigned)eofa, (unsigned)(eofa - bofa));
            // TODO: wifi_send(m + bofa, eofa - bofa);
        }

        sim_is_writing = false;

    } else {
        // ---------------------------------------------------------------
        // Read path (after OPEN / LOAD)

        if (sim_file_type == FILE_BAC) {
            // BAC load: ROM goes OPEN → CLOSE directly (no BL_IN, no INPUT).
            // Write the raw tokenised bytes from sim_buf back to the program
            // area at BOFA, then fix EOFA and HEAP.
            uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
            if (sim_len > 0) {
                memcpy(m + bofa, sim_buf, sim_len);
                uint16_t new_eofa = (uint16_t)(bofa + sim_len);
                m[0xFE1Eu] = (uint8_t)(new_eofa & 0xFF);
                m[0xFE1Fu] = (uint8_t)(new_eofa >> 8);
                m[0xFE20u] = (uint8_t)(new_eofa & 0xFF);
                m[0xFE21u] = (uint8_t)(new_eofa >> 8);
                printf("SD: CLOSE BAC load  BOFA=%04X  EOFA=%04X  len=%u\n",
                       (unsigned)bofa, (unsigned)new_eofa, (unsigned)sim_len);
            } else {
                printf("SD: CLOSE BAC load  sim_buf empty\n");
            }

        } else if (sim_file_type == FILE_BAS) {
            // BAS load: the ROM has already called our INPUT handler for every
            // line and compiled each one via Radcompile internally.
            // Nothing more to do here.
            printf("SD: CLOSE BAS load done  %d lines\n", bas_line_n);

        } else {
            printf("SD: CLOSE read  unknown type=%d\n", (int)sim_file_type);
        }
    }

    sd_ok();
}

// ---------------------------------------------------------------------------
// Handler: BL_IN — supply next block to ROM input routine (read path)
//
// Called by: ROM 0x0015 when the blocking input buffer is exhausted.
// Entry:     IX = FCB
// Exit OK:   HL = SD_BUF (buffer start), IX+10,11 = SD_BUF, IX+13 = N bytes
//            A=0, CY=0 (more data available)
// Exit EOF:  A=0, CY=1 (no more data)
//
// We must fill m[SD_BUF..SD_BUF+N-1] with the next N bytes from the file,
// then reset IX+10,11 to SD_BUF and set IX+13 to N.
// Returning HL=SD_BUF tells the ROM where the new buffer starts.

static void sd_handle_blread(void) {
    // BL_IN is not expected to be called for either file type:
    //   BAC: ROM goes OPEN → CLOSE directly, no block reads.
    //   BAS: ROM calls our INPUT handler (SD_TRAP_INPUT) directly per line.
    //
    // If BL_IN is ever needed (e.g. for INPUT# on open text files in the
    // future), the WiFi implementation would go here:
    //   1. Send CMD_BL_IN to SD-Pico
    //   2. SD-Pico replies with N (uint8) + N bytes, or N=0 for EOF
    //   3. If N==0: sd_error(SD_ERR_EOF); return
    //   4. memcpy(m + SD_BUF, received, N)
    //   5. m[ix+10] = SD_BUF & 0xFF;  m[ix+11] = SD_BUF >> 8;  m[ix+13] = N
    //   6. h = SD_BUF >> 8;  l = SD_BUF & 0xFF;  sd_ok()
    printf("SD: BL_IN  unexpected  type=%d\n", (int)sim_file_type);
    sd_error(SD_ERR_NOT_READY);
}

// ---------------------------------------------------------------------------
// Handler: BL_UT — flush full block to SD-Pico (write path)
//
// Called by: ROM 0x001B when the blocking output buffer is full.
//            Triggered when IX+6 reaches IX+7 (pos == maxpos == 132).
//
// Entry:
//   HL = current write pointer = SD_BUF + bytes_written
//   IX+8,9 = SD_BUF  (buffer start — set by sd_setup_fcb_buf)
//   The data to flush is m[SD_BUF .. HL-1], length = HL - SD_BUF.
//
// Exit:
//   HL = SD_BUF  (reset buffer pointer — ROM 0x001B updates IX+6/IX+10,11 from this)
//   IX+13 = 252  (reset free-bytes counter for next block)
//   IX+14 bit 0 set  (marks that this buffer was flushed)
//   A=0, CY=0
//
// IMPORTANT — reset IX+6, IX+10, IX+11 and DE here.
//   The SAVE loop (ROM ~0x0DF5) uses DE as the buffer write pointer and
//   does NOT reload it from IX+10,11 after BL_UT returns.  If DE is not
//   reset to buf_start the write pointer runs past the buffer boundary,
//   corrupting all of Z80 RAM.  IX+6 (pos) must also be reset to 0 so
//   the flush trigger fires correctly for the next block.
//
// IMPORTANT — always reset IX+13 = 252.
//   If IX+13 is not reset, ROM's decrement wraps 0-->255 and the IX+13
//   trigger never fires again.  The IX+6/IX+7 trigger will still fire but
//   without IX+13 tracking the free bytes correctly the buffer accounting
//   breaks down, producing growing and incorrect data_len values.

static void sd_handle_blwrite(void) {
    uint16_t buf_start = (uint16_t)(m[(uint16_t)(ix + 8)] | (m[(uint16_t)(ix + 9)] << 8));
    uint16_t buf_cur   = (uint16_t)((h << 8) | l);   // HL = write ptr on entry
    uint16_t data_len  = (uint16_t)(buf_cur - buf_start);

    uint16_t ret_addr   = (uint16_t)(m[sp] | (m[(uint16_t)(sp + 1u)] << 8));
    uint16_t saved_src  = (uint16_t)(m[(uint16_t)(sp + 2u)] | (m[(uint16_t)(sp + 3u)] << 8));
    uint16_t bofa       = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
    uint16_t eofa       = (uint16_t)(m[0xFE1Eu] | ((uint16_t)m[0xFE1Fu] << 8));
    printf("SD: BL_UT  len=%u  HL=%04X  ix=%04X  sp=%04X  ret=%04X\n",
           (unsigned)data_len, (unsigned)buf_cur, (unsigned)ix,
           (unsigned)sp, (unsigned)ret_addr);
    printf("  saved_src=%04X  BOFA=%04X  EOFA=%04X\n",
           (unsigned)saved_src, (unsigned)bofa, (unsigned)eofa);

    // BAS save: accumulate this block into sim_buf.
    // The ROM emits the program listing as plain ASCII text; each BL_UT call
    // carries a chunk of that text.  CLOSE flushes the final partial block.
    if (sim_is_writing && sim_file_type == FILE_BAS) {
        if (sim_len + data_len <= SIM_BUF_SIZE) {
            memcpy(sim_buf + sim_len, m + buf_start, data_len);
            sim_len += data_len;
        }
        printf("SD: BL_UT  BAS +%u bytes  total=%u\n",
               (unsigned)data_len, (unsigned)sim_len);
    }

    // Reset FCB and registers for next block (required for all file types).
    m[(uint16_t)(ix + 6)]  = 0;
    m[(uint16_t)(ix + 10)] = (uint8_t)(buf_start & 0xFF);
    m[(uint16_t)(ix + 11)] = (uint8_t)(buf_start >> 8);
    m[(uint16_t)(ix + 13)] = 252;
    m[(uint16_t)(ix + 14)] |= 0x01;

    d = (uint8_t)(buf_start >> 8);
    e = (uint8_t)(buf_start & 0xFF);
    h = (uint8_t)(buf_start >> 8);
    l = (uint8_t)(buf_start & 0xFF);

    sd_ok();
}

// ---------------------------------------------------------------------------
// Handler: error stubs (indices 7 and 8 — not used by normal BASIC operations)

static void sd_handle_err(void) {
    printf("SD: ERR (stub)\n");
    sd_error(SD_ERR_NOT_READY);
}

// ---------------------------------------------------------------------------
// sd_device_init — inject SD: into the enhetslistan and install traps
//
// Called from abc80_init() after the ROM image is loaded into m[].
//
// Memory written:
//   m[0x00B8,0x00B9]    — patch ROM init to write SD_ENTRY into 0xFE0A
//   m[SD_ENTRY+0..+6]   — 7-byte enhetslista entry
//   m[SD_HTABLE+0..+26] — 9 × JP nn (handler jump table)
//   m[trap_addr]        — HALT guard at each trap address

void sd_device_init(void) {
    // Patch the ROM's enhetslistan-head store instruction.
    // ROM 0x00B7: LD HL, 0x018C / LD (0xFE0A), HL
    // We change the LD HL immediate to SD_ENTRY so the ROM writes our
    // entry's address into the system variable 0xFE0A.
    m[0x00B8] = (uint8_t)(SD_ENTRY & 0xFF);
    m[0x00B9] = (uint8_t)(SD_ENTRY >> 8);

    // Device entry at SD_ENTRY (0xFF80):
    //   +0,+1  next_ptr = 0x018C  (original list head — keeps chain intact)
    //   +2..4  name = "SD "
    //   +5,+6  htable_ptr = SD_HTABLE
    m[SD_ENTRY + 0] = 0x8C;                             // next_ptr lo
    m[SD_ENTRY + 1] = 0x01;                             // next_ptr hi (0x018C)
    m[SD_ENTRY + 2] = 'S';
    m[SD_ENTRY + 3] = 'D';
    m[SD_ENTRY + 4] = ' ';
    m[SD_ENTRY + 5] = (uint8_t)(SD_HTABLE & 0xFF);     // htable_ptr lo
    m[SD_ENTRY + 6] = (uint8_t)(SD_HTABLE >> 8);       // htable_ptr hi

    // Handler jump table at SD_HTABLE (0xFF87): 9 × JP nn (opcode 0xC3)
    //   index 4 (PRINT) --> ROM 0x001B — no trap, use existing ROM routine
    static const uint16_t htab[9] = {
        SD_TRAP_OPEN,    // 0: OPEN
        SD_TRAP_INIT,    // 1: PREPARE
        SD_TRAP_CLOSE,   // 2: CLOSE
        SD_TRAP_INPUT,   // 3: INPUT  (our handler for BAS load)
        0x001Bu,         // 4: PRINT  (ROM blocking print)
        SD_TRAP_BLREAD,  // 5: BL_IN
        SD_TRAP_BLWRITE, // 6: BL_UT
        SD_TRAP_ERR7,    // 7: error stub
        SD_TRAP_ERR8,    // 8: error stub
    };
    for (int i = 0; i < 9; i++) {
        m[SD_HTABLE + i*3 + 0] = 0xC3;                         // JP opcode
        m[SD_HTABLE + i*3 + 1] = (uint8_t)(htab[i] & 0xFF);    // address lo
        m[SD_HTABLE + i*3 + 2] = (uint8_t)(htab[i] >> 8);      // address hi
    }

    // Install HALT guards at each trap address.
    // We only trap our own addresses; ROM 0x001B is not trapped.
    static const uint16_t traps[] = {
        SD_TRAP_OPEN,  SD_TRAP_INIT,    SD_TRAP_CLOSE,
        SD_TRAP_INPUT, SD_TRAP_BLREAD,  SD_TRAP_BLWRITE,
        SD_TRAP_ERR7,  SD_TRAP_ERR8,
    };
    for (int i = 0; i < 8; i++)
        m[traps[i]] = 0x76;   // HALT
}

// ---------------------------------------------------------------------------
// sd_device_dispatch — check pc against trap table and call handler
//
// Called from abc80_step() after every step().
// Returns true if the trap was handled (abc80_step must not re-execute).

bool sd_device_dispatch(uint16_t trap_pc) {
    switch (trap_pc) {
        case SD_TRAP_OPEN:    sd_handle_open();    return true;
        case SD_TRAP_INIT:    sd_handle_prepare(); return true;
        case SD_TRAP_CLOSE:   sd_handle_close();   return true;
        case SD_TRAP_INPUT:   sd_handle_input();   return true;
        case SD_TRAP_BLREAD:  sd_handle_blread();  return true;
        case SD_TRAP_BLWRITE: sd_handle_blwrite(); return true;
        case SD_TRAP_ERR7:    sd_handle_err();     return true;
        case SD_TRAP_ERR8:    sd_handle_err();     return true;
        default:              return false;
    }
}
