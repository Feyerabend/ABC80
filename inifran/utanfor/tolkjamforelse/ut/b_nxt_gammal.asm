; utanför boken: BASIC II, NEXT för en heltalsvariabel utan IX
; NEXT ($2809) läser FOR-blocket med (IX+n), 19-20 T per byte, och går
; omkring 467 T per varv. Här går HL genom blocket i ordning. Flyttal
; och fel 183 lämnas till ROM:ens kod. Blocket: +0 efter NEXT, +2
; variabelns referens, +4 slingans början, +6 typ, +7 steget, +9 gränsen.
        ORG  $7840
NEXTX:  EX   DE,HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)              ; DE = blockets referens
        LD   HL,($FF2E)          ; VARBEG
        LD   B,H
        LD   C,L
        ADD  HL,DE               ; HL = blocket
        PUSH HL
        INC  HL
        LD   A,(HL)              ; FOR inte utfört?
        AND  A
        JR   Z,NXROM
        INC  HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)              ; DE = variabelns referens
        INC  HL
        INC  HL
        INC  HL
        BIT  0,(HL)              ; +6: flyttal?
        JR   Z,NXROM
        EX   DE,HL
        ADD  HL,BC               ; variabelns adress
        LD   ($FF36),HL          ; VFF36, som i ROM:en
        EX   DE,HL
        INC  HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)              ; BC = steget
        EX   DE,HL
        LD   A,(HL)              ; variabeln + steget, utan spillkontroll
        ADD  A,C
        LD   (HL),A
        LD   C,A
        INC  HL
        LD   A,(HL)
        ADC  A,B
        LD   (HL),A
        EX   DE,HL
        LD   D,A
        LD   E,C                 ; DE = nya värdet
        INC  HL
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A                 ; HL = gränsen
        LD   A,B
        AND  A                   ; CY = 0
        JP   P,NXPOS
        EX   DE,HL               ; negativt steg: vänd
NXPOS:  SBC  HL,DE
        JP   PO,NXV
        LD   A,$80
        XOR  H
NXV:    POP  HL                  ; blocket
        JP   M,NXUT
        LD   DE,$0004            ; nästa varv: slingans början
        ADD  HL,DE
NXUT:   LD   E,(HL)
        INC  HL
        LD   D,(HL)
        RET
NXROM:  POP  IX                  ; ROM:ens väg, IX = blocket
        LD   DE,($FF2E)
        JP   $2816
