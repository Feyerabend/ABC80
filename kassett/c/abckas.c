/*
 * abckas.c  kärnan för kassettverktygen: block, filer och ABC80-signalen.
 *
 * Lagren (se FORMAT.md):
 *   1 signal   FM som ABC80-ROM:en skriver den (CASWBL/CASWBI), till och från WAV
 *   2 block    256 nollbitar, 3 x SYN, STX, 256 byte, ETX, summa (lo, hi)
 *   3 fil      namnblock FF FF FF + namn (8 + 3) [+ tidkonstant, ABC 800],
 *              datablock 00 + blocknummer (lo, hi) + 253 byte
 *   4 innehåll text som TXTPRT skriver den ($09 n = n blanksteg, $03 =
 *              blockslut, sist ett block med sex nollor), annars rå data
 *
 * Samma sak som python/abckas.py, och samma utdata byte för byte.
 *
 * S. Lonnert 2026. Public domain.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abckas.h"

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "slut på minne\n");
        exit(2);
    }
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "slut på minne\n");
        exit(2);
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* listor */

void blocks_init(Blocks *bl)
{
    bl->b = NULL;
    bl->n = bl->cap = 0;
}

void blocks_add(Blocks *bl, const byte *data)
{
    if (bl->n == bl->cap) {
        bl->cap = bl->cap ? 2 * bl->cap : 64;
        bl->b = (byte (*)[BLOCK]) xrealloc(bl->b, (size_t) bl->cap * BLOCK);
    }
    memcpy(bl->b[bl->n++], data, BLOCK);
}

void blocks_free(Blocks *bl)
{
    free(bl->b);
    blocks_init(bl);
}

typedef struct {
    byte *p;
    long n, cap;
} Buf;

static void buf_add(Buf *b, int c)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? 2 * b->cap : 1024;
        b->p = (byte *) xrealloc(b->p, (size_t) b->cap);
    }
    b->p[b->n++] = (byte) c;
}

/* ------------------------------------------------------------------ */
/* lager 2: block */

unsigned checksum(const byte *data)
{
    unsigned s = 3;
    int i;
    for (i = 0; i < BLOCK; i++)
        s += data[i];
    return s & 0xFFFFu;
}

/* SYN SYN SYN STX data ETX summa; ABC 800 (CASWR): SYN STX data ETX
   summa FF FF */
static int block_bytes(const byte *data, byte *out, int abc800)
{
    unsigned s = checksum(data);
    int k = 0;
    if (!abc800)
        out[k++] = SYN, out[k++] = SYN;
    out[k++] = SYN;
    out[k++] = STX;
    memcpy(out + k, data, BLOCK);
    k += BLOCK;
    out[k++] = ETX;
    out[k++] = (byte) (s & 0xFF);
    out[k++] = (byte) (s >> 8);
    if (abc800)
        out[k++] = 0xFF, out[k++] = 0xFF;
    return k;
}

static int block_tk(const byte *b)
{
    return b[14] ? b[14] : TK800;
}

/* ------------------------------------------------------------------ */
/* lager 3: filer */

/* den svenska 7-bitarskoden: kod och Latin-1 */
static const int swedish[][2] = {
    {0x40, 0xC9}, {0x5B, 0xC4}, {0x5C, 0xD6}, {0x5D, 0xC5}, {0x5E, 0xDC},
    {0x60, 0xE9}, {0x7B, 0xE4}, {0x7C, 0xF6}, {0x7D, 0xE5}, {0x7E, 0xFC},
    {0, 0}
};

static int put_utf8(char *out, int code)
{
    if (code < 0x80) {
        out[0] = (char) code;
        return 1;
    }
    out[0] = (char) (0xC0 | (code >> 6));
    out[1] = (char) (0x80 | (code & 0x3F));
    return 2;
}

void abc_name(const char *path, byte out[11])
{
    const char *base = strrchr(path, '/');
    const unsigned char *s;
    byte name[256];
    int n = 0, i, dot = -1, k;

    base = base ? base + 1 : path;
    s = (const unsigned char *) base;
    while (*s && n < 255) {
        int c = *s++;
        if ((c & 0xE0) == 0xC0 && (*s & 0xC0) == 0x80) {
            c = ((c & 0x1F) << 6) | (*s++ & 0x3F);
        } else if (c >= 0xE0 && c < 0xF8) {      /* tre eller fyra byte */
            while ((*s & 0xC0) == 0x80)
                s++;
            c = '?';
        }
        if (c >= 'a' && c <= 'z')
            c -= 32;
        else if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
            c -= 32;
        for (k = 0; swedish[k][0]; k++)
            if (swedish[k][1] == c)
                c = swedish[k][0];
        if (c > 0xFF)
            c = '?';
        name[n++] = (byte) c;
    }
    for (i = 0; i < n; i++)
        if (name[i] == '.')
            dot = i;
    memset(out, ' ', 11);
    if (dot < 0) {
        for (i = 0; i < n && i < 8; i++)
            out[i] = name[i];
    } else {
        for (i = 0; i < dot && i < 8; i++)
            out[i] = name[i];
        for (i = 0; dot + 1 + i < n && i < 3; i++)
            out[8 + i] = name[dot + 1 + i];
    }
}

static int is_space(int c)
{
    return c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)
        || c == 0x85 || c == 0xA0;
}

static int put_abc_char(char *out, int c)
{
    int k;
    if (c == 0x24)
        return put_utf8(out, '$');
    for (k = 0; swedish[k][0]; k++)
        if (swedish[k][0] == c)
            return put_utf8(out, swedish[k][1]);
    return put_utf8(out, c);
}

void show_name(const byte *raw11, char *out)
{
    int a = 8, b = 3, i;
    while (a > 0 && is_space(raw11[a - 1]))
        a--;
    while (b > 0 && is_space(raw11[8 + b - 1]))
        b--;
    for (i = 0; i < a; i++)
        out += put_abc_char(out, raw11[i]);
    if (b) {
        *out++ = '.';
        for (i = 0; i < b; i++)
            out += put_abc_char(out, raw11[8 + i]);
    }
    *out = 0;
}

int is_name_block(const byte *b)
{
    return b[0] == 0xFF && b[1] == 0xFF && b[2] == 0xFF;
}

unsigned block_number(const byte *b)
{
    return b[1] | (b[2] << 8);
}

static void add_data_block(Blocks *bl, long nr, const byte *p, long len)
{
    byte b[BLOCK];
    memset(b, 0, BLOCK);
    b[1] = (byte) (nr & 0xFF);
    b[2] = (byte) ((nr >> 8) & 0xFF);
    if (len > 0)
        memcpy(b + 3, p, (size_t) len);
    blocks_add(bl, b);
}

/* text som TXTPRT och TXTEND skriver den; cur är ROM:ens buffert, som
   skrivs hel och inte nollas mellan blocken; basic2 som BASIC II i
   ABC 800-serien, som skriver ett vanligt blanksteg på sista platsen
   (se abckas.py) */
static void text_blocks(Blocks *bl, const byte *data, long len, int basic2)
{
    byte cur[PAYLOAD];
    long n = 0, i, j, t = 0, nr = 0;
    byte *s = (byte *) xmalloc((size_t) len);
    int c;

    memset(cur, 0, PAYLOAD);
    /* radslutet blir CR: CRLF och en ensam LF */
    for (i = 0; i < len; i++) {
        if (data[i] == 0x0A) {
            if (i > 0 && data[i - 1] == 0x0D)
                continue;
            s[n++] = 0x0D;
        } else
            s[n++] = data[i];
    }

#define PUT() do { cur[t++] = ETX; add_data_block(bl, nr++, cur, PAYLOAD); t = 0; } while (0)

    i = 0;
    while (i < n) {
        c = s[i];
        if (c == 0x20 && i + 1 < n && s[i + 1] == 0x20) {
            j = i;
            while (j < n && s[j] == 0x20 && j - i < 255)
                j++;
            if (basic2 && t == TEXTMAX - 1) {
                cur[t++] = 0x20;                /* sista platsen: ingen packning */
                i++;
            } else {
                if (!basic2 && TEXTMAX - t <= 2)
                    PUT();
                cur[t++] = 0x09;
                cur[t++] = (byte) (j - i);
                i = j;
            }
        } else {
            cur[t++] = (byte) c;
            i++;
        }
        if (t >= TEXTMAX)
            PUT();
    }
    PUT();                                      /* TXTEND: aktuellt block ... */
    memset(cur, 0, 6);                          /* ... och sex nollor */
    t = 6;
    PUT();
#undef PUT
    free(s);
}

static int ext_is_text(const char *path)
{
    const char *base = strrchr(path, '/'), *dot;
    char e[5];
    int i;
    base = base ? base + 1 : path;
    while (*base == '.')
        base++;
    dot = strrchr(base, '.');
    if (!dot || strlen(dot) != 4)
        return 0;
    for (i = 0; i < 4; i++)
        e[i] = (char) ((dot[i] >= 'a' && dot[i] <= 'z') ? dot[i] - 32 : dot[i]);
    e[4] = 0;
    return !strcmp(e, ".BAS") || !strcmp(e, ".TXT") || !strcmp(e, ".ASC");
}

/* bara skrivbara 7-bitarstecken och radslut */
static int plain_text(const byte *data, long len)
{
    long i;
    if (len <= 0)
        return 0;
    for (i = 0; i < len; i++)
        if (!((data[i] >= 0x20 && data[i] < 0x7F) || data[i] == 0x0A || data[i] == 0x0D))
            return 0;
    return 1;
}

void make_file(Blocks *bl, const char *path, const byte *data, long len,
               int kind, int tk)
{
    byte h[BLOCK];
    long i, n;

    if (kind == KIND_GUESS)
        kind = ext_is_text(path) || plain_text(data, len) ? KIND_TEXT : KIND_BIN;
    memset(h, 0, BLOCK);
    h[0] = h[1] = h[2] = 0xFF;
    abc_name(path, h + 3);
    if (tk >= 0)
        h[14] = (byte) tk;
    blocks_add(bl, h);

    if (kind == KIND_TEXT) {
        text_blocks(bl, data, len, tk >= 0);
    } else if (len > 0 && (data[0] == 0x82 || data[0] == 0x8F) && len % BLOCK == 0) {
        /* BAC från skivan: 256-bytes sektorer, de 253 första går på bandet */
        for (i = 0; i < len / BLOCK; i++)
            add_data_block(bl, i, data + i * BLOCK, PAYLOAD);
    } else {
        n = (len + PAYLOAD - 1) / PAYLOAD;
        if (n < 1)
            n = 1;
        for (i = 0; i < n; i++) {
            long m = len - i * PAYLOAD;
            add_data_block(bl, i, data + i * PAYLOAD, m > PAYLOAD ? PAYLOAD : m);
        }
    }
}

TapeFile *group_files(const Blocks *bl, long *nfiles)
{
    TapeFile *f = NULL, *cur = NULL;
    long n = 0, cap = 0, i;

    for (i = 0; i < bl->n; i++) {
        const byte *b = bl->b[i];
        int name = is_name_block(b);
        long nr;
        if (!name && b[0] != 0)
            continue;
        nr = (long) block_number(b);
        if (name || cur == NULL || (nr == 0 && cur->count > 0)) {
            if (n == cap) {
                cap = cap ? 2 * cap : 16;
                f = (TapeFile *) xrealloc(f, (size_t) cap * sizeof *f);
            }
            cur = &f[n++];
            cur->header = name ? b : NULL;
            cur->blk = NULL;
            cur->nblk = cur->count = 0;
            if (name)
                continue;
        }
        if (nr >= cur->nblk) {
            long k;
            cur->blk = (const byte **) xrealloc((void *) cur->blk,
                                                (size_t) (nr + 1) * sizeof(byte *));
            for (k = cur->nblk; k <= nr; k++)
                cur->blk[k] = NULL;
            cur->nblk = nr + 1;
        }
        if (!cur->blk[nr]) {
            cur->blk[nr] = b;
            cur->count++;
        }
    }
    *nfiles = n;
    return f;
}

void free_files(TapeFile *f, long n)
{
    long i;
    for (i = 0; i < n; i++)
        free((void *) f[i].blk);
    free(f);
}

static int looks_like_text(const TapeFile *f)
{
    long nr;
    if (f->count == 0)
        return 0;
    for (nr = 0; nr < f->nblk; nr++) {
        const byte *p;
        int i;
        if (!f->blk[nr])
            continue;
        p = f->blk[nr] + 3;
        /* ETX som TXTINP letar: n i $09 n hoppas över (det kan vara 3) */
        for (i = 0; i < PAYLOAD && p[i] != ETX;) {
            if (p[i] == 0x09)
                i += 2;
            else if (p[i] >= 0x80)
                return 0;
            else
                i++;
        }
        if (i >= PAYLOAD)
            return 0;
    }
    return 1;
}

int file_kind(const TapeFile *f)
{
    long nr;
    for (nr = 0; nr < f->nblk; nr++)
        if (f->blk[nr]) {
            byte c = f->blk[nr][3];
            if (c == 0x82 || c == 0x8F)
                return KIND_BAC;
            break;
        }
    return looks_like_text(f) ? KIND_TEXT : KIND_BIN;
}

const char *kind_name(int kind)
{
    return kind == KIND_TEXT ? "TEXT" : kind == KIND_BAC ? "BAC" : "BIN";
}

byte *file_content(const TapeFile *f, long *len)
{
    Buf b;
    long nr;
    int kind = file_kind(f), i;

    b.p = NULL;
    b.n = b.cap = 0;
    for (nr = 0; nr < f->nblk; nr++) {
        const byte *p;
        if (!f->blk[nr])
            continue;
        p = f->blk[nr] + 3;
        if (kind == KIND_TEXT) {
            for (i = 0; i < PAYLOAD;) {
                if (p[i] == ETX)
                    break;
                if (p[i] == 0x00)
                    goto done;
                if (p[i] == 0x09 && i + 1 < PAYLOAD) {
                    int k;
                    for (k = 0; k < p[i + 1]; k++)
                        buf_add(&b, ' ');
                    i += 2;
                    continue;
                }
                buf_add(&b, p[i]);
                i++;
            }
        } else {
            for (i = 0; i < PAYLOAD; i++)
                buf_add(&b, p[i]);
            if (kind == KIND_BAC)
                for (i = 0; i < 3; i++)
                    buf_add(&b, 0);
        }
    }
done:
    *len = b.n;
    if (!b.p)
        b.p = (byte *) xmalloc(1);
    return b.p;
}

long file_missing(const TapeFile *f, long *out)
{
    long nr, n = 0;
    for (nr = 0; nr < f->nblk; nr++)
        if (!f->blk[nr]) {
            if (out)
                out[n] = nr;
            n++;
        }
    return n;
}

void file_name(const TapeFile *f, char *out)
{
    if (f->header)
        show_name(f->header + 3, out);
    else
        out[0] = 0;
}

/* ------------------------------------------------------------------ */
/* filer på disken */

byte *read_file(const char *path, long *len)
{
    FILE *fp = fopen(path, "rb");
    Buf b;
    int c;
    if (!fp)
        return NULL;
    b.p = NULL;
    b.n = b.cap = 0;
    while ((c = getc(fp)) != EOF)
        buf_add(&b, c);
    fclose(fp);
    *len = b.n;
    if (!b.p)
        b.p = (byte *) xmalloc(1);
    return b.p;
}

int read_kas(const char *path, Blocks *bl)
{
    long len, i;
    byte *d = read_file(path, &len);
    if (!d) {
        perror(path);
        return 1;
    }
    if (len % BLOCK) {
        fprintf(stderr, "%s: %ld byte är inte en multipel av 256\n", path, len);
        free(d);
        return 1;
    }
    for (i = 0; i < len; i += BLOCK)
        blocks_add(bl, d + i);
    free(d);
    return 0;
}

int write_kas(const char *path, const Blocks *bl)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return 1;
    }
    if (bl->n)
        fwrite(bl->b, BLOCK, (size_t) bl->n, fp);
    return fclose(fp) ? 1 : 0;
}

int utf8_width(const char *s)
{
    int n = 0;
    for (; *s; s++)
        if ((*s & 0xC0) != 0x80)
            n++;
    return n;
}

void print_padded(const char *s, int width)
{
    int w = utf8_width(s);
    fputs(s, stdout);
    for (; w < width; w++)
        putchar(' ');
}

/* ------------------------------------------------------------------ */
/* lager 1: ABC80-signalen ut */

static void put_le(FILE *fp, unsigned long v, int n)
{
    while (n--) {
        putc((int) (v & 0xFF), fp);
        v >>= 8;
    }
}

static void wav_header(FILE *fp, long rate, int bits, unsigned long datalen)
{
    int bw = bits / 8;
    fwrite("RIFF", 1, 4, fp);
    put_le(fp, 36 + datalen, 4);
    fwrite("WAVEfmt ", 1, 8, fp);
    put_le(fp, 16, 4);
    put_le(fp, 1, 2);
    put_le(fp, 1, 2);
    put_le(fp, (unsigned long) rate, 4);
    put_le(fp, (unsigned long) rate * bw, 4);
    put_le(fp, (unsigned long) bw, 2);
    put_le(fp, (unsigned long) bits, 2);
    fwrite("data", 1, 4, fp);
    put_le(fp, datalen, 4);
}

typedef struct {
    FILE *fp;
    int bits;
    unsigned long n;            /* sampel */
} Out;

static void samples(Out *o, int v, long count)
{
    for (; count > 0; count--, o->n++) {
        if (o->bits == 8)
            putc((v + 128) & 0xFF, o->fp);
        else
            put_le(o->fp, (unsigned long) (v & 0xFFFF), 2);
    }
}

int write_wav(const char *path, const Blocks *bl, long rate, double lead,
              double gap, double amp, int bits, int abc800)
{
    Out o;
    int hi = (int) (amp * (bits == 8 ? 127 : 32767));
    long i, clock = abc800 ? CLOCK800 : CLOCK, tk = TK800;

    o.fp = fopen(path, "wb");
    if (!o.fp) {
        perror(path);
        return 1;
    }
    o.bits = bits;
    o.n = 0;
    wav_header(o.fp, rate, bits, 0);

    for (i = 0; i < bl->n; i++) {
        const byte *b = bl->b[i];
        byte by[263];
        long t = 0, prev = 0, s, e, zero = T_ZERO, one = T_ONE;
        int level = -hi, k, bit, nb, j, name = is_name_block(b);

        samples(&o, 0, (long) ((name ? lead : gap) * rate));
        if (abc800) {           /* 32 x tk T; namnblocket med tk 129 */
            zero = one = 32 * (name ? TK800 : tk);
            if (name)
                tk = block_tk(b);
        }
        nb = block_bytes(b, by, abc800);
        /* 256 nollbitar, sedan byten med minst signifikanta biten först.
           Varje bitcell slutar med en flank, en etta har en till i mitten. */
        for (j = -32; j < nb; j++)
            for (k = 0; k < 8; k++) {
                int nedge, m;
                long edge[2];
                bit = j < 0 ? 0 : (by[j] >> k) & 1;
                if (bit) {
                    edge[0] = t + one / 2;
                    t += one;
                    edge[1] = t;
                    nedge = 2;
                } else {
                    t += zero;
                    edge[0] = t;
                    nedge = 1;
                }
                for (m = 0; m < nedge; m++) {
                    e = edge[m];
                    s = (long) floor(((double) e * rate + clock / 2) / clock);
                    samples(&o, level, s - prev);
                    prev = s;
                    level = -level;
                }
            }
        samples(&o, level, (long) (0.002 * rate));   /* håll sista nivån */
    }
    samples(&o, 0, (long) (gap * rate));

    fseek(o.fp, 0, SEEK_SET);
    wav_header(o.fp, rate, bits, o.n * (unsigned long) (bits / 8));
    return fclose(o.fp) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* lager 1: signalen in */

static unsigned long get_le(const byte *p, int n)
{
    unsigned long v = 0;
    while (n--)
        v = (v << 8) | p[n];
    return v;
}

/* kanalerna som double, en efter en: x[c * n + i] */
static double *read_wav(const char *path, long *sr, int *nch, long *nsamp)
{
    long len, pos = 12, i, n = 0;
    byte *d = read_file(path, &len);
    int fmt = 0, ch = 0, bits = 0, c, w;
    const byte *data = NULL;
    unsigned long dlen = 0;
    double *x;

    if (!d) {
        perror(path);
        return NULL;
    }
    if (len < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) {
        fprintf(stderr, "%s: inte en WAV-fil\n", path);
        free(d);
        return NULL;
    }
    while (pos + 8 <= len) {
        unsigned long size = get_le(d + pos + 4, 4);
        if (!memcmp(d + pos, "fmt ", 4)) {
            fmt = (int) get_le(d + pos + 8, 2);
            ch = (int) get_le(d + pos + 10, 2);
            *sr = (long) get_le(d + pos + 12, 4);
            bits = (int) get_le(d + pos + 22, 2);
        } else if (!memcmp(d + pos, "data", 4)) {
            data = d + pos + 8;
            dlen = size;
            if ((long) dlen > len - pos - 8)
                dlen = (unsigned long) (len - pos - 8);
            break;
        }
        pos += 8 + (long) size + (long) (size & 1);
    }
    if ((fmt != 1 && fmt != 0xFFFE) || !data || ch < 1 ||
        (bits != 8 && bits != 16 && bits != 24 && bits != 32)) {
        fprintf(stderr, "%s: bara PCM med 8, 16, 24 eller 32 bitar\n", path);
        free(d);
        return NULL;
    }
    w = bits / 8;
    n = (long) (dlen / (unsigned long) (w * ch));
    x = (double *) xmalloc((size_t) n * ch * sizeof(double));
    for (i = 0; i < n; i++)
        for (c = 0; c < ch; c++) {
            const byte *p = data + (i * ch + c) * w;
            double v;
            if (w == 1) {
                v = (double) p[0] - 128.0;
            } else {
                long u = (long) get_le(p, w);
                long top = 1L << (bits - 1);
                if (bits == 32)
                    v = (double) (unsigned long) u >= 2147483648.0
                        ? (double) (unsigned long) u - 4294967296.0
                        : (double) (unsigned long) u;
                else
                    v = (double) (u >= top ? u - 2 * top : u);
            }
            x[c * n + i] = v;
        }
    free(d);
    *nch = ch;
    *nsamp = n;
    return x;
}

static void moving_average(const double *x, long n, long k, double *out)
{
    double *c = (double *) xmalloc((size_t) (n + 1) * sizeof(double));
    long i, pad, m;
    if (k > n)
        k = n;
    if (k < 1)
        k = 1;
    c[0] = 0;
    for (i = 0; i < n; i++)
        c[i + 1] = c[i] + x[i];
    m = n - k + 1;
    pad = k / 2;
    for (i = 0; i < n; i++) {
        long j = i - pad;
        if (j < 0)
            j = 0;
        if (j > m - 1)
            j = m - 1;
        out[i] = (c[j + k] - c[j]) / k;
    }
    free(c);
}

/* flankerna med Schmitt-trigger; brum under ungefär 100 Hz tas bort först */
static long find_edges(const double *x0, long n, long sr, double hyst, long **edges)
{
    double *x = (double *) xmalloc((size_t) n * sizeof(double));
    double *a = (double *) xmalloc((size_t) n * sizeof(double));
    double *env = (double *) xmalloc((size_t) n * sizeof(double));
    double mx = 0, floor_;
    long i, ne = 0, k;
    int last = 0;

    k = sr / 100;
    moving_average(x0, n, k < 3 ? 3 : k, a);
    for (i = 0; i < n; i++) {
        x[i] = x0[i] - a[i];
        a[i] = fabs(x[i]);
        if (a[i] > mx)
            mx = a[i];
    }
    k = sr / 20;
    moving_average(a, n, k < 3 ? 3 : k, env);
    floor_ = 1e-3 * (mx + 1);
    *edges = (long *) xmalloc((size_t) (n + 1) * sizeof(long));
    for (i = 0; i < n; i++) {
        double h = env[i] * hyst;
        int m;
        if (h < floor_)
            h = floor_;
        m = x[i] > h ? 1 : x[i] < -h ? -1 : 0;
        if (m) {
            if (last && m != last)
                (*edges)[ne++] = i;
            last = m;
        }
    }
    free(x);
    free(a);
    free(env);
    return ne;
}

/* FM-avkodning med följd bitcellslängd: 0, 1 eller 2 (oläsbar). tk 0:
   bitcellen utgår från ABC80 (som också täcker ABC 800 i 700 baud),
   annars från ABC 800 (32 x tk T). */
static long edges_to_bits(const long *edges, long ne, long sr, double split,
                          int tk, signed char *bits, long *pos)
{
    double nominal = tk ? (double) sr * 32 * tk / CLOCK800
                        : (double) sr * (T_ZERO + T_ONE) / 2 / CLOCK;
    double cell = nominal;
    long i = 0, n = ne - 1, nb = 0;

    while (i < n) {
        double t = (double) (edges[i + 1] - edges[i]);
        if (split * cell < t && t < 1.5 * cell) {
            bits[nb] = 0;
            pos[nb++] = edges[i];
            cell += 0.05 * (t - cell);
            i += 1;
        } else if (0.2 * cell < t && t <= split * cell && i + 1 < n &&
                   0.7 * cell < t + (edges[i + 2] - edges[i + 1]) &&
                   t + (edges[i + 2] - edges[i + 1]) < 1.4 * cell) {
            bits[nb] = 1;
            pos[nb++] = edges[i];
            cell += 0.05 * (t + (edges[i + 2] - edges[i + 1]) - cell);
            i += 2;
        } else {
            bits[nb] = 2;
            pos[nb++] = edges[i];
            i += 1;
            if (t > 3 * cell)
                cell = nominal;
        }
        if (cell < 0.7 * nominal)
            cell = 0.7 * nominal;
        if (cell > 1.3 * nominal)
            cell = 1.3 * nominal;
    }
    return nb;
}

typedef struct {
    double t;
    byte d[BLOCK];
} Good;

typedef struct {
    Good *g;
    long ng, capg;
    Report *r;
    long nr, capr;
} Found;

static int near_good(const Found *f, double t)
{
    long i;
    for (i = 0; i < f->ng; i++)
        if (fabs(t - f->g[i].t) < 1.0)
            return 1;
    return 0;
}

static int near_bad(const Found *f, double t)
{
    long i;
    for (i = 0; i < f->nr; i++)
        if (fabs(t - f->r[i].t) < 1.0)
            return 1;
    return 0;
}

static void add_bad(Found *f, double t, const char *st)
{
    if (f->nr == f->capr) {
        f->capr = f->capr ? 2 * f->capr : 64;
        f->r = (Report *) xrealloc(f->r, (size_t) f->capr * sizeof(Report));
    }
    f->r[f->nr].t = t;
    f->r[f->nr++].status = st;
}

static long find_blocks(const signed char *bits, const long *pos, long nb,
                        long sr, Found *f)
{
    /* ABC80: SYN SYN SYN STX; ABC 800: 00 00 SYN STX */
    static signed char sync[32], sync8[32];
    static const int sb[4] = {SYN, SYN, SYN, STX}, sb8[4] = {0, 0, SYN, STX};
    long h, added = 0;
    int i, k;

    for (i = 0; i < 4; i++)
        for (k = 0; k < 8; k++)
            sync[i * 8 + k] = (signed char) ((sb[i] >> k) & 1),
            sync8[i * 8 + k] = (signed char) ((sb8[i] >> k) & 1);

    for (h = 0; h + 32 <= nb; h++) {
        const signed char *seg;
        byte by[259];
        double t;
        int bad = 0;
        if (memcmp(bits + h, sync, 32) && memcmp(bits + h, sync8, 32))
            continue;
        if (h + 32 + 259 * 8 > nb)
            continue;
        seg = bits + h + 32;
        t = (double) pos[h] / sr;
        for (i = 0; i < 259 * 8; i++)
            if (seg[i] == 2)
                bad = 1;
        if (bad) {
            if (!near_bad(f, t))
                add_bad(f, t, "bitar");
            continue;
        }
        for (i = 0; i < 259; i++) {
            int v = 0;
            for (k = 0; k < 8; k++)
                v |= seg[i * 8 + k] << k;
            by[i] = (byte) v;
        }
        if (by[256] != ETX) {
            if (!near_bad(f, t))
                add_bad(f, t, "etx");
        } else if (checksum(by) != (unsigned) (by[257] | (by[258] << 8))) {
            if (!near_bad(f, t))
                add_bad(f, t, "summa");
        } else if (!near_good(f, t)) {
            if (f->ng == f->capg) {
                f->capg = f->capg ? 2 * f->capg : 64;
                f->g = (Good *) xrealloc(f->g, (size_t) f->capg * sizeof(Good));
            }
            f->g[f->ng].t = t;
            memcpy(f->g[f->ng++].d, by, BLOCK);
            added++;
        }
    }
    return added;
}

static int cmp_good(const void *a, const void *b)
{
    const Good *x = (const Good *) a, *y = (const Good *) b;
    if (x->t != y->t)
        return x->t < y->t ? -1 : 1;
    return memcmp(x->d, y->d, BLOCK);
}

static int cmp_report(const void *a, const void *b)
{
    const Report *x = (const Report *) a, *y = (const Report *) b;
    if (x->t != y->t)
        return x->t < y->t ? -1 : 1;
    return strcmp(x->status, y->status);
}

static const double passes[][2] = {
    {0.40, 0.75}, {0.25, 0.75}, {0.60, 0.75}, {0.40, 0.68}, {0.40, 0.82},
    {0.15, 0.75}, {0.80, 0.75}
};

static void decode_channel(int c, const double *x, long n, long sr, int tk,
                           int quick, Found *f, LogFn log)
{
    long p, np = quick ? 1 : (long) (sizeof passes / sizeof passes[0]);
    char msg[512], tks[16];

    for (p = 0; p < np; p++) {
        long *edges, ne, nb, added;
        signed char *bits;
        long *pos;
        ne = find_edges(x, n, sr, passes[p][0], &edges);
        bits = (signed char *) xmalloc((size_t) (ne + 1));
        pos = (long *) xmalloc((size_t) (ne + 1) * sizeof(long));
        nb = edges_to_bits(edges, ne, sr, passes[p][1], tk, bits, pos);
        added = find_blocks(bits, pos, nb, sr, f);
        if (log) {
            tks[0] = 0;
            if (tk)
                sprintf(tks, ", tk %d", tk);
            sprintf(msg, "  kanal %d, hysteres %g, gräns %g%s: +%ld block (%ld)",
                    c, passes[p][0], passes[p][1], tks, added, f->ng);
            log(msg);
        }
        free(edges);
        free(bits);
        free(pos);
    }
}

int decode_wav(const char *path, int quick, LogFn log,
               Blocks *out, Report **rep, long *nrep)
{
    long sr = 0, n, i;
    int nch, c, tk;
    double *x = read_wav(path, &sr, &nch, &n);
    Found f;
    char msg[512], har[256];
    long nr;

    if (!x)
        return 1;
    memset(&f, 0, sizeof f);
    if (log) {
        sprintf(msg, "%.400s: %ld Hz, %d kanal(er), %.1f s", path, sr, nch,
                (double) n / sr);
        log(msg);
    }
    for (c = 0; c < nch; c++)
        decode_channel(c, x + (long) c * n, n, sr, 0, quick, &f, log);
    /* ABC 800: datablocken går med tidkonstanten i namnblocket */
    memset(har, 0, sizeof har);
    for (i = 0; i < f.ng; i++)
        if (is_name_block(f.g[i].d))
            har[f.g[i].d[14]] = 1;
    for (tk = 1; tk < 256; tk++)
        if (har[tk] && tk != TK800)         /* 129 täcks av första omgången */
            for (c = 0; c < nch; c++)
                decode_channel(c, x + (long) c * n, n, sr, tk, quick, &f, log);
    free(x);

    qsort(f.g, (size_t) f.ng, sizeof(Good), cmp_good);
    for (i = 0; i < f.ng; i++)
        blocks_add(out, f.g[i].d);

    *rep = (Report *) xmalloc((size_t) (f.ng + f.nr) * sizeof(Report));
    nr = 0;
    for (i = 0; i < f.ng; i++) {
        (*rep)[nr].t = f.g[i].t;
        (*rep)[nr++].status = "ok";
    }
    for (i = 0; i < f.nr; i++)
        if (!near_good(&f, f.r[i].t))
            (*rep)[nr++] = f.r[i];
    qsort(*rep, (size_t) nr, sizeof(Report), cmp_report);
    *nrep = nr;
    free(f.g);
    free(f.r);
    return 0;
}


/* ------------------------------------------------------------------ */
/* text till och från dagens filer */

/* ABC:s 7-bitarskod: @[\\]^ `{|}~ är ÉÄÖÅÜ éäöåü. '$' får vara '$'. */
static const byte abc_tkn[10] = { 0x40, 0x5B, 0x5C, 0x5D, 0x5E,
                                  0x60, 0x7B, 0x7C, 0x7D, 0x7E };
static const byte tkn_850[10] = { 0x90, 0x8E, 0x99, 0x8F, 0x9A,
                                  0x82, 0x84, 0x94, 0x86, 0x81 };
static const unsigned tkn_uni[10] = { 0xC9, 0xC4, 0xD6, 0xC5, 0xDC,
                                      0xE9, 0xE4, 0xF6, 0xE5, 0xFC };

/* Latin-1 $A0-$FF i teckentabell 850 ($80-$9F finns inte: ?) */
static const byte l1_850[96] = {
    0xFF,0xAD,0xBD,0x9C,0xCF,0xBE,0xDD,0xF5,0xF9,0xB8,0xA6,0xAE,0xAA,0xF0,0xA9,0xEE,
    0xF8,0xF1,0xFD,0xFC,0xEF,0xE6,0xF4,0xFA,0xF7,0xFB,0xA7,0xAF,0xAC,0xAB,0xF3,0xA8,
    0xB7,0xB5,0xB6,0xC7,0x8E,0x8F,0x92,0x80,0xD4,0x90,0xD2,0xD3,0xDE,0xD6,0xD7,0xD8,
    0xD1,0xA5,0xE3,0xE0,0xE2,0xE5,0x99,0x9E,0x9D,0xEB,0xE9,0xEA,0x9A,0xED,0xE8,0xE1,
    0x85,0xA0,0x83,0xC6,0x84,0x86,0x91,0x87,0x8A,0x82,0x88,0x89,0x8D,0xA1,0x8C,0x8B,
    0xD0,0xA4,0x95,0xA2,0x93,0xE4,0x94,0xF6,0x9B,0x97,0xA3,0x96,0x81,0xEC,0xE7,0x98
};

int kod_nr(const char *s)
{
    if (!strcmp(s, "7"))
        return KOD_7;
    if (!strcmp(s, "dos"))
        return KOD_DOS;
    if (!strcmp(s, "utf8"))
        return KOD_UTF8;
    return -1;
}

const char *rad_text(const char *s)
{
    if (!strcmp(s, "cr"))
        return "\r";
    if (!strcmp(s, "crlf"))
        return "\r\n";
    if (!strcmp(s, "lf"))
        return "\n";
    return NULL;
}

static void tecken_ut(FILE *fp, int c, int kod)
{
    int i;
    unsigned u = (unsigned) c;
    if (kod == KOD_7) {
        putc(c, fp);
        return;
    }
    for (i = 0; i < 10; i++)
        if (abc_tkn[i] == c) {          /* svenskt tecken: färdigt */
            u = kod == KOD_DOS ? tkn_850[i] : tkn_uni[i];
            break;
        }
    if (i == 10 && u >= 0x80)           /* annars Latin-1 */
        u = kod == KOD_UTF8 ? u : u >= 0xA0 ? l1_850[u - 0xA0] : '?';
    if (kod == KOD_DOS || u < 0x80)
        putc((int) u, fp);
    else {
        putc((int) (0xC0 | u >> 6), fp);
        putc((int) (0x80 | (u & 0x3F)), fp);
    }
}

void text_ut(FILE *fp, const byte *s, long n, int kod, const char *rad)
{
    long i;
    for (i = 0; i < n; i++) {
        if (s[i] == '\r' || s[i] == '\n') {
            fputs(rad, fp);
            if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n')
                i++;
        } else
            tecken_ut(fp, s[i], kod);
    }
}

/* ett tecken i UTF-8 från s[*i]: tecknets nummer, eller -1 om det är fel;
   då hoppar *i över hela den felaktiga biten (som Pythons 'replace': den
   längsta början som kunde ha blivit ett tecken, minst en byte) */
static long utf8_tkn(const byte *s, long n, long *i)
{
    byte c = s[(*i)++], lo = 0x80, hi = 0xBF;
    long u;
    int k;
    if (c < 0x80)
        return c;
    if (c >= 0xC2 && c < 0xE0)
        k = 1, u = c & 0x1F;
    else if (c >= 0xE0 && c < 0xF0)
        k = 2, u = c & 0x0F;
    else if (c >= 0xF0 && c < 0xF5)
        k = 3, u = c & 0x07;
    else
        return -1;
    if (c == 0xE0)
        lo = 0xA0;                          /* inte för långt */
    else if (c == 0xED)
        hi = 0x9F;                          /* inga surrogat */
    else if (c == 0xF0)
        lo = 0x90;
    else if (c == 0xF4)
        hi = 0x8F;                          /* högst $10FFFF */
    while (k--) {
        if (*i >= n || s[*i] < lo || s[*i] > hi)
            return -1;
        u = u << 6 | (s[(*i)++] & 0x3F);
        lo = 0x80, hi = 0xBF;
    }
    return u;
}

static int utf8_ok(const byte *s, long n)
{
    long i = 0;
    while (i < n)
        if (utf8_tkn(s, n, &i) < 0)
            return 0;
    return 1;
}

byte *text_in(const byte *s, long n, int kod, long *ut)
{
    byte *o = (byte *) xmalloc((size_t) n + 1);
    long i = 0, k = 0;
    int j;
    if (kod < 0) {
        kod = KOD_7;
        for (i = 0; i < n; i++)
            if (s[i] >= 0x80)
                kod = utf8_ok(s, n) ? KOD_UTF8 : KOD_DOS;
        i = 0;
    }
    while (i < n) {
        unsigned u = s[i++];
        if (u == '\r' || u == '\n') {
            if (u == '\r' && i < n && s[i] == '\n')
                i++;
            o[k++] = '\r';
            continue;
        }
        if (kod == KOD_DOS && u >= 0x80) {
            for (j = 0; j < 10; j++)
                if (tkn_850[j] == u)
                    break;
            u = j < 10 ? abc_tkn[j] : '?';
        } else if (kod == KOD_UTF8 && u >= 0x80) {
            long c;
            i--;
            c = utf8_tkn(s, n, &i);
            for (j = 0; j < 10; j++)
                if (c >= 0 && tkn_uni[j] == (unsigned long) c)
                    break;
            u = j < 10 ? abc_tkn[j] : '?';
        }
        o[k++] = (byte) u;
    }
    *ut = k;
    return o;
}
