
; Forthaid (utanfor/forthaid): de nya maskinkodsorden. Läggs efter
; bokens kärna i kopian (gor.sh), i samma form som där.

;W * STAR
; ( n1 n2 -- n3 ) De låga 16 bitarna av produkten.
STAR:   POP  HL
        PUSH DE
        EX   DE,HL          ; DE = n1
        LD   HL,0
        LD   A,16
STAR1:  ADD  HL,HL
        SLA  C
        RL   B
        JR   NC,STAR2
        ADD  HL,DE
STAR2:  DEC  A
        JR   NZ,STAR1
        POP  DE
        LD   B,H
        LD   C,L
        JP   NEXT

;W U/MOD UDIVM
; ( u1 u2 -- rest kvot ) Utan tecken. Med u2 = 0 blir kvoten $FFFF och
; resten u1.
UDIVM:  POP  HL             ; u1
        PUSH DE             ; IP
        LD   D,B
        LD   E,C            ; DE = u2
        LD   A,H
        LD   C,L            ; AC = u1, blir kvoten
        LD   HL,0           ; resten
        LD   B,16
UDIV1:  SLA  C
        RLA
        ADC  HL,HL
        JR   C,UDIV2        ; 17 bitar: större än u2
        SBC  HL,DE          ; CY = 0 här
        JR   NC,UDIV3
        ADD  HL,DE          ; gick inte: tillbaka
        JR   UDIV4
UDIV2:  CCF                 ; CY = 0
        SBC  HL,DE
UDIV3:  INC  C              ; en etta i kvoten
UDIV4:  DJNZ UDIV1
        POP  DE
        PUSH HL             ; resten
        LD   B,A
        JP   NEXT

;W DETOK XDETOK
; ( rad buf -- slut ) ROM:ens DETOK gör om BASIC-raden till text i buf;
; slut pekar efter radens CR. DETOK ändrar IX, returstacken.
XDETOK: POP  HL             ; raden
        PUSH DE
        PUSH IX
        LD   D,B
        LD   E,C
        CALL $1195          ; DETOK, samma adress i 9913 och 11273
        POP  IX
        LD   B,D
        LD   C,E
        POP  DE
        JP   NEXT

; NVSUB: nästa variabel i en BASIC-rads kod, från HL. A = 0: HL är
; början av ett element (radens kod börjar på raden + 3); A = 1: HL är
; inne i uttryckskoden (efter en variabel, v + 5). Z vid radens CR,
; annars NZ och HL = v, variabelns token. Elementen hoppas över med
; ROM:ens SKPELM och uttryckskoden token för token med SKPEXP, som i
; förlänkningen; båda på samma adress i 9913 och 11273. Ändrar alla
; register utom IX. MATCHVAR och SETVARS går igenom raden med den.
NVSUB:  OR   A
        JR   NZ,NVEX
NVEL:   LD   A,(HL)
        CP   $0D
        RET  Z              ; radens slut
        CP   $C0
        JR   NC,NVEX        ; ett uttryck börjar
        CALL $0EFB          ; SKPELM
        JR   NVEL
NVEX:   LD   A,(HL)
        SUB  $C0            ; variabel: $C0 + 4 * index + typ, typ 0-2
        CP   $0B
        JR   NC,NVSK
        AND  3
        CP   3
        RET  NZ             ; en variabel
NVSK:   CALL $10C3          ; SKPEXP; CY = uttrycket slut
        JR   NC,NVEX
        JR   NVEL

;W MATCHVAR MATCHVAR
; ( a m k -- v 1 | 0 0 ) Nästa förekomst av variabeln k i raden, från a
; med m som A i NVSUB; 0 0 vid radens CR (VAR namn och CHANGE). k har
; bokstaven i den höga byten och namnbyten i den låga, med bit 2 och
; inte bit 3 för index, som NB i fa.fs; antalet index räknas inte. Nästa
; förekomst letas från v + 5 med m = 1.
MATCHVAR: POP HL            ; m
        LD   A,L
        POP  HL             ; a
        PUSH DE
        PUSH BC             ; k
MVLP:   CALL NVSUB
        JR   Z,MVEND
        POP  BC
        PUSH BC
        INC  HL
        LD   A,(HL)         ; namnbyten
        AND  $0C
        LD   A,(HL)
        JR   Z,MV1
        AND  $F3
        OR   4              ; med index
MV1:    INC  HL
        CP   C
        JR   NZ,MVNX
        LD   A,(HL)
        CP   B
        JR   Z,MVHIT
MVNX:   INC  HL
        INC  HL
        INC  HL             ; v + 5
        LD   A,1
        JR   MVLP
MVHIT:  DEC  HL
        DEC  HL             ; v
        POP  BC
        POP  DE
        PUSH HL
        LD   BC,1
        JP   NEXT
MVEND:  POP  BC
        POP  DE
        LD   BC,0
        PUSH BC
        JP   NEXT

;W SEARCH SEARCH
; ( a n p m -- f ) Står texten p m någonstans i texten a n? m är 1-255.
; Varje läge i a jämförs byte för byte med p (FIND); på stacken under
; jämförelsen: läget, p, antalet lägen som är kvar och IP.
SEARCH: LD   A,C            ; m
        POP  BC             ; p
        POP  HL             ; n
        EX   (SP),HL        ; HL = a
        EX   DE,HL          ; DE = a, HL = IP
        EX   (SP),HL        ; HL = n, IP på stacken
        PUSH BC
        LD   C,A
        LD   B,0
        OR   A
        SBC  HL,BC
        INC  HL             ; n - m + 1 lägen
        EX   (SP),HL
        EX   DE,HL          ; HL = a, DE = p
        JR   C,SRNO         ; texten kortare än p
SRLP:   PUSH HL
        PUSH DE
        LD   B,C
SRCMP:  LD   A,(DE)
        CP   (HL)
        JR   NZ,SRNX
        INC  HL
        INC  DE
        DJNZ SRCMP
        POP  HL             ; träff
        POP  HL
        LD   BC,-1
        JR   SREND
SRNX:   POP  DE
        POP  HL
        INC  HL
        EX   (SP),HL
        DEC  HL
        LD   A,H
        OR   L
        EX   (SP),HL
        JR   NZ,SRLP
SRNO:   LD   BC,0
SREND:  POP  HL             ; lägena
        POP  DE
        JP   NEXT

;W SETVARS SETVARS
; ( rad -- ) Sätter biten i tabellen på heapen för varje variabel i
; raden (VAR utan namn).
SETVARS: LD  H,B
        LD   L,C
        INC  HL
        INC  HL
        INC  HL             ; radens kod
        PUSH DE
        XOR  A
SVLP:   CALL NVSUB
        JR   Z,SVEND
        PUSH HL
        LD   B,H
        LD   C,L
        CALL SETBIT
        POP  HL
        LD   BC,5
        ADD  HL,BC          ; v + 5
        LD   A,1
        JR   SVLP
SVEND:  POP  DE
        POP  BC
        JP   NEXT

; SETBIT: sätter variabeln v:s bit (BC = v); v + 1 är namnbyten och
; v + 2 bokstaven. Ändrar AF, BC, DE och HL. Biten är
; ((bokstav * 11 + siffra) * 3 + typ) * 2 + index, med siffran 0 utan
; siffra och annars 1-10, så att tabellen står i ordningen bokstav,
; siffra, typ och index: 26 * 11 * 3 * 2 = 1716 bitar i 215 byte.
SETBIT: INC  BC
        LD   A,(BC)
        LD   E,A            ; namnbyten
        INC  BC
        LD   A,(BC)
        SUB  'A'
        LD   L,A
        LD   H,0
        LD   B,H
        LD   C,L
        ADD  HL,HL
        ADD  HL,HL
        ADD  HL,BC
        ADD  HL,HL
        ADD  HL,BC          ; bokstaven * 11
        LD   A,E
        RRCA
        RRCA
        RRCA
        RRCA
        INC  A
        AND  15             ; $F (ingen siffra) blir 0
        LD   C,A
        ADD  HL,BC
        LD   B,H
        LD   C,L
        ADD  HL,HL
        ADD  HL,BC          ; * 3
        LD   A,E
        AND  3
        LD   C,A
        LD   B,0
        ADD  HL,BC
        ADD  HL,HL
        LD   A,E
        AND  $0C
        JR   Z,SETV1
        INC  HL             ; med index
SETV1:  LD   A,L
        AND  7
        LD   B,A
        INC  B
        ADD  HL,HL          ; * 32, så att H är bitens nummer / 8
        ADD  HL,HL
        ADD  HL,HL
        ADD  HL,HL
        ADD  HL,HL
        LD   E,H
        LD   D,0
        LD   HL,($FE20)     ; HEAP
        ADD  HL,DE
        LD   A,1
        JR   SETV3
SETV2:  ADD  A,A
SETV3:  DJNZ SETV2
        OR   (HL)
        LD   (HL),A
        RET

;W J JDX
; Den yttre DO-slingans index (som I, fyra byte längre upp).
JDX:    PUSH BC
        LD   C,(IX+4)
        LD   B,(IX+5)
        JP   NEXT

;W CMOVE CMOVE
; ( från till n -- ) n byte framåt, med LDIR; till får ligga före från.
CMOVE:  EX   DE,HL          ; HL = IP
        POP  DE             ; till
        EX   (SP),HL        ; HL = från, IP på stacken
        LD   A,B
        OR   C
        JR   Z,CMOV1
        LDIR
CMOV1:  POP  DE
        POP  BC
        JP   NEXT

;W ROMCALL ROMCALL
; ( a -- ) Anropa rutinen på a i ROM:en; DE och IX sparas.
ROMCALL: PUSH DE
        PUSH IX
        LD   HL,RCALL1
        PUSH HL
        LD   H,B
        LD   L,C
        JP   (HL)
RCALL1: POP  IX
        POP  DE
        POP  BC
        JP   NEXT

;W ENDPRG XENDP
; Tillbaka till BASIC som när ett program är slut: CLEAR och READY på
; $00C9, som skriver ABC80 och går till kommandoloopen. CALL:s variabel
; får inget värde.
XENDP:  JP   $00C9

;W (INS) XINS
; ( n -- f ) Raden i BASIC:s radbuffert $FE40, utan radnummer, görs om
; med ROM:ens TOKLIN, får numret n och läggs in med INSLIN, som i
; kommandoloopen. f = 0 om raden var tom. TOKLIN ligger på $188E i
; 9913 och $1890 i 11273; adressen läses ur kommandoloopens anrop på
; $0101. Ett fel går till BASIC, som i kommandoloopen.
XINS:   PUSH DE
        PUSH IX
        PUSH BC             ; n
        LD   HL,$FE40
        RST  $20            ; första tecknet som inte är blankt
        CP   $0D
        JR   Z,XINS2
        LD   BC,XINS1
        PUSH BC
        LD   BC,($0102)     ; TOKLIN
        PUSH BC
        RET
XINS1:  JP   C,$0669        ; ERRSTP
        POP  BC
        LD   (IX+1),C       ; numret
        LD   (IX+2),B
        CALL $0750          ; INSLIN
        JP   C,$062E        ; ERROR
        LD   BC,-1
        JR   XINS3
XINS2:  POP  BC
        LD   BC,0
XINS3:  POP  IX
        POP  DE
        JP   NEXT

;W I@ IFETCH
; ( -- n ) I-registret: $5F när kretsen är inkopplad.
IFETCH: PUSH BC
        LD   A,I
        LD   C,A
        LD   B,0
        JP   NEXT

;W I! ISTORE
; ( n -- ) I-registret, sidan med avbrottsvektorerna: $5F kopplar in
; kretsens tangentbordsrutin (fai.asm), 0 ROM:ens som förut.
ISTORE: LD   A,C
        LD   I,A
        POP  BC
        JP   NEXT

;W TOROM TOROM
; Raden i $FE40 till ROM:ens kommandoloop, som om den hade läst den
; själv: stacken som där, radbuffertens adress överst, in efter TSTBRK.
; Direktläget sätts som i kommandoloopen ($FE24 = 1), för ED vid OK
; i ett program.
TOROM:  LD   (IY+$0E),1
        LD   HL,($FE27)     ; STKTOP
        LD   SP,HL
        LD   HL,$FE40
        PUSH HL
        JP   $00F3

;W (START) XSTART
; ( rad runlin -- ) Programmet körs från raden rad som med RUN, men
; utan CLEAR: variablerna står kvar. Stacken som i RUN, med ROM:ens
; förlänkning PRELNK (som sätter programläget, och hittar variablerna
; som redan finns) och sedan RUNLIN, som ligger på $0D6F i 9913 och
; $0D71 i 11273 (fa.fs).
XSTART: POP  DE             ; raden
        LD   HL,($FE27)     ; STKTOP
        LD   SP,HL
        PUSH BC
        PUSH DE
        CALL $0E6E          ; PRELNK, samma adress i 9913 och 11273
        POP  DE
        POP  HL             ; RUNLIN
        LD   BC,$071B       ; som i RUN: RETURN utan GOSUB ger fel
        PUSH BC
        JP   (HL)

;W CMD? CMDQ
; ( h -- xt | 0 ) Huvudet h (från LOOKUP, 0 = finns inte) ger ordets
; adress om ordet är ett av kommandona som går direkt i BASIC.
CMDQ:   LD   A,B
        OR   C
        JP   Z,NEXT
        INC  BC
        INC  BC
        LD   A,(BC)         ; namnets längd
        AND  $7F
        INC  A
        ADD  A,C
        LD   C,A
        JR   NC,CMDQ1
        INC  B              ; BC = ordets adress
CMDQ1:  LD   HL,CMDTAB
CMDQ2:  LD   A,(HL)
        INC  HL
        CP   C
        LD   A,(HL)
        INC  HL
        JR   NZ,CMDQ3
        CP   B
        JP   Z,NEXT
CMDQ3:  OR   A              ; tabellens slut (inget ord på $00xx)
        JR   NZ,CMDQ2
        LD   BC,0
        JP   NEXT
CMDTAB: DEFW F_SYS,F_LIST,F_PEEK,F_FIND,F_VAR,F_DEL,F_AUTO,F_OLD
        DEFW F_CHANGE,F_REN,F_ED,F_START,F_HELP,F_FORTH,F_EX,0

;W EDIT EDIT
; ( a n -- n2 ) Som ACCEPT, men med kretsens editor (EDLIN): markören
; går fritt över skärmen, och en rad därifrån kan läsas in igen.
EDIT:   POP  HL
        PUSH HL
        PUSH DE
        CALL EDLIN          ; HL = CR:en efter raden
        POP  DE
        POP  BC
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

; EDLIN: en rad till HL, högst C tecken, med CR sist. Som ROM:ens
; INLINE ($02AA) och med dess rutiner; nedre delen av ROM:en ($0000-
; $03FF) är densamma i 11273 och 9913. GETKEY visar markören och ger
; tecknet i A, markörens adress i DE och IX = $FDF3 (raden, kolumnen).
;   högerpilen   tecknet under markören läses in i raden
;   vänsterpilen det sista tecknet tas bort, CTRL-X alla
;   CTRL-W A S Z markören upp, vänster, höger, ned
;   CTRL-Q       markören till övre vänstra hörnet
;   CTRL-L       skärmen rensas
;   CTRL-D       tecknet under markören tas bort ur skärmraden
;   CTRL-P       ett blanktecken sätts in i skärmraden
EGETKY: EQU  $02EA
ERUB:   EQU  $035B
ECRLF:  EQU  $0234
ECLS:   EQU  $0276
EDLIN:  PUSH IX
        LD   B,C
ED0:    CALL EGETKY
        CP   $0D
        JR   Z,EDCR
        CP   $09            ; högerpilen
        JR   NZ,ED1
        LD   A,(DE)
        JR   EDPUT
ED1:    CP   $08            ; vänsterpilen
        JR   Z,EDBS
        CP   $18            ; CTRL-X
        JR   Z,EDX
        CP   $17            ; CTRL-W
        JR   Z,EDUP
        CP   $01            ; CTRL-A
        JR   Z,EDLEFT
        CP   $13            ; CTRL-S
        JR   Z,EDRGT
        CP   $1A            ; CTRL-Z
        JR   Z,EDDOWN
        CP   $11            ; CTRL-Q
        JR   Z,EDHOME
        CP   $0C            ; CTRL-L
        JR   Z,EDCLS
        CP   $04            ; CTRL-D
        JR   Z,EDDEL
        CP   $10            ; CTRL-P
        JR   Z,EDINS
        CP   $20            ; andra styrtecken gör ingenting
        JR   C,ED0
EDPUT:  LD   (HL),A         ; in i raden och på skärmen, som INLINE
        INC  HL
        LD   (DE),A
        INC  (IX+1)
        LD   A,(IX+1)
        CP   40
        CALL NC,ECRLF
        DJNZ ED0
        DEC  HL
EDCR:   LD   (HL),$0D
        POP  IX
        RET
EDBS:   CALL ERUB
        JR   ED0
EDX:    CALL ERUB
        JR   NZ,EDX
        JR   ED0
ED9:    JR   ED0            ; JR når inte ED0 längre ned
EDUP:   DEC  (IX+0)
        JP   P,ED9
        INC  (IX+0)
        JR   ED9
EDLEFT: DEC  (IX+1)
        JP   P,ED9
        INC  (IX+1)
        JR   ED9
EDRGT:  LD   A,(IX+1)
        CP   39
        JR   NC,ED9
        INC  (IX+1)
        JR   ED9
EDDOWN: LD   A,(IX+0)
        CP   23
        JR   NC,ED9
        INC  (IX+0)
        JR   ED9
EDHOME: XOR  A
        LD   (IX+0),A
        LD   (IX+1),A
        JR   ED9
EDCLS:  CALL ECLS
        JR   ED9
; CTRL-D och CTRL-P flyttar resten av skärmraden, 39 - kolumnen tecken.
EDDEL:  PUSH HL
        PUSH BC
        CALL EDREST
        JR   Z,EDSP
        LD   H,D
        LD   L,E
        INC  HL
        LDIR                ; DE = radens sista tecken
        JR   EDSP
EDINS:  PUSH HL
        PUSH BC
        CALL EDREST
        JR   Z,EDSP
        EX   DE,HL
        ADD  HL,BC          ; radens sista tecken
        LD   D,H
        LD   E,L
        DEC  HL
        LDDR                ; DE = markören
EDSP:   LD   A,' '
        LD   (DE),A
        POP  BC
        POP  HL
        JR   ED9
EDREST: LD   A,39           ; BC = 39 - kolumnen, Z om 0
        SUB  (IX+1)
        LD   C,A
        LD   B,0
        RET

;W (LOOKUP) ILOOK
; ( a n h -- h' | 0 ) Som kärnans (LOOKUP), som gor.sh tar bort ur
; kopian av kärnan, men tecknen jämförs utan
; skillnad på versaler och gemener: $61-$7E (a-z och ä ö å ü) blir
; $41-$5E i båda namnen före jämförelsen.
ILOOK:  POP  HL             ; n
        EX   (SP),HL        ; HL = a, n på stacken
        EX   DE,HL          ; DE = a, HL = IP
        EX   (SP),HL        ; HL = n, IP på stacken
        PUSH HL             ; n överst
IL1:    LD   A,B
        OR   C
        JR   Z,IL9          ; listans slut, BC = 0
        LD   H,B
        LD   L,C
        INC  HL
        INC  HL
        LD   A,(HL)
        AND  $7F            ; namnets längd
        EX   (SP),HL
        CP   L              ; = n?
        EX   (SP),HL
        JR   NZ,IL3
        PUSH BC
        PUSH DE
        LD   B,A
IL2:    INC  HL             ; jämför tecken för tecken
        LD   A,(HL)
        CALL UPPER
        LD   C,A
        LD   A,(DE)
        CALL UPPER
        CP   C
        JR   NZ,IL4
        INC  DE
        DJNZ IL2
        POP  DE             ; lika: BC = huvudet
        POP  BC
        JR   IL9
IL4:    POP  DE
        POP  BC
IL3:    LD   H,B            ; nästa huvud
        LD   L,C
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JR   IL1
IL9:    POP  HL
        POP  DE
        JP   NEXT
UPPER:  CP   $61            ; A = tecknet i versal
        RET  C
        CP   $7F
        RET  NC
        SUB  $20
        RET
