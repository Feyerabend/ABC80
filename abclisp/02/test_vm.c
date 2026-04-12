/*
 * test_vm.c — Z80 transpilation test harness for abclisp
 *
 * Each test assembles a Z80 source snippet, runs it in the emulator, and
 * checks an expected result.  Tests are ordered by phase from TRANSPILE.md:
 *
 *   Phase 0 — harness smoke test
 *   Phase 2 — Val encoding / decoding (tag + payload)
 *   Phase 3 — Lisp stack (IX-relative push/pop of Val words)
 *   Phase 4 — INT arithmetic (extract payload, add, re-tag)
 *   Phase 5 — Fetch-dispatch loop (NOP / HALT via jump table)
 *   Phase 6 — OP_PUSH + OP_ADD end-to-end mini-VM
 *   Phase 7 — ENV_ADDR, ENV_DEF, ENV_GET subroutines
 *
 * Z80 constraints discovered:
 *   - LD (nn), r  is only valid for r=A; use LD (nn), HL / BC / DE for 16-bit.
 *   - ADD HL, HL doubles HL entirely; to double only an index use
 *     LD L, index; LD H, 0; ADD HL, HL; LD DE, base; ADD HL, DE.
 *
 * Build (single translation unit — includes all deps):
 *
 *   cc -std=c11 -O2 -Wall -Wno-unused-function -o test_vm test_vm.c
 *
 * Run:
 *   ./test_vm
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "vm_config.h"

/* -----------------------------------------------------------------------
 * Pull in the Z80 emulator.
 * z80.h defines m[65536], pc, sp, a–l, halted, init(), step() etc.
 * Port hooks must be visible before z80.c code; z80.h typedefs u8 etc.
 * ----------------------------------------------------------------------- */
#include "z80.h"

/* Character output hook: port 0 → out_buf (used by the port-based PUTCHAR). */
static char out_buf[512];
static int  out_pos = 0;

u8   port_in (u8 port)          { (void)port; return 0; }
void port_out(u8 port, u8 val)  { if (port == 0 && out_pos < 511) out_buf[out_pos++] = (char)val; }

#define Z80ASM_EMBEDDED          /* embedded API: z80asm_assemble(), no main() */
#include "z80.c"
#include "z80asm.c"

/* Memory-read hook required by disasm.c. */
uint8_t z80_read_mem(uint16_t addr) { return m[addr]; }
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#else
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "disasm.c"

/* Include the Lisp interpreter in embedded mode: exposes lisp_reset(),
 * lisp_compile_src(), lisp_run_c(), plus the raw ops[]/args[]/funs[] arrays
 * and the OP_xxx enum.  The Z80 emulator's 'u16 sp' does not conflict because
 * lisp.c's Lisp stack pointer is renamed 'lsp' under LISP_EMBEDDED.         */
#define LISP_EMBEDDED
#include "lisp.c"

/* -----------------------------------------------------------------------
 * Harness helpers
 * ----------------------------------------------------------------------- */
static int pass_count = 0;
static int fail_count = 0;

/* Reset, assemble src, run up to max_steps; return 1 if halted normally. */
static int run_asm(const char *src, int max_steps)
{
    memset(m, 0, sizeof m);
    out_pos = 0;
    int rc = z80asm_assemble(src, (int)strlen(src), m, 0x0000, NULL, false);
    if (rc < 0) {
        fprintf(stderr, "  [assemble error %d]\n", rc);
        return 0;
    }
    init();
    pc = 0x0000;
    sp = 0xFE00;    /* keep hardware stack far from data and code */
    for (int i = 0; i < max_steps && !halted; i++) step();
    return (int)halted;
}

/* Assemble src into m[] at given origin; caller must memset/init first. */
static int asm_at(const char *src, uint16_t origin)
{
    int rc = z80asm_assemble(src, (int)strlen(src), m, origin, NULL, false);
    if (rc < 0) fprintf(stderr, "  [assemble error %d]\n", rc);
    return rc;
}

/* Disassemble [start, end) to stdout for debugging. */
static void disasm_range(uint16_t start, uint16_t end)
{
    char buf[64];
    uint16_t addr = start;
    while (addr < end) {
        int n = z80_disasm(addr, buf, (int)sizeof buf);
        printf("    %04X  %s\n", addr, buf);
        if (n <= 0) break;
        addr = (uint16_t)(addr + n);
    }
}

#define CHECK(desc, cond) \
    do { \
        if (cond) { printf("PASS  %s\n", desc); pass_count++; } \
        else      { printf("FAIL  %s\n", desc); fail_count++;  } \
    } while (0)

/* -----------------------------------------------------------------------
 * Val encoding constants (mirror lisp.c)
 *   bits [15:12] = tag,  bits [11:0] = payload (12-bit signed)
 * ----------------------------------------------------------------------- */
#define T_NIL   0
#define T_INT   1
#define T_SYM   2
#define T_PAIR  3
#define T_FUN   4
#define T_BOOL  5
#define T_CHAR  6
#define T_STR   7

#define MK_VAL(tag, pay)  (uint16_t)(((uint16_t)(tag) << 12) | ((uint16_t)(pay) & 0x0FFF))
#define VAL_TAG(v)        (((v) >> 12) & 0x0F)
#define VAL_PAY(v)        ((v) & 0x0FFF)

/* -----------------------------------------------------------------------
 * Phase 0 — Harness smoke test
 * ----------------------------------------------------------------------- */
static void test_harness(void)
{
    puts("--- Phase 0: harness ---");

    run_asm(
        "  LD   A, 42\n"
        "  HALT\n",
        100);

    CHECK("LD A,42 / HALT  (a == 42)", a == 42);
}

/* -----------------------------------------------------------------------
 * Phase 2a — Val tag extraction
 *
 * Z80 convention: HL holds a Val (16-bit).
 *   tag  = H >> 4  (i.e. 4 × RRCA then AND 0Fh)
 *   pay  = (H & 0Fh) << 8 | L
 *
 * Z80 note: LD (nn), A is the only legal 8-bit absolute store.
 * ----------------------------------------------------------------------- */
static void test_val_tag(void)
{
    puts("--- Phase 2a: Val tag extraction ---");

    char src[256];
    uint16_t vals[] = {
        MK_VAL(T_NIL,  0),
        MK_VAL(T_INT,  42),
        MK_VAL(T_SYM,  3),
        MK_VAL(T_BOOL, 1),
    };
    int expected_tags[] = { T_NIL, T_INT, T_SYM, T_BOOL };
    const char *names[]  = { "NIL(0)", "INT(42)", "SYM(3)", "BOOL(#t)" };

    for (int i = 0; i < 4; i++) {
        snprintf(src, sizeof src,
            "  LD   HL, 0x%04X\n"
            "  LD   A, H\n"
            "  RRCA\n"
            "  RRCA\n"
            "  RRCA\n"
            "  RRCA\n"
            "  AND  0x0F\n"
            "  LD   (0x8000), A\n"   /* LD (nn),A is the only valid 8-bit store */
            "  HALT\n",
            vals[i]);

        run_asm(src, 200);
        int got_tag = m[0x8000];
        char desc[64];
        snprintf(desc, sizeof desc, "tag(%s) == %d", names[i], expected_tags[i]);
        CHECK(desc, got_tag == expected_tags[i]);
    }
}

/* -----------------------------------------------------------------------
 * Phase 2b — Val payload extraction
 * ----------------------------------------------------------------------- */
static void test_val_payload(void)
{
    puts("--- Phase 2b: Val payload extraction ---");

    struct { uint16_t val; int pay; const char *name; } cases[] = {
        { MK_VAL(T_INT,   0),    0,   "INT(0)"    },
        { MK_VAL(T_INT,  42),   42,   "INT(42)"   },
        { MK_VAL(T_SYM,  15),   15,   "SYM(15)"   },
        { MK_VAL(T_INT, 255),  255,   "INT(255)"  },
    };

    char src[256];
    for (int i = 0; i < 4; i++) {
        snprintf(src, sizeof src,
            "  LD   HL, 0x%04X\n"
            "  LD   A, H\n"
            "  AND  0x0F\n"          /* high nibble of payload */
            "  LD   (0x8000), A\n"
            "  LD   A, L\n"          /* low byte of payload */
            "  LD   (0x8001), A\n"
            "  HALT\n",
            cases[i].val);

        run_asm(src, 200);
        int hi  = m[0x8000];
        int lo  = m[0x8001];
        int pay = (hi << 8) | lo;
        char desc[64];
        snprintf(desc, sizeof desc, "payload(%s) == %d", cases[i].name, cases[i].pay);
        CHECK(desc, pay == cases[i].pay);
    }
}

/* -----------------------------------------------------------------------
 * Phase 3 — Lisp stack (push / pop via IX)
 *
 * The Lisp value stack lives at 0x4000 upward (separate from Z80 hardware
 * stack which uses SP).  IX is the Lisp stack pointer.
 *
 * PUSH HL  →  LD (IX+0),L / LD (IX+1),H / INC IX / INC IX
 * POP  HL  →  DEC IX / DEC IX / LD L,(IX+0) / LD H,(IX+1)
 *
 * Z80 note: to store a 16-bit register pair to an absolute address use
 * LD (nn), HL  or  LD (nn), DE  (ED prefix) — not LD (nn), r for 8-bit.
 * ----------------------------------------------------------------------- */
static void test_lisp_stack(void)
{
    puts("--- Phase 3: Lisp stack push/pop ---");

    /* Push INT(42), pop into DE, store to 0x8000 via LD (nn),DE. */
    uint16_t val = MK_VAL(T_INT, 42);
    char src[512];
    snprintf(src, sizeof src,
        "  LD   IX, 0x4000\n"
        "  LD   HL, 0x%04X\n"
        "  LD   (IX+0), L\n"
        "  LD   (IX+1), H\n"
        "  INC  IX\n"
        "  INC  IX\n"
        "  DEC  IX\n"
        "  DEC  IX\n"
        "  LD   E, (IX+0)\n"
        "  LD   D, (IX+1)\n"
        "  LD   (0x8000), DE\n"      /* ED prefix: legal 16-bit store */
        "  HALT\n",
        val);

    run_asm(src, 300);
    uint16_t result = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
    CHECK("push INT(42) / pop → same Val", result == val);

    /* Push two Vals, pop them, check LIFO order. */
    uint16_t v1 = MK_VAL(T_INT, 10);
    uint16_t v2 = MK_VAL(T_SYM,  7);
    snprintf(src, sizeof src,
        "  LD   IX, 0x4000\n"
        "  LD   HL, 0x%04X\n"        /* push v1 */
        "  LD   (IX+0), L\n"
        "  LD   (IX+1), H\n"
        "  INC  IX\n"
        "  INC  IX\n"
        "  LD   HL, 0x%04X\n"        /* push v2 */
        "  LD   (IX+0), L\n"
        "  LD   (IX+1), H\n"
        "  INC  IX\n"
        "  INC  IX\n"
        "  DEC  IX\n"                /* pop → BC  (should be v2) */
        "  DEC  IX\n"
        "  LD   C, (IX+0)\n"
        "  LD   B, (IX+1)\n"
        "  DEC  IX\n"                /* pop → DE  (should be v1) */
        "  DEC  IX\n"
        "  LD   E, (IX+0)\n"
        "  LD   D, (IX+1)\n"
        "  LD   (0x8000), BC\n"      /* store v2 */
        "  LD   (0x8002), DE\n"      /* store v1 */
        "  HALT\n",
        v1, v2);

    run_asm(src, 500);
    uint16_t top    = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
    uint16_t second = (uint16_t)(m[0x8002] | ((uint16_t)m[0x8003] << 8));
    CHECK("LIFO: pop returns v2 first", top    == v2);
    CHECK("LIFO: pop returns v1 second", second == v1);
}

/* -----------------------------------------------------------------------
 * Phase 4 — INT addition (extract + add + re-tag)
 *
 * Loads two Val words from fixed memory, extracts 12-bit payloads
 * (sign-extending at bit 11), adds, masks back to 12 bits, re-tags INT.
 *
 * Z80 note: LD (nn),HL is the 16-bit store; use that instead of two
 *           separate LD (nn),H / LD (nn),L which are not valid Z80.
 * ----------------------------------------------------------------------- */
static void test_int_add(void)
{
    puts("--- Phase 4: INT arithmetic (12-bit payload add) ---");

    const char *tmpl =
        "  LD   HL, (0x9000)\n"  /* HL = Val_a */
        "  LD   A, H\n"
        "  AND  0x0F\n"          /* mask tag bits */
        "  BIT  3, A\n"          /* sign-extend bit 11 */
        "  JR   Z, no_ext_a\n"
        "  OR   0xF0\n"
        "no_ext_a:\n"
        "  LD   H, A\n"
        "  PUSH HL\n"            /* save payload_a on Z80 stack */
        "  LD   HL, (0x9002)\n"  /* HL = Val_b */
        "  LD   A, H\n"
        "  AND  0x0F\n"
        "  BIT  3, A\n"
        "  JR   Z, no_ext_b\n"
        "  OR   0xF0\n"
        "no_ext_b:\n"
        "  LD   H, A\n"
        "  EX   DE, HL\n"        /* DE = payload_b */
        "  POP  HL\n"            /* HL = payload_a */
        "  ADD  HL, DE\n"        /* HL = payload_a + payload_b (16-bit) */
        "  LD   A, H\n"
        "  AND  0x0F\n"          /* mask to bits 11:8 */
        "  OR   0x10\n"          /* tag = 1 (T_INT) */
        "  LD   H, A\n"
        "  LD   (0x8000), HL\n"  /* LD (nn),HL — legal 16-bit absolute store */
        "  HALT\n";

    /* INT(3) + INT(4) = INT(7) */
    {
        uint16_t va = MK_VAL(T_INT, 3), vb = MK_VAL(T_INT, 4);
        memset(m, 0, sizeof m);
        asm_at(tmpl, 0x0000);
        m[0x9000] = (uint8_t)(va & 0xFF); m[0x9001] = (uint8_t)(va >> 8);
        m[0x9002] = (uint8_t)(vb & 0xFF); m[0x9003] = (uint8_t)(vb >> 8);
        init(); pc = 0; sp = 0xFE00;
        for (int i = 0; i < 500 && !halted; i++) step();
        uint16_t result = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
        CHECK("INT(3) + INT(4) = INT(7)", result == MK_VAL(T_INT, 7));
    }

    /* INT(2000) + INT(100): 2100 & 0xFFF = 52 (12-bit wrap) */
    {
        uint16_t va = MK_VAL(T_INT, 2000), vb = MK_VAL(T_INT, 100);
        memset(m, 0, sizeof m);
        asm_at(tmpl, 0x0000);
        m[0x9000] = (uint8_t)(va & 0xFF); m[0x9001] = (uint8_t)(va >> 8);
        m[0x9002] = (uint8_t)(vb & 0xFF); m[0x9003] = (uint8_t)(vb >> 8);
        init(); pc = 0; sp = 0xFE00;
        for (int i = 0; i < 500 && !halted; i++) step();
        uint16_t result = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
        int expected_pay = (2000 + 100) & 0x0FFF;   /* 52 */
        CHECK("INT(2000)+INT(100) wraps to INT(52)", result == MK_VAL(T_INT, expected_pay));
    }
}

/* -----------------------------------------------------------------------
 * Phase 5 — Fetch-dispatch loop  (NOP and HALT opcodes)
 *
 * Memory layout:
 *   0x4100  ops[]  — one byte per instruction slot
 *   0x4200  optbl  — jump table, 2 bytes per entry (LE address)
 *
 * B = vm_ip (uint8_t).
 * Fetch:
 *   HL = ops_base + B  →  A = opcode  →  INC B
 *   HL = 2*A + optbl_base  →  JP (HL)
 *
 * Opcodes: 0=NOP, 1=HALT
 * Program: ops[0]=NOP, ops[1]=NOP, ops[2]=HALT
 * Expected: halted, B==3.
 * ----------------------------------------------------------------------- */
static void test_dispatch(void)
{
    puts("--- Phase 5: fetch-dispatch loop ---");

    const char *src =
        "  ORG  0x0000\n"
        "  LD   SP, 0xFE00\n"
        "  LD   B, 0\n"

        "fetch:\n"
        "  LD   HL, 0x4100\n"   /* ops_base */
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"       /* HL = &ops[B] */
        "  LD   A, (HL)\n"      /* A = opcode */
        "  INC  B\n"            /* vm_ip++ */
        "  LD   L, A\n"         /* compute handler address */
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"       /* HL = 2 * opcode */
        "  LD   DE, 0x4200\n"   /* DE = optbl base */
        "  ADD  HL, DE\n"       /* HL = &optbl[opcode] */
        "  LD   C, (HL)\n"
        "  INC  HL\n"
        "  LD   H, (HL)\n"
        "  LD   L, C\n"
        "  JP   (HL)\n"

        "op_nop:\n"
        "  JP   fetch\n"

        "op_halt:\n"
        "  HALT\n"

        "  ORG  0x4200\n"       /* optbl */
        "  DEFW op_nop\n"       /* [0] NOP */
        "  DEFW op_halt\n"      /* [1] HALT */

        "  ORG  0x4100\n"       /* ops program */
        "  DEFB 0, 0, 1\n";     /* NOP, NOP, HALT */

    int ok = run_asm(src, 1000);
    CHECK("dispatch: NOP,NOP,HALT → halted",  ok == 1);
    CHECK("dispatch: vm_ip advanced to 3",    b == 3);
}

/* -----------------------------------------------------------------------
 * Phase 6 — OP_PUSH + OP_ADD mini-VM end-to-end
 *
 * Opcodes: 0=NOP, 1=PUSH(arg), 2=ADD, 3=HALT
 * Program: PUSH INT(3), PUSH INT(4), ADD, HALT
 * Expected: INT(7) on Lisp stack at 0x8000 after execution.
 *
 * Memory layout:
 *   IX       Lisp stack pointer (base 0x4000, grows upward)
 *   B        vm_ip (uint8_t)
 *   DE       vm_arg (loaded during fetch)
 *   0x4100   ops[]  (1 byte each)
 *   0x4200   args[] (2 bytes each, little-endian)
 *   0x4300   optbl  (2 bytes per entry)
 *
 * Z80 bug to avoid:
 *   "ADD HL, HL" doubles the FULL HL including base address.
 *   Correct word-index: LD L,B; LD H,0; ADD HL,HL; LD DE,base; ADD HL,DE.
 * ----------------------------------------------------------------------- */
static void test_push_add(void)
{
    puts("--- Phase 6: OP_PUSH + OP_ADD mini-VM ---");

    const char *src =
        "  ORG  0x0000\n"
        "  LD   IX, 0x4000\n"   /* Lisp stack base */
        "  LD   SP, 0xFE00\n"
        "  LD   B, 0\n"         /* vm_ip = 0 */

        /* ---- fetch: load opcode (A) and vm_arg (DE), advance ip (B) ---- */
        "fetch:\n"
        /* opcode = ops[B] */
        "  LD   HL, 0x4100\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"       /* HL = &ops[B] */
        "  LD   A, (HL)\n"      /* A  = opcode */
        /* arg = args[B] — word array: correct indexing = 2*B + base */
        "  LD   L, B\n"         /* HL = B (as word index) */
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"       /* HL = 2*B */
        "  LD   DE, 0x4200\n"
        "  ADD  HL, DE\n"       /* HL = &args[B] */
        "  LD   E, (HL)\n"      /* E  = low byte of arg */
        "  INC  HL\n"
        "  LD   D, (HL)\n"      /* D  = high byte; DE = vm_arg */
        /* ip++ */
        "  INC  B\n"
        /* dispatch via jump table at 0x4300 */
        "  LD   L, A\n"
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"       /* HL = 2 * opcode */
        "  PUSH BC\n"           /* save B=vm_ip (BC clobbered by table base) */
        "  LD   BC, 0x4300\n"
        "  ADD  HL, BC\n"       /* HL = &optbl[opcode] */
        "  LD   C, (HL)\n"
        "  INC  HL\n"
        "  LD   H, (HL)\n"
        "  LD   L, C\n"
        "  POP  BC\n"           /* restore B=vm_ip */
        "  JP   (HL)\n"         /* DE = vm_arg still intact */

        /* ---- op_nop (0) ---- */
        "op_nop:\n"
        "  JP   fetch\n"

        /* ---- op_push (1): push vm_arg (DE) onto Lisp stack ---- */
        "op_push:\n"
        "  LD   (IX+0), E\n"
        "  LD   (IX+1), D\n"
        "  INC  IX\n"
        "  INC  IX\n"
        "  JP   fetch\n"

        /* ---- op_add (2): pop two INT Vals, add payloads, push result ---- */
        "op_add:\n"
        /* pop TOS → HL, strip tag to get 12-bit payload */
        "  DEC  IX\n"
        "  DEC  IX\n"
        "  LD   L, (IX+0)\n"
        "  LD   H, (IX+1)\n"
        "  LD   A, H\n"
        "  AND  0x0F\n"
        "  LD   H, A\n"         /* HL = payload of TOS */
        /* pop second → DE, strip tag */
        "  DEC  IX\n"
        "  DEC  IX\n"
        "  LD   E, (IX+0)\n"
        "  LD   D, (IX+1)\n"
        "  LD   A, D\n"
        "  AND  0x0F\n"
        "  LD   D, A\n"         /* DE = payload of second */
        /* add, mask to 12 bits, re-tag as INT */
        "  ADD  HL, DE\n"
        "  LD   A, H\n"
        "  AND  0x0F\n"
        "  OR   0x10\n"         /* tag = 1 (T_INT) */
        "  LD   H, A\n"
        /* push result */
        "  LD   (IX+0), L\n"
        "  LD   (IX+1), H\n"
        "  INC  IX\n"
        "  INC  IX\n"
        "  JP   fetch\n"

        /* ---- op_halt (3): store TOS to 0x8000, halt ---- */
        "op_halt:\n"
        "  DEC  IX\n"
        "  DEC  IX\n"
        "  LD   A, (IX+0)\n"    /* LD A,(IX+d) is valid (DD 7E d) */
        "  LD   (0x8000), A\n"
        "  LD   A, (IX+1)\n"
        "  LD   (0x8001), A\n"
        "  HALT\n"

        /* ---- data tables ---- */
        "  ORG  0x4300\n"
        "  DEFW op_nop\n"
        "  DEFW op_push\n"
        "  DEFW op_add\n"
        "  DEFW op_halt\n"

        "  ORG  0x4100\n"
        "  DEFB 1, 1, 2, 3\n"   /* PUSH, PUSH, ADD, HALT */

        "  ORG  0x4200\n"
        "  DEFW 0x1003\n"        /* args[0] = INT(3) */
        "  DEFW 0x1004\n"        /* args[1] = INT(4) */
        "  DEFW 0x0000\n"        /* args[2] (ADD  — unused) */
        "  DEFW 0x0000\n";       /* args[3] (HALT — unused) */

    int ok = run_asm(src, 5000);
    uint16_t result = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));

    CHECK("PUSH INT(3), PUSH INT(4), ADD → INT(7)", result == MK_VAL(T_INT, 7));
    CHECK("vm_ip advanced to 4 (PUSH,PUSH,ADD,HALT)", b == 4);
    CHECK("halted after HALT opcode", ok == 1);
}

/* -----------------------------------------------------------------------
 * Phase 7 — ENV_ADDR, ENV_DEF, ENV_GET
 *
 * Env struct layout (32 bytes, padded to power of 2):
 *   offset  0..5  : key[6]   (uint8_t sym indices)
 *   offset  6..17 : val[6]   (uint16_t × 6, little-endian)
 *   offset 18     : n        (uint8_t — number of bindings)
 *   offset 19     : parent   (uint8_t — 0xFF = no parent)
 *   offset 20..31 : _pad[12]
 *
 * ENVS_BASE = 0x4000 (first Env frame in RAM)
 *
 * ENV_ADDR(A=frame_index) → HL = ENVS_BASE + A*32
 *   5× ADD HL,HL then ADD HL,DE
 *
 * ENV_DEF(A=frame, C=sym, HL=val)
 *   Scan key[] for C; if found update val[i].
 *   If not found and n<6, append at slot n, increment n.
 *   Carry clear on success, set on overflow (n==6 and not found).
 *
 * ENV_GET(A=frame, C=sym) → HL=val, CF=0 found / CF=1 not-found
 *   Scan current frame; tail-recurse to parent if not found.
 *   Returns CF=1 when parent==0xFF and sym not in frame.
 *
 * Register conventions:
 *   IX = current frame base (set by CALL ENV_ADDR; PUSH HL; POP IX)
 *   B  = loop counter i
 *   C  = sym key (preserved throughout)
 *   D  = frame n (read once from (IX+18))
 *   Hardware stack: val pushed at entry, recovered at write-back point.
 * ----------------------------------------------------------------------- */

/* Shared ENV subroutines assembled at 0x0200 so tests can CALL them. */
static const char *env_subs =
    "  ORG  0x0200\n"

    /* ------------------------------------------------------------------
     * ENV_ADDR: A=frame_index → HL = 0x4000 + A*32
     * ------------------------------------------------------------------ */
    "ENV_ADDR:\n"
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* ×2  */
    "  ADD  HL, HL\n"       /* ×4  */
    "  ADD  HL, HL\n"       /* ×8  */
    "  ADD  HL, HL\n"       /* ×16 */
    "  ADD  HL, HL\n"       /* ×32 */
    "  LD   DE, 0x4000\n"   /* ENVS_BASE */
    "  ADD  HL, DE\n"
    "  RET\n"

    /* ------------------------------------------------------------------
     * ENV_DEF: A=frame, C=sym, HL=val
     *   CF=0 on success; CF=1 if frame full (n==6)
     * ------------------------------------------------------------------ */
    "ENV_DEF:\n"
    "  PUSH HL\n"           /* save val on hardware stack              */
    "  CALL ENV_ADDR\n"     /* HL = frame base                         */
    "  PUSH HL\n"
    "  POP  IX\n"           /* IX = frame base                         */
    "  LD   B, 0\n"         /* i = 0                                   */
    "  LD   D, (IX+18)\n"   /* D = frame.n                             */
    /* scan: does key[i] == C ? */
    "EDEF_SCAN:\n"
    "  LD   A, B\n"
    "  CP   D\n"            /* i == n ? (all slots checked)            */
    "  JR   Z, EDEF_ADD\n"
    "  LD   A, (IX+0)\n"    /* key[0]... but we need key[i]            */
    /* key[i]: IX+i, but Z80 displacement is a constant.
       Walk a separate HL pointer through key[]. */
    "  PUSH IX\n"
    "  POP  HL\n"           /* HL = frame base                         */
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* HL = &key[i]                            */
    "  LD   A, (HL)\n"      /* A = key[i]                              */
    "  CP   C\n"            /* key[i] == sym ?                         */
    "  JR   Z, EDEF_UPDATE\n"
    "  INC  B\n"
    "  JR   EDEF_SCAN\n"

    /* found at slot B — update val[B] */
    "EDEF_UPDATE:\n"
    "  PUSH IX\n"
    "  POP  HL\n"           /* HL = frame base                         */
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* HL = frame + i (byte offset)            */
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"       /* HL = &val[0] + 2*i  — wait, need 2*i    */
    /* val[i] is at offset 6 + 2*i */
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* HL = base + i                           */
    "  ADD  HL, DE\n"       /* HL = base + 2*i                         */
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"       /* HL = base + 6 + 2*i = &val[i]           */
    "  POP  DE\n"           /* DE = saved val                          */
    "  LD   (HL), E\n"      /* val[i] low byte                         */
    "  INC  HL\n"
    "  LD   (HL), D\n"      /* val[i] high byte                        */
    "  AND  A\n"            /* CF = 0 (success)                        */
    "  RET\n"

    /* not found — append if room */
    "EDEF_ADD:\n"
    "  LD   A, (IX+18)\n"   /* re-read n                               */
    "  CP   6\n"            /* n == 6 ? frame full                     */
    "  JR   Z, EDEF_FULL\n"
    /* write key[n] = C */
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, A\n"         /* A = n                                   */
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* HL = &key[n]                            */
    "  LD   (HL), C\n"      /* key[n] = sym                            */
    /* write val[n] = saved val */
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"       /* HL = base + 2*n                         */
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"       /* HL = &val[n]                            */
    "  POP  DE\n"           /* DE = saved val                          */
    "  LD   (HL), E\n"
    "  INC  HL\n"
    "  LD   (HL), D\n"
    /* n++ */
    "  LD   A, (IX+18)\n"
    "  INC  A\n"
    "  LD   (IX+18), A\n"
    "  AND  A\n"            /* CF = 0 (success)                        */
    "  RET\n"

    "EDEF_FULL:\n"
    "  POP  DE\n"           /* balance stack                           */
    "  SCF\n"               /* CF = 1 (overflow)                       */
    "  RET\n"

    /* ------------------------------------------------------------------
     * ENV_GET: A=frame, C=sym → HL=val, CF=0 found / CF=1 not-found
     *   Tail-recurses to parent frame.
     * ------------------------------------------------------------------ */
    "ENV_GET:\n"
    "  CP   0xFF\n"         /* frame == ENV_NULL ?                     */
    "  JR   Z, EGET_MISS\n"
    "  CALL ENV_ADDR\n"
    "  PUSH HL\n"
    "  POP  IX\n"           /* IX = frame base                         */
    "  LD   B, 0\n"
    "  LD   D, (IX+18)\n"   /* D = n                                   */
    "EGET_SCAN:\n"
    "  LD   A, B\n"
    "  CP   D\n"
    "  JR   Z, EGET_PARENT\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* HL = &key[i]                            */
    "  LD   A, (HL)\n"
    "  CP   C\n"
    "  JR   Z, EGET_FOUND\n"
    "  INC  B\n"
    "  LD   D, (IX+18)\n"   /* reload n (D was clobbered)              */
    "  JR   EGET_SCAN\n"

    "EGET_FOUND:\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"       /* HL = base + 2*i                         */
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"       /* HL = &val[i]                            */
    "  LD   E, (HL)\n"
    "  INC  HL\n"
    "  LD   D, (HL)\n"
    "  EX   DE, HL\n"       /* HL = val                                */
    "  AND  A\n"            /* CF = 0                                  */
    "  RET\n"

    "EGET_PARENT:\n"
    "  LD   A, (IX+19)\n"   /* A = parent index                        */
    "  JP   ENV_GET\n"      /* tail call (stack depth stays constant)  */

    "EGET_MISS:\n"
    "  SCF\n"               /* CF = 1                                  */
    "  RET\n";

/* -----------------------------------------------------------------------
 * Phase 7a — ENV_ADDR indexing
 * ----------------------------------------------------------------------- */
static void test_env_addr(void)
{
    puts("--- Phase 7a: ENV_ADDR ---");

    /* frame 0 → 0x4000 */
    {
        memset(m, 0, sizeof m);
        out_pos = 0;
        asm_at(env_subs, 0x0200);
        asm_at(
            "  ORG  0x0000\n"
            "  LD   A, 0\n"
            "  CALL 0x0200\n"        /* ENV_ADDR */
            "  LD   (0x8000), HL\n"
            "  HALT\n",
            0x0000);
        init(); pc = 0x0000; sp = 0xFE00;
        for (int i = 0; i < 500 && !halted; i++) step();
        uint16_t addr = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
        CHECK("ENV_ADDR(0) == 0x4000", addr == 0x4000);
    }

    /* frame 1 → 0x4020 */
    {
        memset(m, 0, sizeof m);
        out_pos = 0;
        asm_at(env_subs, 0x0200);
        asm_at(
            "  ORG  0x0000\n"
            "  LD   A, 1\n"
            "  CALL 0x0200\n"
            "  LD   (0x8000), HL\n"
            "  HALT\n",
            0x0000);
        init(); pc = 0x0000; sp = 0xFE00;
        for (int i = 0; i < 500 && !halted; i++) step();
        uint16_t addr = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
        CHECK("ENV_ADDR(1) == 0x4020", addr == 0x4020);
    }

    /* frame 3 → 0x4060 */
    {
        memset(m, 0, sizeof m);
        out_pos = 0;
        asm_at(env_subs, 0x0200);
        asm_at(
            "  ORG  0x0000\n"
            "  LD   A, 3\n"
            "  CALL 0x0200\n"
            "  LD   (0x8000), HL\n"
            "  HALT\n",
            0x0000);
        init(); pc = 0x0000; sp = 0xFE00;
        for (int i = 0; i < 500 && !halted; i++) step();
        uint16_t addr = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
        CHECK("ENV_ADDR(3) == 0x4060", addr == 0x4060);
    }
}

/* -----------------------------------------------------------------------
 * Phase 7b — ENV_DEF + ENV_GET (single frame)
 * ----------------------------------------------------------------------- */
static void test_env_def_get(void)
{
    puts("--- Phase 7b: ENV_DEF / ENV_GET single frame ---");

    /* ENV_ADDR at 0x0200, ENV_DEF at 0x0220, ENV_GET at 0x0290
     * (generous spacing; each sub body is < 96 bytes) */
    memset(m, 0, sizeof m);
    out_pos = 0;
    m[0x4000 + 18] = 0;    /* n = 0     */
    m[0x4000 + 19] = 0xFF; /* no parent */

    /* ENV_ADDR at 0x0200, ENV_DEF at 0x0220, ENV_GET at 0x0290
     * (generous spacing; the sub bodies are < 64 bytes each) */
    static const char *env_addr_src =
        "  ORG  0x0200\n"
        "ENV_ADDR:\n"
        "  LD   L, A\n"
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  LD   DE, 0x4000\n"
        "  ADD  HL, DE\n"
        "  RET\n";

    static const char *env_def_src =
        "  ORG  0x0220\n"
        "ENV_DEF:\n"
        "  PUSH HL\n"
        "  CALL 0x0200\n"
        "  PUSH HL\n"
        "  POP  IX\n"
        "  LD   B, 0\n"
        "  LD   D, (IX+18)\n"
        "EDEF_SCAN:\n"
        "  LD   A, B\n"
        "  CP   D\n"
        "  JR   Z, EDEF_ADD\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   A, (HL)\n"
        "  CP   C\n"
        "  JR   Z, EDEF_UPDATE\n"
        "  INC  B\n"
        "  LD   D, (IX+18)\n"
        "  JR   EDEF_SCAN\n"
        "EDEF_UPDATE:\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  POP  DE\n"
        "  LD   (HL), E\n"
        "  INC  HL\n"
        "  LD   (HL), D\n"
        "  AND  A\n"
        "  RET\n"
        "EDEF_ADD:\n"
        "  LD   A, (IX+18)\n"
        "  CP   6\n"
        "  JR   Z, EDEF_FULL\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, A\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   (HL), C\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, A\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  POP  DE\n"
        "  LD   (HL), E\n"
        "  INC  HL\n"
        "  LD   (HL), D\n"
        "  LD   A, (IX+18)\n"
        "  INC  A\n"
        "  LD   (IX+18), A\n"
        "  AND  A\n"
        "  RET\n"
        "EDEF_FULL:\n"
        "  POP  DE\n"
        "  SCF\n"
        "  RET\n";

    static const char *env_get_src =
        "  ORG  0x0290\n"
        "ENV_GET:\n"
        "  CP   0xFF\n"
        "  JR   Z, EGET_MISS\n"
        "  CALL 0x0200\n"
        "  PUSH HL\n"
        "  POP  IX\n"
        "  LD   B, 0\n"
        "  LD   D, (IX+18)\n"
        "EGET_SCAN:\n"
        "  LD   A, B\n"
        "  CP   D\n"
        "  JR   Z, EGET_PARENT\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   A, (HL)\n"
        "  CP   C\n"
        "  JR   Z, EGET_FOUND\n"
        "  INC  B\n"
        "  LD   D, (IX+18)\n"
        "  JR   EGET_SCAN\n"
        "EGET_FOUND:\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  LD   E, (HL)\n"
        "  INC  HL\n"
        "  LD   D, (HL)\n"
        "  EX   DE, HL\n"
        "  AND  A\n"
        "  RET\n"
        "EGET_PARENT:\n"
        "  LD   A, (IX+19)\n"
        "  JP   0x0290\n"          /* ENV_GET tail call */
        "EGET_MISS:\n"
        "  SCF\n"
        "  RET\n";

    asm_at(env_addr_src, 0x0200);
    asm_at(env_def_src,  0x0220);
    asm_at(env_get_src,  0x0290);

    /* Test: ENV_DEF frame=0 sym=3 val=INT(42) */
    asm_at(
        "  ORG  0x0000\n"
        "  LD   A, 0\n"
        "  LD   C, 3\n"
        "  LD   HL, 0x102A\n"
        "  CALL 0x0220\n"          /* ENV_DEF */
        "  LD   A, 0\n"            /* LD doesn't affect flags */
        "  ADC  A, 0\n"            /* A = 0 + CF (0=success) */
        "  LD   (0x8000), A\n"     /* [0x8000] = CF (should be 0) */
        /* ENV_GET frame=0 sym=3 → HL */
        "  LD   A, 0\n"
        "  LD   C, 3\n"
        "  CALL 0x0290\n"          /* ENV_GET */
        "  LD   (0x8002), HL\n"    /* [0x8002] = retrieved val */
        /* LD A,0 preserves CF; ADC A,0 captures it */
        "  LD   A, 0\n"
        "  ADC  A, 0\n"
        "  LD   (0x8004), A\n"     /* [0x8004] = CF from GET (should be 0) */
        /* ENV_GET frame=0 sym=7 (not defined) */
        "  LD   A, 0\n"
        "  LD   C, 7\n"
        "  CALL 0x0290\n"
        "  LD   A, 0\n"
        "  ADC  A, 0\n"
        "  LD   (0x8006), A\n"     /* [0x8006] = CF (should be 1) */
        "  HALT\n",
        0x0000);

    init(); pc = 0x0000; sp = 0xFE00;
    for (int i = 0; i < 2000 && !halted; i++) step();

    uint8_t  def_cf  = m[0x8000];
    uint16_t got_val = (uint16_t)(m[0x8002] | ((uint16_t)m[0x8003] << 8));
    uint8_t  get_cf  = m[0x8004];
    uint8_t  miss_cf = m[0x8006];

    CHECK("ENV_DEF returns CF=0 (success)",          def_cf  == 0);
    CHECK("ENV_GET retrieves INT(42) = 0x102A",      got_val == 0x102A);
    CHECK("ENV_GET found: CF=0",                     get_cf  == 0);
    CHECK("ENV_GET missing sym: CF=1",               miss_cf == 1);

    /* Verify frame.n incremented to 1 */
    CHECK("frame.n == 1 after one ENV_DEF",          m[0x4000 + 18] == 1);
}

/* -----------------------------------------------------------------------
 * Phase 7c — ENV_DEF update existing key
 * ----------------------------------------------------------------------- */
static void test_env_update(void)
{
    puts("--- Phase 7c: ENV_DEF update existing key ---");

    memset(m, 0, sizeof m);
    out_pos = 0;
    m[0x4000 + 18] = 0;
    m[0x4000 + 19] = 0xFF;

    static const char *env_addr_src2 =
        "  ORG  0x0200\n"
        "ENV_ADDR:\n"
        "  LD   L, A\n"
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  LD   DE, 0x4000\n"
        "  ADD  HL, DE\n"
        "  RET\n";

    static const char *env_def_src2 =
        "  ORG  0x0220\n"
        "ENV_DEF:\n"
        "  PUSH HL\n"
        "  CALL 0x0200\n"
        "  PUSH HL\n"
        "  POP  IX\n"
        "  LD   B, 0\n"
        "  LD   D, (IX+18)\n"
        "EDEF_SCAN:\n"
        "  LD   A, B\n"
        "  CP   D\n"
        "  JR   Z, EDEF_ADD\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   A, (HL)\n"
        "  CP   C\n"
        "  JR   Z, EDEF_UPDATE\n"
        "  INC  B\n"
        "  LD   D, (IX+18)\n"
        "  JR   EDEF_SCAN\n"
        "EDEF_UPDATE:\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  POP  DE\n"
        "  LD   (HL), E\n"
        "  INC  HL\n"
        "  LD   (HL), D\n"
        "  AND  A\n"
        "  RET\n"
        "EDEF_ADD:\n"
        "  LD   A, (IX+18)\n"
        "  CP   6\n"
        "  JR   Z, EDEF_FULL\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, A\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   (HL), C\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, A\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  POP  DE\n"
        "  LD   (HL), E\n"
        "  INC  HL\n"
        "  LD   (HL), D\n"
        "  LD   A, (IX+18)\n"
        "  INC  A\n"
        "  LD   (IX+18), A\n"
        "  AND  A\n"
        "  RET\n"
        "EDEF_FULL:\n"
        "  POP  DE\n"
        "  SCF\n"
        "  RET\n";

    asm_at(env_addr_src2, 0x0200);
    asm_at(env_def_src2,  0x0220);

    /* DEF sym=5 → INT(10), then DEF sym=5 → INT(99), GET should return INT(99) */
    static const char *env_get_src2 =
        "  ORG  0x0290\n"
        "ENV_GET:\n"
        "  CP   0xFF\n"
        "  JR   Z, EGET_MISS\n"
        "  CALL 0x0200\n"
        "  PUSH HL\n"
        "  POP  IX\n"
        "  LD   B, 0\n"
        "  LD   D, (IX+18)\n"
        "EGET_SCAN:\n"
        "  LD   A, B\n"
        "  CP   D\n"
        "  JR   Z, EGET_PARENT\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   A, (HL)\n"
        "  CP   C\n"
        "  JR   Z, EGET_FOUND\n"
        "  INC  B\n"
        "  LD   D, (IX+18)\n"
        "  JR   EGET_SCAN\n"
        "EGET_FOUND:\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  LD   E, (HL)\n"
        "  INC  HL\n"
        "  LD   D, (HL)\n"
        "  EX   DE, HL\n"
        "  AND  A\n"
        "  RET\n"
        "EGET_PARENT:\n"
        "  LD   A, (IX+19)\n"
        "  JP   0x0290\n"
        "EGET_MISS:\n"
        "  SCF\n"
        "  RET\n";

    asm_at(env_get_src2, 0x0290);

    asm_at(
        "  ORG  0x0000\n"
        /* first def: sym=5, INT(10)=0x100A */
        "  LD   A, 0\n"
        "  LD   C, 5\n"
        "  LD   HL, 0x100A\n"
        "  CALL 0x0220\n"
        /* second def (update): sym=5, INT(99)=0x1063 */
        "  LD   A, 0\n"
        "  LD   C, 5\n"
        "  LD   HL, 0x1063\n"
        "  CALL 0x0220\n"
        /* get sym=5 */
        "  LD   A, 0\n"
        "  LD   C, 5\n"
        "  CALL 0x0290\n"
        "  LD   (0x8000), HL\n"
        "  HALT\n",
        0x0000);

    init(); pc = 0x0000; sp = 0xFE00;
    for (int i = 0; i < 3000 && !halted; i++) step();

    uint16_t updated = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
    CHECK("ENV_DEF update: GET returns new value INT(99)", updated == 0x1063);
    CHECK("frame.n stays 1 after update (no new slot)",   m[0x4000 + 18] == 1);
}

/* -----------------------------------------------------------------------
 * Phase 7d — ENV_GET parent-chain lookup
 * ----------------------------------------------------------------------- */
static void test_env_chain(void)
{
    puts("--- Phase 7d: ENV_GET parent chain ---");

    /* Three frames:
     *   frame 0 (grandparent): sym=1 → INT(10), parent=0xFF
     *   frame 1 (parent):      sym=2 → INT(20), parent=0
     *   frame 2 (child):       sym=3 → INT(30), parent=1
     *
     * Lookups from frame 2:
     *   sym=3 → found in frame 2 (INT(30))
     *   sym=2 → found in frame 1 (INT(20))
     *   sym=1 → found in frame 0 (INT(10))
     *   sym=9 → not found (CF=1)
     */

    memset(m, 0, sizeof m);
    out_pos = 0;

    /* Frame 0 at 0x4000: n=1, parent=0xFF, key[0]=1, val[0]=INT(10) */
    m[0x4000 + 0]  = 1;      /* key[0] = sym 1   */
    m[0x4000 + 6]  = 0x0A;   /* val[0] low  = 10 */
    m[0x4000 + 7]  = 0x10;   /* val[0] high = tag INT → 0x100A */
    m[0x4000 + 18] = 1;
    m[0x4000 + 19] = 0xFF;

    /* Frame 1 at 0x4020: n=1, parent=0, key[0]=2, val[0]=INT(20) */
    m[0x4020 + 0]  = 2;
    m[0x4020 + 6]  = 0x14;   /* 20 = 0x14 */
    m[0x4020 + 7]  = 0x10;
    m[0x4020 + 18] = 1;
    m[0x4020 + 19] = 0;      /* parent = frame 0 */

    /* Frame 2 at 0x4040: n=1, parent=1, key[0]=3, val[0]=INT(30) */
    m[0x4040 + 0]  = 3;
    m[0x4040 + 6]  = 0x1E;   /* 30 = 0x1E */
    m[0x4040 + 7]  = 0x10;
    m[0x4040 + 18] = 1;
    m[0x4040 + 19] = 1;      /* parent = frame 1 */

    static const char *ea =
        "  ORG  0x0200\n"
        "ENV_ADDR:\n"
        "  LD   L, A\n"
        "  LD   H, 0\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  ADD  HL, HL\n"
        "  LD   DE, 0x4000\n"
        "  ADD  HL, DE\n"
        "  RET\n";

    static const char *eg =
        "  ORG  0x0290\n"
        "ENV_GET:\n"
        "  CP   0xFF\n"
        "  JR   Z, EGET_MISS\n"
        "  CALL 0x0200\n"
        "  PUSH HL\n"
        "  POP  IX\n"
        "  LD   B, 0\n"
        "  LD   D, (IX+18)\n"
        "EGET_SCAN:\n"
        "  LD   A, B\n"
        "  CP   D\n"
        "  JR   Z, EGET_PARENT\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  LD   A, (HL)\n"
        "  CP   C\n"
        "  JR   Z, EGET_FOUND\n"
        "  INC  B\n"
        "  LD   D, (IX+18)\n"
        "  JR   EGET_SCAN\n"
        "EGET_FOUND:\n"
        "  PUSH IX\n"
        "  POP  HL\n"
        "  LD   E, B\n"
        "  LD   D, 0\n"
        "  ADD  HL, DE\n"
        "  ADD  HL, DE\n"
        "  LD   DE, 6\n"
        "  ADD  HL, DE\n"
        "  LD   E, (HL)\n"
        "  INC  HL\n"
        "  LD   D, (HL)\n"
        "  EX   DE, HL\n"
        "  AND  A\n"
        "  RET\n"
        "EGET_PARENT:\n"
        "  LD   A, (IX+19)\n"
        "  JP   0x0290\n"
        "EGET_MISS:\n"
        "  SCF\n"
        "  RET\n";

    asm_at(ea, 0x0200);
    asm_at(eg, 0x0290);

    asm_at(
        "  ORG  0x0000\n"
        /* sym=3 in frame 2 */
        "  LD   A, 2\n"
        "  LD   C, 3\n"
        "  CALL 0x0290\n"
        "  LD   (0x8000), HL\n"
        /* sym=2 in frame 1 via parent chain */
        "  LD   A, 2\n"
        "  LD   C, 2\n"
        "  CALL 0x0290\n"
        "  LD   (0x8002), HL\n"
        /* sym=1 in frame 0 via grandparent */
        "  LD   A, 2\n"
        "  LD   C, 1\n"
        "  CALL 0x0290\n"
        "  LD   (0x8004), HL\n"
        /* sym=9 not found */
        "  LD   A, 2\n"
        "  LD   C, 9\n"
        "  CALL 0x0290\n"
        "  LD   A, 0\n"            /* LD doesn't affect flags */
        "  ADC  A, 0\n"            /* A = 0 + CF (1=miss) */
        "  LD   (0x8006), A\n"
        "  HALT\n",
        0x0000);

    init(); pc = 0x0000; sp = 0xFE00;
    for (int i = 0; i < 5000 && !halted; i++) step();

    uint16_t v3   = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
    uint16_t v2   = (uint16_t)(m[0x8002] | ((uint16_t)m[0x8003] << 8));
    uint16_t v1   = (uint16_t)(m[0x8004] | ((uint16_t)m[0x8005] << 8));
    uint8_t  miss = m[0x8006];

    CHECK("chain: sym=3 found in child frame → INT(30)=0x101E",   v3   == 0x101E);
    CHECK("chain: sym=2 found in parent frame → INT(20)=0x1014",  v2   == 0x1014);
    CHECK("chain: sym=1 found in grandparent → INT(10)=0x100A",   v1   == 0x100A);
    CHECK("chain: sym=9 not found → CF=1",                        miss == 1);
}

/* -----------------------------------------------------------------------
 * Phase 8 — OP_MKCLOS, OP_CALL, OP_RET
 *
 * Memory map (all Phase 8 tests share this layout):
 *   0x0000  VM entry + fetch loop + all op handlers
 *   0x0200  ENV_ADDR   : A=frame → HL = ENVS_BASE + A*32
 *   0x0220  ENV_NEW    : A=parent → A=new_env_id; inits n=0, parent
 *   0x0250  ENV_DEF    : A=frame, C=sym, HL=val  (bind/update)
 *   0x0300  ENV_GET    : A=frame, C=sym → HL=val, CF=0/1
 *   0x0380  FUN_ADDR   : A=fid   → HL = FUNS_BASE + A*8
 *   0x4000  ENVS[32]   32 bytes each
 *   0x4400  FUNS[48]    8 bytes each
 *   0x4580  CS[16]      3 bytes each  {ip, env, fid}
 *   0x45A0  CUR_ENV    (uint8_t)
 *   0x45A1  CUR_FID    (uint8_t)
 *   0x45A2  CSP        (uint8_t)
 *   0x45A3  NFUNS      (uint8_t)
 *   0x45A4  NENVS      (uint8_t)
 *   0x45C0  SCRATCH    32 bytes for OP_CALL temporaries
 *   0x4600  STK_BASE   Lisp stack (IY grows up, 2 bytes per Val)
 *   0x4700  OPTBL      jump table, 2 bytes × 24 entries
 *   0x4900  OPS        bytecode ops (byte stream)
 *   0x4A00  ARGS       bytecode args (word stream)
 *
 * Register convention during VM execution:
 *   IY  = Lisp stack pointer (TOS at IY-2..IY-1; push: store then INC IY×2)
 *   B   = vm_ip
 *   DE  = vm_arg (current instruction argument)
 *   IX  = used locally inside subroutines; callers must not rely on IX
 *
 * Scratch layout for OP_CALL (0x45C0):
 *   +0  argc     +1  fid      +2  ne(new_env)  +3  body_addr
 *   +4  penv     +5  fargc    +6..+9  fargs[0..3]
 *   +10..+17  avals[0..3] (2 bytes each)
 *   +18 bind_i   +19 tmp_sym  +20 saved_ip
 *
 * Test: ((lambda (x) (+ x 1)) 41) → INT(42) = 0x102A
 *   Bytecode:
 *     ip=0: JMP(18)    arg=5
 *     ip=1: LOAD(4)    arg=0       ← body start; sym_x = sym index 0
 *     ip=2: PUSH(1)    arg=0x1001  INT(1)
 *     ip=3: ADD(6)     arg=0
 *     ip=4: RET(22)    arg=0
 *     ip=5: MKCLOS(19) arg=0       template fid=0
 *     ip=6: PUSH(1)    arg=0x1029  INT(41)
 *     ip=7: CALL(20)   arg=1
 *     ip=8: HALT(23)   arg=0
 *   funs[0]: addr=1, env=0xFF, argc=1, args[0]=0
 *   envs[0]: n=0, parent=0xFF  (root frame)
 *   init: CUR_ENV=0, CUR_FID=0xFF, CSP=0, NFUNS=1, NENVS=1
 * ----------------------------------------------------------------------- */

/* ---- subroutines ---- */

static const char *p8_env_addr =
    "  ORG  0x0200\n"
    "ENV_ADDR:\n"          /* A=frame_idx → HL = ENV_BASE + A*32 */
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"      /* ×2  */
    "  ADD  HL, HL\n"      /* ×4  */
    "  ADD  HL, HL\n"      /* ×8  */
    "  ADD  HL, HL\n"      /* ×16 */
    "  ADD  HL, HL\n"      /* ×32 */
    "  LD   DE, " XSTR(CFG_ENV_BASE) "\n"
    "  ADD  HL, DE\n"
    "  RET\n";

static const char *p8_env_new =
    "  ORG  0x0220\n"
    "ENV_NEW:\n"            /* A=parent → A=new_env_id; sets n=0, parent */
    "  LD   C, A\n"         /* C = parent (preserved: ENV_ADDR doesn't touch C) */
    "  LD   HL, 0x45A4\n"   /* &NENVS */
    "  LD   A, (HL)\n"      /* A = old nenvs = new id */
    "  LD   B, A\n"         /* B = new_id (D is clobbered by ENV_ADDR's LD DE,0x4000) */
    "  INC  (HL)\n"         /* NENVS++ */
    "  CALL 0x0200\n"       /* ENV_ADDR(A=new_id) → HL */
    "  PUSH HL\n"
    "  POP  IX\n"           /* IX = &envs[new_id] */
    "  LD   (IX+18), 0\n"   /* n = 0 */
    "  LD   (IX+19), C\n"   /* parent = original parent */
    "  LD   A, B\n"         /* return new_id (from B, not D) */
    "  RET\n";

static const char *p8_env_def =
    "  ORG  0x0250\n"
    "ENV_DEF:\n"            /* A=frame, C=sym, HL=val; CF=0 ok / CF=1 full */
    "  PUSH HL\n"
    "  CALL 0x0200\n"       /* ENV_ADDR */
    "  PUSH HL\n"
    "  POP  IX\n"
    "  LD   B, 0\n"
    "  LD   D, (IX+18)\n"
    "EDEF_SCAN:\n"
    "  LD   A, B\n"
    "  CP   D\n"
    "  JR   Z, EDEF_ADD\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  LD   A, (HL)\n"
    "  CP   C\n"
    "  JR   Z, EDEF_UPDATE\n"
    "  INC  B\n"
    "  LD   D, (IX+18)\n"
    "  JR   EDEF_SCAN\n"
    "EDEF_UPDATE:\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"
    "  POP  DE\n"
    "  LD   (HL), E\n"
    "  INC  HL\n"
    "  LD   (HL), D\n"
    "  AND  A\n"
    "  RET\n"
    "EDEF_ADD:\n"
    "  LD   A, (IX+18)\n"
    "  CP   6\n"
    "  JR   Z, EDEF_FULL\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  LD   (HL), C\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"
    "  POP  DE\n"
    "  LD   (HL), E\n"
    "  INC  HL\n"
    "  LD   (HL), D\n"
    "  LD   A, (IX+18)\n"
    "  INC  A\n"
    "  LD   (IX+18), A\n"
    "  AND  A\n"
    "  RET\n"
    "EDEF_FULL:\n"
    "  POP  DE\n"
    "  SCF\n"
    "  RET\n";

static const char *p8_env_get =
    "  ORG  0x0300\n"
    "ENV_GET:\n"            /* A=frame, C=sym → HL=val, CF=0 found/CF=1 miss */
    "  CP   0xFF\n"
    "  JR   Z, EGET_MISS\n"
    "  CALL 0x0200\n"
    "  PUSH HL\n"
    "  POP  IX\n"
    "  LD   B, 0\n"
    "  LD   D, (IX+18)\n"
    "EGET_SCAN:\n"
    "  LD   A, B\n"
    "  CP   D\n"
    "  JR   Z, EGET_PARENT\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  LD   A, (HL)\n"
    "  CP   C\n"
    "  JR   Z, EGET_FOUND\n"
    "  INC  B\n"
    "  LD   D, (IX+18)\n"
    "  JR   EGET_SCAN\n"
    "EGET_FOUND:\n"
    "  PUSH IX\n"
    "  POP  HL\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"
    "  LD   DE, 6\n"
    "  ADD  HL, DE\n"
    "  LD   E, (HL)\n"
    "  INC  HL\n"
    "  LD   D, (HL)\n"
    "  EX   DE, HL\n"
    "  AND  A\n"
    "  RET\n"
    "EGET_PARENT:\n"
    "  LD   A, (IX+19)\n"
    "  JP   0x0300\n"       /* ENV_GET tail call */
    "EGET_MISS:\n"
    "  SCF\n"
    "  RET\n";

static const char *p8_fun_addr =
    "  ORG  0x0380\n"
    "FUN_ADDR:\n"           /* A=fid → HL = FUN_BASE + A*8 */
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* ×2 */
    "  ADD  HL, HL\n"       /* ×4 */
    "  ADD  HL, HL\n"       /* ×8 */
    "  LD   DE, " XSTR(CFG_FUN_BASE) "\n"
    "  ADD  HL, DE\n"
    "  RET\n";

/* -----------------------------------------------------------------------
 * I/O subroutines at fixed addresses 0x03A0 (PUTCHAR) and 0x03C0 (GETCHAR)
 *
 * Two variants of each — port-based (OUT/IN instructions) and memory-mapped
 * (for hardware without IN/OUT).  Assemble exactly one variant per run.
 *
 * Memory-mapped scratch layout (MMIO variant):
 *   0x45F0  MMIO_OUT_PTR  (word) — write pointer into output buffer
 *   0x45F2  MMIO_IN_PTR   (word) — read pointer into input buffer
 *   0x45F4  MMIO_IN_END   (word) — one-past-end of input buffer
 *   0x4800  MMIO_OUT_BUF         — output character buffer
 * ----------------------------------------------------------------------- */

/* PUTCHAR port variant: OUT (0), A  — one byte to port 0 */
static const char *p9_putchar_port =
    "  ORG  " XSTR(CFG_PUTCHAR) "\n"
    "PUTCHAR:\n"
    "  OUT  (0), A\n"
    "  RET\n";

/* PUTCHAR memory-mapped variant: append A to buffer pointed to by
 * MMIO_OUT_PTR (CFG_MMIO_OUT_PTR); advance pointer.                     */
static const char *p9_putchar_mem =
    "  ORG  " XSTR(CFG_PUTCHAR) "\n"
    "PUTCHAR:\n"
    "  PUSH HL\n"
    "  LD   HL, (0x45F0)\n" /* load output-write pointer */
    "  LD   (HL), A\n"      /* store character */
    "  INC  HL\n"
    "  LD   (0x45F0), HL\n" /* update pointer */
    "  POP  HL\n"
    "  RET\n";

/* GETCHAR port variant: IN A,(0)  — read one byte from port 0           */
static const char *p9_getchar_port =
    "  ORG  0x03C0\n"
    "GETCHAR:\n"
    "  IN   A, (0)\n"
    "  RET\n";

/* GETCHAR memory-mapped variant: read next byte from input buffer.
 * MMIO_IN_PTR (0x45F2) and MMIO_IN_END (0x45F4) bound the buffer.
 * Returns 0 in A when the buffer is empty.                              */
static const char *p9_getchar_mem =
    "  ORG  0x03C0\n"
    "GETCHAR:\n"
    "  PUSH HL\n"
    "  PUSH DE\n"
    "  LD   HL, (0x45F2)\n" /* IN_PTR */
    "  LD   DE, (0x45F4)\n" /* IN_END */
    "  LD   A, H\n"
    "  CP   D\n"
    "  JR   NZ, gcm_rd\n"
    "  LD   A, L\n"
    "  CP   E\n"
    "  JR   NZ, gcm_rd\n"
    "  XOR  A\n"            /* buffer empty: return 0 */
    "  JR   gcm_done\n"
    "gcm_rd:\n"
    "  LD   A, (HL)\n"
    "  INC  HL\n"
    "  LD   (0x45F2), HL\n" /* update IN_PTR */
    "gcm_done:\n"
    "  POP  DE\n"
    "  POP  HL\n"
    "  RET\n";

/* ---- full VM ---- */
static const char *p8_vm =
    "  ORG  0x0000\n"
    /* entry: init IY (Lisp stack), hardware SP, vm_ip */
    "  LD   IY, " XSTR(CFG_STK_BASE) "\n"
    "  LD   SP, " XSTR(CFG_HW_SP) "\n"
    "  LD   B, 0\n"
    "  JP   fetch\n"

    /* ---- fetch ---- */
    "fetch:\n"
    /* opcode = ops[B] */
    "  LD   HL, " XSTR(CFG_OPS_BASE) "\n"
    "  LD   E, B\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  LD   A, (HL)\n"      /* A = opcode */
    /* arg = args[B] (word) */
    "  LD   L, B\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* 2*B */
    "  LD   DE, " XSTR(CFG_ARGS_BASE) "\n"
    "  ADD  HL, DE\n"
    "  LD   E, (HL)\n"
    "  INC  HL\n"
    "  LD   D, (HL)\n"      /* DE = vm_arg */
    "  INC  B\n"            /* B = vm_ip++ */
    /* dispatch via jump table */
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* 2 * opcode */
    "  PUSH BC\n"
    "  LD   BC, " XSTR(CFG_OPTBL) "\n"
    "  ADD  HL, BC\n"
    "  LD   C, (HL)\n"
    "  INC  HL\n"
    "  LD   H, (HL)\n"
    "  LD   L, C\n"
    "  POP  BC\n"
    "  JP   (HL)\n"         /* DE = vm_arg intact; B = vm_ip */

    /* ---- op_nop (0) ---- */
    "op_nop:\n"
    "  JP   fetch\n"

    /* ---- op_push (1): push vm_arg (DE) ---- */
    "op_push:\n"
    "  LD   (IY+0), E\n"
    "  LD   (IY+1), D\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  JP   fetch\n"

    /* ---- op_load (4): push env_get(cur_env, sym) ---- */
    "op_load:\n"
    "  PUSH BC\n"
    "  LD   A, (0x45A0)\n"  /* CUR_ENV */
    "  LD   C, E\n"         /* C = sym (low byte of DE) */
    "  CALL 0x0300\n"       /* ENV_GET → HL=val, CF=miss */
    "  JR   NC, oload_ok\n"
    "  LD   HL, 0\n"        /* NIL on miss */
    "oload_ok:\n"
    "  LD   (IY+0), L\n"
    "  LD   (IY+1), H\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  POP  BC\n"
    "  JP   fetch\n"

    /* ---- op_add (6): pop two INTs, add 12-bit payloads, push result ---- */
    "op_add:\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  LD   H, A\n"         /* strip tag from H */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   E, (IY+0)\n"
    "  LD   D, (IY+1)\n"
    "  LD   A, D\n"
    "  AND  0x0F\n"
    "  LD   D, A\n"
    "  ADD  HL, DE\n"
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  OR   0x10\n"         /* retag as T_INT=1 */
    "  LD   H, A\n"
    "  LD   (IY+0), L\n"
    "  LD   (IY+1), H\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  JP   fetch\n"

    /* New arithmetic handlers placed at 0x0400 to avoid overlapping the
     * subroutines assembled at 0x0200-0x03BF (env_addr, env_def, etc.).  */
    "  ORG  0x0400\n"

    /* ---- op_sub (7): pop two INTs, compute second - TOS, push result ---- */
    "op_sub:\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  LD   H, A\n"          /* HL = b (TOS, subtrahend) */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   E, (IY+0)\n"
    "  LD   D, (IY+1)\n"
    "  LD   A, D\n"
    "  AND  0x0F\n"
    "  LD   D, A\n"          /* DE = a (second, minuend) */
    "  EX   DE, HL\n"        /* HL = a, DE = b */
    "  OR   A\n"             /* clear carry */
    "  SBC  HL, DE\n"        /* HL = a - b */
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  OR   0x10\n"          /* retag as T_INT */
    "  LD   H, A\n"
    "  LD   (IY+0), L\n"
    "  LD   (IY+1), H\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  JP   fetch\n"

    /* ---- op_mul (8): pop two INTs, multiply (12-bit), push result ---- */
    /* Algorithm: shift-and-add over 12 bits.                             */
    /* Registers: BC = multiplier (b, saved around loop), DE = multiplicand (a), */
    /* HL = result accumulator.  B is first saved to Z80 stack (=vm_ip). */
    /* Loop counter stored at scratch byte 0x45E0.                        */
    "op_mul:\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  LD   H, A\n"          /* HL = b (multiplier) */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   E, (IY+0)\n"
    "  LD   D, (IY+1)\n"
    "  LD   A, D\n"
    "  AND  0x0F\n"
    "  LD   D, A\n"          /* DE = a (multiplicand) */
    "  PUSH BC\n"            /* save vm_ip (B) */
    "  LD   B, H\n"
    "  LD   C, L\n"          /* BC = b */
    "  LD   HL, 0\n"         /* HL = result = 0 */
    "  LD   A, 12\n"
    "  LD   (0x45E0), A\n"   /* loop counter = 12 */
    "omul_loop:\n"
    "  LD   A, C\n"
    "  AND  1\n"
    "  JR   Z, omul_skip\n"
    "  ADD  HL, DE\n"        /* if b[0] set: result += a */
    "omul_skip:\n"
    "  SRL  B\n"             /* BC >>= 1  (shift multiplier right) */
    "  RR   C\n"
    "  SLA  E\n"             /* DE <<= 1  (shift multiplicand left) */
    "  RL   D\n"
    "  LD   A, (0x45E0)\n"
    "  DEC  A\n"
    "  LD   (0x45E0), A\n"
    "  JR   NZ, omul_loop\n"
    "  LD   A, H\n"
    "  AND  0x0F\n"
    "  OR   0x10\n"          /* retag as T_INT */
    "  LD   H, A\n"
    "  LD   (IY+0), L\n"
    "  LD   (IY+1), H\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  POP  BC\n"            /* restore vm_ip */
    "  JP   fetch\n"

    /* ---- op_jmp (18): set vm_ip = arg ---- */
    "op_jmp:\n"
    "  LD   B, E\n"         /* B = vm_ip = arg low byte */
    "  JP   fetch\n"

    /* ---- op_jz (17): jump if TOS is #f (0x5000) or NIL (0x0000) ---- */
    "op_jz:\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"   /* HL = condition value */
    "  LD   A, H\n"
    "  OR   L\n"
    "  JR   Z, ojz_jump\n" /* NIL (0x0000) → jump */
    "  LD   A, H\n"
    "  CP   0x50\n"
    "  JR   NZ, ojz_no\n"
    "  LD   A, L\n"
    "  OR   A\n"
    "  JR   Z, ojz_jump\n" /* #f (0x5000) → jump */
    "ojz_no:\n"
    "  JP   fetch\n"       /* truthy → fall through */
    "ojz_jump:\n"
    "  LD   B, E\n"        /* B = vm_ip = target ip */
    "  JP   fetch\n"

    /* ---- odsp_notint: non-INT display handler ---- */
    /* Called when op_display pops a Val whose tag ≠ T_INT.              */
    /* Entry: A = D & 0xF0 (tag nibble << 4); D:E = original Val.       */
    /* B is saved on Z80 stack (PUSH BC at op_display entry).            */
    "odsp_notint:\n"
    /* ---- NIL (tag=0) → "()" ---- */
    "  CP   0x00\n"
    "  JR   NZ, odsp_nt_bool\n"
    "  LD   A, 0x28\n"         /* '(' */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  LD   A, 0x29\n"         /* ')' */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  JP   odsp_done\n"
    /* ---- BOOL (tag=5) → "#t" or "#f" ---- */
    "odsp_nt_bool:\n"
    "  CP   0x50\n"
    "  JR   NZ, odsp_nt_char\n"
    "  LD   A, 0x23\n"         /* '#' */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  LD   A, E\n"            /* payload: 0=#f, 1=#t */
    "  OR   A\n"
    "  LD   A, 0x66\n"         /* 'f' */
    "  JR   Z, odsp_nt_bch\n"
    "  LD   A, 0x74\n"         /* 't' */
    "odsp_nt_bch:\n"
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  JP   odsp_done\n"
    /* ---- CHAR (tag=6) → print E directly ---- */
    "odsp_nt_char:\n"
    "  CP   0x60\n"
    "  JR   NZ, odsp_nt_str\n"
    "  LD   A, E\n"
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  JP   odsp_done\n"
    /* ---- STR (tag=7) → print strs[E] from 0x4580 + E*16 ---- */
    "odsp_nt_str:\n"
    "  CP   0x70\n"
    "  JP   NZ, odsp_done\n"   /* unknown tag → skip */
    "  LD   L, E\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"          /* HL = E * 2 */
    "  ADD  HL, HL\n"          /* HL = E * 4 */
    "  ADD  HL, HL\n"          /* HL = E * 8 */
    "  ADD  HL, HL\n"          /* HL = E * 16 */
    "  LD   DE, " XSTR(CFG_STRS_BASE) "\n"
    "  ADD  HL, DE\n"          /* HL = &strs[E] */
    "odsp_nt_sl:\n"
    "  LD   A, (HL)\n"
    "  OR   A\n"
    "  JP   Z, odsp_done\n"    /* null terminator */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"
    "  INC  HL\n"
    "  JR   odsp_nt_sl\n"

    /* ---- op_mkclos (19): copy fun template, capture cur_env ---- */
    /* DE = template_fid.  After: PUSH T_FUN(new_fid) */
    "op_mkclos:\n"
    "  PUSH BC\n"           /* save vm_ip (B), will restore later */
    /* new_fid = NFUNS; NFUNS++ */
    "  LD   HL, 0x45A3\n"   /* &NFUNS */
    "  LD   C, (HL)\n"      /* C = new_fid */
    "  INC  (HL)\n"
    /* src = &funs[tmpl]: FUN_ADDR(A=E) */
    "  LD   A, E\n"
    "  CALL 0x0380\n"       /* FUN_ADDR → HL = src */
    "  PUSH HL\n"
    /* dst = &funs[new_fid]: FUN_ADDR(A=C) */
    "  LD   A, C\n"
    "  CALL 0x0380\n"       /* → HL = dst */
    "  EX   DE, HL\n"       /* DE = dst */
    "  POP  HL\n"           /* HL = src */
    /* copy 8 bytes */
    "  LD   B, 8\n"
    "omkclos_cp:\n"
    "  LD   A, (HL)\n"
    "  LD   (DE), A\n"
    "  INC  HL\n"
    "  INC  DE\n"
    "  DJNZ omkclos_cp\n"   /* B: 8→0; C = new_fid (intact) */
    /* set .env = CUR_ENV at offset +1 */
    "  LD   A, C\n"         /* A = new_fid */
    "  CALL 0x0380\n"       /* HL = &funs[new_fid] */
    "  PUSH HL\n"
    "  POP  IX\n"
    "  LD   A, (0x45A0)\n"  /* CUR_ENV */
    "  LD   (IX+1), A\n"
    /* push T_FUN(new_fid) onto Lisp stack */
    "  LD   (IY+0), C\n"    /* low byte = new_fid */
    "  LD   (IY+1), 0x40\n" /* high byte = T_FUN tag (4 << 4 = 0x40) */
    "  INC  IY\n"
    "  INC  IY\n"
    "  POP  BC\n"           /* restore vm_ip */
    "  JP   fetch\n"

    /* ---- op_call (20): call function ---- */
    /* DE = argc.  Pops args (argc), then fn_val from Lisp stack. */
    "op_call:\n"
    "  LD   A, B\n"
    "  LD   (0x45D4), A\n"  /* scratch: saved_ip */
    "  LD   A, E\n"
    "  LD   (0x45C0), A\n"  /* scratch: argc */
    /* pop args → scratch avals[0..argc-1] */
    /* avals[argc-1] at 0x45CA + 2*(argc-1) = 0x45C8 + 2*argc */
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* 2*argc */
    "  LD   DE, 0x45C8\n"
    "  ADD  HL, DE\n"       /* HL = &avals[argc-1] */
    "  LD   C, A\n"         /* C = argc (loop counter) */
    "ocall_pa:\n"
    "  LD   A, C\n"
    "  OR   A\n"
    "  JR   Z, ocall_pa_done\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   A, (IY+0)\n"
    "  LD   (HL), A\n"      /* low byte */
    "  INC  HL\n"
    "  LD   A, (IY+1)\n"
    "  LD   (HL), A\n"      /* high byte */
    "  DEC  HL\n"
    "  DEC  HL\n"
    "  DEC  HL\n"           /* HL -= 2 (go to prev aval slot) */
    "  DEC  C\n"
    "  JR   ocall_pa\n"
    "ocall_pa_done:\n"
    /* pop fn_val, extract fid */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   A, (IY+0)\n"    /* fn_val low byte = fid (< 256) */
    "  LD   (0x45C1), A\n"  /* scratch: fid */
    /* read funs[fid] fields */
    "  CALL 0x0380\n"       /* FUN_ADDR(A=fid) → HL */
    "  PUSH HL\n"
    "  POP  IX\n"
    "  LD   A, (IX+0)\n"
    "  LD   (0x45C3), A\n"  /* body_addr */
    "  LD   A, (IX+1)\n"
    "  LD   (0x45C4), A\n"  /* parent_env */
    "  LD   A, (IX+2)\n"
    "  LD   (0x45C5), A\n"  /* fun.argc */
    "  LD   A, (IX+3)\n"
    "  LD   (0x45C6), A\n"  /* fargs[0] */
    "  LD   A, (IX+4)\n"
    "  LD   (0x45C7), A\n"  /* fargs[1] */
    "  LD   A, (IX+5)\n"
    "  LD   (0x45C8), A\n"  /* fargs[2] */
    "  LD   A, (IX+6)\n"
    "  LD   (0x45C9), A\n"  /* fargs[3] */
    /* ENV_NEW(parent = parent_env) → ne */
    "  LD   A, (0x45C4)\n"
    "  CALL 0x0220\n"       /* ENV_NEW → A = ne */
    "  LD   (0x45C2), A\n"  /* scratch: ne */
    /* push call frame: cs[csp++] = {saved_ip, cur_env, cur_fid} */
    "  LD   A, (0x45A2)\n"  /* CSP */
    "  LD   HL, 0x4580\n"   /* CS_BASE */
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"       /* HL = CS_BASE + 3*csp */
    "  LD   A, (0x45D4)\n"
    "  LD   (HL), A\n"      /* saved_ip */
    "  INC  HL\n"
    "  LD   A, (0x45A0)\n"
    "  LD   (HL), A\n"      /* cur_env */
    "  INC  HL\n"
    "  LD   A, (0x45A1)\n"
    "  LD   (HL), A\n"      /* cur_fid */
    "  LD   A, (0x45A2)\n"
    "  INC  A\n"
    "  LD   (0x45A2), A\n"  /* csp++ */
    /* bind params: for i in 0..fargc-1: env_def(ne, fargs[i], avals[i]) */
    "  XOR  A\n"
    "  LD   (0x45D2), A\n"  /* scratch: bind_i = 0 */
    "ocall_bind:\n"
    "  LD   A, (0x45D2)\n"  /* i */
    "  LD   HL, 0x45C5\n"   /* &fun.argc */
    "  CP   (HL)\n"         /* i == fargc? */
    "  JR   Z, ocall_bind_done\n"
    /* sym = fargs[i] */
    "  LD   HL, 0x45C6\n"   /* SCALL_FARGS base */
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  ADD  HL, DE\n"       /* &fargs[i] */
    "  LD   A, (HL)\n"      /* sym */
    "  LD   (0x45D3), A\n"  /* scratch: tmp_sym */
    /* avals[i]: 0x45CA + 2*i */
    "  LD   A, (0x45D2)\n"  /* i */
    "  LD   L, A\n"
    "  LD   H, 0\n"
    "  ADD  HL, HL\n"       /* 2*i */
    "  LD   DE, 0x45CA\n"
    "  ADD  HL, DE\n"       /* &avals[i] */
    "  LD   E, (HL)\n"
    "  INC  HL\n"
    "  LD   D, (HL)\n"      /* DE = avals[i] */
    "  EX   DE, HL\n"       /* HL = avals[i] */
    /* ENV_DEF(A=ne, C=sym, HL=val) */
    "  LD   A, (0x45D3)\n"  /* sym */
    "  LD   C, A\n"
    "  LD   A, (0x45C2)\n"  /* ne */
    "  CALL 0x0250\n"       /* ENV_DEF */
    /* i++ */
    "  LD   A, (0x45D2)\n"
    "  INC  A\n"
    "  LD   (0x45D2), A\n"
    "  JR   ocall_bind\n"
    "ocall_bind_done:\n"
    /* update VM state, jump to function body */
    "  LD   A, (0x45C3)\n"  /* body_addr */
    "  LD   B, A\n"         /* B = new vm_ip */
    "  LD   A, (0x45C2)\n"
    "  LD   (0x45A0), A\n"  /* CUR_ENV = ne */
    "  LD   A, (0x45C1)\n"
    "  LD   (0x45A1), A\n"  /* CUR_FID = fid */
    "  JP   fetch\n"

    /* ---- op_ret (22): return from function ---- */
    "op_ret:\n"
    /* pop return val */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"    /* HL = return val */
    /* if csp==0: halt */
    "  LD   A, (0x45A2)\n"  /* CSP */
    "  OR   A\n"
    "  JR   NZ, oret_pop\n"
    "  LD   A, L\n"
    "  LD   (0x8000), A\n"  /* csp=0: write result and halt */
    "  LD   A, H\n"
    "  LD   (0x8001), A\n"
    "  HALT\n"
    "oret_pop:\n"
    "  PUSH HL\n"           /* save return val — HL will be clobbered below */
    "  DEC  A\n"            /* A = new csp */
    "  LD   (0x45A2), A\n"  /* csp-- */
    "  LD   E, A\n"
    "  LD   D, 0\n"
    "  LD   HL, 0x4580\n"   /* CS_BASE */
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"
    "  ADD  HL, DE\n"       /* HL = &cs[csp] */
    "  LD   B, (HL)\n"      /* B = saved_ip */
    "  INC  HL\n"
    "  LD   A, (HL)\n"
    "  LD   (0x45A0), A\n"  /* CUR_ENV = saved */
    "  INC  HL\n"
    "  LD   A, (HL)\n"
    "  LD   (0x45A1), A\n"  /* CUR_FID = saved */
    "  POP  HL\n"           /* restore return val */
    /* push return val */
    "  LD   (IY+0), L\n"
    "  LD   (IY+1), H\n"
    "  INC  IY\n"
    "  INC  IY\n"
    "  JP   fetch\n"

    /* ---- op_halt (23): pop TOS, store to 0x8000, halt ---- */
    "op_halt:\n"
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   L, (IY+0)\n"
    "  LD   H, (IY+1)\n"
    "  LD   A, L\n"
    "  LD   (0x8000), A\n"
    "  LD   A, H\n"
    "  LD   (0x8001), A\n"
    "  HALT\n"

    /* ---- op_display (36): pop INT Val, print decimal via OUT (0),A ---- */
    /* Scratch digit buffer: 5 bytes at 0x45E0 (after existing scratch).   */
    /* B = vm_ip — must be preserved.  PUSH BC at entry, POP BC at exit.  */
    "op_display:\n"
    "  PUSH BC\n"           /* save vm_ip (B) and vm_arg-lo (C) */
    "  DEC  IY\n"
    "  DEC  IY\n"
    "  LD   E, (IY+0)\n"    /* pop Val into DE */
    "  LD   D, (IY+1)\n"
    /* Check T_INT tag: high nibble of D must equal 0x10 */
    "  LD   A, D\n"
    "  AND  0xF0\n"
    "  CP   0x10\n"
    "  JP   NZ, odsp_notint\n" /* not INT: branch to type handler */
    /* Extract 12-bit signed payload → HL */
    "  LD   A, D\n"
    "  AND  0x0F\n"
    "  LD   H, A\n"
    "  LD   L, E\n"         /* HL = 12-bit value */
    /* Negative if bit 11 (H bit 3) is set */
    "  BIT  3, H\n"
    "  JR   Z, odsp_pos\n"
    "  LD   A, 0x2D\n"      /* '-' */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"       /* PUTCHAR('-') */
    /* Negate HL mod 2^12 */
    "  XOR  A\n"
    "  SUB  L\n"
    "  LD   L, A\n"
    "  LD   A, 0\n"
    "  SBC  A, H\n"
    "  AND  0x0F\n"
    "  LD   H, A\n"
    "odsp_pos:\n"
    /* Handle zero */
    "  LD   A, H\n"
    "  OR   L\n"
    "  JR   NZ, odsp_nonz\n"
    "  LD   A, 0x30\n"      /* '0' */
    "  CALL " XSTR(CFG_PUTCHAR) "\n"       /* PUTCHAR('0') */
    "  JR   odsp_done\n"
    "odsp_nonz:\n"
    /* Write digits LSB-first into scratch buffer at 0x45E0 via IX */
    "  LD   IX, 0x45E0\n"
    "  LD   B, 0\n"         /* B = digit count (vm_ip saved on stack) */
    "odsp_dl:\n"
    "  LD   A, H\n"
    "  OR   L\n"
    "  JR   Z, odsp_pr\n"
    /* Inline divide HL by 10: quotient → C (fits ≤204), remainder → L */
    "  LD   C, 0\n"
    "odsp_dv:\n"
    "  LD   A, H\n"
    "  OR   A\n"
    "  JR   NZ, odsp_ds\n"
    "  LD   A, L\n"
    "  CP   10\n"
    "  JR   C, odsp_dd\n"
    "odsp_ds:\n"
    "  LD   A, L\n"
    "  SUB  10\n"
    "  LD   L, A\n"
    "  JR   NC, odsp_nc\n"
    "  DEC  H\n"
    "odsp_nc:\n"
    "  INC  C\n"
    "  JR   odsp_dv\n"
    "odsp_dd:\n"
    /* L = remainder; C = quotient */
    "  LD   A, L\n"
    "  ADD  A, 0x30\n"      /* digit → ASCII */
    "  LD   (IX+0), A\n"    /* store in buffer */
    "  INC  IX\n"
    "  INC  B\n"
    "  LD   H, 0\n"
    "  LD   L, C\n"         /* HL = quotient */
    "  JR   odsp_dl\n"
    "odsp_pr:\n"
    /* Reverse-print: IX points past last digit; step back and output */
    "  LD   A, B\n"
    "  OR   A\n"
    "  JR   Z, odsp_done\n"
    "  DEC  IX\n"
    "  LD   A, (IX+0)\n"
    "  CALL " XSTR(CFG_PUTCHAR) "\n"       /* PUTCHAR(digit) */
    "  DEC  B\n"
    "  JR   odsp_pr\n"
    "odsp_done:\n"
    "  POP  BC\n"           /* restore vm_ip */
    "  JP   fetch\n"

    /* ---- jump table (37 entries × 2 bytes) ---- */
    "  ORG  " XSTR(CFG_OPTBL) "\n"
    "  DEFW op_nop\n"       /* 0  NOP     */
    "  DEFW op_push\n"      /* 1  PUSH    */
    "  DEFW op_nop\n"       /* 2  POP     */
    "  DEFW op_nop\n"       /* 3  DUP     */
    "  DEFW op_load\n"      /* 4  LOAD    */
    "  DEFW op_nop\n"       /* 5  STORE   */
    "  DEFW op_add\n"       /* 6  ADD     */
    "  DEFW op_sub\n"       /* 7  SUB     */
    "  DEFW op_mul\n"       /* 8  MUL     */
    "  DEFW op_nop\n"       /* 9  EQ      */
    "  DEFW op_nop\n"       /* 10 LT      */
    "  DEFW op_nop\n"       /* 11 NOT     */
    "  DEFW op_nop\n"       /* 12 CONS    */
    "  DEFW op_nop\n"       /* 13 CAR     */
    "  DEFW op_nop\n"       /* 14 CDR     */
    "  DEFW op_nop\n"       /* 15 PAIRP   */
    "  DEFW op_nop\n"       /* 16 NULLP   */
    "  DEFW op_jz\n"        /* 17 JZ      */
    "  DEFW op_jmp\n"       /* 18 JMP     */
    "  DEFW op_mkclos\n"    /* 19 MKCLOS  */
    "  DEFW op_call\n"      /* 20 CALL    */
    "  DEFW op_nop\n"       /* 21 TAILCALL*/
    "  DEFW op_ret\n"       /* 22 RET     */
    "  DEFW op_halt\n"      /* 23 HALT    */
    "  DEFW op_nop\n"       /* 24 CHARP   */
    "  DEFW op_nop\n"       /* 25 CHAR2INT*/
    "  DEFW op_nop\n"       /* 26 INT2CHAR*/
    "  DEFW op_nop\n"       /* 27 STRP    */
    "  DEFW op_nop\n"       /* 28 STRLEN  */
    "  DEFW op_nop\n"       /* 29 STRREF  */
    "  DEFW op_nop\n"       /* 30 STRCAT  */
    "  DEFW op_nop\n"       /* 31 STREQ   */
    "  DEFW op_nop\n"       /* 32 SYM2STR */
    "  DEFW op_nop\n"       /* 33 STR2SYM */
    "  DEFW op_nop\n"       /* 34 NUM2STR */
    "  DEFW op_nop\n"       /* 35 STR2NUM */
    "  DEFW op_display\n";  /* 36 DISPLAY */

/* -----------------------------------------------------------------------
 * Phase 8a — FUN_ADDR indexing
 * ----------------------------------------------------------------------- */
static void test_fun_addr(void)
{
    puts("--- Phase 8a: FUN_ADDR ---");

    uint16_t results[3];
    for (int fi = 0; fi < 3; fi++) {
        memset(m, 0, sizeof m);
        out_pos = 0;
        asm_at(p8_fun_addr, 0x0380);
        char src[64];
        snprintf(src, sizeof src,
            "  ORG  0x0000\n"
            "  LD   A, %d\n"
            "  CALL 0x0380\n"
            "  LD   (0x8000), HL\n"
            "  HALT\n", fi);
        asm_at(src, 0x0000);
        init(); pc = 0x0000; sp = 0xFE00;
        for (int i = 0; i < 200 && !halted; i++) step();
        results[fi] = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
    }
    CHECK("FUN_ADDR(0) == 0x4400", results[0] == 0x4400);
    CHECK("FUN_ADDR(1) == 0x4408", results[1] == 0x4408);
    CHECK("FUN_ADDR(2) == 0x4410", results[2] == 0x4410);
}

/* -----------------------------------------------------------------------
 * Phase 8b — ((lambda (x) (+ x 1)) 41) → INT(42)
 * ----------------------------------------------------------------------- */
static void test_lambda_call(void)
{
    puts("--- Phase 8b: lambda call ((lambda (x) (+ x 1)) 41) ---");

    memset(m, 0, sizeof m);
    out_pos = 0;

    /* VM first so any spill past 0x01FF is overwritten by subroutines */
    asm_at(p8_vm,       0x0000);
    /* --- subroutines written last --- */
    asm_at(p8_env_addr, 0x0200);
    asm_at(p8_env_new,  0x0220);
    asm_at(p8_env_def,  0x0250);
    asm_at(p8_env_get,  0x0300);
    asm_at(p8_fun_addr, 0x0380);

    /* --- fun template funs[0] at 0x4400 ---
     *   offset 0: addr=1 (body ip)
     *   offset 1: env=0xFF (no captured env in template)
     *   offset 2: argc=1
     *   offset 3: args[0]=0 (sym index 0 = x)
     *   offset 4..7: 0 (pad)                          */
    m[0x4400 + 0] = 1;    /* addr  */
    m[0x4400 + 1] = 0xFF; /* env   */
    m[0x4400 + 2] = 1;    /* argc  */
    m[0x4400 + 3] = 0;    /* args[0] = sym 0 */

    /* --- env[0] root frame at 0x4000 --- */
    m[0x4000 + 18] = 0;    /* n = 0      */
    m[0x4000 + 19] = 0xFF; /* no parent  */

    /* --- VM state --- */
    m[0x45A0] = 0;    /* CUR_ENV = 0    */
    m[0x45A1] = 0xFF; /* CUR_FID = none */
    m[0x45A2] = 0;    /* CSP = 0        */
    m[0x45A3] = 1;    /* NFUNS = 1 (one template) */
    m[0x45A4] = 1;    /* NENVS = 1 (root env) */

    /* --- bytecode ---
     *   ops[]  at 0x4900: {18, 4, 1, 6, 22, 19, 1, 20, 23}
     *   args[] at 0x4A00: {5, 0, 0x1001, 0, 0, 0, 0x1029, 1, 0}
     */
    /* ops */
    m[0x4900 + 0] = 18; /* JMP    */
    m[0x4900 + 1] =  4; /* LOAD   */
    m[0x4900 + 2] =  1; /* PUSH   */
    m[0x4900 + 3] =  6; /* ADD    */
    m[0x4900 + 4] = 22; /* RET    */
    m[0x4900 + 5] = 19; /* MKCLOS */
    m[0x4900 + 6] =  1; /* PUSH   */
    m[0x4900 + 7] = 20; /* CALL   */
    m[0x4900 + 8] = 23; /* HALT   */

    /* args (little-endian words) */
    /* ip=0: JMP, arg=5 */
    m[0x4A00 + 0] = 5;  m[0x4A00 + 1] = 0;
    /* ip=1: LOAD, arg=0 (sym x) */
    m[0x4A02 + 0] = 0;  m[0x4A02 + 1] = 0;
    /* ip=2: PUSH INT(1) = 0x1001 */
    m[0x4A04 + 0] = 0x01; m[0x4A04 + 1] = 0x10;
    /* ip=3: ADD, arg=0 */
    m[0x4A06 + 0] = 0;  m[0x4A06 + 1] = 0;
    /* ip=4: RET, arg=0 */
    m[0x4A08 + 0] = 0;  m[0x4A08 + 1] = 0;
    /* ip=5: MKCLOS, arg=0 (template fid=0) */
    m[0x4A0A + 0] = 0;  m[0x4A0A + 1] = 0;
    /* ip=6: PUSH INT(41) = 0x1029 */
    m[0x4A0C + 0] = 0x29; m[0x4A0C + 1] = 0x10;
    /* ip=7: CALL, arg=1 */
    m[0x4A0E + 0] = 1;  m[0x4A0E + 1] = 0;
    /* ip=8: HALT, arg=0 */
    m[0x4A10 + 0] = 0;  m[0x4A10 + 1] = 0;

    init(); pc = 0x0000; sp = 0xFE00;
    for (int i = 0; i < 20000 && !halted; i++) step();

    uint16_t result = (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));

    CHECK("VM halted after HALT opcode",               halted == 1);
    CHECK("((lambda (x) (+ x 1)) 41) = INT(42)=0x102A", result == 0x102A);
    /* verify the new closure was created (fid=1) */
    CHECK("NFUNS == 2 (template + one closure)",        m[0x45A3] == 2);
    /* verify the argument env was created (env[1]) */
    CHECK("NENVS == 2 (root + argument frame)",         m[0x45A4] == 2);
    /* verify env[1] has sym=0 → INT(41) */
    CHECK("env[1].key[0] == 0 (sym x bound)",           m[0x4020 + 0] == 0);
    uint16_t bound = (uint16_t)(m[0x4020 + 6] | ((uint16_t)m[0x4020 + 7] << 8));
    CHECK("env[1].val[0] == INT(41) = 0x1029",          bound == 0x1029);
}

/* -----------------------------------------------------------------------
 * Phase 9 — OP_DISPLAY: pop INT Val, print decimal to port 0
 * ----------------------------------------------------------------------- */

/* MMIO scratch pointers (must match p9_putchar_mem / p9_getchar_mem) */
#define MMIO_OUT_PTR  ((uint16_t)CFG_MMIO_OUT_PTR)
#define MMIO_IN_PTR   ((uint16_t)CFG_MMIO_IN_PTR)
#define MMIO_IN_END   ((uint16_t)CFG_MMIO_IN_END)
#define MMIO_OUT_BUF  ((uint16_t)CFG_MMIO_OUT_BUF)

/* Run PUSH val / DISPLAY / HALT using port-based PUTCHAR.
 * Characters land in out_buf via the port_out hook. */
static void run_display(uint16_t val)
{
    memset(m, 0, sizeof m);
    out_buf[0] = '\0'; out_pos = 0;

    asm_at(p8_vm,          0x0000);
    asm_at(p9_putchar_port, CFG_PUTCHAR);

    /* ops: PUSH(1), DISPLAY(36), HALT(23) */
    m[CFG_OPS_BASE + 0] = 1;  m[CFG_OPS_BASE + 1] = 36; m[CFG_OPS_BASE + 2] = 23;
    /* args: val word */
    m[CFG_ARGS_BASE + 0] = (uint8_t)(val & 0xFF);
    m[CFG_ARGS_BASE + 1] = (uint8_t)(val >> 8);

    init(); pc = 0x0000; sp = CFG_HW_SP; iy = CFG_STK_BASE;
    for (int i = 0; i < 5000 && !halted; i++) step();
    out_buf[out_pos] = '\0';
}

/* Run PUSH val / DISPLAY / HALT using memory-mapped PUTCHAR.
 * Characters land in m[MMIO_OUT_BUF..]; caller reads them directly. */
static void run_display_mmio(uint16_t val)
{
    memset(m, 0, sizeof m);

    asm_at(p8_vm,         0x0000);
    asm_at(p9_putchar_mem, CFG_PUTCHAR);

    /* initialise MMIO_OUT_PTR to point at the output buffer */
    m[MMIO_OUT_PTR + 0] = (uint8_t)(MMIO_OUT_BUF & 0xFF);
    m[MMIO_OUT_PTR + 1] = (uint8_t)(MMIO_OUT_BUF >> 8);

    /* ops: PUSH(1), DISPLAY(36), HALT(23) */
    m[CFG_OPS_BASE + 0] = 1;  m[CFG_OPS_BASE + 1] = 36; m[CFG_OPS_BASE + 2] = 23;
    m[CFG_ARGS_BASE + 0] = (uint8_t)(val & 0xFF);
    m[CFG_ARGS_BASE + 1] = (uint8_t)(val >> 8);

    init(); pc = 0x0000; sp = CFG_HW_SP; iy = CFG_STK_BASE;
    for (int i = 0; i < 5000 && !halted; i++) step();

    /* null-terminate: MMIO_OUT_PTR now holds the end address */
    uint16_t end_ptr = (uint16_t)(m[MMIO_OUT_PTR] | ((uint16_t)m[MMIO_OUT_PTR + 1] << 8));
    m[end_ptr] = 0;
}

static void test_display(void)
{
    puts("--- Phase 9: OP_DISPLAY (port) ---");

    run_display(0x102A);  /* INT(42) */
    CHECK("display INT(42)   → \"42\"",   strcmp(out_buf, "42")   == 0);

    run_display(0x1000);  /* INT(0) */
    CHECK("display INT(0)    → \"0\"",    strcmp(out_buf, "0")    == 0);

    run_display(0x1001);  /* INT(1) */
    CHECK("display INT(1)    → \"1\"",    strcmp(out_buf, "1")    == 0);

    run_display(0x17FF);  /* INT(2047): max positive 12-bit */
    CHECK("display INT(2047) → \"2047\"", strcmp(out_buf, "2047") == 0);

    /* INT(-5): 12-bit two's-complement of -5 = 0xFFB; Val = 0x1FFB */
    run_display(0x1FFB);
    CHECK("display INT(-5)   → \"-5\"",   strcmp(out_buf, "-5")   == 0);

    /* NIL (0x0000) → "()" */
    run_display(0x0000);
    CHECK("display NIL       → \"()\"",   strcmp(out_buf, "()")   == 0);

    /* BOOL #t: MK_VAL(T_BOOL=5, 1) = 0x5001 */
    run_display(0x5001);
    CHECK("display #t        → \"#t\"",   strcmp(out_buf, "#t")   == 0);

    /* BOOL #f: MK_VAL(T_BOOL=5, 0) = 0x5000 */
    run_display(0x5000);
    CHECK("display #f        → \"#f\"",   strcmp(out_buf, "#f")   == 0);

    /* CHAR 'A': MK_VAL(T_CHAR=6, 65) = 0x6041 */
    run_display(0x6041);
    CHECK("display CHAR('A') → \"A\"",    strcmp(out_buf, "A")    == 0);

    /* STR "hi": manually load string at strs[0] = m[CFG_STRS_BASE], push T_STR(0) */
    {
        memset(m, 0, sizeof m);
        out_buf[0] = '\0'; out_pos = 0;
        asm_at(p8_vm, 0x0000);
        asm_at(p9_putchar_port, CFG_PUTCHAR);
        m[CFG_STRS_BASE + 0] = 'h'; m[CFG_STRS_BASE + 1] = 'i'; m[CFG_STRS_BASE + 2] = 0;
        m[CFG_OPS_BASE  + 0] = 1;  m[CFG_OPS_BASE + 1] = 36; m[CFG_OPS_BASE + 2] = 23;
        m[CFG_ARGS_BASE + 0] = 0x00; m[CFG_ARGS_BASE + 1] = 0x70; /* T_STR(0) = 0x7000 */
        init(); pc = 0x0000; sp = CFG_HW_SP; iy = CFG_STK_BASE;
        for (int i = 0; i < 5000 && !halted; i++) step();
        out_buf[out_pos] = '\0';
    }
    CHECK("display STR(\"hi\") → \"hi\"",  strcmp(out_buf, "hi")   == 0);
}

static void test_display_mmio(void)
{
    puts("--- Phase 9 (MMIO): OP_DISPLAY via memory-mapped PUTCHAR ---");

    run_display_mmio(0x102A);  /* INT(42) */
    CHECK("MMIO display INT(42)   → \"42\"",   strcmp((char *)&m[MMIO_OUT_BUF], "42")   == 0);

    run_display_mmio(0x1000);  /* INT(0) */
    CHECK("MMIO display INT(0)    → \"0\"",    strcmp((char *)&m[MMIO_OUT_BUF], "0")    == 0);

    run_display_mmio(0x1001);  /* INT(1) */
    CHECK("MMIO display INT(1)    → \"1\"",    strcmp((char *)&m[MMIO_OUT_BUF], "1")    == 0);

    run_display_mmio(0x17FF);  /* INT(2047): max positive 12-bit */
    CHECK("MMIO display INT(2047) → \"2047\"", strcmp((char *)&m[MMIO_OUT_BUF], "2047") == 0);

    /* INT(-5): 12-bit two's-complement of -5 = 0xFFB; Val = 0x1FFB */
    run_display_mmio(0x1FFB);
    CHECK("MMIO display INT(-5)   → \"-5\"",   strcmp((char *)&m[MMIO_OUT_BUF], "-5")   == 0);
}

/* -----------------------------------------------------------------------
 * Phase 10 — Cross-check: C compiler → bytecode dump → Z80 VM
 *
 * For each test expression:
 *   1. lisp_compile_src() parses + compiles it, filling ops[]/args[]/funs[].
 *   2. dump_bytecode() prints the full bytecode listing so it can be
 *      inspected before anything is assembled.
 *   3. The bytecode is loaded into Z80 emulator memory and the Z80 VM runs.
 *   4. The C VM runs the same bytecode independently.
 *   5. Both results are checked against the expected value AND each other.
 * ----------------------------------------------------------------------- */

/* Human-readable opcode names (must match the OP_xxx enum in lisp.c). */
static const char *op_names[] = {
    "NOP",   "PUSH",  "POP",    "DUP",
    "LOAD",  "STORE",
    "ADD",   "SUB",   "MUL",
    "EQ",    "LT",    "NOT",
    "CONS",  "CAR",   "CDR",    "PAIRP",  "NULLP",
    "JZ",    "JMP",
    "MKCLOS","CALL",  "TAILCALL","RET",   "HALT",
    "CHARP", "CHAR2INT","INT2CHAR",
    "STRP",  "STRLEN","STRREF", "STRCAT",
    "STREQ", "SYM2STR","STR2SYM","NUM2STR","STR2NUM",
    "DISPLAY","NEWLINE","WRITE","READ",   "ERROR",
    "APPLY",
    "LIST",  "APPEND"
};
#define NUM_OP_NAMES  ((int)(sizeof op_names / sizeof op_names[0]))

/* Print a Val in the same style as lisp.c's print_val, used in the dump. */
static void xc_print_val(uint16_t v)
{
    unsigned tag = v >> 12;
    unsigned dat = v & 0x0FFFu;
    switch (tag) {
    case 0:  printf("()");  break;  /* NIL  */
    case 1:  { int16_t n = (dat & 0x800u) ? (int16_t)(dat | 0xF000u) : (int16_t)dat;
               printf("INT(%d)", n); break; }
    case 2:  printf("SYM(%s)", sym_name((uint8_t)dat)); break;
    case 4:  printf("FUN(%u)", dat); break;
    case 5:  printf("%s", dat ? "#t" : "#f"); break;
    case 6:  { char c = (char)(dat & 0x7Fu);
               if (c == ' ')  printf("#\\space");
               else if (c == '\n') printf("#\\newline");
               else printf("#\\%c", c); break; }
    case 7:  printf("STR(%u)", dat); break;
    default: printf("0x%04X", v); break;
    }
}

/* Print the compiled bytecode for instructions [from, ncode). */
static void dump_bytecode(uint8_t from, uint8_t to)
{
    for (int ip = from; ip < to; ip++) {
        uint8_t  op  = ops[ip];
        uint16_t arg = args[ip];
        const char *nm = (op < (uint8_t)NUM_OP_NAMES) ? op_names[op] : "???";
        printf("    ip %2d  %-10s", ip, nm);
        /* decode argument based on opcode */
        switch (op) {
        case OP_PUSH:
            printf("  "); xc_print_val(arg); break;
        case OP_LOAD: case OP_STORE:
            printf("  sym[%u]=%s", arg, sym_name((uint8_t)arg)); break;
        case OP_JZ: case OP_JMP:
            printf("  → ip %u", arg); break;
        case OP_MKCLOS:
            printf("  fid=%u", arg); break;
        case OP_CALL: case OP_TAILCALL:
            printf("  argc=%u", arg); break;
        default:
            if (arg) printf("  arg=0x%04X", arg); break;
        }
        putchar('\n');
    }
    if (nfuns > 0) {
        puts("    funs:");
        for (int f = 0; f < nfuns; f++) {
            printf("      funs[%d]: addr=%u env=0x%02X argc=%u args=[",
                   f, funs[f].addr, funs[f].env, funs[f].argc);
            for (int k = 0; k < funs[f].argc; k++) {
                if (k) printf(",");
                printf("%s", sym_name(funs[f].args[k]));
            }
            puts("]");
        }
    }
}

/* Assemble all VM subroutines into m[] and initialise Z80 state.
 * Call after memset(m,0) has been done by the caller.            */
static void xc_load_vm(void)
{
    asm_at(p8_vm,          0x0000);
    asm_at(p8_env_addr,    0x0200);
    asm_at(p8_env_new,     0x0220);
    asm_at(p8_env_def,     0x0250);
    asm_at(p8_env_get,     0x0300);
    asm_at(p8_fun_addr,    0x0380);
    asm_at(p9_putchar_port, 0x03A0);
}

/* Load the compiled bytecode (ops, args, funs) from lisp.c's global arrays
 * into Z80 emulator memory at the standard layout addresses.               */
static void xc_load_bytecode(void)
{
    for (int i = 0; i < ncode; i++) {
        m[CFG_OPS_BASE  + i      ] = ops[i];
        m[CFG_ARGS_BASE + i*2    ] = (uint8_t)(args[i] & 0xFF);
        m[CFG_ARGS_BASE + i*2 + 1] = (uint8_t)(args[i] >> 8);
    }
    /* funs[] */
    for (int f = 0; f < nfuns; f++) {
        uint16_t base = (uint16_t)(CFG_FUN_BASE + (unsigned)f * 8u);
        m[base + 0] = funs[f].addr;
        m[base + 1] = funs[f].env;   /* 0xFF = no captured env */
        m[base + 2] = funs[f].argc;
        for (int k = 0; k < 4; k++) m[base + 3 + k] = funs[f].args[k];
    }
    /* VM state scalars */
    m[CFG_VM_STATE + 0] = 0;              /* CUR_ENV = 0  (root env) */
    m[CFG_VM_STATE + 1] = 0xFF;           /* CUR_FID = none */
    m[CFG_VM_STATE + 2] = 0;              /* CSP = 0 */
    m[CFG_VM_STATE + 3] = (uint8_t)nfuns; /* NFUNS */
    m[CFG_VM_STATE + 4] = 1;              /* NENVS = 1 (root env pre-allocated) */
    /* root env[0] */
    m[CFG_ENV_BASE + 18] = 0;             /* n = 0 (no bindings) */
    m[CFG_ENV_BASE + 19] = 0xFF;          /* parent = none */
    /* strs[] */
    for (int i = 0; i < nstrs; i++) {
        uint16_t base = (uint16_t)(CFG_STRS_BASE + (unsigned)i * 16u);
        for (int k = 0; k < 16; k++) m[base + k] = (uint8_t)strs[i].s[k];
    }
}

/* Run the Z80 emulator; return the Val written to m[0x8000..0x8001]. */
static uint16_t xc_run_z80(void)
{
    out_buf[0] = '\0'; out_pos = 0;
    init(); pc = 0x0000; sp = CFG_HW_SP; iy = CFG_STK_BASE;
    for (int i = 0; i < 50000 && !halted; i++) step();
    return (uint16_t)(m[0x8000] | ((uint16_t)m[0x8001] << 8));
}

/* Run one cross-check: compile expr, dump bytecode, run on both VMs,
 * check both equal expected, check they agree with each other.       */
static void xcheck_one(const char *expr, uint16_t expected, const char *desc)
{
    /* --- compile --- */
    lisp_reset();
    uint8_t start_ip = lisp_compile_src(expr);

    /* --- dump bytecode (the "see what is produced" step) --- */
    printf("  %s\n", expr);
    dump_bytecode(start_ip, ncode);

    /* --- run C VM --- */
    Val c_result = lisp_run_c(start_ip);

    /* --- load bytecode into Z80 memory and run --- */
    memset(m, 0, sizeof m);
    xc_load_vm();
    xc_load_bytecode();
    uint16_t z80_result = xc_run_z80();

    /* --- check --- */
    char cbuf[64];
    snprintf(cbuf, sizeof cbuf, "%s [C VM]",   desc);
    CHECK(cbuf, (uint16_t)c_result == expected);
    snprintf(cbuf, sizeof cbuf, "%s [Z80 VM]", desc);
    CHECK(cbuf, z80_result == expected);
    snprintf(cbuf, sizeof cbuf, "%s [C==Z80]", desc);
    CHECK(cbuf, (uint16_t)c_result == z80_result);
}

static void test_xcheck(void)
{
    puts("--- Phase 10: cross-check (C compiler → Z80 VM) ---");
    puts("");

    /* Basic arithmetic */
    xcheck_one("(+ 3 4)",    0x1007u, "(+ 3 4)   → INT(7)");
    puts("");
    xcheck_one("(* 6 7)",    0x102Au, "(* 6 7)   → INT(42)");
    puts("");
    xcheck_one("(- 10 3)",   0x1007u, "(- 10 3)  → INT(7)");
    puts("");

    /* Conditionals */
    xcheck_one("(if #t 1 2)", 0x1001u, "(if #t 1 2) → INT(1)");
    puts("");
    xcheck_one("(if #f 1 2)", 0x1002u, "(if #f 1 2) → INT(2)");
    puts("");

    /* Let binding */
    xcheck_one("(let ((x 3) (y 4)) (+ x y))", 0x1007u,
               "(let ((x 3)(y 4)) (+ x y)) → INT(7)");
    puts("");

    /* Lambda application */
    xcheck_one("((lambda (x) (* x x)) 7)", 0x1031u,
               "((lambda (x) (* x x)) 7) → INT(49)");
    puts("");
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* Suppress unused-variable warnings for disasm_range and GETCHAR strings */
    (void)(&disasm_range == &disasm_range);
    (void)p9_getchar_port;
    (void)p9_getchar_mem;

    puts("=== abclisp Z80 transpilation tests ===\n");

    test_harness();
    puts("");
    test_val_tag();
    puts("");
    test_val_payload();
    puts("");
    test_lisp_stack();
    puts("");
    test_int_add();
    puts("");
    test_dispatch();
    puts("");
    test_push_add();
    puts("");
    test_env_addr();
    puts("");
    test_env_def_get();
    puts("");
    test_env_update();
    puts("");
    test_env_chain();
    puts("");
    test_fun_addr();
    puts("");
    test_lambda_call();
    puts("");
    test_display();
    puts("");
    test_display_mmio();
    puts("");
    test_xcheck();
    puts("");

    printf("Results: %d passed, %d failed  (total %d)\n",
           pass_count, fail_count, pass_count + fail_count);
    if (fail_count == 0) puts("ALL PASS");
    else puts("SOME FAILURES");

    return fail_count == 0 ? 0 : 1;
}
