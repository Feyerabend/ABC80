
; Tangentbordets avbrott när kretsen är inkopplad (fai.asm, läggs sist
; i fa.asm av gor.sh). ABC80 har avbrottsläge 2: med I = $5F läses
; vektorerna på $5F34, tangentbordets hit och kassettens till ROM:en.
; Står BASIC:en i kommandoloopen och väntar på första tecknet i en rad
; tar kretsen över: ROM:ens rutin tar emot tangenten, och sedan läser
; ingång 3 (KBD i fa.fs) raden. Annars går avbrottet till ROM:en.

KEYINT: EQU  $031E          ; ROM: tangentbordets avbrott
CASINT: EQU  $0594          ; ROM: kassettens avbrott
READY:  EQU  $00CC          ; ROM: efter ett kommando
CURADR: EQU  $0293          ; ROM: DE = markörens adress, IX = $FDF3
STEPF:  EQU  $7F7E          ; <> 0 medan CTRL-T eller CTRL-S hålls ned
                            ; (bildminnets sista lediga variabelplats)
KBDSP:  EQU  $FD00          ; toppen på KBD:s stackar, i kassettbuffertarna

; CTRL-C i pausen: BRKFLG sätts som i ROM:ens rutin. Stannade
; programmet i ROM:ens TSTBRK ($033E) mellan läsningen av BRKFLG och
; nollningen ($0344), skulle den skriva över flaggan; då börjar TSTBRK
; om. Programmets återhopp ligger under PAUSE:s, IX, DE, HL och AF.
PSBRK:  LD   A,$83
        LD   ($FE07),A      ; BRKFLG
        LD   HL,10
        ADD  HL,SP
        LD   A,(HL)
        SUB  $3E
        CP   7              ; $033E-$0344
        RET  NC
        INC  HL
        LD   A,(HL)
        CP   $03
        RET  NZ
        DEC  HL
        LD   (HL),$3E
        RET

        DEFS $5F34-$        ; gor.sh prövar att KTAB hamnar på $5F34
KTAB:   DEFW KBDINT,CASINT

; Kommandoloopen (CMDLP, $00DB) har satt $FE24 = 1, SP = (STKTOP), lagt
; $FE40 på stacken och anropat INLINE, med återhoppet $00F0.
KBDINT: PUSH AF
        PUSH HL
        IN   A,($38)
        RLCA                ; CY = tangenten är nedtryckt
        JP   NC,KBDREL      ; släppt: stegningen med CTRL-T, CTRL-S
        LD   A,($FE24)
        DEC  A
        JR   NZ,KBDRUN      ; inte direktläge
        LD   HL,($FE27)     ; STKTOP
        DEC  HL
        LD   A,(HL)
        CP   $FE            ; $FE40
        JR   NZ,KBD1
        DEC  HL
        LD   A,(HL)
        CP   $40
        JR   NZ,KBD1
        DEC  HL
        LD   A,(HL)
        AND  A              ; $00F0
        JR   NZ,KBD1
        DEC  HL
        LD   A,(HL)
        CP   $F0
        JR   NZ,KBD1
; Radinmatningen överges. Stacken sätts om först, så att ett nytt
; avbrott inte ser kommandoloopen; ROM:ens rutin slutar med EI och RETI,
; som går till KBD2.
        LD   HL,($FE27)
        LD   SP,HL
        LD   HL,KBD2
        PUSH HL
        JP   KEYINT
; Ett program kör: CTRL-C ger en paus, andra tangenter går till ROM:en,
; utom medan programmet stegas.
KBDRUN: LD   A,(STEPF)
        OR   A
        JP   NZ,KBD9
        IN   A,($38)
        CP   $83            ; CTRL-C med strobebiten
        JP   Z,PAUSE
KBD1:   POP  HL
        POP  AF
        JP   KEYINT
; Ingång 3, som Z=CALL(16384,3) men med READY som återhopp: BYE går
; tillbaka dit som efter ROM:ens egna kommandon. Stackarna ligger i
; kassettbuffertarnas översta 128 byte (TAK i fa.fs).
KBD2:   LD   SP,KBDSP
        LD   HL,READY
        PUSH HL
        LD   DE,3
        JP   COLD

; Stegningen. Pausen står kvar i avbrottet, med ett utropstecken i
; markören, tills nästa tangent: CTRL-T kör vidare med TRACE och CTRL-S
; utan, så länge tangenten hålls ned; PIO A ger då avbrott när den
; släpps, och programmet stannar igen. CTRL-C avbryter som i ROM:en,
; andra tangenter kör vidare som förut. Ett tryck under pausen ger ett
; avbrott som tas efter RETI; då är tangenten släppt, och STEPF = 0
; säger att det inte är stegningens.
KBDREL: LD   A,(STEPF)
        OR   A
        JR   Z,KBD9
        XOR  A
        LD   (STEPF),A
        LD   A,$B7          ; PIO A: avbrott när en tangent trycks ned
        OUT  ($39),A
        LD   A,$7F          ; (bit 7), som ROM:en ställer den
        OUT  ($39),A
        LD   A,($FE24)
        DEC  A
        JR   Z,KBD9         ; programmet hann bli klart
PAUSE:  PUSH DE
        PUSH IX
        CALL CURADR
        LD   A,(DE)
        LD   H,A
        LD   A,'!'
        LD   (DE),A
PS1:    IN   A,($38)        ; vänta tills tangenten släpps
        RLCA
        JR   C,PS1
PS2:    IN   A,($38)        ; och på nästa
        RLCA
        JR   NC,PS2
        LD   A,H
        LD   (DE),A
        LD   HL,$FE25       ; bit 4 = TRACE
        IN   A,($38)
        AND  $7F
        CP   $14            ; CTRL-T
        JR   Z,PST
        CP   $13            ; CTRL-S
        JR   Z,PSS
        CP   $03            ; CTRL-C
        JR   NZ,PS3
        CALL PSBRK
PS3:    IN   A,($38)        ; vänta tills tangenten släpps
        RLCA
        JR   C,PS3
        JR   PSX
PST:    SET  4,(HL)
        JR   PSGO
PSS:    RES  4,(HL)
PSGO:   LD   (STEPF),A      ; A <> 0
        LD   A,$97          ; PIO A: avbrott när tangenten släpps
        OUT  ($39),A
        LD   A,$7F
        OUT  ($39),A
PSX:    POP  IX
        POP  DE
KBD9:   POP  HL
        POP  AF
        EI
        RETI
