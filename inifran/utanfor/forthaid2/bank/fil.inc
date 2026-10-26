
;W FCALL FCALL
; ( fb -- ) Anropa en av ROM:ens filrutiner (SAVEW och LOADW, fil.fs).
; Fälten i fb: +0 FCB:n (15 byte), +16 HL, +18 BC, +20 A, +22 rutinen;
; IX = fb under anropet. Stacken är fb och nedåt, på BASIC:s lediga
; minne över HEAP, så att DOS:en inte når de egna orden under $FC80.
; Efteråt står HL i +16 och DE i +26 (bufferten efter GET och PUT).
; Rutinerna bevarar IX. Ett fel går till BASIC, som i kommandoloopen.
FCALL:  PUSH DE             ; IP
        PUSH IX             ; returstacken
        PUSH BC
        POP  IX             ; IX = fb
        LD   HL,0
        ADD  HL,SP
        LD   (IX+24),L      ; datastacken
        LD   (IX+25),H
        LD   SP,IX
        LD   HL,FCALL1
        PUSH HL
        LD   L,(IX+22)      ; rutinen
        LD   H,(IX+23)
        PUSH HL
        LD   C,(IX+18)
        LD   B,(IX+19)
        LD   A,(IX+20)
        LD   L,(IX+16)
        LD   H,(IX+17)
        RET
FCALL1: LD   (IX+16),L
        LD   (IX+17),H
        LD   (IX+26),E
        LD   (IX+27),D
        LD   L,(IX+24)
        LD   H,(IX+25)
        LD   SP,HL
        POP  IX
        POP  DE
        POP  BC
        JP   NEXT
