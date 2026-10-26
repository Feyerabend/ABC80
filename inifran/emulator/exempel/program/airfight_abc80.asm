; airfight_abc80.asm -- maskinfilen för AIRFIGHT med ABC80:s egen ROM
;
; gor.sh lägger den efter airfight.asm. Spelet ligger på $E000 och
; variablerna på $EF00: i en ABC80 med 16 KB ovanför BASIC-programmet
; och under stacken (STKTOP $FB00).

KOD         EQU 0E000H  ; CALL(57344)
VAR_BASE    EQU 0EF00H
CLKLO       EQU 0FDF0H  ; klockans låga byte, NMI räknar upp den
KEYFLG      EQU 0FDF5H  ; bit 7: en tangent väntar (KEYINT)
BRKFLG      EQU 0FE07H  ; CTRL-C, KEYINT sätter och TSTBRK nollar

; NOLLBRK -- nollar BREAK och tangentflaggan
; Clobbers: A
NOLLBRK:
        XOR A
        LD (BRKFLG), A
        LD (KEYFLG), A
        RET
