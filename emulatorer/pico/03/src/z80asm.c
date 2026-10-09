/*
 * z80asm.c -- 2-pass Z80 assembler in C
 *
 * Companion to z80_disassembler.cpp; encoding tables mirror disasm.c.
 *
 * Build:
 *   cc -std=c11 -Wall -O2 -o z80asm z80asm.c
 *
 * Usage:
 *   z80asm [-o out.bin] [-x out.hex] [-l] [-v] [-g origin] source.asm
 *     -o file   binary output  (default: <source>.bin)
 *     -x file   Intel HEX output
 *     -l        listing to stdout
 *     -v        verbose symbol table
 *     -g org    initial load origin (default 0)
 *
 * Pseudo-ops:
 *   ORG  expr            set program counter
 *   EQU  expr            define symbol  (label EQU val, or label = val)
 *   DEFB expr[,...]      emit bytes
 *   DEFW expr[,...]      emit 16-bit words (little-endian)
 *   DEFS expr [,fill]    reserve bytes (fill defaults to 0)
 *   DEFM "str"[,...]     emit string bytes (no terminator)
 *   END                  stop assembly
 *   IF expr / ELSE / ENDIF  conditional assembly
 *
 * Number literals:
 *   decimal:   255
 *   hex:       0xFF  $FF  0FFH  (leading digit required for ...H form)
 *   binary:    0b1010  %1010  1010B
 *   octal:     0o17  17O
 *   character: 'A'
 *   cur PC:    $  (only when not the start of a hex literal)
 *
 * Expression operators (highest to lowest precedence):
 *   unary: - ~
 *   * / % (multiply/divide/mod)
 *   + -
 *   << >>
 *   &  ^  |
 *
 * Comments: ; ... or // ...
 */

#include <stdio.h>      /* snprintf/vsnprintf -- available including Pico newlib */
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef Z80ASM_EMBEDDED
#include <stdlib.h>     /* strtol, fopen/fclose -- not needed on embedded */
#endif

/* 
 * Configuration
 * Z80ASM_EMBEDDED: build as a library with no file I/O and reduced static
 * memory.  Define this when building for Raspberry Pi Pico or similar MCU.
 * Without it the standalone CLI tool is built (requires stdio/stdlib).
 */
#ifdef Z80ASM_EMBEDDED
#define MAX_LINES       16384   /* loop guard only - no static line_store */
#define MAX_LINE_LEN    128
#define MAX_SYMBOLS     512
#define SYM_NAME_MAX    32
#define ARENA_SIZE      (MAX_SYMBOLS * (SYM_NAME_MAX + 4))
#else
#define MAX_LINES       16384
#define MAX_LINE_LEN    256
#define MAX_SYMBOLS     2048
#define SYM_NAME_MAX    64
#define ARENA_SIZE      (MAX_SYMBOLS * (SYM_NAME_MAX + 4))
#endif
#define OUTPUT_SIZE     65536   /* 64 KB */

/* Arena allocator -- symbol name strings */
typedef struct {
    char  buf[ARENA_SIZE];
    int   used;
} Arena;

static void arena_init(Arena *a) { a->used = 0; }

static char *arena_strdup(Arena *a, const char *s) {
    int n = (int)strlen(s) + 1;
    if (a->used + n > ARENA_SIZE) return NULL;   /* table full; caller must check */
    char *p = a->buf + a->used;
    memcpy(p, s, (size_t)n);
    a->used += n;
    return p;
}

/* Symbol table */
typedef struct {
    char    *name;
    int32_t  value;
    bool     defined;
} Symbol;

typedef struct {
    Symbol  tab[MAX_SYMBOLS];
    int     count;
    Arena   arena;
} SymTable;

static void sym_init(SymTable *st) {
    st->count = 0;
    arena_init(&st->arena);
}

/* Lookup: returns NULL if not found */
static Symbol *sym_find(SymTable *st, const char *name) {
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->tab[i].name, name) == 0)
            return &st->tab[i];
    return NULL;
}

/* Define or redefine a symbol */
static void sym_define(SymTable *st, const char *name, int32_t val) {
    Symbol *s = sym_find(st, name);
    if (s) { s->value = val; s->defined = true; return; }
    if (st->count >= MAX_SYMBOLS) return;   /* table full; symbol silently dropped */
    char *stored = arena_strdup(&st->arena, name);
    if (!stored) return;                    /* arena full; silently dropped */
    s = &st->tab[st->count++];
    s->name    = stored;
    s->value   = val;
    s->defined = true;
}

/* Get value; returns false through *defined if unknown */
static int32_t sym_get(SymTable *st, const char *name, bool *defined) {
    Symbol *s = sym_find(st, name);
    if (!s || !s->defined) { *defined = false; return 0; }
    *defined = true;
    return s->value;
}

/* Assembler context */
typedef struct {
    uint8_t  *out;               /* output buffer - caller provides 64 KB */
    uint16_t  pc;                /* program counter */
    int       pass;              /* 1 or 2 */
    int       lineno;
    int       errors;
    bool      end_seen;
    int       if_depth;          /* nesting depth for IF */
    bool      if_skip;           /* true = currently skipping due to false IF */
    int       if_skip_depth;     /* depth at which skip started */
    bool      listing;
    bool      verbose;
    SymTable  syms;
    /* per-line listing buffer */
    char      list_bytes[32];
    int       list_nbytes;
    /* output callback for listing lines and errors (NULL = use printf/stderr) */
    void    (*emit_line)(const char *s);
} Ctx;

/* Error reporting */
static void ctx_puts(Ctx *ctx, const char *s) {
    if (ctx->emit_line) ctx->emit_line(s);
#ifndef Z80ASM_EMBEDDED
    else fputs(s, stderr);
#endif
}

static void error(Ctx *ctx, const char *fmt, ...) {
    /* Report errors on both passes so the caller sees them in pass 1
     * for undef-safe diagnostics and in pass 2 for the final count. */
    char buf[MAX_LINE_LEN];
    int n = snprintf(buf, sizeof(buf), "line %d: error: ", ctx->lineno);
    va_list ap; va_start(ap, fmt); vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, ap); va_end(ap);
    ctx_puts(ctx, buf);
    if (ctx->pass == 2) ctx->errors++;
}

static void warning(Ctx *ctx, const char *fmt, ...) {
    char buf[MAX_LINE_LEN];
    int n = snprintf(buf, sizeof(buf), "line %d: warning: ", ctx->lineno);
    va_list ap; va_start(ap, fmt); vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, ap); va_end(ap);
    ctx_puts(ctx, buf);
}

/* 
 * Emit helpers
 * On pass 1: only advance PC, no writes.
 * On pass 2: write to ctx->out.
 * The listing buffer is filled on pass 2 only.
 */
static void emit(Ctx *ctx, uint8_t b) {
    if (ctx->pass == 2) {
        ctx->out[ctx->pc] = b;
        if (ctx->listing && ctx->list_nbytes < 8) {
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%02X ", b);
            strcat(ctx->list_bytes, tmp);
            ctx->list_nbytes++;
        }
    }
    ctx->pc++;
}

static void emit_word(Ctx *ctx, uint16_t w) {
    emit(ctx, (uint8_t)(w & 0xFF));
    emit(ctx, (uint8_t)(w >> 8));
}

/* Lexer */
typedef enum {
    TT_EOF, TT_EOL, TT_IDENT, TT_NUM, TT_STR,
    TT_COMMA, TT_COLON, TT_LPAREN, TT_RPAREN,
    TT_PLUS, TT_MINUS, TT_STAR, TT_SLASH, TT_PERCENT,
    TT_AMP, TT_PIPE, TT_CARET, TT_TILDE, TT_BANG,
    TT_LSHIFT, TT_RSHIFT, TT_DOLLAR, TT_EQ, TT_HASH,
} TokType;

typedef struct {
    TokType  type;
    int32_t  num;
    char     text[SYM_NAME_MAX + 2];
    bool     is_undef;  /* set by expr evaluator when symbol not found */
} Token;

/* Lexer state: a pointer we advance through the line */
typedef struct {
    const char *p;
    Ctx        *ctx;
} Lex;

static void lex_skip_ws(Lex *lx) {
    while (*lx->p == ' ' || *lx->p == '\t') lx->p++;
    /* Skip comment */
    if (*lx->p == ';') { while (*lx->p) lx->p++; }
    if (lx->p[0] == '/' && lx->p[1] == '/') { while (*lx->p) lx->p++; }
}

static Token lex_next(Lex *lx) {
    Token t;
    memset(&t, 0, sizeof(t));
    lex_skip_ws(lx);
    const char *start = lx->p;

    if (!*lx->p) { t.type = TT_EOF; return t; }
    if (*lx->p == '\n') { t.type = TT_EOL; lx->p++; return t; }

    /* Operators and punctuation */
    switch (*lx->p) {
    case ',': t.type = TT_COMMA;  lx->p++; return t;
    case ':': t.type = TT_COLON;  lx->p++; return t;
    case '(': t.type = TT_LPAREN; lx->p++; return t;
    case ')': t.type = TT_RPAREN; lx->p++; return t;
    case '+': t.type = TT_PLUS;   lx->p++; return t;
    case '-': t.type = TT_MINUS;  lx->p++; return t;
    case '*': t.type = TT_STAR;   lx->p++; return t;
    case '/': t.type = TT_SLASH;  lx->p++; return t;
    case '%':
        if (lx->p[1] == '0' || lx->p[1] == '1') {
            /* %binary literal */
            lx->p++;
            int32_t v = 0;
            while (*lx->p == '0' || *lx->p == '1') v = v*2 + (*lx->p++ - '0');
            t.type = TT_NUM; t.num = v; return t;
        }
        t.type = TT_PERCENT; lx->p++; return t;
    case '&': t.type = TT_AMP;   lx->p++; return t;
    case '|': t.type = TT_PIPE;  lx->p++; return t;
    case '^': t.type = TT_CARET; lx->p++; return t;
    case '~': t.type = TT_TILDE; lx->p++; return t;
    case '!': t.type = TT_BANG;  lx->p++; return t;
    case '=': t.type = TT_EQ;    lx->p++; return t;
    case '#': t.type = TT_HASH;  lx->p++; return t;
    case '<':
        if (lx->p[1] == '<') { t.type = TT_LSHIFT; lx->p += 2; return t; }
        break;
    case '>':
        if (lx->p[1] == '>') { t.type = TT_RSHIFT; lx->p += 2; return t; }
        break;
    case '\'': { /* character literal 'x' */
        lx->p++;
        int32_t v = (unsigned char)*lx->p;
        if (*lx->p) lx->p++;
        if (*lx->p == '\'') lx->p++;
        t.type = TT_NUM; t.num = v; return t;
    }
    case '"': { /* string literal "..." -> TT_STR */
        lx->p++;
        int i = 0;
        while (*lx->p && *lx->p != '"' && i < (int)sizeof(t.text)-1)
            t.text[i++] = *lx->p++;
        if (*lx->p == '"') lx->p++;
        t.text[i] = '\0';
        t.type = TT_STR; return t;
    }
    case '$':
        /* $XX hex literal or standalone $ (current PC) */
        if (isxdigit((unsigned char)lx->p[1])) {
            lx->p++;
            int32_t v = 0;
            while (isxdigit((unsigned char)*lx->p)) {
                char c = *lx->p++;
                v = v*16 + (isdigit((unsigned char)c) ? c-'0' :
                             toupper((unsigned char)c)-'A'+10);
            }
            t.type = TT_NUM; t.num = v; return t;
        }
        t.type = TT_DOLLAR; lx->p++; return t;
    }

    /* Number */
    if (isdigit((unsigned char)*lx->p)) {
        if (lx->p[0] == '0' && (lx->p[1] == 'x' || lx->p[1] == 'X')) {
            lx->p += 2;
            int32_t v = 0;
            while (isxdigit((unsigned char)*lx->p)) {
                char c = *lx->p++;
                v = v*16 + (isdigit((unsigned char)c) ? c-'0' :
                             toupper((unsigned char)c)-'A'+10);
            }
            t.type = TT_NUM; t.num = v; return t;
        }
        if (lx->p[0] == '0' && (lx->p[1] == 'b' || lx->p[1] == 'B')) {
            lx->p += 2;
            int32_t v = 0;
            while (*lx->p == '0' || *lx->p == '1') v = v*2 + (*lx->p++ - '0');
            t.type = TT_NUM; t.num = v; return t;
        }
        if (lx->p[0] == '0' && (lx->p[1] == 'o' || lx->p[1] == 'O')) {
            lx->p += 2;
            int32_t v = 0;
            while (*lx->p >= '0' && *lx->p <= '7') v = v*8 + (*lx->p++ - '0');
            t.type = TT_NUM; t.num = v; return t;
        }
        /* Decimal - then check for trailing H/B/O/D suffix */
        const char *ns = lx->p;
        int32_t v = 0;
        bool is_hex = false;
        while (isxdigit((unsigned char)*lx->p)) {
            if (!isdigit((unsigned char)*lx->p)) is_hex = true;
            lx->p++;
        }
        char suf = (char)toupper((unsigned char)*lx->p);
        if (suf == 'H') {
            lx->p++;
            /* Reparse as hex */
            v = 0;
            while (ns < lx->p - 1) {
                char c = *ns++;
                v = v*16 + (isdigit((unsigned char)c) ? c-'0' :
                             toupper((unsigned char)c)-'A'+10);
            }
        } else if (suf == 'B' && !is_hex) {
            lx->p++;
            v = 0; const char *bp = ns;
            while (bp < lx->p - 1) v = v*2 + (*bp++ - '0');
        } else if (suf == 'O' && !is_hex) {
            lx->p++;
            v = 0; const char *bp = ns;
            while (bp < lx->p - 1) v = v*8 + (*bp++ - '0');
        } else if (suf == 'D' && !is_hex) {
            lx->p++;
            v = 0; const char *bp = ns;
            while (bp < lx->p - 1) v = v*10 + (*bp++ - '0');
        } else {
            /* Plain decimal */
            v = 0; const char *bp = ns;
            while (bp < lx->p) v = v*10 + (*bp++ - '0');
        }
        t.type = TT_NUM; t.num = v; return t;
    }

    /* Identifier */
    if (isalpha((unsigned char)*lx->p) || *lx->p == '_' || *lx->p == '.') {
        int i = 0;
        while ((isalnum((unsigned char)*lx->p) || *lx->p == '_' || *lx->p == '\'' || *lx->p == '.')
               && i < (int)sizeof(t.text)-1)
            t.text[i++] = (char)toupper((unsigned char)*lx->p++);
        t.text[i] = '\0';
        t.type = TT_IDENT;
        return t;
    }

    /* Unknown - skip */
    t.text[0] = *lx->p++;
    t.text[1] = '\0';
    t.type = TT_IDENT;
    (void)start;
    return t;
}

/* Peek without consuming */
static Token lex_peek(Lex *lx) {
    const char *saved = lx->p;
    Token t = lex_next(lx);
    lx->p = saved;
    return t;
}

/*
 * Expression evaluator -- recursive descent
 * Returns the value; sets *undef=true if any symbol was not yet defined
 * (expected and safe in pass 1; error in pass 2).
 */
static int32_t eval_expr(Ctx *ctx, Lex *lx, bool *undef);

static int32_t eval_primary(Ctx *ctx, Lex *lx, bool *undef) {
    Token t = lex_peek(lx);

    if (t.type == TT_MINUS) {
        lex_next(lx);
        return -eval_primary(ctx, lx, undef);
    }
    if (t.type == TT_TILDE) {
        lex_next(lx);
        return ~eval_primary(ctx, lx, undef);
    }
    if (t.type == TT_BANG) {
        lex_next(lx);
        return !eval_primary(ctx, lx, undef) ? 1 : 0;
    }
    if (t.type == TT_PLUS) {
        lex_next(lx);
        return eval_primary(ctx, lx, undef);
    }
    if (t.type == TT_NUM) {
        lex_next(lx);
        return t.num;
    }
    if (t.type == TT_DOLLAR) {
        lex_next(lx);
        return (int32_t)(uint16_t)ctx->pc;
    }
    if (t.type == TT_LPAREN) {
        lex_next(lx);
        int32_t v = eval_expr(ctx, lx, undef);
        Token cl = lex_next(lx);
        if (cl.type != TT_RPAREN)
            error(ctx, "expected ')'");
        return v;
    }
    if (t.type == TT_IDENT) {
        lex_next(lx);
        bool def = true;
        int32_t v = sym_get(&ctx->syms, t.text, &def);
        if (!def) { *undef = true; return 0; }
        return v;
    }
    /* Unknown token -- don't consume it */
    return 0;
}

static int32_t eval_muldiv(Ctx *ctx, Lex *lx, bool *undef) {
    int32_t v = eval_primary(ctx, lx, undef);
    for (;;) {
        Token t = lex_peek(lx);
        if (t.type == TT_STAR)    { lex_next(lx); v *= eval_primary(ctx, lx, undef); }
        else if (t.type == TT_SLASH) {
            lex_next(lx);
            int32_t r = eval_primary(ctx, lx, undef);
            v = r ? v / r : 0;
        }
        else if (t.type == TT_PERCENT) {
            lex_next(lx);
            int32_t r = eval_primary(ctx, lx, undef);
            v = r ? v % r : 0;
        }
        else break;
    }
    return v;
}

static int32_t eval_addsub(Ctx *ctx, Lex *lx, bool *undef) {
    int32_t v = eval_muldiv(ctx, lx, undef);
    for (;;) {
        Token t = lex_peek(lx);
        if      (t.type == TT_PLUS)  { lex_next(lx); v += eval_muldiv(ctx, lx, undef); }
        else if (t.type == TT_MINUS) { lex_next(lx); v -= eval_muldiv(ctx, lx, undef); }
        else break;
    }
    return v;
}

static int32_t eval_shift(Ctx *ctx, Lex *lx, bool *undef) {
    int32_t v = eval_addsub(ctx, lx, undef);
    for (;;) {
        Token t = lex_peek(lx);
        if      (t.type == TT_LSHIFT) { lex_next(lx); v <<= eval_addsub(ctx, lx, undef); }
        else if (t.type == TT_RSHIFT) { lex_next(lx); v >>= eval_addsub(ctx, lx, undef); }
        else break;
    }
    return v;
}

static int32_t eval_expr(Ctx *ctx, Lex *lx, bool *undef) {
    int32_t v = eval_shift(ctx, lx, undef);
    for (;;) {
        Token t = lex_peek(lx);
        if      (t.type == TT_AMP)   { lex_next(lx); v &= eval_shift(ctx, lx, undef); }
        else if (t.type == TT_CARET) { lex_next(lx); v ^= eval_shift(ctx, lx, undef); }
        else if (t.type == TT_PIPE)  { lex_next(lx); v |= eval_shift(ctx, lx, undef); }
        else break;
    }
    return v;
}

/* 
 * Register / operand parsing
 * The Z80 instruction set uses two register encodings:
 *   reg8  : B=0 C=1 D=2 E=3 H=4 L=5 (HL)=6 A=7
 *   reg16 : BC=0 DE=1 HL=2 SP=3  (also: AF=3 for PUSH/POP)
 *   cond  : NZ=0 Z=1 NC=2 C=3 PO=4 PE=5 P=6 M=7
 */

/* Look up a register name string -- reg8 index, or -1 if not a reg8 */
static int reg8_idx(const char *s) {
    if (!strcmp(s,"B"))    return 0;
    if (!strcmp(s,"C"))    return 1;
    if (!strcmp(s,"D"))    return 2;
    if (!strcmp(s,"E"))    return 3;
    if (!strcmp(s,"H"))    return 4;
    if (!strcmp(s,"L"))    return 5;
    if (!strcmp(s,"A"))    return 7;
    return -1;
}

static int reg16_idx(const char *s) {
    if (!strcmp(s,"BC")) return 0;
    if (!strcmp(s,"DE")) return 1;
    if (!strcmp(s,"HL")) return 2;
    if (!strcmp(s,"SP")) return 3;
    return -1;
}


static int cond_idx(const char *s) {
    if (!strcmp(s,"NZ")) return 0;
    if (!strcmp(s,"Z"))  return 1;
    if (!strcmp(s,"NC")) return 2;
    if (!strcmp(s,"C"))  return 3;
    if (!strcmp(s,"PO")) return 4;
    if (!strcmp(s,"PE")) return 5;
    if (!strcmp(s,"P"))  return 6;
    if (!strcmp(s,"M"))  return 7;
    return -1;
}

/* Operand types */
typedef enum {
    OP_NONE,
    OP_REG8,        /* B C D E H L A */
    OP_REG16,       /* BC DE HL SP */
    OP_REG16AF,     /* BC DE HL AF  (for PUSH/POP) */
    OP_IX, OP_IY,   /* IX / IY */
    OP_IND_HL,      /* (HL) */
    OP_IND_BC,      /* (BC) */
    OP_IND_DE,      /* (DE) */
    OP_IND_SP,      /* (SP) */
    OP_IND_IX,      /* (IX+d) */
    OP_IND_IY,      /* (IY+d) */
    OP_IND_C,       /* (C) - for IN/OUT */
    OP_IND_ADDR,    /* (nn) - indirect address */
    OP_IMM,         /* immediate value */
    OP_CC,          /* condition code */
    OP_REG_I,       /* I */
    OP_REG_R,       /* R */
    OP_AF_ALT,      /* AF' */
    OP_IND_IX_IY,   /* placeholder set by caller for (IX/IY+d) */
} OpType;

typedef struct {
    OpType  type;
    int32_t val;    /* for OP_IMM, OP_IND_ADDR, OP_IND_IX/IY offset */
    int     reg;    /* for OP_REG8: 0-7; for OP_REG16/AF: 0-3; for OP_CC: 0-7 */
    bool    undef;  /* expression contained undefined symbol */
} Operand;

/* Parse a single operand at the current lex position.
 * Does NOT consume a trailing comma. */
static Operand parse_operand(Ctx *ctx, Lex *lx) {
    Operand op; memset(&op, 0, sizeof(op));
    Token t = lex_peek(lx);

    /* Indirect: ( ... ) */
    if (t.type == TT_LPAREN) {
        lex_next(lx);  /* consume '(' */
        Token inner = lex_peek(lx);

        if (inner.type == TT_IDENT) {
            /* (HL), (BC), (DE), (SP), (IX...), (IY...), (C) */
            if (!strcmp(inner.text, "HL"))  { lex_next(lx); t = lex_next(lx); op.type = OP_IND_HL; return op; }
            if (!strcmp(inner.text, "BC"))  { lex_next(lx); t = lex_next(lx); op.type = OP_IND_BC; return op; }
            if (!strcmp(inner.text, "DE"))  { lex_next(lx); t = lex_next(lx); op.type = OP_IND_DE; return op; }
            if (!strcmp(inner.text, "SP"))  { lex_next(lx); t = lex_next(lx); op.type = OP_IND_SP; return op; }
            if (!strcmp(inner.text, "C") && lex_peek(lx).type != TT_COMMA) {
                /* (C) for IN/OUT - peek checks no comma following before ')' */
                const char *save = lx->p;
                lex_next(lx); /* skip C */
                Token cl = lex_peek(lx);
                if (cl.type == TT_RPAREN) {
                    lex_next(lx); op.type = OP_IND_C; return op;
                }
                lx->p = save;  /* restore, fall through */
            }
            if (!strcmp(inner.text, "IX") || !strcmp(inner.text, "IY")) {
                bool is_iy = !strcmp(inner.text, "IY");
                lex_next(lx); /* consume IX/IY */
                Token next = lex_peek(lx);
                int32_t offset = 0;
                bool undef = false;
                if (next.type == TT_PLUS) {
                    lex_next(lx);
                    offset = eval_expr(ctx, lx, &undef);
                } else if (next.type == TT_MINUS) {
                    lex_next(lx);
                    offset = -eval_expr(ctx, lx, &undef);
                }
                t = lex_next(lx); /* expect ')' */
                if (t.type != TT_RPAREN) error(ctx, "expected ')' after IX/IY+d");
                op.type  = is_iy ? OP_IND_IY : OP_IND_IX;
                op.val   = offset;
                op.undef = undef;
                return op;
            }
        }
        /* (expr) - indirect address */
        bool undef = false;
        int32_t addr = eval_expr(ctx, lx, &undef);
        t = lex_next(lx); /* expect ')' */
        if (t.type != TT_RPAREN) error(ctx, "expected ')' after address");
        op.type = OP_IND_ADDR; op.val = addr; op.undef = undef;
        return op;
    }

    /* Registers and keywords */
    if (t.type == TT_IDENT) {
        lex_next(lx);
        int r8 = reg8_idx(t.text);
        if (r8 >= 0) { op.type = OP_REG8;  op.reg = r8; return op; }
        int r16 = reg16_idx(t.text);
        if (r16 >= 0) { op.type = OP_REG16; op.reg = r16; return op; }
        if (!strcmp(t.text,"AF")) {
            /* AF or AF' */
            if (lex_peek(lx).type == TT_IDENT &&
                !strcmp(lex_peek(lx).text, "'")) {
                lex_next(lx); op.type = OP_AF_ALT; return op;
            }
            op.type = OP_REG16AF; op.reg = 3; return op;
        }
        if (!strcmp(t.text,"AF'")) { op.type = OP_AF_ALT; return op; }
        if (!strcmp(t.text,"IX"))  { op.type = OP_IX;     return op; }
        if (!strcmp(t.text,"IY"))  { op.type = OP_IY;     return op; }
        if (!strcmp(t.text,"I"))   { op.type = OP_REG_I;  return op; }
        if (!strcmp(t.text,"R"))   { op.type = OP_REG_R;  return op; }

        /* Condition codes */
        int cc = cond_idx(t.text);
        if (cc >= 0) { op.type = OP_CC; op.reg = cc; return op; }

        /* Symbol or number expression - put it back via a fake const */
        /* We need to evaluate as an expression; re-parse from t.text as symbol */
        bool defined = false;
        int32_t v = sym_get(&ctx->syms, t.text, &defined);
        if (!defined) {
            if (ctx->pass == 2)
                error(ctx, "undefined symbol '%s'", t.text);
        }
        op.type = OP_IMM; op.val = v; op.undef = !defined; return op;
    }

    /* Immediate expression */
    bool undef = false;
    int32_t v = eval_expr(ctx, lx, &undef);
    op.type = OP_IMM; op.val = v; op.undef = undef;
    return op;
}

/* 
 * Instruction encoders
 * One function per major mnemonic / group.
 * Each function emits bytes and returns number of bytes emitted.
 * Encoding tables match disasm.c (same bit-field structure, reversed).
 */

/* ALU base opcodes: ADD=0 ADC=1 SUB=2 SBC=3 AND=4 XOR=5 OR=6 CP=7
 * For reg:  base*8 + r  (= 0x80/0x88/0x90/0x98/0xA0/0xA8/0xB0/0xB8 + r)
 * For imm:  0xC6 + base*8
 */
static void asm_alu(Ctx *ctx, Lex *lx, int base) {
    /* Some mnemonics accept optional "A," prefix: ADD A,r or ADD r */
    Operand dst = parse_operand(ctx, lx);

    /* 16-bit ADD HL,rr / ADD IX,rr / ADD IY,rr */
    if (base == 0) {
        Operand src;
        if (dst.type == OP_REG16 && dst.reg == 2) {
            /* ADD HL,rr */
            if (lex_peek(lx).type == TT_COMMA) { lex_next(lx); src = parse_operand(ctx, lx); }
            else { error(ctx, "ADD HL expects source"); return; }
            if (src.type == OP_REG16) { emit(ctx, (uint8_t)(0x09 | (src.reg << 4))); return; }
            error(ctx, "ADD HL: expected rr"); return;
        }
        if (dst.type == OP_IX) {
            if (lex_peek(lx).type == TT_COMMA) { lex_next(lx); src = parse_operand(ctx, lx); }
            else { error(ctx, "ADD IX expects source"); return; }
            int rp = (src.type == OP_IX) ? 2 : (src.type == OP_REG16 ? src.reg : -1);
            if (rp < 0) { error(ctx, "ADD IX: invalid source"); return; }
            emit(ctx, 0xDD); emit(ctx, (uint8_t)(0x09 | (rp << 4))); return;
        }
        if (dst.type == OP_IY) {
            if (lex_peek(lx).type == TT_COMMA) { lex_next(lx); src = parse_operand(ctx, lx); }
            else { error(ctx, "ADD IY expects source"); return; }
            int rp = (src.type == OP_IY) ? 2 : (src.type == OP_REG16 ? src.reg : -1);
            if (rp < 0) { error(ctx, "ADD IY: invalid source"); return; }
            emit(ctx, 0xFD); emit(ctx, (uint8_t)(0x09 | (rp << 4))); return;
        }
    }

    /* 16-bit ADC HL,rr / SBC HL,rr */
    if (base == 1 || base == 3) {
        if (dst.type == OP_REG16 && dst.reg == 2) {
            if (lex_peek(lx).type != TT_COMMA) { error(ctx, "missing source"); return; }
            lex_next(lx);
            Operand src = parse_operand(ctx, lx);
            if (src.type != OP_REG16) { error(ctx, "expected rr"); return; }
            uint8_t op2 = (base == 1) ? 0x4A : 0x42;
            emit(ctx, 0xED); emit(ctx, (uint8_t)(op2 | (src.reg << 4))); return;
        }
    }

    /* 8-bit: SUB/AND/OR/XOR/CP accept single operand (implicit A) */
    Operand src;
    if (base == 2 || base == 4 || base == 5 || base == 6 || base == 7) {
        /* dst is the only operand; check if it was "A," */
        if (dst.type == OP_REG8 && dst.reg == 7 && lex_peek(lx).type == TT_COMMA) {
            lex_next(lx); src = parse_operand(ctx, lx);
        } else {
            src = dst;
        }
    } else {
        /* ADD/ADC/SBC: must have two operands A,src */
        if (dst.type == OP_REG8 && dst.reg == 7 && lex_peek(lx).type == TT_COMMA) {
            lex_next(lx); src = parse_operand(ctx, lx);
        } else {
            src = dst;
        }
    }

    uint8_t base_op = (uint8_t)(0x80 | (base << 3));
    uint8_t imm_op  = (uint8_t)(0xC6 | (base << 3));

    if (src.type == OP_REG8)    { emit(ctx, base_op | (uint8_t)src.reg); return; }
    if (src.type == OP_IND_HL)  { emit(ctx, base_op | 6); return; }
    if (src.type == OP_IMM)     { emit(ctx, imm_op); emit(ctx, (uint8_t)src.val); return; }
    if (src.type == OP_IND_IX)  { emit(ctx, 0xDD); emit(ctx, base_op|6); emit(ctx,(uint8_t)src.val); return; }
    if (src.type == OP_IND_IY)  { emit(ctx, 0xFD); emit(ctx, base_op|6); emit(ctx,(uint8_t)src.val); return; }
    error(ctx, "invalid ALU operand");
}

/* INC / DEC  (is_dec=0 for INC, 1 for DEC) */
static void asm_inc_dec(Ctx *ctx, Lex *lx, int is_dec) {
    Operand op = parse_operand(ctx, lx);
    uint8_t r_op  = is_dec ? 0x05 : 0x04;  /* INC/DEC r */
    uint8_t rp_op = is_dec ? 0x0B : 0x03;  /* INC/DEC rr */

    if (op.type == OP_REG8)   { emit(ctx, (uint8_t)(r_op  | ((uint8_t)op.reg << 3))); return; }
    if (op.type == OP_IND_HL) { emit(ctx, (uint8_t)(r_op  | (6 << 3))); return; }
    if (op.type == OP_REG16)  { emit(ctx, (uint8_t)(rp_op | ((uint8_t)op.reg << 4))); return; }
    if (op.type == OP_IX)     { emit(ctx, 0xDD); emit(ctx, is_dec ? 0x2B : 0x23); return; }
    if (op.type == OP_IY)     { emit(ctx, 0xFD); emit(ctx, is_dec ? 0x2B : 0x23); return; }
    if (op.type == OP_IND_IX) { emit(ctx,0xDD); emit(ctx,(uint8_t)((r_op|(6<<3))));
                                 emit(ctx,(uint8_t)op.val); return; }
    if (op.type == OP_IND_IY) { emit(ctx,0xFD); emit(ctx,(uint8_t)((r_op|(6<<3))));
                                 emit(ctx,(uint8_t)op.val); return; }
    error(ctx, "invalid INC/DEC operand");
}

/* LD - the biggest group */
static void asm_ld(Ctx *ctx, Lex *lx) {
    Operand dst = parse_operand(ctx, lx);
    if (lex_peek(lx).type != TT_COMMA) { error(ctx,"LD: expected ','"); return; }
    lex_next(lx);
    Operand src = parse_operand(ctx, lx);

    /* LD r, r' */
    if (dst.type == OP_REG8 && src.type == OP_REG8) {
        uint8_t b = (uint8_t)(0x40 | ((uint8_t)dst.reg << 3) | (uint8_t)src.reg);
        if (b == 0x76) { error(ctx,"LD H,L encodes as HALT"); return; }
        emit(ctx, b); return;
    }
    /* LD r, n */
    if (dst.type == OP_REG8 && src.type == OP_IMM) {
        emit(ctx, (uint8_t)(0x06 | ((uint8_t)dst.reg << 3)));
        emit(ctx, (uint8_t)src.val); return;
    }
    /* LD r, (HL) */
    if (dst.type == OP_REG8 && src.type == OP_IND_HL) {
        emit(ctx, (uint8_t)(0x46 | ((uint8_t)dst.reg << 3))); return;
    }
    /* LD (HL), r */
    if (dst.type == OP_IND_HL && src.type == OP_REG8) {
        emit(ctx, (uint8_t)(0x70 | (uint8_t)src.reg)); return;
    }
    /* LD (HL), n */
    if (dst.type == OP_IND_HL && src.type == OP_IMM) {
        emit(ctx, 0x36); emit(ctx, (uint8_t)src.val); return;
    }
    /* LD r, (IX/IY+d) */
    if (dst.type == OP_REG8 && (src.type == OP_IND_IX || src.type == OP_IND_IY)) {
        uint8_t pfx = (src.type == OP_IND_IY) ? 0xFD : 0xDD;
        emit(ctx, pfx);
        emit(ctx, (uint8_t)(0x46 | ((uint8_t)dst.reg << 3)));
        emit(ctx, (uint8_t)src.val); return;
    }
    /* LD (IX/IY+d), r */
    if ((dst.type == OP_IND_IX || dst.type == OP_IND_IY) && src.type == OP_REG8) {
        uint8_t pfx = (dst.type == OP_IND_IY) ? 0xFD : 0xDD;
        emit(ctx, pfx); emit(ctx, (uint8_t)(0x70 | (uint8_t)src.reg));
        emit(ctx, (uint8_t)dst.val); return;
    }
    /* LD (IX/IY+d), n */
    if ((dst.type == OP_IND_IX || dst.type == OP_IND_IY) && src.type == OP_IMM) {
        uint8_t pfx = (dst.type == OP_IND_IY) ? 0xFD : 0xDD;
        emit(ctx, pfx); emit(ctx, 0x36);
        emit(ctx, (uint8_t)dst.val); emit(ctx, (uint8_t)src.val); return;
    }
    /* LD A, (BC/DE) */
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_IND_BC) { emit(ctx, 0x0A); return; }
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_IND_DE) { emit(ctx, 0x1A); return; }
    /* LD (BC/DE), A */
    if (dst.type == OP_IND_BC && src.type == OP_REG8 && src.reg == 7) { emit(ctx, 0x02); return; }
    if (dst.type == OP_IND_DE && src.type == OP_REG8 && src.reg == 7) { emit(ctx, 0x12); return; }
    /* LD A, (nn) */
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_IND_ADDR) {
        emit(ctx, 0x3A); emit_word(ctx, (uint16_t)src.val); return;
    }
    /* LD (nn), A */
    if (dst.type == OP_IND_ADDR && src.type == OP_REG8 && src.reg == 7) {
        emit(ctx, 0x32); emit_word(ctx, (uint16_t)dst.val); return;
    }
    /* LD HL, (nn) / LD (nn), HL */
    if (dst.type == OP_REG16 && dst.reg == 2 && src.type == OP_IND_ADDR) {
        emit(ctx, 0x2A); emit_word(ctx, (uint16_t)src.val); return;
    }
    if (dst.type == OP_IND_ADDR && src.type == OP_REG16 && src.reg == 2) {
        emit(ctx, 0x22); emit_word(ctx, (uint16_t)dst.val); return;
    }
    /* LD rr, nn */
    if (dst.type == OP_REG16 && src.type == OP_IMM) {
        emit(ctx, (uint8_t)(0x01 | ((uint8_t)dst.reg << 4)));
        emit_word(ctx, (uint16_t)src.val); return;
    }
    /* LD rr, (nn) - ED 4B prefix */
    if (dst.type == OP_REG16 && src.type == OP_IND_ADDR) {
        emit(ctx, 0xED); emit(ctx, (uint8_t)(0x4B | ((uint8_t)dst.reg << 4)));
        emit_word(ctx, (uint16_t)src.val); return;
    }
    /* LD (nn), rr - ED 43 prefix */
    if (dst.type == OP_IND_ADDR && src.type == OP_REG16) {
        emit(ctx, 0xED); emit(ctx, (uint8_t)(0x43 | ((uint8_t)src.reg << 4)));
        emit_word(ctx, (uint16_t)dst.val); return;
    }
    /* LD SP, HL/IX/IY */
    if (dst.type == OP_REG16 && dst.reg == 3 && src.type == OP_REG16 && src.reg == 2) {
        emit(ctx, 0xF9); return;
    }
    if (dst.type == OP_REG16 && dst.reg == 3 && src.type == OP_IX) { emit(ctx,0xDD); emit(ctx,0xF9); return; }
    if (dst.type == OP_REG16 && dst.reg == 3 && src.type == OP_IY) { emit(ctx,0xFD); emit(ctx,0xF9); return; }
    /* LD IX/IY, nn */
    if (dst.type == OP_IX && src.type == OP_IMM) {
        emit(ctx,0xDD); emit(ctx,0x21); emit_word(ctx,(uint16_t)src.val); return;
    }
    if (dst.type == OP_IY && src.type == OP_IMM) {
        emit(ctx,0xFD); emit(ctx,0x21); emit_word(ctx,(uint16_t)src.val); return;
    }
    /* LD IX/IY, (nn) */
    if (dst.type == OP_IX && src.type == OP_IND_ADDR) {
        emit(ctx,0xDD); emit(ctx,0x2A); emit_word(ctx,(uint16_t)src.val); return;
    }
    if (dst.type == OP_IY && src.type == OP_IND_ADDR) {
        emit(ctx,0xFD); emit(ctx,0x2A); emit_word(ctx,(uint16_t)src.val); return;
    }
    /* LD (nn), IX/IY */
    if (dst.type == OP_IND_ADDR && src.type == OP_IX) {
        emit(ctx,0xDD); emit(ctx,0x22); emit_word(ctx,(uint16_t)dst.val); return;
    }
    if (dst.type == OP_IND_ADDR && src.type == OP_IY) {
        emit(ctx,0xFD); emit(ctx,0x22); emit_word(ctx,(uint16_t)dst.val); return;
    }
    /* LD I/R, A and LD A, I/R */
    if (dst.type == OP_REG_I && src.type == OP_REG8 && src.reg == 7) { emit(ctx,0xED); emit(ctx,0x47); return; }
    if (dst.type == OP_REG_R && src.type == OP_REG8 && src.reg == 7) { emit(ctx,0xED); emit(ctx,0x4F); return; }
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_REG_I)  { emit(ctx,0xED); emit(ctx,0x57); return; }
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_REG_R)  { emit(ctx,0xED); emit(ctx,0x5F); return; }

    error(ctx, "LD: unrecognised operand combination");
}

/* PUSH / POP */
static void asm_push_pop(Ctx *ctx, Lex *lx, int is_pop) {
    Operand op = parse_operand(ctx, lx);
    uint8_t base = is_pop ? 0xC1 : 0xC5;
    if (op.type == OP_REG16 || op.type == OP_REG16AF) {
        emit(ctx, (uint8_t)(base | ((uint8_t)op.reg << 4))); return;
    }
    if (op.type == OP_IX) { emit(ctx,0xDD); emit(ctx, is_pop ? 0xE1 : 0xE5); return; }
    if (op.type == OP_IY) { emit(ctx,0xFD); emit(ctx, is_pop ? 0xE1 : 0xE5); return; }
    error(ctx, "%s: invalid register", is_pop ? "POP" : "PUSH");
}

/* JP / JR / DJNZ / CALL / RET / RST */
static void asm_jp(Ctx *ctx, Lex *lx) {
    Operand op = parse_operand(ctx, lx);
    /* "C" is parsed as OP_REG8; coerce to carry condition when comma follows */
    if (op.type == OP_REG8 && op.reg == 1 && lex_peek(lx).type == TT_COMMA)
        { op.type = OP_CC; op.reg = 3; }
    /* JP (HL/IX/IY) */
    if (op.type == OP_IND_HL)  { emit(ctx, 0xE9); return; }
    if (op.type == OP_IND_IX || op.type == OP_IX) { /* JP (IX) */
        /* handle both (IX) and IX */
        emit(ctx,0xDD); emit(ctx,0xE9); return;
    }
    if (op.type == OP_IND_IY || op.type == OP_IY) {
        emit(ctx,0xFD); emit(ctx,0xE9); return;
    }
    /* JP cc, nn */
    if (op.type == OP_CC) {
        int cc = op.reg;
        if (lex_peek(lx).type != TT_COMMA) { error(ctx,"JP cc: expected ','"); return; }
        lex_next(lx);
        bool undef = false;
        int32_t addr = eval_expr(ctx, lx, &undef);
        if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
        emit(ctx, (uint8_t)(0xC2 | ((uint8_t)cc << 3)));
        emit_word(ctx, (uint16_t)addr); return;
    }
    /* JP nn */
    if (op.type == OP_IMM) {
        emit(ctx, 0xC3); emit_word(ctx, (uint16_t)op.val); return;
    }
    error(ctx, "JP: invalid operand");
}

static void asm_jr(Ctx *ctx, Lex *lx) {
    Operand op = parse_operand(ctx, lx);
    /* "C" is parsed as OP_REG8; coerce to carry condition when comma follows */
    if (op.type == OP_REG8 && op.reg == 1 && lex_peek(lx).type == TT_COMMA)
        { op.type = OP_CC; op.reg = 3; }
    int32_t target;
    bool undef = false;
    if (op.type == OP_CC) {
        /* Only NZ Z NC C allowed for JR */
        int cc = op.reg;
        if (cc > 3) { error(ctx,"JR: condition must be NZ/Z/NC/C"); return; }
        if (lex_peek(lx).type != TT_COMMA) { error(ctx,"JR cc: expected ','"); return; }
        lex_next(lx);
        target = eval_expr(ctx, lx, &undef);
        if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
        int32_t offset = target - (int32_t)(ctx->pc + 2);
        if (ctx->pass == 2 && (offset < -128 || offset > 127))
            error(ctx,"JR offset out of range (%d)", (int)offset);
        uint8_t ops[4] = {0};
        ops[0] = (uint8_t)(0x20 | ((uint8_t)cc << 3));
        ops[1] = (uint8_t)(int8_t)offset;
        emit(ctx, ops[0]); emit(ctx, ops[1]); return;
    }
    /* JR nn (unconditional) */
    if (op.type == OP_IMM) {
        target = op.val; undef = op.undef;
        int32_t offset = target - (int32_t)(ctx->pc + 2);
        if (ctx->pass == 2 && (offset < -128 || offset > 127))
            error(ctx,"JR offset out of range (%d)", (int)offset);
        emit(ctx, 0x18); emit(ctx, (uint8_t)(int8_t)offset); return;
    }
    error(ctx, "JR: invalid operand");
}

static void asm_djnz(Ctx *ctx, Lex *lx) {
    bool undef = false;
    int32_t target = eval_expr(ctx, lx, &undef);
    if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
    int32_t offset = target - (int32_t)(ctx->pc + 2);
    if (ctx->pass == 2 && (offset < -128 || offset > 127))
        error(ctx,"DJNZ offset out of range (%d)", (int)offset);
    emit(ctx, 0x10); emit(ctx, (uint8_t)(int8_t)offset);
}

static void asm_call(Ctx *ctx, Lex *lx) {
    Operand op = parse_operand(ctx, lx);
    /* "C" is parsed as OP_REG8; coerce to carry condition when comma follows */
    if (op.type == OP_REG8 && op.reg == 1 && lex_peek(lx).type == TT_COMMA)
        { op.type = OP_CC; op.reg = 3; }
    if (op.type == OP_CC) {
        int cc = op.reg;
        if (lex_peek(lx).type != TT_COMMA) { error(ctx,"CALL cc: expected ','"); return; }
        lex_next(lx);
        bool undef = false;
        int32_t addr = eval_expr(ctx, lx, &undef);
        if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
        emit(ctx, (uint8_t)(0xC4 | ((uint8_t)cc << 3)));
        emit_word(ctx, (uint16_t)addr); return;
    }
    if (op.type == OP_IMM) {
        emit(ctx, 0xCD); emit_word(ctx, (uint16_t)op.val); return;
    }
    error(ctx, "CALL: invalid operand");
}

static void asm_ret(Ctx *ctx, Lex *lx) {
    Token t = lex_peek(lx);
    if (t.type == TT_IDENT) {
        int cc = cond_idx(t.text);
        if (cc >= 0) {
            lex_next(lx);
            emit(ctx, (uint8_t)(0xC0 | ((uint8_t)cc << 3))); return;
        }
    }
    emit(ctx, 0xC9);
}

static void asm_rst(Ctx *ctx, Lex *lx) {
    bool undef = false;
    int32_t v = eval_expr(ctx, lx, &undef);
    if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
    if (v & ~0x38) warning(ctx,"RST vector not a multiple of 8 in range 0-0x38");
    emit(ctx, (uint8_t)(0xC7 | ((uint8_t)v & 0x38)));
}

/* CB-prefix: rotates and shifts.
 * op: RLC=0 RRC=1 RL=2 RR=3 SLA=4 SRA=5 SLL=6 SRL=7  */
static void asm_rot(Ctx *ctx, Lex *lx, int op) {
    Operand reg = parse_operand(ctx, lx);
    if (reg.type == OP_REG8) {
        emit(ctx, 0xCB); emit(ctx, (uint8_t)((op << 3) | reg.reg)); return;
    }
    if (reg.type == OP_IND_HL) {
        emit(ctx, 0xCB); emit(ctx, (uint8_t)((op << 3) | 6)); return;
    }
    if (reg.type == OP_IND_IX || reg.type == OP_IND_IY) {
        uint8_t pfx = (reg.type == OP_IND_IY) ? 0xFD : 0xDD;
        emit(ctx, pfx); emit(ctx, 0xCB);
        emit(ctx, (uint8_t)reg.val); emit(ctx, (uint8_t)((op << 3) | 6)); return;
    }
    error(ctx, "invalid rotate operand");
}

/* BIT / SET / RES: op = 0x40/0xC0/0x80 */
static void asm_bit_op(Ctx *ctx, Lex *lx, uint8_t base) {
    bool undef = false;
    int32_t bit = eval_expr(ctx, lx, &undef);
    if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
    if (bit < 0 || bit > 7) error(ctx,"bit number must be 0-7");
    if (lex_peek(lx).type != TT_COMMA) { error(ctx,"BIT/SET/RES: expected ','"); return; }
    lex_next(lx);
    Operand reg = parse_operand(ctx, lx);
    uint8_t cb_byte = (uint8_t)(base | ((uint8_t)(bit & 7) << 3));
    if (reg.type == OP_REG8) {
        emit(ctx, 0xCB); emit(ctx, (uint8_t)(cb_byte | (uint8_t)reg.reg)); return;
    }
    if (reg.type == OP_IND_HL) {
        emit(ctx, 0xCB); emit(ctx, (uint8_t)(cb_byte | 6)); return;
    }
    if (reg.type == OP_IND_IX || reg.type == OP_IND_IY) {
        uint8_t pfx = (reg.type == OP_IND_IY) ? 0xFD : 0xDD;
        emit(ctx, pfx); emit(ctx, 0xCB);
        emit(ctx, (uint8_t)reg.val); emit(ctx, (uint8_t)(cb_byte | 6)); return;
    }
    error(ctx, "BIT/SET/RES: invalid register operand");
}

/* EX instructions */
static void asm_ex(Ctx *ctx, Lex *lx) {
    Operand a = parse_operand(ctx, lx);
    if (lex_peek(lx).type != TT_COMMA) { error(ctx,"EX: expected ','"); return; }
    lex_next(lx);
    Operand b = parse_operand(ctx, lx);

    if (a.type == OP_REG16 && a.reg == 3 && b.type == OP_REG16 && b.reg == 2) { /* EX DE,HL */
        /* Actually DE=1, HL=2 */
        emit(ctx, 0xEB); return;
    }
    if (a.type == OP_REG16 && a.reg == 1 && b.type == OP_REG16 && b.reg == 2) { emit(ctx, 0xEB); return; } /* EX DE,HL */
    if (a.type == OP_REG16AF && a.reg == 3 && b.type == OP_AF_ALT) { emit(ctx, 0x08); return; } /* EX AF,AF' */
    if (a.type == OP_AF_ALT) { emit(ctx, 0x08); return; }
    if (a.type == OP_IND_SP) {
        if (b.type == OP_REG16 && b.reg == 2) { emit(ctx, 0xE3); return; } /* EX (SP),HL */
        if (b.type == OP_IX) { emit(ctx,0xDD); emit(ctx,0xE3); return; }
        if (b.type == OP_IY) { emit(ctx,0xFD); emit(ctx,0xE3); return; }
    }
    error(ctx, "EX: unrecognised operand combination");
}

/* IN / OUT */
static void asm_in(Ctx *ctx, Lex *lx) {
    Operand dst = parse_operand(ctx, lx);
    if (lex_peek(lx).type != TT_COMMA) { error(ctx,"IN: expected ','"); return; }
    lex_next(lx);
    Operand src = parse_operand(ctx, lx);
    /* IN A,(n) */
    if (dst.type == OP_REG8 && dst.reg == 7 && src.type == OP_IND_ADDR) {
        emit(ctx, 0xDB); emit(ctx, (uint8_t)src.val); return;
    }
    /* IN r,(C) */
    if (dst.type == OP_REG8 && src.type == OP_IND_C) {
        emit(ctx, 0xED); emit(ctx, (uint8_t)(0x40 | ((uint8_t)dst.reg << 3))); return;
    }
    error(ctx, "IN: invalid operands");
}

static void asm_out(Ctx *ctx, Lex *lx) {
    Operand dst = parse_operand(ctx, lx);
    if (lex_peek(lx).type != TT_COMMA) { error(ctx,"OUT: expected ','"); return; }
    lex_next(lx);
    Operand src = parse_operand(ctx, lx);
    /* OUT (n),A */
    if (dst.type == OP_IND_ADDR && src.type == OP_REG8 && src.reg == 7) {
        emit(ctx, 0xD3); emit(ctx, (uint8_t)dst.val); return;
    }
    /* OUT (C),r */
    if (dst.type == OP_IND_C && src.type == OP_REG8) {
        emit(ctx, 0xED); emit(ctx, (uint8_t)(0x41 | ((uint8_t)src.reg << 3))); return;
    }
    error(ctx, "OUT: invalid operands");
}

/* IM 0/1/2 */
static void asm_im(Ctx *ctx, Lex *lx) {
    bool undef = false;
    int32_t mode = eval_expr(ctx, lx, &undef);
    switch (mode) {
    case 0: emit(ctx,0xED); emit(ctx,0x46); return;
    case 1: emit(ctx,0xED); emit(ctx,0x56); return;
    case 2: emit(ctx,0xED); emit(ctx,0x5E); return;
    default: error(ctx,"IM: mode must be 0, 1 or 2");
    }
}

/*
 * DEFB / DEFW / DEFS / DEFM
 */
static void asm_defb(Ctx *ctx, Lex *lx) {
    for (;;) {
        Token t = lex_peek(lx);
        if (t.type == TT_STR) {
            lex_next(lx);
            for (int i = 0; t.text[i]; i++) emit(ctx, (uint8_t)t.text[i]);
        } else {
            bool undef = false;
            int32_t v = eval_expr(ctx, lx, &undef);
            if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
            emit(ctx, (uint8_t)v);
        }
        if (lex_peek(lx).type != TT_COMMA) break;
        lex_next(lx); /* consume ',' */
    }
}

static void asm_defw(Ctx *ctx, Lex *lx) {
    for (;;) {
        bool undef = false;
        int32_t v = eval_expr(ctx, lx, &undef);
        if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
        emit_word(ctx, (uint16_t)v);
        if (lex_peek(lx).type != TT_COMMA) break;
        lex_next(lx);
    }
}

static void asm_defs(Ctx *ctx, Lex *lx) {
    bool undef = false;
    int32_t count = eval_expr(ctx, lx, &undef);
    if (undef && ctx->pass == 2) error(ctx,"undefined symbol");
    int32_t fill = 0;
    if (lex_peek(lx).type == TT_COMMA) {
        lex_next(lx);
        fill = eval_expr(ctx, lx, &undef);
    }
    for (int32_t i = 0; i < count; i++) emit(ctx, (uint8_t)fill);
}

/*
 * Line assembler
 */
static void assemble_line(Ctx *ctx, const char *line) {
    Lex lx;
    lx.p   = line;
    lx.ctx = ctx;

    if (ctx->listing && ctx->pass == 2) {
        ctx->list_bytes[0] = '\0';
        ctx->list_nbytes   = 0;
    }

    Token t = lex_next(&lx);

    /* Blank or comment line */
    if (t.type == TT_EOF) return;

    /* Label definition */
    char label[SYM_NAME_MAX + 2] = "";
    if (t.type == TT_IDENT) {
        Token next = lex_peek(&lx);
        if (next.type == TT_COLON) {
            snprintf(label, SYM_NAME_MAX + 1, "%s", t.text);
            lex_next(&lx); /* consume ':' */
            /* In pass 1 and pass 2 define the label */
            sym_define(&ctx->syms, label, (int32_t)(uint16_t)ctx->pc);
            t = lex_next(&lx); /* move to mnemonic */
        } else if (next.type == TT_IDENT || next.type == TT_EQ) {
            /* Could be "LABEL EQU val" or "LABEL = val" - handle below */
        }
        /* else: t is the mnemonic, fall through */
    }

    if (t.type == TT_EOF) return;
    if (t.type != TT_IDENT) { error(ctx,"expected mnemonic"); return; }

    /* Handle IF/ELSE/ENDIF before skip check so nesting works */
    if (!strcmp(t.text,"IF")) {
        bool undef = false;
        int32_t v = eval_expr(ctx, &lx, &undef);
        if (!ctx->if_skip) {
            if (!v) { ctx->if_skip = true; ctx->if_skip_depth = ctx->if_depth; }
        }
        ctx->if_depth++;
        return;
    }
    if (!strcmp(t.text,"ELSE")) {
        if (ctx->if_depth == 0) { error(ctx,"ELSE without IF"); return; }
        if (!ctx->if_skip) { ctx->if_skip = true; ctx->if_skip_depth = ctx->if_depth - 1; }
        else if (ctx->if_skip_depth == ctx->if_depth - 1) ctx->if_skip = false;
        return;
    }
    if (!strcmp(t.text,"ENDIF")) {
        if (ctx->if_depth == 0) { error(ctx,"ENDIF without IF"); return; }
        ctx->if_depth--;
        if (ctx->if_skip && ctx->if_skip_depth == ctx->if_depth) ctx->if_skip = false;
        return;
    }

    if (ctx->if_skip) return;

    uint16_t line_pc = ctx->pc;  /* for listing */

    /* EQU / = */
    if (!strcmp(t.text,"EQU") || t.type == TT_EQ) {
        if (label[0] == '\0') { error(ctx,"EQU without label"); return; }
        bool undef = false;
        int32_t v = eval_expr(ctx, &lx, &undef);
        if (!undef) sym_define(&ctx->syms, label, v);
        else if (ctx->pass == 2) error(ctx,"EQU: undefined symbol in expression");
        /* EQU does not advance PC */
        ctx->pc = line_pc;
        return;
    }
    /* Check for implicit "label EQU" (label followed by = token) */
    if (t.type == TT_IDENT) {
        Token peek2 = lex_peek(&lx);
        if (peek2.type == TT_EQ) {
            /* label = expr */
            snprintf(label, SYM_NAME_MAX + 1, "%s", t.text);
            lex_next(&lx); /* consume '=' */
            bool undef = false;
            int32_t v = eval_expr(ctx, &lx, &undef);
            if (!undef) sym_define(&ctx->syms, label, v);
            ctx->pc = line_pc;
            return;
        }
        if (!strcmp(peek2.text, "EQU")) {
            snprintf(label, SYM_NAME_MAX + 1, "%s", t.text);
            lex_next(&lx); /* consume EQU */
            bool undef = false;
            int32_t v = eval_expr(ctx, &lx, &undef);
            if (!undef) sym_define(&ctx->syms, label, v);
            ctx->pc = line_pc;
            return;
        }
    }

    /* ORG */
    if (!strcmp(t.text,"ORG")) {
        bool undef = false;
        int32_t v = eval_expr(ctx, &lx, &undef);
        if (!undef) ctx->pc = (uint16_t)v;
        return;
    }
    /* END */
    if (!strcmp(t.text,"END")) { ctx->end_seen = true; return; }

    /* Data pseudo-ops */
    if (!strcmp(t.text,"DEFB") || !strcmp(t.text,"DB")) { asm_defb(ctx, &lx); goto listing; }
    if (!strcmp(t.text,"DEFW") || !strcmp(t.text,"DW")) { asm_defw(ctx, &lx); goto listing; }
    if (!strcmp(t.text,"DEFS") || !strcmp(t.text,"DS")) { asm_defs(ctx, &lx); goto listing; }
    if (!strcmp(t.text,"DEFM") || !strcmp(t.text,"DM")) { asm_defb(ctx, &lx); goto listing; }

    /* Real instructions */
    if (!strcmp(t.text,"NOP"))  { emit(ctx, 0x00); goto listing; }
    if (!strcmp(t.text,"HALT")) { emit(ctx, 0x76); goto listing; }
    if (!strcmp(t.text,"EI"))   { emit(ctx, 0xFB); goto listing; }
    if (!strcmp(t.text,"DI"))   { emit(ctx, 0xF3); goto listing; }
    if (!strcmp(t.text,"EXX"))  { emit(ctx, 0xD9); goto listing; }
    if (!strcmp(t.text,"RLCA")) { emit(ctx, 0x07); goto listing; }
    if (!strcmp(t.text,"RRCA")) { emit(ctx, 0x0F); goto listing; }
    if (!strcmp(t.text,"RLA"))  { emit(ctx, 0x17); goto listing; }
    if (!strcmp(t.text,"RRA"))  { emit(ctx, 0x1F); goto listing; }
    if (!strcmp(t.text,"DAA"))  { emit(ctx, 0x27); goto listing; }
    if (!strcmp(t.text,"CPL"))  { emit(ctx, 0x2F); goto listing; }
    if (!strcmp(t.text,"SCF"))  { emit(ctx, 0x37); goto listing; }
    if (!strcmp(t.text,"CCF"))  { emit(ctx, 0x3F); goto listing; }
    if (!strcmp(t.text,"RET"))  { asm_ret(ctx, &lx); goto listing; }
    if (!strcmp(t.text,"NEG"))  { emit(ctx,0xED); emit(ctx,0x44); goto listing; }
    if (!strcmp(t.text,"RETN")) { emit(ctx,0xED); emit(ctx,0x45); goto listing; }
    if (!strcmp(t.text,"RETI")) { emit(ctx,0xED); emit(ctx,0x4D); goto listing; }
    if (!strcmp(t.text,"RLD"))  { emit(ctx,0xED); emit(ctx,0x6F); goto listing; }
    if (!strcmp(t.text,"RRD"))  { emit(ctx,0xED); emit(ctx,0x67); goto listing; }
    if (!strcmp(t.text,"LDI"))  { emit(ctx,0xED); emit(ctx,0xA0); goto listing; }
    if (!strcmp(t.text,"CPI"))  { emit(ctx,0xED); emit(ctx,0xA1); goto listing; }
    if (!strcmp(t.text,"INI"))  { emit(ctx,0xED); emit(ctx,0xA2); goto listing; }
    if (!strcmp(t.text,"OUTI")) { emit(ctx,0xED); emit(ctx,0xA3); goto listing; }
    if (!strcmp(t.text,"LDD"))  { emit(ctx,0xED); emit(ctx,0xA8); goto listing; }
    if (!strcmp(t.text,"CPD"))  { emit(ctx,0xED); emit(ctx,0xA9); goto listing; }
    if (!strcmp(t.text,"IND"))  { emit(ctx,0xED); emit(ctx,0xAA); goto listing; }
    if (!strcmp(t.text,"OUTD")) { emit(ctx,0xED); emit(ctx,0xAB); goto listing; }
    if (!strcmp(t.text,"LDIR")) { emit(ctx,0xED); emit(ctx,0xB0); goto listing; }
    if (!strcmp(t.text,"CPIR")) { emit(ctx,0xED); emit(ctx,0xB1); goto listing; }
    if (!strcmp(t.text,"INIR")) { emit(ctx,0xED); emit(ctx,0xB2); goto listing; }
    if (!strcmp(t.text,"OTIR")) { emit(ctx,0xED); emit(ctx,0xB3); goto listing; }
    if (!strcmp(t.text,"LDDR")) { emit(ctx,0xED); emit(ctx,0xB8); goto listing; }
    if (!strcmp(t.text,"CPDR")) { emit(ctx,0xED); emit(ctx,0xB9); goto listing; }
    if (!strcmp(t.text,"INDR")) { emit(ctx,0xED); emit(ctx,0xBA); goto listing; }
    if (!strcmp(t.text,"OTDR")) { emit(ctx,0xED); emit(ctx,0xBB); goto listing; }

    if (!strcmp(t.text,"LD"))   { asm_ld(ctx, &lx);              goto listing; }
    if (!strcmp(t.text,"PUSH")) { asm_push_pop(ctx, &lx, 0);     goto listing; }
    if (!strcmp(t.text,"POP"))  { asm_push_pop(ctx, &lx, 1);     goto listing; }
    if (!strcmp(t.text,"JP"))   { asm_jp(ctx, &lx);              goto listing; }
    if (!strcmp(t.text,"JR"))   { asm_jr(ctx, &lx);              goto listing; }
    if (!strcmp(t.text,"DJNZ")) { asm_djnz(ctx, &lx);            goto listing; }
    if (!strcmp(t.text,"CALL")) { asm_call(ctx, &lx);            goto listing; }
    if (!strcmp(t.text,"RST"))  { asm_rst(ctx, &lx);             goto listing; }
    if (!strcmp(t.text,"EX"))   { asm_ex(ctx, &lx);              goto listing; }
    if (!strcmp(t.text,"IN"))   { asm_in(ctx, &lx);              goto listing; }
    if (!strcmp(t.text,"OUT"))  { asm_out(ctx, &lx);             goto listing; }
    if (!strcmp(t.text,"IM"))   { asm_im(ctx, &lx);              goto listing; }

    if (!strcmp(t.text,"ADD"))  { asm_alu(ctx, &lx, 0); goto listing; }
    if (!strcmp(t.text,"ADC"))  { asm_alu(ctx, &lx, 1); goto listing; }
    if (!strcmp(t.text,"SUB"))  { asm_alu(ctx, &lx, 2); goto listing; }
    if (!strcmp(t.text,"SBC"))  { asm_alu(ctx, &lx, 3); goto listing; }
    if (!strcmp(t.text,"AND"))  { asm_alu(ctx, &lx, 4); goto listing; }
    if (!strcmp(t.text,"XOR"))  { asm_alu(ctx, &lx, 5); goto listing; }
    if (!strcmp(t.text,"OR"))   { asm_alu(ctx, &lx, 6); goto listing; }
    if (!strcmp(t.text,"CP"))   { asm_alu(ctx, &lx, 7); goto listing; }
    if (!strcmp(t.text,"INC"))  { asm_inc_dec(ctx, &lx, 0); goto listing; }
    if (!strcmp(t.text,"DEC"))  { asm_inc_dec(ctx, &lx, 1); goto listing; }

    if (!strcmp(t.text,"RLC"))  { asm_rot(ctx,&lx,0); goto listing; }
    if (!strcmp(t.text,"RRC"))  { asm_rot(ctx,&lx,1); goto listing; }
    if (!strcmp(t.text,"RL"))   { asm_rot(ctx,&lx,2); goto listing; }
    if (!strcmp(t.text,"RR"))   { asm_rot(ctx,&lx,3); goto listing; }
    if (!strcmp(t.text,"SLA"))  { asm_rot(ctx,&lx,4); goto listing; }
    if (!strcmp(t.text,"SRA"))  { asm_rot(ctx,&lx,5); goto listing; }
    if (!strcmp(t.text,"SLL"))  { asm_rot(ctx,&lx,6); goto listing; } /* undocumented */
    if (!strcmp(t.text,"SRL"))  { asm_rot(ctx,&lx,7); goto listing; }
    if (!strcmp(t.text,"BIT"))  { asm_bit_op(ctx,&lx,0x40); goto listing; }
    if (!strcmp(t.text,"RES"))  { asm_bit_op(ctx,&lx,0x80); goto listing; }
    if (!strcmp(t.text,"SET"))  { asm_bit_op(ctx,&lx,0xC0); goto listing; }

    error(ctx, "unknown mnemonic '%s'", t.text);

listing:
    if (ctx->listing && ctx->pass == 2) {
        char lst[MAX_LINE_LEN];
        while (strlen(ctx->list_bytes) < 12) strcat(ctx->list_bytes, "   ");
        snprintf(lst, sizeof(lst), "%04X  %-12s  %s", line_pc, ctx->list_bytes, line);
        if (ctx->emit_line) ctx->emit_line(lst);
#ifndef Z80ASM_EMBEDDED
        else printf("%s\n", lst);
#endif
    }
    return;
}

/*
 * Source reader - scans a text buffer line by line for two-pass assembly.
 * Replaces the line_store[][MAX_LINE_LEN] static array from the CLI version,
 * keeping memory usage independent of source length.
 */
typedef struct {
    const char *buf;
    int         len;
    int         pos;
} SrcReader;

static void src_rewind(SrcReader *r) { r->pos = 0; }

/* Reads next line into `out` (max outlen bytes).  Strips CR/LF.
 * Returns true if a line was read (even an empty one), false at EOF. */
static bool src_next_line(SrcReader *r, char *out, int outlen) {
    if (r->pos >= r->len) return false;
    int i = 0;
    while (r->pos < r->len) {
        char c = r->buf[r->pos++];
        if (c == '\n') break;
        if (c == '\r') {
            if (r->pos < r->len && r->buf[r->pos] == '\n') r->pos++;
            break;
        }
        if (i < outlen - 1) out[i++] = c;
    }
    out[i] = '\0';
    return true;
}

/*
 * Two-pass driver (shared by CLI and embedded builds)
 */
static int assemble(SrcReader *src, Ctx *ctx, uint16_t origin) {
    char line[MAX_LINE_LEN];

    /* --- Pass 1: build symbol table --- */
    ctx->pass     = 1;
    ctx->pc       = origin;
    ctx->errors   = 0;
    ctx->end_seen = false;
    ctx->if_depth = 0;
    ctx->if_skip  = false;
    sym_init(&ctx->syms);
    src_rewind(src);

    for (int i = 0; i < MAX_LINES && !ctx->end_seen; i++) {
        if (!src_next_line(src, line, sizeof(line))) break;
        ctx->lineno = i + 1;
        assemble_line(ctx, line);
    }

    /* --- Pass 2: emit code --- */
    ctx->pass     = 2;
    ctx->pc       = origin;
    ctx->end_seen = false;
    ctx->if_depth = 0;
    ctx->if_skip  = false;
    src_rewind(src);

    uint16_t first_pc = origin, last_pc = origin;

    for (int i = 0; i < MAX_LINES && !ctx->end_seen; i++) {
        if (!src_next_line(src, line, sizeof(line))) break;
        ctx->lineno = i + 1;
        uint16_t pc_before = ctx->pc;
        assemble_line(ctx, line);
        if (ctx->pc > pc_before) last_pc = ctx->pc;
    }

    return ctx->errors ? ctx->errors : (int)(last_pc - first_pc);
}

/*
 * Embedded API  (Z80ASM_EMBEDDED build - no file I/O, no main)
 *
 * z80asm_assemble - assemble a null-terminated (or length-bounded) source
 * text buffer.  Output is written directly into mem[] starting at origin.
 *
 *   src        - assembly source text (may contain \n or \r\n line endings)
 *   src_len    - byte length of src (excluding any trailing NUL)
 *   mem        - 64 KB output array (caller provides; e.g. Z80 m[])
 *   origin     - assemble origin address (e.g. 0x8000)
 *   emit_fn    - callback for listing lines and error messages (may be NULL)
 *   listing    - true = emit a listing through emit_fn
 *
 * Returns number of bytes assembled (>= 0) or negative on error.
 */
#ifdef Z80ASM_EMBEDDED

int z80asm_assemble(const char *src, int src_len,
                    uint8_t *mem, uint16_t origin,
                    void (*emit_fn)(const char *s),
                    bool listing) {
    static Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out       = mem;
    ctx.listing   = listing;
    ctx.emit_line = emit_fn;

    SrcReader sr = { src, src_len, 0 };
    int rc = assemble(&sr, &ctx, origin);

    if (ctx.errors) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d error(s)", ctx.errors);
        if (emit_fn) emit_fn(buf);
        return -ctx.errors;
    }
    return rc;
}

#else  /* ------------------------------------------------------------------ */
/* 
 * Standalone CLI  (default build - requires stdio/stdlib)
 */
int main(int argc, char *argv[]) {
    const char *infile  = NULL;
    const char *binfile = NULL;
    const char *hexfile = NULL;
    bool listing = false;
    bool verbose = false;
    uint16_t origin = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i+1 < argc) { binfile = argv[++i]; }
        else if (!strcmp(argv[i], "-x") && i+1 < argc) { hexfile = argv[++i]; }
        else if (!strcmp(argv[i], "-l")) { listing = true; }
        else if (!strcmp(argv[i], "-v")) { verbose = true; }
        else if (!strcmp(argv[i], "-g") && i+1 < argc) { origin = (uint16_t)strtol(argv[++i], NULL, 0); }
        else if (argv[i][0] != '-') { infile = argv[i]; }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!infile) {
        fprintf(stderr,
            "z80asm - two-pass Z80 assembler\n"
            "Usage: z80asm [-o out.bin] [-x out.hex] [-l] [-v] [-g org] source.asm\n");
        return 1;
    }

    /* Default binary output: replace .asm extension with .bin */
    static char defbin[256];
    if (!binfile && !hexfile) {
        strncpy(defbin, infile, sizeof(defbin) - 5);
        char *dot = strrchr(defbin, '.');
        if (dot) *dot = '\0';
        strcat(defbin, ".bin");
        binfile = defbin;
    }

    /* Read source file into buffer */
    FILE *f = fopen(infile, "r");
    if (!f) { perror(infile); return 1; }
    static char src_buf[MAX_LINES * MAX_LINE_LEN];
    int src_len = (int)fread(src_buf, 1, sizeof(src_buf) - 1, f);
    fclose(f);
    if (src_len <= 0) { fprintf(stderr, "%s: empty or unreadable\n", infile); return 1; }
    src_buf[src_len] = '\0';

    /* Allocate context and output buffer */
    static Ctx     ctx;
    static uint8_t out_buf[65536];
    memset(&ctx, 0, sizeof(ctx));
    ctx.out     = out_buf;
    ctx.listing = listing;
    ctx.verbose = verbose;

    SrcReader sr = { src_buf, src_len, 0 };
    int rc = assemble(&sr, &ctx, origin);

    if (ctx.errors) {
        fprintf(stderr, "%d error(s); no output written.\n", ctx.errors);
        return ctx.errors;
    }

    int len = rc;   /* assemble() returns byte count on success */

    /* --- Binary output --- */
    if (binfile) {
        FILE *of = fopen(binfile, "wb");
        if (!of) { perror(binfile); return 1; }
        fwrite(ctx.out + origin, 1, (size_t)len, of);
        fclose(of);
        printf("Wrote %d bytes to %s\n", len, binfile);
    }

    /* --- Intel HEX output --- */
    if (hexfile) {
        FILE *of = fopen(hexfile, "w");
        if (!of) { perror(hexfile); return 1; }
        uint16_t addr = origin;
        uint16_t end  = (uint16_t)(origin + (uint16_t)len);
        while (addr < end) {
            int chunk = end - addr;
            if (chunk > 16) chunk = 16;
            uint8_t cksum = (uint8_t)chunk + (uint8_t)(addr >> 8) + (uint8_t)addr;
            fprintf(of, ":%02X%04X00", chunk, addr);
            for (int i = 0; i < chunk; i++) {
                fprintf(of, "%02X", ctx.out[addr + i]);
                cksum += ctx.out[addr + i];
            }
            fprintf(of, "%02X\n", (uint8_t)(0 - cksum));
            addr += (uint16_t)chunk;
        }
        fprintf(of, ":00000001FF\n");
        fclose(of);
        printf("Wrote Intel HEX to %s\n", hexfile);
    }

    /* Symbol table dump */
    if (verbose) {
        printf("\nSymbol table (%d entries):\n", ctx.syms.count);
        for (int i = 0; i < ctx.syms.count; i++)
            printf("  %-24s  %04X (%d)\n",
                   ctx.syms.tab[i].name,
                   (uint16_t)ctx.syms.tab[i].value,
                   ctx.syms.tab[i].value);
    }

    return 0;
}
#endif /* Z80ASM_EMBEDDED */
