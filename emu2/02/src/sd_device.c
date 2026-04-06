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
//     7     (delete)   SD_TRAP_ERR7       returns NOT_READY (delete unsupported)
//     8     (rename)   SD_TRAP_ERR8       returns NOT_READY (rename unsupported)
//
//   Order confirmed by ABC80 device driver spec (elementärt-enhetslista PDF).
//   Index 3 = INPUT (called by LOAD/RUN), index 4 = PRINT (called by LIST).
//   Trap 4 (PRINT) jumps to the existing ROM routine 0x001B which handles
//   the blocking buffer and calls BL_UT when the buffer fills up.
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

#include "pico/stdlib.h"  // sleep_ms
#include "z80.h"          // m[], pc, sp, a, b, c, d, e, h, l, ix, cf, ...
#include "sd_device.h"
#include "wifi_client.h"  // http_get, http_post, wifi_ready

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
static uint8_t     sim_buf[SIM_BUF_SIZE];
static uint16_t    sim_len        = 0;
static uint16_t    sim_pos        = 0;
static bool        sim_is_writing = false;
static file_type_t sim_file_type  = FILE_OTHER;

// Filename saved at OPEN/PREPARE so BL_UT and CLOSE can use it.
static char     sd_fname[13];
// Running write offset — tracks how many bytes have been uploaded so far.
// 0 causes the server to create/truncate the file; >0 causes append.
static uint32_t sd_write_off = 0;

static void sd_ret(void);   // forward declaration

// ---------------------------------------------------------------------------
// WiFi helpers — download / upload file chunks to/from the PicoFS server

// Download a complete file into sim_buf by issuing successive GET /read
// requests of up to 2048 bytes each.  Returns true on success.
// Maximum number of attempts for any HTTP operation (1 + 2 retries).
#define HTTP_MAX_ATTEMPTS  3
// Delay between retries (ms) — gives SD-Pico time to finish previous request.
#define HTTP_RETRY_DELAY_MS  500

static bool sd_wifi_load(const char *fname) {
    char qry[80];
    sim_len = 0;
    while (sim_len < SIM_BUF_SIZE) {
        uint16_t room = (uint16_t)(SIM_BUF_SIZE - sim_len);
        if (room > 2048) room = 2048;
        uint16_t got  = 0;
        snprintf(qry, sizeof(qry), "/read?path=%s&off=%u&n=%u",
                 fname, (unsigned)sim_len, (unsigned)room);
        int rc = HTTP_ERR_TIMEOUT;
        for (int attempt = 0; attempt < HTTP_MAX_ATTEMPTS; attempt++) {
            if (attempt > 0) {
                printf("SD: load retry %d after %d ms\n", attempt, HTTP_RETRY_DELAY_MS);
                sleep_ms(HTTP_RETRY_DELAY_MS);
            }
            rc = http_get(qry, sim_buf + sim_len, room, &got);
            if (rc != HTTP_ERR_TIMEOUT && rc != HTTP_ERR_NETWORK) break;
        }
        if (rc == HTTP_NOT_FOUND) {
            printf("SD: load %s -> not found\n", fname);
            return false;
        }
        if (rc != HTTP_OK) {
            printf("SD: load %s -> http error %d\n", fname, rc);
            return false;
        }
        sim_len += got;
        if (got < room) break;   // server returned less than requested -> EOF
    }
    printf("SD: loaded %s  %u B\n", fname, (unsigned)sim_len);
    return true;
}

// Upload one chunk.  off==0 -> server creates/truncates the file;
// off>0 -> server appends.  Returns true on success.
static bool sd_wifi_save_chunk(const char *fname, uint32_t off,
                               const uint8_t *data, uint16_t len) {
    if (len == 0) return true;
    char qry[80];
    snprintf(qry, sizeof(qry), "/write?path=%s&off=%lu",
             fname, (unsigned long)off);
    uint8_t resp[8];
    uint16_t resp_len = 0;
    int rc = HTTP_ERR_TIMEOUT;
    for (int attempt = 0; attempt < HTTP_MAX_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            printf("SD: upload retry %d after %d ms\n", attempt, HTTP_RETRY_DELAY_MS);
            sleep_ms(HTTP_RETRY_DELAY_MS);
        }
        resp_len = 0;
        rc = http_post(qry, data, len, resp, sizeof(resp), &resp_len);
        if (rc != HTTP_ERR_TIMEOUT && rc != HTTP_ERR_NETWORK) break;
    }
    if (rc != HTTP_OK) {
        printf("SD: upload chunk off=%lu len=%u -> http error %d\n",
               (unsigned long)off, (unsigned)len, rc);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// BAS save as ASCII — detokenizer -- TEST! WILL IT WORK?
//
// When the user types SAVE SD:TEST, the ABC80 ROM stores (should store) the
// program as binary tokens (same as .BAC) rather than ASCII text.
// This function reads the tokenised program from m[BOFA..EOFA-1] and
// converts it to plain ASCII text lines (same format as LIST output),
// then uploads it to the server.
//
// Token format in memory (check the disassembly):
//   Each line: [next_addr:2LE][linenum:2LE][token bytes...][0x00]
//   End of program: 0x01 at EOFA (placed there by sd_handle_open BAC path)
//
// Token byte values:
//   0x00         : end of line
//   0x01         : end of program (at EOFA)
//   0x20–0x7E    : literal ASCII character (pass through, incl ABC80 Swedish chars)
//   0x80–0xBD    : single-byte statement keyword (context: statement start)
//   0xBE–0xFC    : single-byte tokens (mostly unused or internal)
//   0xFF         : two-byte token prefix — read one more byte
//
// Two-byte tokens (0xFF, second_byte):
//   second 0x80–0x95: statement I/O commands (stmt context) or math functions (expr context)
//   second 0xB7–0xBD: structural keywords (TO, STEP, ELSE, THEN, AS, ASFILE)
//   second 0xDD–0xFD: logical/arithmetic operators (AND, OR, NOT, +, -, *, /, ^, comparisons)
//
// Context rules: stmt_mode=true at start of each line.  The first high-byte
// token on a line is a statement keyword; subsequent tokens are expression
// keywords / functions.  THEN and ELSE reset stmt_mode to true (new clause).

// Single-byte statement keywords (used at statement-start position on a line)
static const char *const S_STMT[256] = {
    [0x80] = "GOTO",       [0x82] = "LET",         [0x84] = "PRINT",
    [0x85] = ";",          [0x88] = "IF",          [0x89] = "INPUT",
    [0x8a] = "INPUTLINE",  [0x8b] = "FOR",         [0x8c] = "NEXT",
    [0x8e] = "READ",       [0x8f] = "RESTORE",     [0x90] = "GOSUB",
    [0x91] = "RETURN",     [0x92] = "ON",          [0x93] = "DATA",
    [0x94] = "ONERROR GOTO",                       [0x96] = "DEFFN",
    [0xbd] = "THEN",
};

// Two-byte tokens (0xFF, second_byte) — statement I/O context (0x80–0x95 range)
static const char *const S_FF_STMT[256] = {
    [0x80] = "DIM",        [0x81] = "POKE",        [0x82] = "OUT",
    [0x84] = "REM",        [0x85] = "OPEN",        [0x86] = "PREPARE",
    [0x87] = "CLOSE",      [0x88] = "RANDOMIZE",   [0x89] = "STOP",
    [0x8a] = "END",        [0x8b] = "CMD",         [0x8e] = "SETDOT",
    [0x8f] = "CLRDOT",     [0x90] = "GET",         [0x91] = "CHAIN",
    [0x92] = "KILL",       [0x93] = "NAME",        [0x94] = "TRACE",
    [0x95] = "NOTRACE",
    // structural (always valid)
    [0xb7] = "AS",         [0xb8] = "ASFILE",      [0xba] = "TO",
    [0xbb] = "STEP",       [0xbc] = "ELSE",        [0xbd] = "THEN",
    // logical/arithmetic operators (expression context only; listed here too so
    // we never output a raw token in either mode)
    [0xdd] = "IMP",        [0xde] = "OR",          [0xe0] = "AND",
    [0xe1] = "NOT",        [0xe2] = "=",           [0xe5] = "<>",
    [0xe8] = "<",          [0xeb] = ">=",          [0xee] = ">",
    [0xf1] = "<=",         [0xf4] = "+",           [0xf7] = "-",
    [0xf9] = "*",          [0xfb] = "/",           [0xfd] = "^",
};

// Two-byte tokens (0xFF, second_byte) — expression/function context (0x80–0xB4)
static const char *const S_FF_FUNC[256] = {
    [0x80] = "FN",         [0x81] = "ABS",         [0x82] = "ATN",
    [0x83] = "COS",        [0x84] = "EXP",         [0x85] = "FIX",
    [0x86] = "INT",        [0x87] = "LOG",         [0x88] = "LOG10",
    [0x89] = "PI",         [0x8a] = "RND",         [0x8b] = "SGN",
    [0x8c] = "SIN",        [0x8d] = "SQR",         [0x8e] = "TAN",
    [0x8f] = "ASC",        [0x90] = "CHR$",        [0x91] = "LEFT$",
    [0x92] = "RIGHT$",     [0x93] = "MID$",        [0x94] = "LEN",
    [0x95] = "INSTR",      [0x96] = "SPACE$",      [0x97] = "STRING$",
    [0x98] = "NUM$",       [0x99] = "VAL",         [0x9a] = "SWAP%",
    [0x9b] = "PEEK",       [0x9c] = "INP",         [0x9d] = "CALL",
    [0x9e] = "ERRCODE",    [0x9f] = "IEC$",        [0xa0] = "TAB",
    [0xa1] = "CUR",        [0xa3] = "DOT",         [0xb0] = "ADD$",
    [0xb1] = "SUB$",       [0xb2] = "MUL$",        [0xb3] = "DIV$",
    [0xb4] = "COMP%",
    // structural (same in both contexts)
    [0xb7] = "AS",         [0xb8] = "ASFILE",      [0xba] = "TO",
    [0xbb] = "STEP",       [0xbc] = "ELSE",        [0xbd] = "THEN",
    // operators (same in both contexts)
    [0xdd] = "IMP",        [0xde] = "OR",          [0xe0] = "AND",
    [0xe1] = "NOT",        [0xe2] = "=",           [0xe5] = "<>",
    [0xe8] = "<",          [0xeb] = ">=",          [0xee] = ">",
    [0xf1] = "<=",         [0xf4] = "+",           [0xf7] = "-",
    [0xf9] = "*",          [0xfb] = "/",           [0xfd] = "^",
};

// Helper: is the keyword a "word" (alpha-start) that needs a trailing space,
// or a "symbol" that should be output verbatim with no extra space?
static inline bool kw_needs_space(const char *kw) {
    if (!kw || !kw[0]) return false;
    uint8_t c = (uint8_t)kw[0];
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Write one byte to the upload stream, flushing sim_buf when full.
// Returns false on upload error.
static bool bas_emit(uint8_t byte) {
    sim_buf[sim_len++] = byte;
    if (sim_len >= SIM_BUF_SIZE) {
        if (!sd_wifi_save_chunk(sd_fname, sd_write_off, sim_buf, sim_len))
            return false;
        sd_write_off += sim_len;
        sim_len = 0;
    }
    return true;
}

// Write a NUL-terminated string to the upload stream.
static bool bas_emit_str(const char *s) {
    while (*s) {
        if (!bas_emit((uint8_t)*s++)) return false;
    }
    return true;
}

// Convert tokenised program at m[BOFA..EOFA-1] to ASCII text and upload it.
// Called from sd_handle_close() for SAVE SD:file.BAS (binary-token path).
__attribute__((unused))
static void bas_save_as_text(void) {
    uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
    uint16_t eofa = (uint16_t)(m[0xFE1Eu] | ((uint16_t)m[0xFE1Fu] << 8));

    sim_len      = 0;
    sd_write_off = 0;

    printf("SD: BAS->TEXT  BOFA=%04X  EOFA=%04X\n",
           (unsigned)bofa, (unsigned)eofa);

    uint16_t ptr = bofa;
    int line_count = 0;

    while (ptr < eofa) {
        // End-of-program marker
        if (m[ptr] == 0x01) break;

        // Read line header: [next_addr:2LE][linenum:2LE]
        uint16_t next_addr = (uint16_t)(m[ptr] | ((uint16_t)m[(uint16_t)(ptr+1u)] << 8));
        uint16_t linenum   = (uint16_t)(m[(uint16_t)(ptr+2u)] | ((uint16_t)m[(uint16_t)(ptr+3u)] << 8));

        if (next_addr == 0) break;  // safety: null pointer = end

        // Emit decimal line number + space
        char lnbuf[8];
        int lnlen = snprintf(lnbuf, sizeof(lnbuf), "%u ", (unsigned)linenum);
        for (int i = 0; i < lnlen; i++) {
            if (!bas_emit((uint8_t)lnbuf[i])) goto upload_done;
        }

        // Process token bytes for this line
        uint16_t tp    = (uint16_t)(ptr + 4u);   // token pointer (skip header)
        bool stmt_mode = true;   // true = statement-start, false = expression

        while (tp < eofa) {
            uint8_t tok = m[tp++];
            if (tok == 0x00) break;   // end of line

            if (tok < 0x80) {
                // Literal ASCII character — pass through as-is
                if (!bas_emit(tok)) goto upload_done;
                continue;
            }

            const char *kw = NULL;

            if (tok == 0xFF) {
                // Two-byte token: read second byte
                if (tp >= eofa) break;
                uint8_t tok2 = m[tp++];
                if (stmt_mode) {
                    kw = S_FF_STMT[tok2];
                } else {
                    kw = S_FF_FUNC[tok2];
                    // fall back to stmt table for tokens only listed there
                    if (!kw) kw = S_FF_STMT[tok2];
                }
                // Structural keywords reset statement mode
                if (tok2 == 0xbc || tok2 == 0xbd) stmt_mode = true;  // ELSE / THEN

            } else {
                // Single-byte token
                kw = S_STMT[tok];
                if (stmt_mode) {
                    // First statement keyword — switch to expression mode
                    stmt_mode = false;
                } else if (!kw) {
                    // Not in stmt table — might be an expression-context byte;
                    // output nothing for now (avoids garbled output)
                }
                // THEN (0xBD) seen as single-byte resets to stmt mode
                if (tok == 0xbd) stmt_mode = true;
            }

            if (kw) {
                if (!bas_emit_str(kw)) goto upload_done;
                if (kw_needs_space(kw)) {
                    if (!bas_emit(' ')) goto upload_done;
                }
            } else {
                // Unknown token — emit placeholder so the user can see it
                char ph[8];
                snprintf(ph, sizeof(ph), "[%02X]", (unsigned)tok);
                if (!bas_emit_str(ph)) goto upload_done;
            }
        }

        // End of line: emit CRLF
        if (!bas_emit(0x0D)) goto upload_done;
        if (!bas_emit(0x0A)) goto upload_done;
        line_count++;

        // Advance to next line
        ptr = next_addr;
    }

upload_done:
    // Flush any remaining bytes
    if (sim_len > 0) {
        if (!sd_wifi_save_chunk(sd_fname, sd_write_off, sim_buf, sim_len))
            printf("SD: BAS->TEXT  final flush failed\n");
        else
            sd_write_off += sim_len;
        sim_len = 0;
    }

    printf("SD: BAS->TEXT  done  %d lines  %lu bytes\n",
           line_count, (unsigned long)sd_write_off);
}

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
// Called by: ROM LOAD loop for BAS files (htab index 3 -> SD_TRAP_INPUT)
// Entry:     HL = destination in Z80 RAM, C = max length
// Exit OK:   text at m[HL], m[HL+len] = 0x0D, A=0, CY=0
// Exit EOF:  A=0, CY=1

static void sd_handle_input(void) {
    uint16_t dst    = (uint16_t)((h << 8) | l);
    uint8_t  maxlen = c;
    uint8_t  i;

    // BAC files: data is already in RAM (loaded in OPEN). Return EOF so the
    // ROM's LOAD loop exits cleanly without trying to Radcompile binary tokens.
    if (sim_file_type == FILE_BAC && !sim_is_writing) {
        printf("SD: INPUT  BAC EOF (data already in RAM)\n");
        a  = 0;
        cf = 1;
        sd_ret();
        return;
    }

    // Loop so we skip empty lines (file ends with newline, CRLF residual LF, etc.)
    // rather than returning a bare CR-only line that confuses Radcompile.
    do {
        if (bas_src_pos >= sim_len) {
            printf("SD: INPUT EOF after %d lines\n", bas_line_n);
            a  = 0;
            cf = 1;
            sd_ret();
            return;
        }

        i = 0;
        while (bas_src_pos < sim_len && i < maxlen) {
            uint8_t ch = sim_buf[bas_src_pos++];
            if (ch == 0x0D) break;          // CR ends line (consumed, not stored)
            if (ch == 0x0A) {
                if (i > 0) break;           // LF after content = end of line
                continue;                   // LF at start = skip (CRLF residual)
            }
            m[(uint16_t)(dst + i++)] = ch;
        }
        // If i == 0 here, the line was empty — loop again to get a real line.
    } while (i == 0);

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
    sd_parse_filename(de, sd_fname);
    sim_file_type  = sd_detect_type(de);
    sim_is_writing = false;
    sim_pos        = 0;
    printf("SD: OPEN  %s  type=%d\n", sd_fname, (int)sim_file_type);

    if (!wifi_ready()) {
        printf("SD: OPEN  no WiFi\n");
        sd_error(SD_ERR_NOT_READY);
        return;
    }

    // Download entire file into sim_buf (used for both BAC and BAS load paths).
    if (!sd_wifi_load(sd_fname)) {
        sd_error(SD_ERR_NOT_FOUND);
        return;
    }

    sd_setup_fcb_buf();

    if (sim_file_type == FILE_BAS) {
        // BAS load: ROM calls INPUT handler once per line; sd_handle_input()
        // reads lines from sim_buf.
        bas_src_pos = 0;
        bas_line_n  = 0;
        printf("SD: OPEN BAS  sim_len=%u\n", (unsigned)sim_len);

    } else if (sim_file_type == FILE_BAC) {
        // BAC load: ROM calls BL_IN after OPEN, but expects cassette-framed
        // blocks — we cannot serve raw tokens that way.  Instead, copy the
        // raw program bytes directly into the program area right now so that
        // BL_IN can return EOF (no blocks) and the ROM will proceed to CLOSE
        // with the correct data already in place.
        uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
        if (sim_len > 0) {
            memcpy(m + bofa, sim_buf, sim_len);
            uint16_t new_eofa = (uint16_t)(bofa + sim_len);
            m[new_eofa] = 0x01;   // end marker required by ABC80 interpreter
            uint16_t heap = (uint16_t)(new_eofa + 1u);
            m[0xFE1Eu] = (uint8_t)(new_eofa & 0xFF);
            m[0xFE1Fu] = (uint8_t)(new_eofa >> 8);
            m[0xFE20u] = (uint8_t)(heap & 0xFF);
            m[0xFE21u] = (uint8_t)(heap >> 8);
            printf("SD: OPEN BAC  copied %u bytes  BOFA=%04X  EOFA=%04X  HEAP=%04X\n",
                   (unsigned)sim_len, (unsigned)bofa, (unsigned)new_eofa, (unsigned)heap);
        } else {
            printf("SD: OPEN BAC  sim_buf empty\n");
        }
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
    sd_parse_filename(de, sd_fname);
    sim_file_type  = sd_detect_type(de);
    sim_is_writing = true;
    sim_len        = 0;
    sd_write_off   = 0;
    printf("SD: PREP  %s  type=%d\n", sd_fname, (int)sim_file_type);

    if (!wifi_ready()) {
        printf("SD: PREP  no WiFi\n");
        sd_error(SD_ERR_NOT_READY);
        return;
    }

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

        if (sim_file_type == FILE_BAS &&
            (m[(uint16_t)(ix + 14)] & 0x80) != 0) {
            // -----------------------------------------------------------
            // BAS/LIST write path (after PREPARE / LIST SD:file.BAS)
            //
            // IX+14 bit 7 was set by ROM 0x001B, confirming ASCII data.
            // Full blocks were already streamed in BL_UT; flush the last
            // partial block that remains in the FCB buffer.
            uint16_t buf_start = (uint16_t)(m[(uint16_t)(ix+8)]  | (m[(uint16_t)(ix+9)]  << 8));
            uint16_t buf_cur   = (uint16_t)(m[(uint16_t)(ix+10)] | (m[(uint16_t)(ix+11)] << 8));
            uint16_t remaining = (uint16_t)(buf_cur - buf_start);
            if (remaining > 0) {
                printf("SD: CLOSE  BAS/LIST final %u bytes  off=%lu\n",
                       (unsigned)remaining, (unsigned long)sd_write_off);
                if (!sd_wifi_save_chunk(sd_fname, sd_write_off,
                                        m + buf_start, remaining)) {
                    printf("SD: CLOSE  BAS/LIST final upload failed!\n");
                } else {
                    sd_write_off += remaining;
                }
            }
            printf("SD: CLOSE  BAS/LIST total %lu bytes\n", (unsigned long)sd_write_off);

        } else {
            // -----------------------------------------------------------
            // BAC write path (SAVE SD:file.BAC)
            // NOTE: BAS/SAVE detokenisation to ASCII disabled — falls through
            // to raw token upload for both .BAC and .BAS SAVE.
            //
            // IX+14 bit 7 is clear: data came from the SAVE loop directly,
            // not through ROM 0x001B.  BL_UT data was cassette-format and
            // already ignored.  Upload the raw token bytes from m[BOFA..EOFA-1].
            uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
            uint16_t eofa = (uint16_t)(m[0xFE1Eu] | ((uint16_t)m[0xFE1Fu] << 8));
            uint16_t prog_len = (uint16_t)(eofa - bofa);
            printf("SD: CLOSE  BAC/SAVE  BOFA=%04X  EOFA=%04X  len=%u\n",
                   (unsigned)bofa, (unsigned)eofa, (unsigned)prog_len);

            uint32_t off = 0;
            while (off < prog_len) {
                uint16_t chunk = (uint16_t)(prog_len - (uint16_t)off);
                if (chunk > 2048) chunk = 2048;
                if (!sd_wifi_save_chunk(sd_fname, off, m + bofa + (uint16_t)off, chunk)) {
                    printf("SD: CLOSE  SAVE upload failed at off=%lu\n",
                           (unsigned long)off);
                    sd_error(SD_ERR_NOT_READY);
                    sim_is_writing = false;
                    return;
                }
                off += chunk;
            }
            printf("SD: CLOSE  SAVE uploaded %lu bytes\n", (unsigned long)off);
        }

        sim_is_writing = false;

    } else {
        // ---------------------------------------------------------------
        // Read path (after OPEN / LOAD)

        if (sim_file_type == FILE_BAC) {
            // BAC load: ROM goes OPEN -> CLOSE directly (no BL_IN, no INPUT).
            // Write the raw tokenised bytes from sim_buf back to the program
            // area at BOFA, then fix EOFA and HEAP.
            uint16_t bofa = (uint16_t)(m[0xFE1Cu] | ((uint16_t)m[0xFE1Du] << 8));
            if (sim_len > 0) {
                memcpy(m + bofa, sim_buf, sim_len);
                uint16_t new_eofa = (uint16_t)(bofa + sim_len);
                m[new_eofa] = 0x01;   // end marker required by ABC80 interpreter
                uint16_t heap = (uint16_t)(new_eofa + 1u);
                m[0xFE1Eu] = (uint8_t)(new_eofa & 0xFF);
                m[0xFE1Fu] = (uint8_t)(new_eofa >> 8);
                m[0xFE20u] = (uint8_t)(heap & 0xFF);
                m[0xFE21u] = (uint8_t)(heap >> 8);
                printf("SD: CLOSE BAC load  BOFA=%04X  EOFA=%04X  HEAP=%04X  len=%u\n",
                       (unsigned)bofa, (unsigned)new_eofa, (unsigned)heap, (unsigned)sim_len);
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
    // BAS load uses our INPUT handler (htab index 3), not BL_IN.
    // BAC load: ROM calls BL_IN to read raw token bytes block by block.
    //   We serve sim_buf (downloaded in OPEN) 252 bytes at a time.
    //   CLOSE also does a memcpy fallback in case the ROM skips BL_IN.
    // BAC load: program bytes were already copied into m[BOFA] in sd_handle_open().
    // Return EOF immediately so the ROM proceeds to CLOSE without trying to
    // parse our raw token bytes as cassette-framed blocks (which causes ERR 11/16).
    // For any other call (wrong type, write mode) return NOT_READY.
    if (sim_file_type == FILE_BAC && !sim_is_writing) {
        printf("SD: BL_IN  BAC EOF (data already in RAM)\n");
        a  = 0;
        cf = 1;   // CY=1, A=0 -> EOF, not an error
        sd_ret();
        return;
    }
    printf("SD: BL_IN  unexpected (type=%d writing=%d) -> NOT_READY\n",
           (int)sim_file_type, (int)sim_is_writing);
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

    // BAS LIST save: stream this block to the server via HTTP POST.
    //   IX+14 bit 7 is set by ROM 0x001B when writing ASCII — this means
    //   the data came via LIST (ASCII text).  If bit 7 is clear the data
    //   came from the SAVE loop directly (raw cassette tokens), so we skip
    //   the upload here; sd_handle_close() will upload from BOFA instead.
    // BAC save: BL_UT carries raw tokens too, but we always defer to CLOSE
    //   (upload from BOFA) for BAC, so we never upload in BL_UT for FILE_BAC.
    if (sim_is_writing && sim_file_type == FILE_BAS) {
        bool from_list = (m[(uint16_t)(ix + 14)] & 0x80) != 0;
        if (from_list) {
            printf("SD: BL_UT  BAS/LIST +%u bytes  off=%lu\n",
                   (unsigned)data_len, (unsigned long)sd_write_off);
            if (!sd_wifi_save_chunk(sd_fname, sd_write_off, m + buf_start, data_len)) {
                printf("SD: BL_UT  upload failed!\n");
            } else {
                sd_write_off += data_len;
            }
        } else {
            printf("SD: BL_UT  BAS/SAVE raw tokens — deferred to CLOSE\n");
        }
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
    //   Index order per ABC80 device driver spec (elementärt-enhetslista):
    //     3 = INPUT  (LOAD NAM:, RUN NAM:, INPUT #N)
    //     4 = PRINT  (LIST NAM:, PRINT #N) -> JP 001BH (ROM blocking print)
    //   index 4 (PRINT) --> ROM 0x001B — no trap, use existing ROM routine
    static const uint16_t htab[9] = {
        SD_TRAP_OPEN,    // 0: OPEN
        SD_TRAP_INIT,    // 1: PREPARE
        SD_TRAP_CLOSE,   // 2: CLOSE
        SD_TRAP_INPUT,   // 3: INPUT  (our handler: supply BAS line to LOAD)
        0x001Bu,         // 4: PRINT  (ROM 0x001B blocking print, called by LIST)
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
