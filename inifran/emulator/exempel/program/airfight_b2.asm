; airfight_b2.asm -- maskinfilen för AIRFIGHT med BASIC II
;
; gor.sh lägger den efter airfight.asm. BASIC II lägger programmet högst
; upp, under MEMTOP ($F268 med DOS:en), och variablerna nedifrån $8000,
; så $E000 är upptaget av laddaren. Spelet ligger på $9000 och
; variablerna på $9F00, som CHIP8Z och SNAKE3Z.

KOD         EQU 09000H  ; CALL(36864)
VAR_BASE    EQU 09F00H
CLKLO       EQU 0FFF5H  ; NMI räknar upp den, slår om vid 50
KEYFLG      EQU 0FFE2H  ; bit 7: ny tangent (KEYINT)
BRKPTR      EQU 0FF85H  ; pekare till BREAK-byten, bit 0

; NOLLBRK -- nollar BREAK-biten och tangentflaggan
; Clobbers: A, HL
NOLLBRK:
        XOR A
        LD (KEYFLG), A
        LD HL, (BRKPTR)
        LD A, H
        OR L
        RET Z            ; ingen BREAK-byte
        RES 0, (HL)
        RET
