; Exempel 6: en seriell mottagare i program, på samma sätt som
; texttelefonen tar emot. Skrivet för boken, inte ur någon krets.
;
; Kretsen sitter på $4000. Från BASIC:
;   Z=CALL(16384)   koppla in: PIO A i läge 1, avbrott varannan bildlinje
;   Z=CALL(16387)   koppla ur: PIO A som ROM:en ställer den
;   A=CALL(16390)   nästa mottagna tecken, eller -1 om inget har kommit
; Hastigheten är 1200 baud, 8 databitar, ingen paritet, en stoppbit.
; Medan kretsen är inkopplad ger tangenterna inget avbrott; tangentbordet
; får läsas på annat sätt (texttelefonen läser det i sin huvudslinga).

CASINT: EQU  $0594          ; ROM:ens kassettrutin (båda versionerna)

; Variablerna ligger i kassettens buffertar, som inte används så länge
; kassetten inte används. Ringbufferten fyller en hel sida, så att
; indexen går runt av sig själva.
STATE:  EQU  $FB00          ; JP till tillståndet (3 bytes)
COUNT:  EQU  $FB03          ; avbrott kvar till nästa läsning
SHIFT:  EQU  $FB04          ; tecknet, bit för bit
TPTR:   EQU  $FB05          ; nästa post i TAB (2 bytes)
INPOS:  EQU  $FB07          ; skrivindex i ringbufferten
OUTPOS: EQU  $FB08          ; läsindex (direkt efter INPOS)
RING:   EQU  $FC            ; ringbufferten $FC00-$FCFF (sidan)

        ORG  $4000

        JP   PA             ; $4000
        JP   AV             ; $4003
        JP   HAMTA          ; $4006

; Koppla in. Tillståndet blir VILA, bufferten töms, avbrottet går via
; tabellen på $4034 och PIO A ställs i läge 1: avbrott på strobe-
; ingången, som växlar för varje bildlinje (7 800 avbrott i sekunden).
PA:     DI
        LD   A,$C3          ; JP
        LD   (STATE),A
        LD   HL,VILA
        LD   (STATE+1),HL
        XOR  A
        LD   (INPOS),A
        LD   (OUTPOS),A
        LD   A,$40
        LD   I,A
        LD   A,$4F          ; läge 1 (inmatning)
        OUT  ($39),A
        LD   A,$34          ; samma vektor som förut
        OUT  ($39),A
        LD   A,$87          ; avbrott på
        OUT  ($39),A
        EI
        RET

        DEFS $4034-$        ; fyll ut till vektortabellen (PA måste
                            ; sluta före $4034)

        DEFW LINJE          ; $4034 PIO A: varannan bildlinje
        DEFW CASINT         ; $4036 PIO B: kassetten, ROM:ens

; Koppla ur: PIO A tillbaka i bitläge med avbrott när bit 7 (tangent
; nedtryckt) blir hög, och ROM:ens egen tabell på $0034.
AV:     DI
        LD   A,$CF          ; läge 3 (bitläge)
        OUT  ($39),A
        LD   A,$FF          ; alla åtta bitarna in
        OUT  ($39),A
        LD   A,$34          ; vektorn
        OUT  ($39),A
        LD   A,$B7          ; avbrott på, mask följer
        OUT  ($39),A
        LD   A,$7F          ; bara bit 7
        OUT  ($39),A
        XOR  A
        LD   I,A
        EI
        RET

; Nästa tecken ur ringbufferten till HL, som blir CALL:s värde.
HAMTA:  LD   HL,(INPOS)     ; L = INPOS, H = OUTPOS
        LD   A,H
        CP   L
        LD   HL,-1          ; tom
        RET  Z
        LD   L,A            ; läs på OUTPOS
        INC  A
        LD   (OUTPOS),A
        LD   H,RING
        LD   L,(HL)
        LD   H,0
        RET

; Avstånden mellan läsningarna, i avbrott. En bit är 7800/1200 =
; 6,5 avbrott. Databit n läses mitt i biten, (1,5 + n) bitar efter
; startbitens början: 9,75 16,25 22,75 29,25 35,75 42,25 48,75 55,25
; avbrott, avrundat 10 16 23 29 36 42 49 55. Avstånden är skillnaderna.
; Varje läsning avrundas för sig, så felet växer inte genom tecknet.
TAB:    DEFB 10             ; startbiten till databit 0
        DEFB 6,7,6,7,6,7,6  ; till databit 1-7
        DEFB 0              ; slut: stoppbiten
STOPN:  EQU  7              ; 9,5 bitar = 61,75 -> 62, dvs. 62 - 55

; Avbrottet. Det hoppar till det aktuella tillståndet genom JP-
; instruktionen i RAM; ett tillstånd byter till nästa genom att skriva
; en ny adress i den. Alla tillstånd slutar i KLAR.
LINJE:  PUSH AF
        PUSH HL
        JP   STATE

KLAR:   POP  HL
        POP  AF
        EI
        RETI

; Vila: vänta på startbiten (RxD = bit 0 i PIO B, 0 = startbit).
VILA:   IN   A,($3A)
        RRCA                ; RxD till CY
        JR   C,KLAR         ; 1: linjen vilar
        LD   HL,TAB
        LD   A,(HL)         ; avstånd till databit 0
        LD   (COUNT),A
        INC  HL
        LD   (TPTR),HL
        LD   HL,DATA
BYT:    LD   (STATE+1),HL   ; nytt tillstånd
        JR   KLAR

; Data: räkna ned; vid noll läses en bit, som skiftas in uppifrån.
; Efter åtta bitar står den först mottagna i bit 0.
DATA:   LD   HL,COUNT
        DEC  (HL)
        JR   NZ,KLAR
        IN   A,($3A)
        RRCA                ; RxD till CY
        LD   A,(SHIFT)
        RRA
        LD   (SHIFT),A
        LD   HL,(TPTR)
        LD   A,(HL)         ; avstånd till nästa bit
        INC  HL
        LD   (TPTR),HL
        AND  A
        JR   Z,D1           ; 0: alla åtta är lästa
        LD   (COUNT),A
        JR   KLAR
D1:     LD   A,STOPN
        LD   (COUNT),A
        LD   HL,STOPP
        JR   BYT

; Stopp: läs mitt i stoppbiten. Är linjen 1 läggs tecknet i bufferten,
; annars (ramfel) kastas det. I båda fallen väntar vi på nästa startbit.
STOPP:  LD   HL,COUNT
        DEC  (HL)
        JR   NZ,KLAR
        LD   HL,VILA
        LD   (STATE+1),HL
        IN   A,($3A)
        RRCA
        JR   NC,KLAR        ; ramfel
        LD   HL,(INPOS)     ; L = INPOS, H = OUTPOS
        LD   A,L
        INC  A
        CP   H
        JR   Z,KLAR         ; bufferten full: tecknet kastas
        LD   (INPOS),A
        LD   H,RING     ; skriv på det gamla INPOS
        LD   A,(SHIFT)
        LD   (HL),A
        JR   KLAR
