#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/*
 * SIZES
 */
#define MAX_CODE      255  /* bytecode slots — uint8_t ip, 0..254     */
#define MAX_STACK      32  /* value stack depth                       */
#define MAX_CALLS      16  /* call frame depth                        */
#define MAX_ENV        32
#define MAX_USER_SYM   24  /* user-defined symbols (keywords are ROM) */
#define MAX_HEAP       64
#define HEAP_SLOTS      6  /* vals per flat cell                      */
#define MAX_FUN        48  /* templates + runtime closure instances   */
#define FUN_ARGS        4
#define ENV_SLOTS       6
#define MAX_STR        32
#define STR_LEN        16

/*
 * VALUE  (16-bit)
 *   bits [15:12] = tag,  bits [11:0] = payload
 */
typedef uint16_t Val;
#define TAG(v)    ((v) >> 12)
#define DAT(v)    ((v) & 0x0FFFu)
#define MK(t,d)   ((Val)(((unsigned)(t) << 12) | ((unsigned)(d) & 0x0FFFu)))

enum {
    T_NIL=0, T_INT=1, T_SYM=2, T_PAIR=3,
    T_FUN=4,  T_BOOL=5, T_CHAR=6, T_STR=7
};

#define NIL        MK(T_NIL,  0)
#define BOOL_T     MK(T_BOOL, 1)
#define BOOL_F     MK(T_BOOL, 0)
#define IS_FALSY(v) (TAG(v)==T_BOOL ? DAT(v)==0 : TAG(v)==T_NIL)

static int16_t sign12(uint16_t d) {
    return (d & 0x800u) ? (int16_t)(d | 0xF000u) : (int16_t)d;
}

/*
 * SYMBOLS — two-tier table
 *
 * Tier 1: keywords — fixed compile-time indices (ROM on Z80).
 *         kw_names[] is a const array; no RAM needed for names.
 * Tier 2: user symbols — dynamic RAM pool.
 *         Indices start at NUM_KEYWORDS.
 *
 * Savings vs. old flat syms[64]: ~480 bytes of RAM.
 */
#define SYM_LEN 12

/* ---- keyword enum (index == position in kw_names[]) ---- */
enum {
    K_QUOTE=0, K_DEFINE, K_SET, K_IF, K_COND, K_ELSE,
    K_LAMBDA, K_LET, K_LETREC, K_BEGIN, K_AND, K_OR,
    K_ADD, K_SUB, K_MUL, K_EQ, K_LT, K_CONS, K_NOT,
    K_CAR, K_CDR, K_PAIRP, K_NULLP,
    K_CHARP, K_CHAR2INT, K_INT2CHAR,
    K_STRP, K_STRLEN, K_STRREF, K_STRCAT,
    K_STREQ, K_SYM2STR, K_STR2SYM, K_NUM2STR, K_STR2NUM,
    K_DISPLAY, K_NEWLINE, K_WRITE, K_READ, K_ERROR,
    K_APPLY,
    K_UNQUOTE, K_LIST, K_APPEND,
    NUM_KEYWORDS   /* = 43 */
};

/* ---- keyword names in ROM (must match enum order exactly) ---- */
static const char kw_names[NUM_KEYWORDS][SYM_LEN] = {
    "quote",    "define",   "set!",     "if",       "cond",    "else",
    "lambda",   "let",      "letrec",   "begin",    "and",     "or",
    "+",        "-",        "*",        "=",        "<",       "cons",  "not",
    "car",      "cdr",      "pair?",    "null?",
    "char?",    "char->int","int->char",
    "string?",  "str-len",  "str-ref",  "str-append",
    "str=?",    "sym->str", "str->sym", "num->str", "str->num",
    "display",  "newline",  "write",    "read",     "error",
    "apply",
    "unquote",  "list",     "append"
};

/* ---- user symbol pool (RAM only) ---- */
static char    user_syms[MAX_USER_SYM][SYM_LEN];
static uint8_t n_user_syms = 0;

/* intern: returns sym index; keywords first, then user pool */
static uint8_t intern(const char* s) {
    for (int i = 0; i < NUM_KEYWORDS; i++)
        if (!strncmp(kw_names[i], s, SYM_LEN-1)) return (uint8_t)i;
    for (uint8_t i = 0; i < n_user_syms; i++)
        if (!strncmp(user_syms[i], s, SYM_LEN-1)) return (uint8_t)(NUM_KEYWORDS + i);
    if (n_user_syms >= MAX_USER_SYM) return 0;
    strncpy(user_syms[n_user_syms], s, SYM_LEN-1);
    user_syms[n_user_syms][SYM_LEN-1] = 0;
    return (uint8_t)(NUM_KEYWORDS + n_user_syms++);
}

/* sym_name: resolve any sym index → its name string */
static const char* sym_name(uint8_t id) {
    if (id < NUM_KEYWORDS) return kw_names[id];
    uint8_t u = (uint8_t)(id - NUM_KEYWORDS);
    return (u < n_user_syms) ? user_syms[u] : "";
}

/*
 * STRING POOL
 */
typedef struct { char s[STR_LEN]; } Str;
static Str     strs[MAX_STR];
static uint8_t nstrs = 0;

static uint8_t str_new(const char* s) {
    if (nstrs >= MAX_STR) return 0;
    uint8_t id = nstrs++;
    strncpy(strs[id].s, s, STR_LEN-1);
    strs[id].s[STR_LEN-1] = 0;
    return id;
}

/*
 * KEYWORD DISPATCH MACRO
 * (enum K_xxx defined in SYMBOLS section above)
 */
#define IS_KW(v, kid) (TAG(v)==T_SYM && DAT(v)==(kid))

/*
 * HEAP
 */
typedef struct { Val v[HEAP_SLOTS]; uint8_t n; uint8_t _pad[3]; } Cell; /* 16 B — i<<4 */
static Cell    heap[MAX_HEAP];
static uint8_t nheap = 0;

/*
 * ENVIRONMENT
 */
#define ENV_NULL 0xFF

typedef struct {
    uint8_t key[ENV_SLOTS];
    Val     val[ENV_SLOTS];
    uint8_t n, parent;
    uint8_t _pad[12];           /* pad to 32 B — i<<5 */
} Env;

static Env     envs[MAX_ENV];
static uint8_t nenvs = 0;

static uint8_t env_new(uint8_t par) {
    if (nenvs >= MAX_ENV) return 0;
    uint8_t id = nenvs++;
    envs[id].n = 0; envs[id].parent = par;
    return id;
}
static void env_def(uint8_t eid, uint8_t k, Val v) {
    Env* e = &envs[eid];
    for (uint8_t i = 0; i < e->n; i++)
        if (e->key[i] == k) { e->val[i] = v; return; }
    if (e->n >= ENV_SLOTS) return;
    e->key[e->n] = k; e->val[e->n] = v; e->n++;
}
static Val env_get(uint8_t eid, uint8_t k) {
    for (uint8_t c = eid; c != ENV_NULL; c = envs[c].parent)
        for (uint8_t i = 0; i < envs[c].n; i++)
            if (envs[c].key[i] == k) return envs[c].val[i];
    return NIL;
}

/*
 * FUNCTIONS
 */
typedef struct {
    uint8_t  addr;              /* code address — fits uint8_t (MAX_CODE=256) */
    uint8_t  env, argc;
    uint8_t  args[FUN_ARGS];
    uint8_t  _pad;              /* pad to 8 B — i<<3 */
} Fun;
static Fun     funs[MAX_FUN];
static uint8_t nfuns = 0;

/*
 * BYTECODE
 */
enum {
    OP_NOP,
    OP_PUSH, OP_POP, OP_DUP,
    OP_LOAD, OP_STORE,
    OP_ADD, OP_SUB, OP_MUL,
    OP_EQ, OP_LT, OP_NOT,
    OP_CONS, OP_CAR, OP_CDR, OP_PAIRP, OP_NULLP,
    OP_JZ, OP_JMP,
    OP_MKCLOS, OP_CALL, OP_TAILCALL, OP_RET, OP_HALT,
    /* chars */
    OP_CHARP, OP_CHAR2INT, OP_INT2CHAR,
    /* strings */
    OP_STRP, OP_STRLEN, OP_STRREF, OP_STRCAT,
    OP_STREQ, OP_SYM2STR, OP_STR2SYM,
    OP_NUM2STR, OP_STR2NUM,
    /* i/o */
    OP_DISPLAY, OP_NEWLINE,
    OP_WRITE,   /* write-style: strings quoted, chars #\x */
    OP_READ,    /* parse one expr from stdin              */
    OP_ERROR,   /* pop code, print E:N, halt              */
    /* higher-order */
    OP_APPLY,
    /* list construction */
    OP_LIST,    /* pop n vals, build list */
    OP_APPEND   /* pop two lists, concatenate */
};

/* Split bytecode arrays — Z80: ops[] is a straight byte stream indexed by
   one register; args[] is a parallel word array for operands.
   ip is uint8_t: one Z80 register holds the program counter.            */
static uint8_t  ops[MAX_CODE];    /* opcode byte per slot                */
static uint16_t args[MAX_CODE];   /* operand word per slot (0 if unused) */
static uint8_t  ncode = 0;

static void lc_emit(uint8_t op, uint16_t a) {  /* lisp compiler bytecode emitter */
    if (ncode < MAX_CODE) { ops[ncode] = op; args[ncode] = a; ncode++; }
}

/* -------------------------------------------------------
 * ERROR CODES   (E:N on stdout — like a ROM error table)
 *   E:1  syntax / unexpected token
 *   E:2  unbound variable
 *   E:3  type error (wrong type for op)
 *   E:4  stack overflow
 *   E:5  out of memory (heap / env full)
 *   E:6  wrong number of arguments
 *   W:1..W:5  resource low (heap/env/code/sym/str)
 * ----------------------------------------------------- */

/*
 * PARSER
 */
static const char* src;
static char        read_buf[128];  /* buffer for (read) */
static Val quasi(Val x);          /* forward — defined after parser */
static void print_val(Val v);     /* forward — defined after VM      */

static void skip_ws(void) {
    for (;;) {
        while (*src && isspace((unsigned char)*src)) src++;
        if (*src == ';') while (*src && *src != '\n') src++;
        else break;
    }
}

static Val parse_expr(void);

static Val parse_num(void) {
    int neg = (*src == '-'); if (neg) src++;
    uint16_t v = 0;
    while (isdigit((unsigned char)*src)) v = (uint16_t)(v*10 + (*src++ - '0'));
    return MK(T_INT, neg ? (uint16_t)(-(int16_t)v) : v);
}

static Val parse_atom(void) {
    char b[SYM_LEN]; int n = 0;
    while (*src && !isspace((unsigned char)*src)
           && *src != '(' && *src != ')' && *src != ';')
        b[n < SYM_LEN-1 ? n++ : n] = *src++;
    b[n] = 0;

    if (!strcmp(b, "#t"))  return BOOL_T;
    if (!strcmp(b, "#f"))  return BOOL_F;
    if (!strcmp(b, "nil")) return NIL;

    /* character literals: #\a  #\space  #\newline  #\tab */
    if (b[0]=='#' && b[1]=='\\') {
        if (!strcmp(b+2, "space"))   return MK(T_CHAR, ' ');
        if (!strcmp(b+2, "newline")) return MK(T_CHAR, '\n');
        if (!strcmp(b+2, "tab"))     return MK(T_CHAR, '\t');
        if (b[2] != 0)               return MK(T_CHAR, (uint8_t)b[2]);
    }

    return MK(T_SYM, intern(b));
}

/* string literal: "hello\nworld" */
static Val parse_string(void) {
    char b[STR_LEN]; int n = 0;
    src++; /* consume '"' */
    while (*src && *src != '"') {
        char c = *src++;
        if (c == '\\' && *src) {
            switch (*src++) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case '"': c = '"';  break;
            default:  c = *(src-1); break;
            }
        }
        if (n < STR_LEN-1) b[n++] = c;
    }
    if (*src == '"') src++;
    b[n] = 0;
    return MK(T_STR, str_new(b));
}

static Val mk_cell(Val* v, int n) {
    if (nheap >= MAX_HEAP) return NIL;
    uint8_t id = nheap++;
    heap[id].n = (uint8_t)(n < HEAP_SLOTS ? n : HEAP_SLOTS);
    for (int i = 0; i < heap[id].n; i++) heap[id].v[i] = v[i];
    return MK(T_PAIR, id);
}

static Val parse_list(void) {
    Val tmp[HEAP_SLOTS]; int n = 0;
    src++;
    for (;;) {
        skip_ws();
        if (!*src || *src == ')') { if (*src) src++; break; }
        if (n < HEAP_SLOTS) tmp[n++] = parse_expr();
        else parse_expr();
    }
    return mk_cell(tmp, n);
}

static Val parse_expr(void) {
    skip_ws();
    if (!*src) return NIL;
    if (*src == '(') return parse_list();
    if (*src == '"') return parse_string();
    if (*src == '\'') {
        src++;
        Val inner = parse_expr();
        Val q[2] = { MK(T_SYM, K_QUOTE), inner };
        return mk_cell(q, 2);
    }
    if (*src == '`') {            /* quasiquote: expand at parse time */
        src++;
        return quasi(parse_expr());
    }
    if (*src == ',') {            /* unquote: reader shorthand for (unquote x) */
        src++;
        Val inner = parse_expr();
        Val u[2] = { MK(T_SYM, K_UNQUOTE), inner };
        return mk_cell(u, 2);
    }
    if (isdigit((unsigned char)*src) ||
        (*src == '-' && isdigit((unsigned char)src[1])))
        return parse_num();
    return parse_atom();
}

/*
 * QUASIQUOTE  (parse-time transformation, no VM changes)
 *
 *  `atom        → (quote atom)
 *  `(unquote x) → x              (i.e. ,x)
 *  `(e1 e2 ...) → (list `e1 `e2 ...)
 *
 *  Splice (,@) deferred — wrap in a lambda for now.
 */
static Val quasi(Val x) {
    /* non-list atom: wrap in quote */
    if (TAG(x) != T_PAIR) {
        Val q[2] = { MK(T_SYM, K_QUOTE), x };
        return mk_cell(q, 2);
    }
    Cell* l = &heap[DAT(x)];
    /* empty list → '() */
    if (l->n == 0) {
        Val q[2] = { MK(T_SYM, K_QUOTE), NIL };
        return mk_cell(q, 2);
    }
    /* (unquote e) → e */
    if (l->n >= 2 && IS_KW(l->v[0], K_UNQUOTE))
        return l->v[1];
    /* general list → (list (quasi e0) (quasi e1) ...) */
    Val tmp[HEAP_SLOTS + 1]; int nt = 0;
    tmp[nt++] = MK(T_SYM, K_LIST);
    for (int i = 0; i < l->n && nt <= HEAP_SLOTS; i++)
        tmp[nt++] = quasi(l->v[i]);
    return mk_cell(tmp, nt);
}

/*
 * COMPILER
 */
static int compile(Val v, int tail);

static int compile_list(Val v, int tail) {
    Cell* l = &heap[DAT(v)];
    if (l->n == 0) { lc_emit(OP_PUSH, NIL); return 0; }
    Val head = l->v[0];

    if (IS_KW(head, K_QUOTE)) {
        lc_emit(OP_PUSH, l->n > 1 ? l->v[1] : NIL); return 0;
    }

    if (IS_KW(head, K_DEFINE) && l->n >= 3) {
        uint8_t name_sym;
        if (TAG(l->v[1]) == T_PAIR) {
            Cell* sig = &heap[DAT(l->v[1])];
            name_sym = (uint8_t)DAT(sig->v[0]);
            Val params[HEAP_SLOTS]; int np = 0;
            for (int i = 1; i < sig->n && np < HEAP_SLOTS; i++) params[np++] = sig->v[i];
            Val arglist = mk_cell(params, np);
            Val lam[HEAP_SLOTS]; int nl = 0;
            lam[nl++] = MK(T_SYM, K_LAMBDA); lam[nl++] = arglist;
            for (int i = 2; i < l->n && nl < HEAP_SLOTS; i++) lam[nl++] = l->v[i];
            compile(mk_cell(lam, nl), 0);
        } else {
            name_sym = (uint8_t)DAT(l->v[1]);
            compile(l->v[2], 0);
        }
        lc_emit(OP_STORE, name_sym); return 0;
    }

    if (IS_KW(head, K_SET) && l->n >= 3) {
        compile(l->v[2], 0);
        lc_emit(OP_STORE, (uint8_t)DAT(l->v[1])); return 0;
    }

    if (IS_KW(head, K_IF) && l->n >= 3) {
        compile(l->v[1], 0);
        uint8_t jz  = ncode; lc_emit(OP_JZ,  0);
        compile(l->v[2], tail);
        uint8_t jmp = ncode; lc_emit(OP_JMP, 0);
        args[jz]  = ncode;
        if (l->n >= 4) compile(l->v[3], tail); else lc_emit(OP_PUSH, NIL);
        args[jmp] = ncode;
        return 0;
    }

    if (IS_KW(head, K_COND) && l->n >= 2) {
        uint8_t jmps[HEAP_SLOTS]; int nj = 0;
        for (int i = 1; i < l->n; i++) {
            if (TAG(l->v[i]) != T_PAIR) continue;
            Cell* cl = &heap[DAT(l->v[i])];
            if (cl->n < 2) continue;
            if (IS_KW(cl->v[0], K_ELSE)) {
                compile(cl->v[1], tail);
            } else {
                compile(cl->v[0], 0);
                uint8_t jz = ncode; lc_emit(OP_JZ, 0);
                compile(cl->v[1], tail);
                if (nj < HEAP_SLOTS) jmps[nj++] = ncode;
                lc_emit(OP_JMP, 0); args[jz] = ncode;
                continue;
            }
            if (nj < HEAP_SLOTS) jmps[nj++] = ncode;
            lc_emit(OP_JMP, 0);
        }
        lc_emit(OP_PUSH, NIL);
        for (int i = 0; i < nj; i++) args[jmps[i]] = ncode;
        return 0;
    }

    if (IS_KW(head, K_LAMBDA) && l->n >= 3) {
        uint8_t skip = ncode; lc_emit(OP_JMP, 0);
        if (nfuns >= MAX_FUN) { args[skip] = ncode; lc_emit(OP_PUSH, NIL); return 0; }
        uint8_t fid = nfuns++;
        funs[fid].addr = ncode; funs[fid].env = ENV_NULL; funs[fid].argc = 0;
        if (TAG(l->v[1]) == T_PAIR) {
            Cell* params = &heap[DAT(l->v[1])];
            funs[fid].argc = (uint8_t)(params->n < FUN_ARGS ? params->n : FUN_ARGS);
            for (int i = 0; i < funs[fid].argc; i++)
                funs[fid].args[i] = (uint8_t)DAT(params->v[i]);
        }
        for (int i = 2; i < l->n - 1; i++) { compile(l->v[i], 0); lc_emit(OP_POP, 0); }
        compile(l->v[l->n-1], 1);
        lc_emit(OP_RET, 0);
        args[skip] = ncode;
        lc_emit(OP_MKCLOS, fid); return 0;
    }

    if (IS_KW(head, K_LET) && l->n >= 3 && TAG(l->v[1]) == T_PAIR) {
        Cell* binds = &heap[DAT(l->v[1])];
        uint8_t skip = ncode; lc_emit(OP_JMP, 0);
        if (nfuns >= MAX_FUN) { args[skip] = ncode; lc_emit(OP_PUSH, NIL); return 0; }
        uint8_t fid = nfuns++;
        funs[fid].addr = ncode; funs[fid].env = ENV_NULL;
        funs[fid].argc = (uint8_t)(binds->n < FUN_ARGS ? binds->n : FUN_ARGS);
        for (int i = 0; i < funs[fid].argc; i++)
            if (TAG(binds->v[i]) == T_PAIR)
                funs[fid].args[i] = (uint8_t)DAT(heap[DAT(binds->v[i])].v[0]);
        for (int i = 2; i < l->n - 1; i++) { compile(l->v[i], 0); lc_emit(OP_POP, 0); }
        compile(l->v[l->n-1], tail);
        lc_emit(OP_RET, 0);
        args[skip] = ncode;
        lc_emit(OP_MKCLOS, fid);
        for (int i = 0; i < binds->n; i++) {
            if (TAG(binds->v[i]) == T_PAIR) {
                Cell* b = &heap[DAT(binds->v[i])];
                if (b->n >= 2) compile(b->v[1], 0); else lc_emit(OP_PUSH, NIL);
            } else lc_emit(OP_PUSH, NIL);
        }
        lc_emit(OP_CALL, (uint16_t)binds->n); return 0;
    }

    if (IS_KW(head, K_LETREC) && l->n >= 3 && TAG(l->v[1]) == T_PAIR) {
        Cell* binds = &heap[DAT(l->v[1])];
        uint8_t skip = ncode; lc_emit(OP_JMP, 0);
        if (nfuns >= MAX_FUN) { args[skip] = ncode; lc_emit(OP_PUSH, NIL); return 0; }
        uint8_t fid = nfuns++;
        funs[fid].addr = ncode; funs[fid].env = ENV_NULL; funs[fid].argc = 0;
        for (int i = 0; i < binds->n; i++) {
            if (TAG(binds->v[i]) != T_PAIR) continue;
            uint8_t sym = (uint8_t)DAT(heap[DAT(binds->v[i])].v[0]);
            lc_emit(OP_PUSH, NIL); lc_emit(OP_STORE, sym); lc_emit(OP_POP, 0);
        }
        for (int i = 0; i < binds->n; i++) {
            if (TAG(binds->v[i]) != T_PAIR) continue;
            Cell* b = &heap[DAT(binds->v[i])];
            uint8_t sym = (uint8_t)DAT(b->v[0]);
            if (b->n >= 2) compile(b->v[1], 0); else lc_emit(OP_PUSH, NIL);
            lc_emit(OP_STORE, sym); lc_emit(OP_POP, 0);
        }
        for (int i = 2; i < l->n - 1; i++) { compile(l->v[i], 0); lc_emit(OP_POP, 0); }
        compile(l->v[l->n-1], tail);
        lc_emit(OP_RET, 0);
        args[skip] = ncode;
        lc_emit(OP_MKCLOS, fid);
        lc_emit(OP_CALL, 0); return 0;
    }

    if (IS_KW(head, K_BEGIN) && l->n >= 2) {
        for (int i = 1; i < l->n - 1; i++) { compile(l->v[i], 0); lc_emit(OP_POP, 0); }
        compile(l->v[l->n-1], tail); return 0;
    }

    if (IS_KW(head, K_AND) && l->n == 3) {
        compile(l->v[1], 0); lc_emit(OP_DUP, 0);
        uint8_t jz = ncode; lc_emit(OP_JZ, 0);
        lc_emit(OP_POP, 0); compile(l->v[2], tail);
        args[jz] = ncode; return 0;
    }

    if (IS_KW(head, K_OR) && l->n == 3) {
        compile(l->v[1], 0); lc_emit(OP_DUP, 0);
        uint8_t jz  = ncode; lc_emit(OP_JZ,  0);
        uint8_t jmp = ncode; lc_emit(OP_JMP, 0);
        args[jz] = ncode; lc_emit(OP_POP, 0);
        compile(l->v[2], tail); args[jmp] = ncode; return 0;
    }

    /* (list e1 e2 ...) */
    if (IS_KW(head, K_LIST)) {
        if (l->n == 1) { lc_emit(OP_PUSH, NIL); return 0; }
        for (int i = 1; i < l->n; i++) compile(l->v[i], 0);
        lc_emit(OP_LIST, (uint16_t)(l->n - 1));
        return 0;
    }

    /* (append lst1 lst2) */
    if (IS_KW(head, K_APPEND) && l->n == 3) {
        compile(l->v[1], 0); compile(l->v[2], 0);
        lc_emit(OP_APPEND, 0); return 0;
    }

    /* (apply fn lst) — call fn with elements of lst as args */
    if (IS_KW(head, K_APPLY) && l->n == 3) {
        compile(l->v[1], 0);   /* function */
        compile(l->v[2], 0);   /* argument list */
        lc_emit(OP_APPLY, 0);
        return 0;
    }

    /* (newline) / (read) — zero-arg I/O */
    if (IS_KW(head, K_NEWLINE) && l->n == 1) {
        lc_emit(OP_NEWLINE, 0); return 0;
    }
    if (IS_KW(head, K_READ) && l->n == 1) {
        lc_emit(OP_READ, 0); return 0;
    }

#define BINOP(kw,op) if (IS_KW(head,kw) && l->n==3) \
    { compile(l->v[1],0); compile(l->v[2],0); lc_emit(op,0); return 0; }
    BINOP(K_ADD,    OP_ADD)    BINOP(K_SUB,    OP_SUB)    BINOP(K_MUL,    OP_MUL)
    BINOP(K_EQ,     OP_EQ)     BINOP(K_LT,     OP_LT)     BINOP(K_CONS,   OP_CONS)
    BINOP(K_STRREF, OP_STRREF) BINOP(K_STRCAT, OP_STRCAT) BINOP(K_STREQ,  OP_STREQ)
#undef BINOP

#define UNOP(kw,op) if (IS_KW(head,kw) && l->n==2) \
    { compile(l->v[1],0); lc_emit(op,0); return 0; }
    UNOP(K_NOT,     OP_NOT)    UNOP(K_CAR,     OP_CAR)    UNOP(K_CDR,     OP_CDR)
    UNOP(K_PAIRP,   OP_PAIRP)  UNOP(K_NULLP,   OP_NULLP)
    UNOP(K_CHARP,   OP_CHARP)  UNOP(K_CHAR2INT,OP_CHAR2INT) UNOP(K_INT2CHAR,OP_INT2CHAR)
    UNOP(K_STRP,    OP_STRP)   UNOP(K_STRLEN,  OP_STRLEN) UNOP(K_SYM2STR, OP_SYM2STR)
    UNOP(K_STR2SYM, OP_STR2SYM) UNOP(K_NUM2STR, OP_NUM2STR) UNOP(K_STR2NUM, OP_STR2NUM)
    UNOP(K_DISPLAY, OP_DISPLAY)
    UNOP(K_WRITE,   OP_WRITE)
    UNOP(K_ERROR,   OP_ERROR)
#undef UNOP

    compile(l->v[0], 0);
    for (int i = 1; i < l->n; i++) compile(l->v[i], 0);
    lc_emit(tail ? OP_TAILCALL : OP_CALL, (uint16_t)(l->n - 1));
    return 0;
}

static int compile(Val v, int tail) {
    switch (TAG(v)) {
    case T_INT: case T_NIL: case T_BOOL: case T_CHAR: case T_STR:
        lc_emit(OP_PUSH, v); return 0;
    case T_SYM:
        lc_emit(OP_LOAD, DAT(v)); return 0;
    case T_PAIR:
        return compile_list(v, tail);
    }
    return 0;
}

/*
 * VM
 */
static Val     stk[MAX_STACK];
static uint8_t lsp     = 0;   /* Lisp value-stack pointer (renamed from sp to avoid
                                  conflict with the Z80 emulator's u16 sp register) */
static uint8_t cur_env = 0;

typedef struct { uint8_t ip; uint8_t env; uint8_t fid; } Frame; /* 3 bytes */
static Frame   cs[MAX_CALLS];
static uint8_t csp     = 0;
static uint8_t cur_fid = 0xFF;

#define PUSH(v) do { if (lsp < MAX_STACK) stk[lsp++] = (v); } while(0)
#define POP()   (lsp > 0 ? stk[--lsp] : NIL)
#define PEEK()  (lsp > 0 ? stk[lsp-1] : NIL)

/* Drain a runtime list into an array of Vals.
   Handles both flat multi-element cells '(1 2 3) AND proper cons chains.
   For n=2 cells: if v[1] is T_PAIR or T_NIL it's a cons tail; otherwise
   it's a plain second element in a flat 2-element list. */
static int drain_list(Val lst, Val* out, int max) {
    int n = 0;
    Val cur = lst;
    while (TAG(cur)==T_PAIR && n<max) {
        Cell* c = &heap[DAT(cur)];
        if (c->n == 0) break;
        if (c->n > 2) {
            for (int i=0; i<c->n && n<max; i++) out[n++] = c->v[i];
            break;
        }
        out[n++] = c->v[0];
        if (c->n < 2) break;
        uint8_t t1 = TAG(c->v[1]);
        if      (t1 == T_PAIR) { cur = c->v[1]; }         /* cons tail */
        else if (t1 == T_NIL)  { break; }                  /* end of proper list */
        else { if (n<max) out[n++] = c->v[1]; break; }    /* flat 2-element cell */
    }
    return n;
}

/* display: print value without quoting strings/chars (for 'display' primitive) */
static void display_val(Val v) {
    switch (TAG(v)) {
    case T_NIL:  fputs("()", stdout); break;
    case T_BOOL: fputs(DAT(v) ? "#t" : "#f", stdout); break;
    case T_INT:  printf("%d", sign12(DAT(v))); break;
    case T_SYM:  fputs(sym_name((uint8_t)DAT(v)), stdout); break;
    case T_CHAR: putchar((int)(DAT(v) & 0x7Fu)); break;
    case T_STR:  fputs(strs[DAT(v)].s, stdout); break;
    case T_PAIR: {
        Cell* c = &heap[DAT(v)];
        putchar('(');
        for (int i = 0; i < c->n; i++) { if (i) putchar(' '); display_val(c->v[i]); }
        putchar(')'); break;
    }
    case T_FUN: printf("#<fn/%d>", funs[DAT(v)].argc); break;
    }
}

/*
 * TRAMPOLINE VM
 *
 * Each opcode is a plain function void(*)(void).
 * The dispatch table optbl[] maps opcode → handler.
 *
 * On Z80 this becomes a JP-table:
 *   LD  A,(DE)        ; fetch opcode byte (DE = ip ptr into ops[])
 *   INC DE
 *   LD  L,A           ; index into 2-byte-address table
 *   LD  H,0
 *   ADD HL,HL
 *   LD  BC,optbl
 *   ADD HL,BC
 *   LD  A,(HL) / INC HL / LD H,(HL) / LD L,A
 *   JP  (HL)          ; one instruction to jump to the handler
 *
 * All state is global — no per-call stack frame in the VM loop.
 */
static uint8_t  vm_ip;
static uint16_t vm_arg;   /* argument of the current instruction */
static uint8_t  vm_halt;
static Val      vm_result;

/* --- helpers visible to all op functions --- */
static void do_call(uint8_t argc, int is_tail) {
    if (lsp < (unsigned)(argc+1)) { while(lsp) POP(); PUSH(NIL); return; }
    Val avals[FUN_ARGS];
    int na = argc < FUN_ARGS ? argc : FUN_ARGS;
    for (int i=na-1; i>=0; i--) avals[i] = POP();
    for (int i=na;   i<argc; i++) POP();
    Val fn = POP();
    if (TAG(fn) != T_FUN) { PUSH(NIL); return; }
    uint8_t fid = (uint8_t)DAT(fn);
    if (fid >= nfuns) { PUSH(NIL); return; }
    Fun* f = &funs[fid];
    if (is_tail && fid == cur_fid) {
        for (int i=0; i<f->argc && i<na; i++) env_def(cur_env, f->args[i], avals[i]);
    } else if (is_tail) {
        uint8_t ne = env_new(f->env);
        for (int i=0; i<f->argc && i<na; i++) env_def(ne, f->args[i], avals[i]);
        cur_env = ne; cur_fid = fid;
    } else {
        uint8_t ne = env_new(f->env);
        for (int i=0; i<f->argc && i<na; i++) env_def(ne, f->args[i], avals[i]);
        if (csp >= MAX_CALLS) { PUSH(NIL); return; }
        cs[csp++] = (Frame){vm_ip, cur_env, cur_fid};
        cur_env = ne; cur_fid = fid;
    }
    vm_ip = f->addr;
}

/* --- opcode handlers --- */
static void op_nop(void)     { }
static void op_push(void)    { PUSH(vm_arg); }
static void op_pop(void)     { POP(); }
static void op_dup(void)     { Val t = PEEK(); PUSH(t); }
static void op_load(void)    { PUSH(env_get(cur_env, (uint8_t)vm_arg)); }
static void op_store(void)   { env_def(cur_env, (uint8_t)vm_arg, POP()); PUSH(NIL); }

static void op_add(void) { Val b=POP(),a=POP(); PUSH(MK(T_INT,(DAT(a)+DAT(b))&0xFFF)); }
static void op_sub(void) { Val b=POP(),a=POP(); PUSH(MK(T_INT,(DAT(a)-DAT(b))&0xFFF)); }
static void op_mul(void) { Val b=POP(),a=POP(); PUSH(MK(T_INT,(DAT(a)*DAT(b))&0xFFF)); }
static void op_eq(void)  { Val b=POP(),a=POP(); PUSH(a==b ? BOOL_T : BOOL_F); }
static void op_lt(void)  {
    Val b=POP(),a=POP();
    PUSH(sign12(DAT(a)) < sign12(DAT(b)) ? BOOL_T : BOOL_F);
}
static void op_not(void)   { Val a=POP(); PUSH(IS_FALSY(a) ? BOOL_T : BOOL_F); }
static void op_pairp(void) { Val a=POP(); PUSH(TAG(a)==T_PAIR ? BOOL_T : BOOL_F); }
static void op_nullp(void) { Val a=POP(); PUSH(TAG(a)==T_NIL  ? BOOL_T : BOOL_F); }

static void op_cons(void) {
    Val b=POP(),a=POP();
    if (nheap>=MAX_HEAP) { PUSH(NIL); return; }
    uint8_t id=nheap++; heap[id].n=2; heap[id].v[0]=a; heap[id].v[1]=b;
    PUSH(MK(T_PAIR,id));
}
static void op_car(void) {
    Val a=POP();
    PUSH(TAG(a)==T_PAIR && heap[DAT(a)].n>0 ? heap[DAT(a)].v[0] : NIL);
}
static void op_cdr(void) {
    Val a=POP();
    if (TAG(a)!=T_PAIR) { PUSH(NIL); return; }
    Cell* c=&heap[DAT(a)];
    if (c->n<=1) { PUSH(NIL); return; }
    if (c->n==2) { PUSH(c->v[1]); return; }
    if (nheap>=MAX_HEAP) { PUSH(NIL); return; }
    uint8_t id=nheap++; heap[id].n=(uint8_t)(c->n-1);
    for (int i=0;i<heap[id].n;i++) heap[id].v[i]=c->v[i+1];
    PUSH(MK(T_PAIR,id));
}

static void op_jz(void)  { Val v=POP(); if (IS_FALSY(v)) vm_ip=(uint8_t)vm_arg; }
static void op_jmp(void) { vm_ip = (uint8_t)vm_arg; }

static void op_mkclos(void) {
    /* Each call allocates a fresh Fun slot (instance) copied from the
       compiled template, then captures the current env.
       Template stays untouched so the same lambda can be instantiated
       multiple times with different captured envs.                    */
    uint8_t tmpl = (uint8_t)vm_arg;
    if (tmpl >= nfuns || nfuns >= MAX_FUN) { PUSH(NIL); return; }
    uint8_t cid = nfuns++;
    funs[cid]     = funs[tmpl];   /* copy addr, argc, args from template */
    funs[cid].env = cur_env;      /* capture live env                    */
    PUSH(MK(T_FUN, cid));
}
static void op_call(void)     { do_call((uint8_t)vm_arg, 0); }
static void op_tailcall(void) { do_call((uint8_t)vm_arg, 1); }

static void op_ret(void) {
    Val r = POP();
    if (csp == 0) { vm_result = r; vm_halt = 1; return; }
    Frame fr = cs[--csp];
    cur_env = fr.env; cur_fid = fr.fid; vm_ip = fr.ip;
    PUSH(r);
}
static void op_halt(void) { vm_result = POP(); vm_halt = 1; }

static void op_charp(void)   { Val a=POP(); PUSH(TAG(a)==T_CHAR ? BOOL_T : BOOL_F); }
static void op_char2int(void){ Val a=POP(); PUSH(TAG(a)==T_CHAR ? MK(T_INT,DAT(a)):NIL); }
static void op_int2char(void){ Val a=POP(); PUSH(MK(T_CHAR, DAT(a) & 0x7Fu)); }

static void op_strp(void)  { Val a=POP(); PUSH(TAG(a)==T_STR ? BOOL_T : BOOL_F); }
static void op_strlen(void) {
    Val a=POP();
    PUSH(TAG(a)==T_STR ? MK(T_INT,(uint16_t)strlen(strs[DAT(a)].s)) : NIL);
}
static void op_strref(void) {
    Val idx=POP(), str=POP();
    if (TAG(str)!=T_STR) { PUSH(NIL); return; }
    int16_t i=sign12(DAT(idx));
    const char* s=strs[DAT(str)].s; int len=(int)strlen(s);
    PUSH(i>=0 && i<len ? MK(T_CHAR,(uint8_t)s[i]) : NIL);
}
static void op_strcat(void) {
    Val b=POP(),a=POP();
    if (TAG(a)!=T_STR||TAG(b)!=T_STR||nstrs>=MAX_STR){PUSH(NIL);return;}
    uint8_t id=nstrs++; int la=(int)strlen(strs[DAT(a)].s);
    strncpy(strs[id].s, strs[DAT(a)].s, STR_LEN-1);
    strncpy(strs[id].s+la, strs[DAT(b)].s, STR_LEN-1-la);
    strs[id].s[STR_LEN-1]=0; PUSH(MK(T_STR,id));
}
static void op_streq(void) {
    Val b=POP(),a=POP();
    if (TAG(a)!=T_STR||TAG(b)!=T_STR){PUSH(BOOL_F);return;}
    PUSH(!strncmp(strs[DAT(a)].s,strs[DAT(b)].s,STR_LEN) ? BOOL_T : BOOL_F);
}
static void op_sym2str(void) {
    Val a=POP();
    if (TAG(a)!=T_SYM||nstrs>=MAX_STR){PUSH(NIL);return;}
    PUSH(MK(T_STR, str_new(sym_name((uint8_t)DAT(a)))));
}
static void op_str2sym(void) {
    Val a=POP();
    if (TAG(a)!=T_STR){PUSH(NIL);return;}
    PUSH(MK(T_SYM, intern(strs[DAT(a)].s)));
}
static void op_num2str(void) {
    Val a=POP();
    if (nstrs>=MAX_STR){PUSH(NIL);return;}
    uint8_t id=nstrs++;
    int16_t n=sign12(DAT(a)); int pos=0; char* buf=strs[id].s;
    if (n<0){buf[pos++]='-';n=(int16_t)-n;}
    if (n==0){buf[pos++]='0';}
    else { char tmp[8]; int tl=0;
           while(n>0){tmp[tl++]=(char)('0'+n%10);n=(int16_t)(n/10);}
           for(int i=tl-1;i>=0&&pos<STR_LEN-1;i--) buf[pos++]=tmp[i]; }
    buf[pos]=0; PUSH(MK(T_STR,id));
}
static void op_str2num(void) {
    Val a=POP();
    if (TAG(a)!=T_STR){PUSH(NIL);return;}
    const char* s=strs[DAT(a)].s;
    int neg=(*s=='-'); if(neg) s++;
    uint16_t v=0;
    while(isdigit((unsigned char)*s)) v=(uint16_t)(v*10+(*s++)-'0');
    PUSH(MK(T_INT, neg?(uint16_t)(-(int16_t)v):v));
}

static void op_display(void) { display_val(POP()); PUSH(NIL); }
static void op_newline(void) { putchar('\n'); PUSH(NIL); }
static void op_write(void)   { print_val(POP()); PUSH(NIL); }
static void op_read(void) {
    const char* saved=src;
    if (fgets(read_buf,(int)sizeof read_buf,stdin)){ src=read_buf; PUSH(parse_expr()); }
    else PUSH(NIL);
    src=saved;
}
static void op_error(void) {
    Val e=POP();
    fputs("\nE:", stdout);
    if (TAG(e)==T_INT) printf("%d", sign12(DAT(e))); else putchar('?');
    putchar('\n');
    vm_result=NIL; vm_halt=1;
}

static void op_apply(void) {
    Val lst=POP(), fn=POP();
    if (TAG(fn)!=T_FUN){PUSH(NIL);return;}
    uint8_t fid=(uint8_t)DAT(fn);
    if (fid>=nfuns){PUSH(NIL);return;}
    Fun* f=&funs[fid];
    Val avals[FUN_ARGS]; int na=0;
    na = drain_list(lst, avals, FUN_ARGS);
    uint8_t ne=env_new(f->env);
    for (int i=0;i<f->argc&&i<na;i++) env_def(ne,f->args[i],avals[i]);
    if (csp>=MAX_CALLS){PUSH(NIL);return;}
    cs[csp++]=(Frame){vm_ip,cur_env,cur_fid};
    cur_env=ne; cur_fid=fid; vm_ip=f->addr;
}

static void op_list(void) {
    uint8_t n=(uint8_t)vm_arg;
    if (n==0){PUSH(NIL);return;}
    if (nheap>=MAX_HEAP){for(uint8_t i=0;i<n;i++)POP();PUSH(NIL);return;}
    uint8_t id=nheap++; uint8_t cn=n<HEAP_SLOTS?n:HEAP_SLOTS;
    heap[id].n=cn;
    for (int i=cn-1;i>=0;i--) heap[id].v[i]=POP();
    for (uint8_t i=cn;i<n;i++) POP();
    PUSH(MK(T_PAIR,id));
}
static void op_append(void) {
    Val b=POP(),a=POP();
    if (TAG(a)==T_NIL){PUSH(b);return;}
    if (TAG(b)==T_NIL){PUSH(a);return;}
    Val tmp[HEAP_SLOTS]; int nt=0;
    nt  = drain_list(a, tmp,    HEAP_SLOTS);
    nt += drain_list(b, tmp+nt, HEAP_SLOTS-nt);
    if (nheap>=MAX_HEAP){PUSH(NIL);return;}
    uint8_t id=nheap++; heap[id].n=(uint8_t)nt;
    for (int i=0;i<nt;i++) heap[id].v[i]=tmp[i];
    PUSH(MK(T_PAIR,id));
}

/*
 * DISPATCH TABLE  (must match the OP_xxx enum order)
 * On Z80: a word-table of JP targets; index = opcode byte.
 */
typedef void (*OpFn)(void);
static const OpFn optbl[] = {
    op_nop,      op_push,    op_pop,     op_dup,
    op_load,     op_store,
    op_add,      op_sub,     op_mul,
    op_eq,       op_lt,      op_not,
    op_cons,     op_car,     op_cdr,     op_pairp,   op_nullp,
    op_jz,       op_jmp,
    op_mkclos,   op_call,    op_tailcall, op_ret,    op_halt,
    op_charp,    op_char2int, op_int2char,
    op_strp,     op_strlen,  op_strref,  op_strcat,
    op_streq,    op_sym2str, op_str2sym,
    op_num2str,  op_str2num,
    op_display,  op_newline,
    op_write,    op_read,    op_error,
    op_apply,
    op_list,     op_append
};

static Val vm_run(uint8_t start_ip, uint8_t start_env) {
    vm_ip = start_ip; vm_halt = 0; vm_result = NIL;
    cur_env = start_env; lsp = 0; csp = 0; cur_fid = 0xFF;
    while (!vm_halt && vm_ip < ncode) {
        vm_arg = args[vm_ip];
        optbl[ops[vm_ip++]]();
    }
    return vm_result;
}

/*
 * PRINT  (write semantics: strings quoted, chars #\x)
 */
static void print_val(Val v) {
    switch (TAG(v)) {
    case T_NIL:  fputs("()", stdout); break;
    case T_BOOL: fputs(DAT(v) ? "#t" : "#f", stdout); break;
    case T_INT:  printf("%d", sign12(DAT(v))); break;
    case T_SYM:  fputs(sym_name((uint8_t)DAT(v)), stdout); break;
    case T_CHAR: {
        uint8_t ch=(uint8_t)DAT(v);
        if      (ch==' ')  fputs("#\\space",   stdout);
        else if (ch=='\n') fputs("#\\newline",  stdout);
        else if (ch=='\t') fputs("#\\tab",      stdout);
        else { putchar('#'); putchar('\\'); putchar(ch); }
        break;
    }
    case T_STR: {
        putchar('"');
        const char* s=strs[DAT(v)].s;
        while (*s) {
            if      (*s=='"')  fputs("\\\"", stdout);
            else if (*s=='\\') fputs("\\\\", stdout);
            else if (*s=='\n') fputs("\\n",  stdout);
            else putchar(*s);
            s++;
        }
        putchar('"'); break;
    }
    case T_PAIR: {
        Cell* c=&heap[DAT(v)];
        putchar('(');
        for (int i=0;i<c->n;i++) { if(i) putchar(' '); print_val(c->v[i]); }
        putchar(')'); break;
    }
    case T_FUN: printf("#<fn/%d>", funs[DAT(v)].argc); break;
    }
}

/*
 * REPL
 */
static void repl(void) {
    char line[128];
    uint8_t genv = env_new(ENV_NULL);
    env_def(genv, intern("t"),   BOOL_T);
    env_def(genv, intern("nil"), NIL);

    for (;;) {
        fputs("> ", stdout); fflush(stdout);
        if (!fgets(line, (int)sizeof line, stdin)) break;

        const char* p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == ';') continue;

        uint8_t start_ip = ncode;
        src = line;
        Val ast = parse_expr();

        compile(ast, 0);
        lc_emit(OP_HALT, 0);

        Val result = vm_run(start_ip, genv);
        genv = cur_env;

        if (result != NIL) { print_val(result); putchar('\n'); }

        if (nheap  > MAX_HEAP - 8)  fputs("W:1\n", stdout);         /* heap low */
        if (nenvs  > MAX_ENV  - 4)  fputs("W:2\n", stdout);         /*  env low */
        if (ncode  > MAX_CODE - 16) fputs("W:3\n", stdout);         /* code low */
        if (n_user_syms > MAX_USER_SYM - 4) fputs("W:4\n", stdout); /*  sym low */
        if (nstrs  > MAX_STR  - 4)  fputs("W:5\n", stdout);         /*  str low */
    }
}

#ifdef LISP_EMBEDDED
/* -----------------------------------------------------------------------
 * Embedded API — available when lisp.c is #included into another TU
 * (e.g. test_vm.c).  Not compiled into the standalone REPL binary.
 * ----------------------------------------------------------------------- */

/* Reset all interpreter state so a fresh compile/run can start. */
static void lisp_reset(void) {
    ncode = 0; nfuns = 0; nenvs = 0; nheap = 0; nstrs = 0;
    n_user_syms = 0;
    lsp = 0; csp = 0; cur_env = 0; cur_fid = 0xFF;
    vm_halt = 0; vm_result = NIL;
}

/* Parse and compile one expression from the C string s.
 * Appends OP_HALT so vm_run() terminates cleanly.
 * Returns the start ip (= ncode before this call).          */
static uint8_t lisp_compile_src(const char *s) {
    src = s;
    Val ast = parse_expr();
    uint8_t start = ncode;
    compile(ast, 0);
    lc_emit(OP_HALT, 0);
    return start;
}

/* Run the C VM from start_ip in a freshly-allocated root env.
 * Returns the final result Val.                              */
static Val lisp_run_c(uint8_t start_ip) {
    uint8_t genv = env_new(ENV_NULL);
    return vm_run(start_ip, genv);
}
#endif /* LISP_EMBEDDED */

#ifndef LISP_EMBEDDED
int main(void) {
    repl();
    return 0;
}
#endif
