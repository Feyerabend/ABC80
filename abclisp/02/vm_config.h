// PENDING: NOT FINISHED YET!!!
/* vm_config.h — machine-specific layout constants for the abclisp Z80 VM.
 *
 * Change the values here to retarget the VM to a different machine.
 * All assembly source strings that embed these addresses use the XSTR
 * macro so that a single edit here propagates everywhere.
 *
 * Current target: generic Z80 with 64 KB RAM, OUT port 0 for output.
 */
#pragma once

/* -----------------------------------------------------------------------
 * I/O
 * ----------------------------------------------------------------------- */
#define CFG_PORT_IO    0       /* Z80 OUT/IN port number for character I/O */
#define CFG_PUTCHAR    0x03A0  /* PUTCHAR subroutine address               */
#define CFG_GETCHAR    0x03C0  /* GETCHAR subroutine address               */

/* -----------------------------------------------------------------------
 * Z80 hardware stack
 * ----------------------------------------------------------------------- */
#define CFG_HW_SP      0xFE00   /* Initial SP — top of scratch RAM         */

/* -----------------------------------------------------------------------
 * VM RAM layout
 *
 *   ENV_BASE   0x4000   environment frames  (32 B × 32 = 1 KB)
 *   FUN_BASE   0x4400   function/closure records  (8 B × 48 = 384 B)
 *   STRS_BASE  0x4580   string pool  (16 B × 32 = 512 B)
 *   VM_STATE   0x45A0   scalar VM registers (5 bytes: CUR_ENV CUR_FID CSP
 *                         NFUNS NENVS)
 *   STK_BASE   0x4600   Lisp value stack (IY base, grows upward, 2 B/Val)
 *   OPTBL      0x4700   opcode dispatch table (2 B × 37 entries = 74 B)
 *   OPS_BASE   0x4900   bytecode opcode stream (1 B each)
 *   ARGS_BASE  0x4A00   bytecode operand stream (2 B each, little-endian)
 *
 * Memory-mapped I/O scratch (used by the MMIO PUTCHAR/GETCHAR variants):
 *   MMIO_OUT_PTR  0x45F0   write pointer into output buffer (word)
 *   MMIO_IN_PTR   0x45F2   read pointer into input buffer (word)
 *   MMIO_IN_END   0x45F4   one-past-end of input buffer (word)
 *   MMIO_OUT_BUF  0x4800   output character buffer
 * ---------------------------------------------------------------------- */
#define CFG_ENV_BASE   0x4000
#define CFG_FUN_BASE   0x4400
#define CFG_STRS_BASE  0x4580
#define CFG_VM_STATE   0x45A0
#define CFG_STK_BASE   0x4600
#define CFG_OPTBL      0x4700
#define CFG_OPS_BASE   0x4900
#define CFG_ARGS_BASE  0x4A00

#define CFG_MMIO_OUT_PTR  0x45F0
#define CFG_MMIO_IN_PTR   0x45F2
#define CFG_MMIO_IN_END   0x45F4
#define CFG_MMIO_OUT_BUF  0x4800

/* ----------------------------------------------------------------------
 * Stringify helper — embeds a numeric constant into an assembly source string.
 *
 * Usage:  "  LD   IY, " XSTR(CFG_STK_BASE) "\n"
 * Result: "  LD   IY, 0x4600\n"
 * ---------------------------------------------------------------------- */
#define XSTR(x)  STR(x)
#define STR(x)   #x
