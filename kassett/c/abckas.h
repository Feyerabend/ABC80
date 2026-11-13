/*
 * abckas.h  kärnan för kassettverktygen: block, filer och ABC80-signalen.
 *
 * Samma sak som python/abckas.py, i C89. Se FORMAT.md.
 *
 * S. Lönnert 2026. Public domain.
 */
#ifndef ABCKAS_H
#define ABCKAS_H

#include <stddef.h>

#define SYN 0x16
#define STX 0x02
#define ETX 0x03
#define BLOCK 256
#define PAYLOAD 253
#define TEXTMAX 252

#define CLOCK 2995200L
#define T_ZERO 3993L
#define T_ONE 4065L

#define KIND_TEXT 0
#define KIND_BIN 1
#define KIND_BAC 2
#define KIND_GUESS -1

typedef unsigned char byte;

/* en växande lista av block, 256 byte styck */
typedef struct {
    byte (*b)[BLOCK];
    long n, cap;
} Blocks;

void blocks_init(Blocks *bl);
void blocks_add(Blocks *bl, const byte *data);
void blocks_free(Blocks *bl);

/* lager 2 */
unsigned checksum(const byte *data);

/* lager 3 */
void abc_name(const char *path, byte out[11]);
void show_name(const byte *raw11, char *out);     /* UTF-8, minst 34 byte */
int is_name_block(const byte *b);
unsigned block_number(const byte *b);
void make_file(Blocks *bl, const char *path, const byte *data, long len,
               int kind, int tk);                 /* tk < 0: ingen */

typedef struct {
    const byte *header;         /* namnblocket, eller NULL */
    const byte **blk;           /* blk[nr], NULL om det saknas */
    long nblk;                  /* högsta nummer + 1 */
    long count;                 /* antal block som finns */
} TapeFile;

TapeFile *group_files(const Blocks *bl, long *nfiles);
void free_files(TapeFile *f, long n);
int file_kind(const TapeFile *f);
const char *kind_name(int kind);
byte *file_content(const TapeFile *f, long *len);   /* malloc */
long file_missing(const TapeFile *f, long *out);    /* out kan vara NULL */
void file_name(const TapeFile *f, char *out);       /* "" om namnet saknas */

/* .kas */
int read_kas(const char *path, Blocks *bl);         /* 0 = ok */
int write_kas(const char *path, const Blocks *bl);

/* lager 1 */
int write_wav(const char *path, const Blocks *bl, long rate, double lead,
              double gap, double amp, int bits);

typedef struct {
    double t;
    const char *status;         /* "ok", "summa", "etx", "bitar" */
} Report;

typedef void (*LogFn)(const char *msg);

/* avkoda WAV: godkända block i tidsordning och en rapport (malloc) */
int decode_wav(const char *path, int quick, LogFn log,
               Blocks *out, Report **rep, long *nrep);

/* hjälp */
byte *read_file(const char *path, long *len);
int utf8_width(const char *s);
void print_padded(const char *s, int width);

#endif
