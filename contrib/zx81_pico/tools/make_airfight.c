/*
 * make_airfight.c  —  Build airfight.p (ZX81 .p file) from airfight.asm
 *
 * Usage:
 *   cc -std=c11 -Wall -O2 -o make_airfight tools/make_airfight.c
 *   ./make_airfight [games/airfight.asm] [games/airfight.p]
 *
 * Builds a minimal ZX81 .p file that:
 *   1. Lays out system variables at 0x4009–0x407C (standard empty state)
 *   2. Inserts a minimal display file at 0x4090 (793 bytes, all spaces)
 *   3. Adds one BASIC line:  10 RAND USR 32768
 *   4. Appends the assembled game code at 0x8000
 *
 * The .p file starts at address 0x4009 (VERSN sysvar).
 * Addresses are absolute; file offset = address - 0x4009.
 *
 * Key sysvar layout (ZX81, 16K, IY=0x4000):
 *   0x4009  VERSN       1 byte  = 0
 *   0x400A  E_PPC       2 bytes = 10 (line number of auto-run line)
 *   0x400C  D_FILE      2 bytes = 0x4090 (display file pointer)
 *   0x400E  DF_CC       2 bytes = 0x4091 (cursor, first byte of dfile content)
 *   0x4010  VARS        2 bytes = 0x43A9 (variables pointer)
 *   0x4012  DEST        2 bytes = 0x0000
 *   0x4014  E_LINE      2 bytes = 0x43AA
 *   0x4016  CH_ADD      2 bytes = 0x43AA
 *   0x4018  X_PTR       2 bytes = 0x0000
 *   0x401A  STKBOT      2 bytes = 0x43AC
 *   0x401C  STKEND      2 bytes = 0x43AC
 *   0x401E  BERG        1 byte  = 0
 *   0x401F  MEM         2 bytes = 0x4000
 *   0x4021  (unused)    1 byte  = 0
 *   0x4022  DF_SZ       1 byte  = 2
 *   0x4023  S_TOP       2 bytes = 10
 *   0x4025  LAST_K      2 bytes = 0
 *   0x4027  DEBOUNCE    1 byte  = 0
 *   0x4028  MARGIN      1 byte  = 55
 *   0x4029  NXTLIN      2 bytes = 0x407D (address of BASIC line 10)
 *   0x402B  OLDPPC      2 bytes = 0
 *   0x402D  FLAGX       1 byte  = 0
 *   0x402E  STRLEN      2 bytes = 0
 *   0x4030  T_ADDR      2 bytes = 0
 *   0x4032  SEED        2 bytes = 0
 *   0x4034  FRAMES      3 bytes = 0,0,0
 *   0x4037  UDG         2 bytes = 0x3C00 (not used but standard)
 *   0x4039  COORDS      2 bytes = 0
 *   0x403B  PR_CC       1 byte  = 0x21 (33)
 *   0x403C  ECHO_E      2 bytes = 0x43AB (DF_CC in printer buffer)
 *   0x403E  DF_CA       2 bytes = 0x43AB
 *   0x4040  PRBUFF      33 bytes = 0x76,0x00,...
 *   0x4062  MEMBOT      30 bytes = 0
 *   0x4080  (calculator memory)
 *
 * Simpler: we only need the sysvars the ROM uses when jumping to 0x0207.
 * Many sysvars can be 0.  The critical ones are:
 *   D_FILE (0x400C):  → our display file at 0x4090
 *   NXTLIN (0x4029):  → first BASIC line so auto-run works after ROM 0x0207
 *   VARS   (0x4010):  → 0x43A9 (end of BASIC program + 1)
 *   E_LINE (0x4014):  → 0x43AA
 *   STKBOT (0x401A):  → 0x43AC
 *   STKEND (0x401C):  → 0x43AC
 *
 * BASIC line 10 at 0x407D:
 *   0x00 0x0A  = line 10 (big-endian)
 *   0x0F 0x00  = length 15 (little-endian)
 *   0xFD       = RAND token
 *   0xD8       = USR token
 *   0x1F 0x1E 0x23 0x22 0x24  = chars "32768" in ZX81 encoding (3=0x1F,...)
 *   0x7E       = number follows
 *   0x8F 0x00 0x00 0x00 0x00  = float 32768.0
 *   0x0D       = NEWLINE
 *
 * Display file at 0x4090: 793 bytes
 *   0x76  (leading HALT)
 *   24 × (32 × 0x00  +  0x76)  = 24 × 33 = 792 bytes
 *
 * VARS at 0x43A9: 0x80 (end-of-vars marker)
 * E_LINE at 0x43AA: 0x76 0x0D (empty editor line)
 *
 * Game code at 0x8000.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* ---- Embed z80asm as a library ------------------------------------------ */
#define Z80ASM_EMBEDDED
#include "../asm/z80asm.c"

/* ---- .p file layout constants ------------------------------------------- */

#define PFILE_ORIGIN    0x4009u   /* .p file starts at VERSN */
#define DFILE_ADDR      0x8600u   /* display file: after game code (0x8000-0x851E) */
#define DFILE_ROWS      24
#define DFILE_COLS      32
#define DFILE_SIZE      (1 + DFILE_ROWS * (DFILE_COLS + 1))   /* 793 */
#define BASIC_LINE_ADDR 0x407Du   /* standard ZX81 BASIC start (sysvars = 0x74 bytes) */
#define VARS_ADDR       0x408Fu   /* BASIC_LINE_ADDR + 18 (4 header + 14 body) */
#define ELINE_ADDR      0x4090u
#define STKBOT_ADDR     0x4092u

#define GAME_CODE_ADDR  0x8000u
#define RAM_END         0xBFFFu

/* Total .p file covers 0x4009 to GAME_CODE_ADDR + game_code_size - 1 */

static uint8_t pfile[RAM_END - PFILE_ORIGIN + 1];

static void w16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void asm_msg(const char *s) {
    fputs(s, stderr);
}

int main(int argc, char **argv)
{
    const char *asm_path = "games/airfight.asm";
    const char *out_path = "games/airfight.p";

    if (argc >= 2) asm_path = argv[1];
    if (argc >= 3) out_path = argv[2];

    /* ---- Read assembly source ------------------------------------------- */
    FILE *f = fopen(asm_path, "r");
    if (!f) { perror(asm_path); return 1; }
    fseek(f, 0, SEEK_END);
    long src_len = ftell(f);
    rewind(f);
    char *src = malloc((size_t)src_len + 1);
    if (!src) { fputs("out of memory\n", stderr); return 1; }
    fread(src, 1, (size_t)src_len, f);
    src[src_len] = '\0';
    fclose(f);

    /* ---- Assemble into a flat 64KB buffer -------------------------------- */
    uint8_t mem[65536];
    memset(mem, 0, sizeof(mem));

    int rc = z80asm_assemble(src, (int)src_len, mem, GAME_CODE_ADDR,
                              asm_msg, false);
    free(src);
    if (rc < 0) {
        fprintf(stderr, "%d assembly error(s)\n", -rc);
        return 1;
    }

    /* Figure out last byte actually written (scan from RAM_END downward) */
    int game_end = GAME_CODE_ADDR;
    for (int i = RAM_END; i >= (int)GAME_CODE_ADDR; i--) {
        if (mem[i]) { game_end = i; break; }
    }
    int game_size = game_end - GAME_CODE_ADDR + 1;
    fprintf(stderr, "Game code: 0x%04X–0x%04X (%d bytes)\n",
            GAME_CODE_ADDR, game_end, game_size);

    /* ---- Build .p file --------------------------------------------------- */
    memset(pfile, 0, sizeof(pfile));

#define WR(addr, val) pfile[(addr) - PFILE_ORIGIN] = (val)
#define WR16(addr, val) w16le(pfile + ((addr) - PFILE_ORIGIN), (val))

    /* Sysvars */
    WR(0x4009, 0x00);               /* VERSN */
    WR16(0x400A, 10);               /* E_PPC = line 10 */
    WR16(0x400C, DFILE_ADDR);       /* D_FILE */
    WR16(0x400E, DFILE_ADDR + 1);   /* DF_CC */
    WR16(0x4010, VARS_ADDR);        /* VARS */
    WR16(0x4014, ELINE_ADDR);       /* E_LINE */
    WR16(0x4016, ELINE_ADDR);       /* CH_ADD */
    WR16(0x401A, STKBOT_ADDR);      /* STKBOT */
    WR16(0x401C, STKBOT_ADDR);      /* STKEND */
    WR(0x401E, 0x00);               /* BERG */
    WR16(0x401F, 0x4000);           /* MEM */
    WR(0x4022, 2);                  /* DF_SZ */
    WR16(0x4023, 10);               /* S_TOP */
    WR(0x4028, 55);                 /* MARGIN */
    WR16(0x4029, BASIC_LINE_ADDR);  /* NXTLIN → line 10 */
    WR(0x403B, 0x40);               /* CDFLAG: bit6=1,bit7=0 → ROM L0207 enters BASIC executor */

    /* BASIC line 10: RAND USR 32768 */
    {
        uint8_t *p = pfile + (BASIC_LINE_ADDR - PFILE_ORIGIN);
        /* Line number: 10 (big-endian) */
        p[0] = 0x00;
        p[1] = 0x0A;
        /* Line length: 15 bytes (little-endian) */
        /* Body: FD D8 [chars "32768"] 7E [float 32768] 0D */
        /* "32768" in ZX81 char encoding:
         *   '3'=0x1F '2'=0x1E '7'=0x23 '6'=0x22 '8'=0x24 */
        /* ZX81 BASIC line body: tokens + number literal + 0x0D
         * ZX81 BASIC line format: line_hi line_lo len_lo len_hi <body> 0x0D
         * len = bytes from after header up to and including 0x0D */
        uint8_t body[] = {
            0xFA,                               /* RAND token */
            0xD5,                               /* USR token */
            0x1F, 0x1E, 0x23, 0x22, 0x24,       /* "32768" in ZX81 char codes */
            0x7E,                               /* number-follows marker */
            0x90, 0x00, 0x00, 0x00, 0x00,       /* float 32768.0: exp=0x90=144, 2^(144-128)×0.5=32768 */
            0x76                                /* NEWLINE (ZX81 BASIC line terminator = 0x76) */
        };
        uint16_t body_len = (uint16_t)sizeof(body);
        p[2] = (uint8_t)(body_len & 0xFF);      /* length lo */
        p[3] = (uint8_t)(body_len >> 8);        /* length hi */
        memcpy(p + 4, body, body_len);
    }

    /* Display file at DFILE_ADDR: leading HALT + 24 rows */
    {
        uint8_t *d = pfile + (DFILE_ADDR - PFILE_ORIGIN);
        d[0] = 0x76;    /* leading HALT */
        int off = 1;
        for (int row = 0; row < DFILE_ROWS; row++) {
            memset(d + off, 0x00, DFILE_COLS);  /* 32 spaces */
            off += DFILE_COLS;
            d[off++] = 0x76;                    /* row HALT */
        }
    }

    /* VARS: 0x80 = end-of-vars marker */
    pfile[VARS_ADDR - PFILE_ORIGIN] = 0x80;

    /* E_LINE: 0x0D (empty editor line — just a NEWLINE byte) */
    pfile[ELINE_ADDR - PFILE_ORIGIN] = 0x0D;

    /* Game code: copy assembled bytes */
    memcpy(pfile + (GAME_CODE_ADDR - PFILE_ORIGIN), mem + GAME_CODE_ADDR,
           (size_t)game_size);

    /* ---- Write output file ----------------------------------------------- */
    /* File must cover up to end of display file (whichever comes last) */
    uint32_t dfile_end = DFILE_ADDR + DFILE_SIZE;
    uint32_t code_end  = GAME_CODE_ADDR + (uint32_t)game_size;
    uint32_t last_addr = dfile_end > code_end ? dfile_end : code_end;
    uint32_t pfile_size = last_addr - PFILE_ORIGIN;

    f = fopen(out_path, "wb");
    if (!f) { perror(out_path); return 1; }
    fwrite(pfile, 1, pfile_size, f);
    fclose(f);

    fprintf(stderr, ".p file: %s  (%u bytes, covers 0x%04X–0x%04X)\n",
            out_path, pfile_size, PFILE_ORIGIN,
            (unsigned)(PFILE_ORIGIN + pfile_size - 1));

    /* ---- Write C header with raw game binary ----------------------------- */
    /* Path is relative to project root (build_airfight.sh cds there first). */
    f = fopen("src/airfight_bin.h", "w");
    if (!f) { perror("src/airfight_bin.h"); return 1; }
    fprintf(f, "/* Auto-generated by make_airfight — do not edit */\n");
    fprintf(f, "#pragma once\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#define AIRFIGHT_ORG 0x%04Xu\n\n", GAME_CODE_ADDR);
    fprintf(f, "static const uint8_t airfight_bin[] = {\n    ");
    for (int i = 0; i < game_size; i++) {
        fprintf(f, "0x%02X", mem[GAME_CODE_ADDR + i]);
        if (i + 1 < game_size)
            fprintf(f, (i % 16 == 15) ? ",\n    " : ", ");
    }
    fprintf(f, "\n};\n");
    fclose(f);
    fprintf(stderr, "Header: src/airfight_bin.h  (%d bytes of game code)\n", game_size);

    return 0;
}
