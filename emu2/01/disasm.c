// Z80 disassembler — adapted from TurboDis Z80 by Markus Fritze (sarnau).
// Original: https://github.com/sarnau/Z80DisAssembler  (freeware)
// Fritzes restriction on distribution:
// "This program is freeware. It is not allowed to be used as a base for a commercial product!"
// Stripped to OpcodeLen + Disassemble only; reads via abc80_read_mem().

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "abc80.h"
#include "disasm.h"

// Read helpers — all memory accesses go through the Z80 address space.
#define M(a)      abc80_read_mem((uint16_t)(a))
#define BYTE_1    M(adr + 1)
#define BYTE_2    M(adr + 2)
#define WORD_1_2  ((uint16_t)(BYTE_1 | ((uint16_t)BYTE_2 << 8)))
#define G(fmt,...) snprintf(s, (size_t)ssize, fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
int z80_oplen(uint16_t addr) {
    uint16_t p = addr;
    int len = 1;
    switch (M(p)) {
    case 0x06: case 0x0E: case 0x10: case 0x16: case 0x18: case 0x1E:
    case 0x20: case 0x26: case 0x28: case 0x2E: case 0x30: case 0x36:
    case 0x38: case 0x3E: case 0xC6: case 0xCE: case 0xD3: case 0xD6:
    case 0xDB: case 0xDE: case 0xE6: case 0xEE: case 0xF6: case 0xFE:
    case 0xCB:
        len = 2; break;
    case 0x01: case 0x11: case 0x21: case 0x22: case 0x2A: case 0x31:
    case 0x32: case 0x3A: case 0xC2: case 0xC3: case 0xC4: case 0xCA:
    case 0xCC: case 0xCD: case 0xD2: case 0xD4: case 0xDA: case 0xDC:
    case 0xE2: case 0xE4: case 0xEA: case 0xEC: case 0xF2: case 0xF4:
    case 0xFA: case 0xFC:
        len = 3; break;
    case 0xDD:
        len = 2;
        switch (M(p+1)) {
        case 0x34: case 0x35: case 0x46: case 0x4E: case 0x56: case 0x5E:
        case 0x66: case 0x6E: case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x77: case 0x7E: case 0x86: case 0x8E:
        case 0x96: case 0x9E: case 0xA6: case 0xAE: case 0xB6: case 0xBE:
            len = 3; break;
        case 0x21: case 0x22: case 0x2A: case 0x36: case 0xCB:
            len = 4; break;
        } break;
    case 0xED:
        len = 2;
        switch (M(p+1)) {
        case 0x43: case 0x4B: case 0x53: case 0x5B: case 0x73: case 0x7B:
            len = 4; break;
        } break;
    case 0xFD:
        len = 2;
        switch (M(p+1)) {
        case 0x34: case 0x35: case 0x46: case 0x4E: case 0x56: case 0x5E:
        case 0x66: case 0x6E: case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x77: case 0x7E: case 0x86: case 0x8E:
        case 0x96: case 0x9E: case 0xA6: case 0xAE: case 0xB6: case 0xBE:
            len = 3; break;
        case 0x21: case 0x22: case 0x2A: case 0x36: case 0xCB:
            len = 4; break;
        } break;
    }
    return len;
}

// ---------------------------------------------------------------------------
int z80_disasm(uint16_t addr, char *out, int outlen) {
    uint16_t adr = addr;
    char *s = out; int ssize = outlen;
    uint8_t a = M(adr);
    uint8_t d = (a >> 3) & 7;
    uint8_t e = a & 7;
    static const char *reg[8]   = { "B","C","D","E","H","L","(HL)","A" };
    static const char *dreg[4]  = { "BC","DE","HL","SP" };
    static const char *cond[8]  = { "NZ","Z","NC","C","PO","PE","P","M" };
    static const char *arith[8] = { "ADD  A,","ADC  A,","SUB","SBC  A,","AND","XOR","OR","CP" };
    const char *ireg;

    switch (a & 0xC0) {
    case 0x00:
        switch (e) {
        case 0:
            switch (d) {
            case 0: G("NOP");                                          break;
            case 1: G("EX   AF,AF'");                                  break;
            case 2: G("DJNZ $%04X", (uint16_t)(adr+2+(int8_t)BYTE_1));break;
            case 3: G("JR   $%04X", (uint16_t)(adr+2+(int8_t)BYTE_1));break;
            default:G("JR   %s,$%04X",cond[d&3],(uint16_t)(adr+2+(int8_t)BYTE_1));break;
            } break;
        case 1:
            if (a & 8) G("ADD  HL,%s", dreg[d>>1]);
            else       G("LD   %s,$%04X", dreg[d>>1], WORD_1_2);
            break;
        case 2:
            switch (d) {
            case 0: G("LD   (BC),A");             break;
            case 1: G("LD   A,(BC)");             break;
            case 2: G("LD   (DE),A");             break;
            case 3: G("LD   A,(DE)");             break;
            case 4: G("LD   ($%04X),HL",WORD_1_2);break;
            case 5: G("LD   HL,($%04X)",WORD_1_2);break;
            case 6: G("LD   ($%04X),A", WORD_1_2);break;
            case 7: G("LD   A,($%04X)", WORD_1_2);break;
            } break;
        case 3:
            if (a & 8) G("DEC  %s", dreg[d>>1]);
            else       G("INC  %s", dreg[d>>1]);
            break;
        case 4: G("INC  %s", reg[d]); break;
        case 5: G("DEC  %s", reg[d]); break;
        case 6: G("LD   %s,$%02X", reg[d], BYTE_1); break;
        case 7: {
            static const char *t[8]={"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
            G("%s", t[d]);
        } break;
        } break;
    case 0x40:
        if (a == 0x76) G("HALT");
        else           G("LD   %s,%s", reg[d], reg[e]);
        break;
    case 0x80:
        G("%-5s%s", arith[d], reg[e]);
        break;
    case 0xC0:
        switch (e) {
        case 0: G("RET  %s", cond[d]); break;
        case 1:
            if (d & 1) {
                switch (d>>1) {
                case 0: G("RET");        break;
                case 1: G("EXX");        break;
                case 2: G("JP   (HL)");  break;
                case 3: G("LD   SP,HL"); break;
                }
            } else {
                G("POP  %s", (d>>1)==3?"AF":dreg[d>>1]);
            } break;
        case 2: G("JP   %s,$%04X", cond[d], WORD_1_2); break;
        case 3:
            switch (d) {
            case 0: G("JP   $%04X", WORD_1_2); break;
            case 1: { // 0xCB
                a = M(++adr); d=(a>>3)&7; e=a&7;
                switch (a & 0xC0) {
                case 0x00: { static const char *t[8]={"RLC","RRC","RL","RR","SLA","SRA","SLL","SRL"};
                             G("%-5s%s",t[d],reg[e]); } break;
                case 0x40: G("BIT  %d,%s",d,reg[e]); break;
                case 0x80: G("RES  %d,%s",d,reg[e]); break;
                case 0xC0: G("SET  %d,%s",d,reg[e]); break;
                }
            } break;
            case 2: G("OUT  ($%02X),A", BYTE_1); break;
            case 3: G("IN   A,($%02X)", BYTE_1); break;
            case 4: G("EX   (SP),HL");  break;
            case 5: G("EX   DE,HL");    break;
            case 6: G("DI");            break;
            case 7: G("EI");            break;
            } break;
        case 4: G("CALL %s,$%04X", cond[d], WORD_1_2); break;
        case 5:
            if (d & 1) {
                switch (d>>1) {
                case 0: G("CALL $%04X", WORD_1_2); break;
                case 2: { // 0xED
                    a = M(++adr); d=(a>>3)&7; e=a&7;
                    switch (a & 0xC0) {
                    case 0x40:
                        switch (e) {
                        case 0: G(a==0x70?"IN   (C)":"IN   %s,(C)",reg[d]); break;
                        case 1: G(a==0x71?"OUT  (C),0":"OUT  (C),%s",reg[d]); break;
                        case 2: G((d&1)?"ADC  HL,%s":"SBC  HL,%s",dreg[d>>1]); break;
                        case 3: if (d&1) G("LD   %s,($%04X)",dreg[d>>1],WORD_1_2);
                                else     G("LD   ($%04X),%s",WORD_1_2,dreg[d>>1]); break;
                        case 4: G("NEG"); break;
                        case 5: G((d==0)?"RETN":"RETI"); break;
                        case 6: G("IM   %d", d?d-1:0); break;
                        case 7: { static const char *t[8]={"LD I,A","LD R,A","LD A,I","LD A,R","RRD","RLD","???","???"};
                                  G("%s",t[d]); } break;
                        } break;
                    case 0x80: { static const char *t[32]={
                        "LDI","CPI","INI","OUTI","?","?","?","?",
                        "LDD","CPD","IND","OUTD","?","?","?","?",
                        "LDIR","CPIR","INIR","OTIR","?","?","?","?",
                        "LDDR","CPDR","INDR","OTDR","?","?","?","?"};
                        G("%s",t[a&0x1F]); } break;
                    default: G("ED%02X",a); break;
                    }
                } break;
                default: { // 0xDD (IX) or 0xFD (IY)
                    ireg = (a & 0x20) ? "IY" : "IX";
                    a = M(++adr); d=(a>>3)&7; e=a&7;
                    switch (a) {
                    case 0x09: G("ADD  %s,BC",ireg); break;
                    case 0x19: G("ADD  %s,DE",ireg); break;
                    case 0x21: G("LD   %s,$%04X",ireg,WORD_1_2); break;
                    case 0x22: G("LD   ($%04X),%s",WORD_1_2,ireg); break;
                    case 0x23: G("INC  %s",ireg); break;
                    case 0x29: G("ADD  %s,%s",ireg,ireg); break;
                    case 0x2A: G("LD   %s,($%04X)",ireg,WORD_1_2); break;
                    case 0x2B: G("DEC  %s",ireg); break;
                    case 0x34: G("INC  (%s+$%02X)",ireg,BYTE_1); break;
                    case 0x35: G("DEC  (%s+$%02X)",ireg,BYTE_1); break;
                    case 0x36: G("LD   (%s+$%02X),$%02X",ireg,BYTE_1,BYTE_2); break;
                    case 0x39: G("ADD  %s,SP",ireg); break;
                    case 0x46: case 0x4E: case 0x56: case 0x5E:
                    case 0x66: case 0x6E: case 0x7E:
                        G("LD   %s,(%s+$%02X)",reg[d],ireg,BYTE_1); break;
                    case 0x70: case 0x71: case 0x72: case 0x73:
                    case 0x74: case 0x75: case 0x77:
                        G("LD   (%s+$%02X),%s",ireg,BYTE_1,reg[e]); break;
                    case 0x86: case 0x8E: case 0x96: case 0x9E:
                    case 0xA6: case 0xAE: case 0xB6: case 0xBE:
                        G("%-5s(%s+$%02X)",arith[d],ireg,BYTE_1); break;
                    case 0xE1: G("POP  %s",ireg); break;
                    case 0xE3: G("EX   (SP),%s",ireg); break;
                    case 0xE5: G("PUSH %s",ireg); break;
                    case 0xE9: G("JP   (%s)",ireg); break;
                    case 0xF9: G("LD   SP,%s",ireg); break;
                    case 0xCB: {
                        a = BYTE_2; d=(a>>3)&7;
                        switch (a & 0xC0) {
                        case 0x00: { static const char *t[8]={"RLC","RRC","RL","RR","SLA","SRA","SLL","SRL"};
                                     G("%-5s(%s+$%02X)",t[d],ireg,BYTE_1); } break;
                        case 0x40: G("BIT  %d,(%s+$%02X)",d,ireg,BYTE_1); break;
                        case 0x80: G("RES  %d,(%s+$%02X)",d,ireg,BYTE_1); break;
                        case 0xC0: G("SET  %d,(%s+$%02X)",d,ireg,BYTE_1); break;
                        }
                    } break;
                    default: G("%s%02X",ireg,a); break;
                    }
                } break;
                }
            } else {
                G("PUSH %s", (d>>1)==3?"AF":dreg[d>>1]);
            } break;
        case 6: G("%-5s$%02X", arith[d], BYTE_1); break;
        case 7: G("RST  $%02X", a & 0x38); break;
        } break;
    }
    return z80_oplen(addr);
}
