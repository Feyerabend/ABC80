; ztolk.asm -- ZTOLK, en tolk för Z-maskinen version 3 i CP/M på ABC80
; med UNI-80, till boken ABC80 inifrån
;
;     ZTOLK fil[.typ]
;
; Spelfilen är en Z-maskinfil i version 3, som Infocoms Zork I; typen
; är Z3 om ingen ges (ZTOLK D:ZORK1). Tolken är skriven efter The
; Z-Machine Standards Document 1.1 (Graham Nelson m.fl.).
;
; Minnet. Spelfilen läses i sidor om 256 bytes. Det dynamiska minnet
; (sidorna under headerns statiska bas) ligger i en följd från DYN och
; stannar där; sedan fylls resten av minnet upp till BDOS:en med så
; många sidor som får plats. Får inte hela filen plats lånas platserna
; ovanför det dynamiska minnet ut i tur och ordning (först in, först
; ut), men aldrig den sida som programräknaren står i. SIDKARTA säger
; för varje sida i filen var den ligger (sidans höga byte, 0 om den
; inte är inläst), och SLOTLAG och SLOTHOG säger för varje plats vilken
; sida som ligger där. En adress i filen har 17 bitar: C:HL.
;
; Programräknaren hålls som en pekare i minnet (PCPEK) och sidans
; nummer (PCSIDA), så att en byte ur koden bara kostar en läsning så
; länge sidan inte tar slut.
;
; Stacken. Z-maskinens stack (ZSTACK) är skild från Z80:ns. En
; anropsram är: gammal FP (ord), returadressens låga 16 bitar (ord),
; dess bit 16 (byte), variabeln som får svaret (byte), antalet lokala
; variabler (ord) och de lokala variablerna; sedan följer det som
; läggs på stacken. Orden ligger med den låga byten först, men i
; spelets minne ligger de med den höga först, som Z-maskinen vill.
;
; Skärmen sköts genom BIOS:en (cpm/bios.asm) direkt: rad 0 är
; statusraden (platsen, poängen och dragen), rad 1-23 texten. När
; texten når rad 23 tas rad 1 bort med ESC R, så att statusraden står
; kvar. Orden bryts vid skärmens bredd, och (MER) väntar på en tangent
; när en skärm har gått förbi utan att spelaren har skrivit något.
;
; Makefile sätter först in BIOS:ens KOLUMNER (40 eller 80) ur
; cpm/bios.lst, som för SKRIV.
;
; Utelämnat: delade fönster, ljud, utskrift till skrivare och
; minne (output_stream) och input_stream; de gör ingenting.

BDOS      EQU  5
FCB       EQU  $5C              ; filnamnet som CCP har tolkat
CR        EQU  13
LF        EQU  10
ESC       EQU  27
ZSTACKSTORLEK EQU 2048
ORDMAX    EQU  80               ; längsta ord som väntar på utskrift
TEXTRADER EQU  22               ; rader innan (MER)
KOSTORLEK EQU  32               ; tangentkön (en potens av 2)

          ORG  $100

START:    LD   SP,STACK
          LD   HL,(1)           ; BIOS:ens WBOOT; CONST, CONIN och CONOUT
          LD   DE,3             ; följer efter den
          ADD  HL,DE
          LD   (BIOS_KONST+1),HL
          ADD  HL,DE
          LD   (BIOS_KONIN+1),HL
          ADD  HL,DE
          LD   (BIOS_KONUT+1),HL
          LD   A,(KOLUMNER)     ; sista kolumnen lämnas tom, annars går
          DEC  A                ; BIOS:en till nästa rad av sig själv
          LD   (BREDD),A
          LD   A,(7)            ; BDOS:ens sida; platserna slutar under den
          LD   (TOPP),A
          LD   A,(FCB+1)
          CP   ' '
          LD   HL,T_ANVANDNING
          JP   Z,FEL
          LD   HL,FCB           ; spelfilens FCB, med typen Z3 om ingen
          LD   DE,SPELFCB       ; är given
          LD   BC,12
          LDIR
          LD   HL,SPELFCB+12
          LD   B,24
NOLLFCB:  LD   (HL),0
          INC  HL
          DJNZ NOLLFCB
          LD   A,(SPELFCB+9)
          CP   ' '
          JR   NZ,TYP_KLAR
          LD   HL,'Z'|('3'<<8)
          LD   (SPELFCB+9),HL
TYP_KLAR: LD   DE,SPELFCB
          LD   C,15             ; öppna
          CALL BDOS
          INC  A
          LD   HL,T_HITTAR_INTE
          JP   Z,FEL

          LD   HL,SIDKARTA      ; inga sidor inlästa
          LD   B,0
TOMKARTA: LD   (HL),0
          INC  HL
          LD   (HL),0
          INC  HL
          DJNZ TOMKARTA
          LD   HL,SLOTHOG       ; inga platser upptagna
TOMSLOT:  LD   (HL),$FF
          INC  L
          JR   NZ,TOMSLOT

          LD   DE,0             ; headern
          LD   A,DYNHI
          CALL LASSIDA
          LD   A,(DYN)
          CP   3
          LD   HL,T_VERSION
          JP   NZ,FEL
          LD   HL,(DYN+$0E)     ; statiska basen: det dynamiska minnet
          CALL VAND             ; slutar där
          LD   (STATBAS),HL
          LD   DE,255
          ADD  HL,DE
          LD   A,H
          LD   (DYNSIDOR),A
          ADD  A,DYNHI
          LD   (FORSTA),A
          LD   (NASTA),A
          ADD  A,2              ; minst två platser till de andra sidorna
          JR   C,FOR_LITE
          LD   B,A
          LD   A,(TOPP)
          CP   B
          JR   NC,MINNE_OK
FOR_LITE: LD   HL,T_MINNE
          JP   FEL
MINNE_OK: LD   HL,(DYN+$1A)     ; filens längd / 2
          CALL VAND
          LD   A,0
          ADD  HL,HL
          RLA
          LD   (FILLANGD),HL
          LD   (FILLANGD+2),A
          LD   DE,255           ; antalet sidor
          ADD  HL,DE
          ADC  A,0
          LD   L,H
          LD   H,A
          LD   (FILSIDOR),HL

          LD   DE,0             ; fyll minnet med filens början
FYLL:     LD   HL,(FILSIDOR)
          OR   A
          SBC  HL,DE
          JR   Z,FYLLT
          LD   A,E
          ADD  A,DYNHI
          LD   B,A
          LD   A,(TOPP)
          CP   B
          JR   Z,FYLLT
          LD   A,B
          CALL LASSIDA
          LD   A,B
          CALL BOKFOR
          INC  DE
          JR   FYLL
FYLLT:
          LD   HL,(DYN+$0C)     ; de globala variablerna
          CALL VAND
          LD   A,H
          ADD  A,DYNHI
          LD   H,A
          LD   (GLOBFYS),HL
          LD   HL,(DYN+$0A)     ; objekten: först 31 standardvärden,
          CALL VAND             ; sedan objekt 1 med 9 bytes vart
          LD   A,H
          ADD  A,DYNHI
          LD   H,A
          LD   (OBJSTD),HL
          LD   DE,62-9
          ADD  HL,DE
          LD   (OBJBAS),HL
          LD   HL,(DYN+$18)     ; förkortningarna
          CALL VAND
          LD   (FORKTAB),HL

          LD   HL,(DYN+$08)     ; ordlistan: avgränsarna, postens längd,
          CALL VAND             ; antalet poster och posterna
          LD   C,0
          CALL LASBINC
          AND  31
          LD   (SEPAR),A
          LD   DE,SEPAR+1
          OR   A
          JR   Z,SEP_KLAR
          LD   B,A
SEP_LAS:  CALL LASBINC
          LD   (DE),A
          INC  DE
          DJNZ SEP_LAS
SEP_KLAR: CALL LASBINC
          LD   (POSTLANGD),A
          CALL LASORDINC
          LD   (POSTANTAL),DE
          LD   (POSTSTART),HL

; Början, och omstarten (restart) när minnet är inläst igen.
OMSTART:  LD   SP,STACK
          LD   C,26             ; töm skärmen och börja på rad 1
          CALL KONUT
          LD   A,1
          LD   (ZRAD),A
          XOR  A
          LD   (KOLUMN),A
          LD   (RADER),A
          LD   (ORDLANGD),A
          LD   (STATUSLAGE),A
          LD   B,1
          LD   C,0
          CALL GATILL
          LD   HL,ZSTACK        ; huvudrutinens ram, utan lokala variabler
          LD   (FP),HL
          LD   B,8
NOLLRAM:  LD   (HL),0
          INC  HL
          DJNZ NOLLRAM
          LD   (ZSP),HL
          LD   HL,(DYN+$06)     ; första instruktionen
          CALL VAND
          LD   C,0
          CALL SETPC
          LD   A,1
          LD   (IGANG),A
SLINGA:   CALL STEG
          LD   HL,POLLRAKNARE   ; var 64:e instruktion: en tangent till
          DEC  (HL)             ; kön, också när spelet räknar länge
          JR   NZ,SLINGA        ; utan att skriva något
          LD   (HL),64
          CALL POLLA
          JR   SLINGA

; HL med bytena i omvänd ordning (ett ord ur spelets minne).
VAND:     LD   A,L
          LD   L,H
          LD   H,A
          RET

; ---------------------------------------------------------------------
; Sidorna

; Läs sidan DE i filen till platsen A (sidans höga byte).
LASSIDA:  PUSH BC
          PUSH DE
          PUSH HL
          LD   (LASPLATS),A
          EX   DE,HL            ; posten = sidan * 2
          ADD  HL,HL
          LD   (SPELFCB+33),HL
          XOR  A
          LD   (SPELFCB+35),A
          CALL LASPOST
          CALL POLLA            ; tangenter som kommer under läsningen
          LD   HL,(SPELFCB+33)
          INC  HL
          LD   (SPELFCB+33),HL
          LD   A,(LASPLATS)
          LD   D,A
          LD   E,$80
          CALL LASPOST2
          CALL POLLA
          POP  HL
          POP  DE
          POP  BC
          RET
LASPOST:  LD   A,(LASPLATS)
          LD   D,A
          LD   E,0
LASPOST2: LD   C,26             ; DMA
          CALL BDOS
          LD   DE,SPELFCB
          LD   C,33             ; läs posten (efter filens slut blir
          JP   BDOS             ; den som den var)

; Skriv i kartan att sidan DE ligger på platsen A.
BOKFOR:   PUSH HL
          PUSH BC
          LD   B,A
          LD   HL,SIDKARTA
          ADD  HL,DE
          LD   (HL),B
          LD   L,B
          LD   H,SLOTLAG>>8
          LD   (HL),E
          LD   H,SLOTHOG>>8
          LD   (HL),D
          LD   A,B
          POP  BC
          POP  HL
          RET

; Sidan HL i filen: A blir platsen den ligger på. Bevarar BC, DE, HL.
SIDFYS:   PUSH DE
          PUSH HL
          EX   DE,HL
          LD   HL,SIDKARTA
          ADD  HL,DE
          LD   A,(HL)
          OR   A
          CALL Z,LADDA
          POP  HL
          POP  DE
          RET

; Läs in sidan DE på nästa plats i tur, men inte på programräknarens.
LADDA:    PUSH BC
LADDA_IGEN:
          LD   A,(NASTA)
          LD   B,A
          INC  A
          LD   HL,TOPP
          CP   (HL)
          JR   C,LADDA_NASTA
          LD   A,(FORSTA)
LADDA_NASTA:
          LD   (NASTA),A
          LD   A,(PCPEK+1)
          CP   B
          JR   Z,LADDA_IGEN
          LD   L,B              ; sidan som låg där finns inte längre
          LD   H,SLOTHOG>>8
          LD   A,(HL)
          CP   $FF
          JR   Z,LADDA_TOM
          PUSH DE
          LD   D,A
          LD   H,SLOTLAG>>8
          LD   E,(HL)
          LD   HL,SIDKARTA
          ADD  HL,DE
          LD   (HL),0
          POP  DE
LADDA_TOM:
          LD   A,B
          CALL LASSIDA
          LD   A,B
          CALL BOKFOR
          POP  BC
          RET

; Byten på adressen C:HL i filen till A. Bevarar BC, DE, HL.
LASB:     PUSH HL
          PUSH DE
          LD   E,L
          LD   L,H
          LD   H,C
          CALL SIDFYS
          LD   H,A
          LD   L,E
          LD   A,(HL)
          POP  DE
          POP  HL
          RET

; Som LASB, och C:HL ökar med ett.
LASBINC:  CALL LASB
          INC  L
          RET  NZ
          INC  H
          RET  NZ
          INC  C
          RET

; Ordet på C:HL till DE, och C:HL ökar med två.
LASORDINC:
          CALL LASBINC
          LD   D,A
          CALL LASBINC
          LD   E,A
          RET

; Adressen HL i det dynamiska minnet till en pekare i HL.
DYNADR:   PUSH DE
          PUSH HL
          LD   DE,(STATBAS)
          OR   A
          SBC  HL,DE
          POP  HL
          POP  DE
          JR   NC,DYNFEL
          LD   A,H
          ADD  A,DYNHI
          LD   H,A
          RET
DYNFEL:   LD   HL,T_DYNAMISKT
          JP   FEL

; ---------------------------------------------------------------------
; Programräknaren

; Nästa byte i koden till A. Bevarar BC och DE.
PCBYTE:   LD   HL,(PCPEK)
          LD   A,(HL)
          INC  L
          JR   Z,PCNYSIDA
          LD   (PCPEK),HL
          RET
PCNYSIDA: PUSH AF
          LD   HL,(PCSIDA)
          INC  HL
          LD   (PCSIDA),HL
          CALL SIDFYS
          LD   H,A
          LD   L,0
          LD   (PCPEK),HL
          POP  AF
          RET

; Nästa ord i koden till HL. Bevarar BC och DE.
PCORD:    CALL PCBYTE
          PUSH AF
          CALL PCBYTE
          LD   L,A
          POP  AF
          LD   H,A
          RET

; Programräknaren till C:HL.
HAMTAPC:  LD   HL,(PCSIDA)
          LD   C,H
          LD   H,L
          LD   A,(PCPEK)
          LD   L,A
          RET

; Programräknaren blir C:HL. Bevarar BC.
SETPC:    LD   E,L
          LD   L,H
          LD   H,C
          LD   (PCSIDA),HL
          CALL SIDFYS
          LD   H,A
          LD   L,E
          LD   (PCPEK),HL
          RET

; Hoppa DE bytes relativt (som i jump och grenar: PC + DE - 2).
HOPPAREL: DEC  DE
          DEC  DE
          PUSH DE
          CALL HAMTAPC
          POP  DE
          ADD  HL,DE
          LD   A,C
          ADC  A,0
          BIT  7,D
          JR   Z,HOPPA_POS
          DEC  A
HOPPA_POS:
          AND  1
          LD   C,A
          JP   SETPC

; ---------------------------------------------------------------------
; Variablerna

; Variabeln A till HL. Bevarar BC och DE.
LASVAR:   OR   A
          JR   Z,ZPOP
          CP   16
          JR   NC,LASGLOB
          PUSH DE
          ADD  A,A
          ADD  A,6
          LD   E,A
          LD   D,0
          LD   HL,(FP)
          ADD  HL,DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          EX   DE,HL
          POP  DE
          RET
LASGLOB:  PUSH DE
          SUB  16
          LD   L,A
          LD   H,0
          ADD  HL,HL
          LD   DE,(GLOBFYS)
          ADD  HL,DE
          LD   D,(HL)
          INC  HL
          LD   E,(HL)
          EX   DE,HL
          POP  DE
          RET

; Det översta på Z-stacken till HL. Bevarar BC och DE.
ZPOP:     PUSH DE
          LD   HL,(ZSP)
          DEC  HL
          LD   D,(HL)
          DEC  HL
          LD   E,(HL)
          LD   (ZSP),HL
          EX   DE,HL
          POP  DE
          RET

; HL på Z-stacken. Bevarar BC, DE och HL.
ZPUSH:    PUSH DE
          EX   DE,HL
          LD   HL,(ZSP)
          LD   (HL),E
          INC  HL
          LD   (HL),D
          INC  HL
          LD   (ZSP),HL
          PUSH DE
          LD   DE,ZSTACKSLUT
          OR   A
          SBC  HL,DE
          POP  HL
          POP  DE
          RET  C
          LD   HL,T_STACK
          JP   FEL

; HL till variabeln A. Bevarar BC, DE och HL.
SKRIVVAR: OR   A
          JR   Z,ZPUSH
          PUSH DE
          PUSH HL
          EX   DE,HL
          CP   16
          JR   NC,SKRIVGLOB
          ADD  A,A
          ADD  A,6
          LD   L,A
          LD   H,0
          PUSH BC
          LD   BC,(FP)
          ADD  HL,BC
          POP  BC
          LD   (HL),E
          INC  HL
          LD   (HL),D
          POP  HL
          POP  DE
          RET
SKRIVGLOB:
          SUB  16
          LD   L,A
          LD   H,0
          ADD  HL,HL
          PUSH BC
          LD   BC,(GLOBFYS)
          ADD  HL,BC
          POP  BC
          LD   (HL),D
          INC  HL
          LD   (HL),E
          POP  HL
          POP  DE
          RET

; Som LASVAR och SKRIVVAR, men variabel 0 är det översta på stacken,
; som stannar där (load, store, inc, dec, inc_chk, dec_chk och pull).
LASVARIND:
          OR   A
          JP   NZ,LASVAR
          PUSH DE
          LD   HL,(ZSP)
          DEC  HL
          LD   D,(HL)
          DEC  HL
          LD   E,(HL)
          EX   DE,HL
          POP  DE
          RET
SKRIVVARIND:
          OR   A
          JP   NZ,SKRIVVAR
          PUSH DE
          PUSH HL
          EX   DE,HL
          LD   HL,(ZSP)
          DEC  HL
          LD   (HL),D
          DEC  HL
          LD   (HL),E
          POP  HL
          POP  DE
          RET

; ---------------------------------------------------------------------
; Instruktionerna

; Läs operanderna efter typbyten A (två bitar var: 00 ett ord, 01 en
; byte, 10 en variabel, 11 ingen) till OPS, och antalet till OPANTAL.
OPERANDER:
          LD   C,A
          LD   HL,OPS
          LD   (OPPEK),HL
          XOR  A
          LD   (OPANTAL),A
          LD   B,4
OP_SLINGA:
          LD   A,C
          AND  $C0
          CP   $C0
          RET  Z
          PUSH BC
          CP   $40
          JR   Z,OP_LITEN
          CP   $80
          JR   Z,OP_VAR
          CALL PCORD
          JR   OP_SPARA
OP_LITEN: CALL PCBYTE
          LD   L,A
          LD   H,0
          JR   OP_SPARA
OP_VAR:   CALL PCBYTE
          CALL LASVAR
OP_SPARA: EX   DE,HL
          LD   HL,(OPPEK)
          LD   (HL),E
          INC  HL
          LD   (HL),D
          INC  HL
          LD   (OPPEK),HL
          LD   HL,OPANTAL
          INC  (HL)
          POP  BC
          SLA  C
          SLA  C
          DJNZ OP_SLINGA
          RET

; En instruktion: läs den och hoppa till den. Instruktionen återvänder
; med RET till SLINGA.
STEG:     CALL PCBYTE
          LD   (OPKOD),A
          CP   $80
          JR   C,LANGFORM
          CP   $C0
          JR   NC,VARFORM
          AND  $30              ; kort form: en operand eller ingen
          CP   $30
          JR   Z,NOLLOP
          ADD  A,A
          ADD  A,A
          OR   $3F
          CALL OPERANDER
          LD   A,(OPKOD)
          AND  $0F
          LD   HL,TAB1OP
          JR   HOPPA
NOLLOP:   LD   A,(OPKOD)
          AND  $0F
          LD   HL,TAB0OP
          JR   HOPPA
LANGFORM: LD   B,$5F            ; två små konstanter, eller variabler
          BIT  6,A
          JR   Z,LANG1
          LD   B,$9F
LANG1:    BIT  5,A
          JR   Z,LANG2
          LD   A,B
          XOR  $30
          LD   B,A
LANG2:    LD   A,B
          CALL OPERANDER
          LD   A,(OPKOD)
          AND  $1F
          LD   HL,TAB2OP
          JR   HOPPA
VARFORM:  CALL PCBYTE
          CALL OPERANDER
          LD   A,(OPKOD)
          LD   HL,TABVAR
          BIT  5,A
          JR   NZ,VARFORM2
          LD   HL,TAB2OP
VARFORM2: AND  $1F
HOPPA:    ADD  A,A
          LD   E,A
          LD   D,0
          ADD  HL,DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          EX   DE,HL
          JP   (HL)

TAB2OP:   DEFW OGILTIG,JE,JL,JG,DEC_CHK,INC_CHK,JIN,TEST
          DEFW OR_,AND_,TEST_ATTR,SET_ATTR,CLEAR_ATTR,STORE,INSERT_OBJ,LOADW
          DEFW LOADB,GET_PROP,GET_PROP_ADDR,GET_NEXT_PROP,ADD_,SUB_,MUL_,DIV_
          DEFW MOD_,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG
TAB1OP:   DEFW JZ,GET_SIBLING,GET_CHILD,GET_PARENT,GET_PROP_LEN,INC_,DEC_,PRINT_ADDR
          DEFW OGILTIG,REMOVE_OBJ,PRINT_OBJ,RET_,JUMP,PRINT_PADDR,LOAD,NOT_
TAB0OP:   DEFW RTRUE,RFALSE,PRINT,PRINT_RET,INGET,SAVE,RESTORE,RESTART
          DEFW RET_POPPED,POP_,QUIT,NEW_LINE,SHOW_STATUS,VERIFY,OGILTIG,GRENSANT
TABVAR:   DEFW CALL_,STOREW,STOREB,PUT_PROP,SREAD,PRINT_CHAR,PRINT_NUM,RANDOM
          DEFW PUSH_,PULL,INGET,INGET,OGILTIG,OGILTIG,OGILTIG,OGILTIG
          DEFW OGILTIG,OGILTIG,OGILTIG,INGET,INGET,INGET,OGILTIG,OGILTIG
          DEFW OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG,OGILTIG

OGILTIG:  LD   HL,T_OGILTIG
          JP   FEL
INGET:    RET

; Spara HL i variabeln som följer i koden.
LAGRA:    PUSH HL
          CALL PCBYTE
          POP  HL
          JP   SKRIVVAR

; Gren: läs grenbyten (och en byte till om offseten är lång) och hoppa
; om villkoret (B: 1 sant, 0 falskt) är det som grenen väntar sig.
; Offset 0 och 1 återvänder från rutinen med falskt och sant.
GRENSANT: LD   B,1
          JR   GREN
GRENFALSK:
          LD   B,0
GREN:     CALL PCBYTE
          LD   C,A
          LD   E,A
          LD   D,0
          BIT  6,A
          JR   NZ,GREN_KORT
          AND  $3F              ; 14 bitar med tecken
          BIT  5,A
          JR   Z,GREN_POS
          OR   $C0
GREN_POS: LD   D,A
          CALL PCBYTE
          LD   E,A
          JR   GREN_VILLKOR
GREN_KORT:
          LD   A,E
          AND  $3F
          LD   E,A
GREN_VILLKOR:
          LD   A,C
          RLCA
          AND  1
          XOR  B
          RET  NZ
          LD   A,D
          OR   A
          JP   NZ,HOPPAREL
          LD   A,E
          CP   2
          JP   NC,HOPPAREL
          LD   L,A
          LD   H,0
          JP   RETUR

; ---------------------------------------------------------------------
; Anrop och retur

CALL_:    LD   HL,(OP1)
          LD   A,H
          OR   L
          JP   Z,LAGRA          ; rutin 0 ger falskt
          CALL PCBYTE
          LD   (ANRLAGRA),A
          LD   HL,(ZSP)         ; plats för ramen och 15 lokala
          LD   DE,8+30
          ADD  HL,DE
          LD   DE,ZSTACKSLUT
          OR   A
          SBC  HL,DE
          LD   HL,T_STACK
          JP   NC,FEL
          CALL HAMTAPC
          LD   A,C
          LD   (ANRHOG),A
          EX   DE,HL
          LD   HL,(ZSP)
          PUSH HL
          LD   BC,(FP)
          LD   (HL),C
          INC  HL
          LD   (HL),B
          INC  HL
          LD   (HL),E
          INC  HL
          LD   (HL),D
          INC  HL
          LD   A,(ANRHOG)
          LD   (HL),A
          INC  HL
          LD   A,(ANRLAGRA)
          LD   (HL),A
          INC  HL
          LD   (ANRPEK),HL
          POP  HL
          LD   (FP),HL
          LD   HL,(OP1)         ; rutinens adress är 2 * operanden
          ADD  HL,HL
          LD   C,0
          RL   C
          CALL SETPC
          CALL PCBYTE           ; antalet lokala variabler
          AND  $0F
          LD   B,A
          LD   HL,(ANRPEK)
          LD   (HL),A
          INC  HL
          LD   (HL),0
          INC  HL
          LD   (ANRPEK),HL
          LD   HL,OP2
          LD   (ARGPEK),HL
          LD   A,(OPANTAL)
          DEC  A
          LD   (ARGKVAR),A
          LD   A,B
          OR   A
          JR   Z,ANR_KLAR
ANR_LOKAL:
          CALL PCORD            ; startvärdet, om inget argument ges
          LD   A,(ARGKVAR)
          OR   A
          JR   Z,ANR_STD
          DEC  A
          LD   (ARGKVAR),A
          LD   HL,(ARGPEK)
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          INC  HL
          LD   (ARGPEK),HL
          EX   DE,HL
ANR_STD:  EX   DE,HL
          LD   HL,(ANRPEK)
          LD   (HL),E
          INC  HL
          LD   (HL),D
          INC  HL
          LD   (ANRPEK),HL
          DJNZ ANR_LOKAL
ANR_KLAR: LD   HL,(ANRPEK)
          LD   (ZSP),HL
          RET

; Återvänd från rutinen med HL.
RETUR:    PUSH HL
          LD   HL,(FP)
          LD   DE,ZSTACK
          OR   A
          SBC  HL,DE
          LD   HL,T_RETUR
          JP   Z,FEL
          LD   HL,(FP)
          LD   (ZSP),HL
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          INC  HL
          PUSH DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          INC  HL
          LD   C,(HL)
          INC  HL
          LD   A,(HL)
          LD   (ANRLAGRA),A
          EX   DE,HL
          CALL SETPC
          POP  HL
          LD   (FP),HL
          POP  HL
          LD   A,(ANRLAGRA)
          JP   SKRIVVAR

RTRUE:    LD   HL,1
          JP   RETUR
RFALSE:   LD   HL,0
          JP   RETUR
RET_:     LD   HL,(OP1)
          JP   RETUR
RET_POPPED:
          CALL ZPOP
          JP   RETUR
JUMP:     LD   DE,(OP1)
          JP   HOPPAREL

; ---------------------------------------------------------------------
; Jämförelser och räkning

JE:       LD   A,(OPANTAL)
          DEC  A
          JP   Z,GRENFALSK
          LD   B,A
          LD   HL,OP2
          LD   DE,(OP1)
JE_SLINGA:
          LD   A,(HL)
          INC  HL
          CP   E
          JR   NZ,JE_NASTA
          LD   A,(HL)
          CP   D
          JP   Z,GRENSANT
JE_NASTA: INC  HL
          DJNZ JE_SLINGA
          JP   GRENFALSK

; Bär om HL < DE med tecken.
MINDRE:   LD   A,H
          XOR  $80
          LD   H,A
          LD   A,D
          XOR  $80
          LD   D,A
          OR   A
          SBC  HL,DE
          RET

JL:       LD   HL,(OP1)
          LD   DE,(OP2)
JL2:      CALL MINDRE
          JP   C,GRENSANT
          JP   GRENFALSK
JG:       LD   HL,(OP2)
          LD   DE,(OP1)
          JR   JL2

DEC_CHK:  LD   A,(OP1)
          CALL LASVARIND
          DEC  HL
          LD   A,(OP1)
          CALL SKRIVVARIND
          LD   DE,(OP2)
          JR   JL2
INC_CHK:  LD   A,(OP1)
          CALL LASVARIND
          INC  HL
          LD   A,(OP1)
          CALL SKRIVVARIND
          EX   DE,HL
          LD   HL,(OP2)
          JR   JL2

INC_:     LD   A,(OP1)
          CALL LASVARIND
          INC  HL
          LD   A,(OP1)
          JP   SKRIVVARIND
DEC_:     LD   A,(OP1)
          CALL LASVARIND
          DEC  HL
          LD   A,(OP1)
          JP   SKRIVVARIND

JZ:       LD   HL,(OP1)
          LD   A,H
          OR   L
          JP   Z,GRENSANT
          JP   GRENFALSK

TEST:     LD   HL,(OP1)
          LD   DE,(OP2)
          LD   A,H
          AND  D
          CP   D
          JP   NZ,GRENFALSK
          LD   A,L
          AND  E
          CP   E
          JP   NZ,GRENFALSK
          JP   GRENSANT

OR_:      LD   HL,(OP1)
          LD   DE,(OP2)
          LD   A,H
          OR   D
          LD   H,A
          LD   A,L
          OR   E
          LD   L,A
          JP   LAGRA
AND_:     LD   HL,(OP1)
          LD   DE,(OP2)
          LD   A,H
          AND  D
          LD   H,A
          LD   A,L
          AND  E
          LD   L,A
          JP   LAGRA
NOT_:     LD   HL,(OP1)
          LD   A,H
          CPL
          LD   H,A
          LD   A,L
          CPL
          LD   L,A
          JP   LAGRA
ADD_:     LD   HL,(OP1)
          LD   DE,(OP2)
          ADD  HL,DE
          JP   LAGRA
SUB_:     LD   HL,(OP1)
          LD   DE,(OP2)
          OR   A
          SBC  HL,DE
          JP   LAGRA
MUL_:     LD   HL,(OP1)
          LD   DE,(OP2)
          CALL MUL16
          JP   LAGRA
DIV_:     LD   HL,(OP1)
          LD   DE,(OP2)
          CALL SDIV
          JP   LAGRA
MOD_:     LD   HL,(OP1)
          LD   DE,(OP2)
          CALL SDIV
          EX   DE,HL
          JP   LAGRA

; HL = HL * DE (de låga 16 bitarna).
MUL16:    LD   B,H
          LD   C,L
          LD   HL,0
          LD   A,16
MUL_SLINGA:
          ADD  HL,HL
          EX   DE,HL
          ADD  HL,HL
          EX   DE,HL
          JR   NC,MUL_NASTA
          ADD  HL,BC
MUL_NASTA:
          DEC  A
          JR   NZ,MUL_SLINGA
          RET

; BC / DE utan tecken: BC blir kvoten och HL resten (DE < 32769).
UDIV:     LD   HL,0
          LD   A,16
UDIV_SLINGA:
          SLA  C
          RL   B
          ADC  HL,HL
          SBC  HL,DE
          JR   NC,UDIV_ETT
          ADD  HL,DE
          JR   UDIV_NASTA
UDIV_ETT: INC  C
UDIV_NASTA:
          DEC  A
          JR   NZ,UDIV_SLINGA
          RET

; HL / DE med tecken: HL blir kvoten (mot noll) och DE resten (med
; täljarens tecken).
SDIV:     LD   A,D
          OR   E
          JR   NZ,SDIV2
          LD   HL,T_NOLL
          JP   FEL
SDIV2:    LD   A,H
          XOR  D
          LD   (KVOTTECKEN),A
          LD   A,H
          LD   (RESTTECKEN),A
          BIT  7,H
          CALL NZ,NEGHL
          EX   DE,HL
          BIT  7,H
          CALL NZ,NEGHL
          EX   DE,HL
          LD   B,H
          LD   C,L
          CALL UDIV
          LD   A,(RESTTECKEN)
          BIT  7,A
          CALL NZ,NEGHL
          EX   DE,HL
          LD   H,B
          LD   L,C
          LD   A,(KVOTTECKEN)
          BIT  7,A
          RET  Z
NEGHL:    XOR  A
          SUB  L
          LD   L,A
          SBC  A,A
          SUB  H
          LD   H,A
          RET

; ---------------------------------------------------------------------
; Minnet

LOADW:    LD   HL,(OP2)
          ADD  HL,HL
          LD   DE,(OP1)
          ADD  HL,DE
          LD   C,0
          CALL LASORDINC
          EX   DE,HL
          JP   LAGRA
LOADB:    LD   HL,(OP1)
          LD   DE,(OP2)
          ADD  HL,DE
          LD   C,0
          CALL LASB
          LD   L,A
          LD   H,0
          JP   LAGRA
STOREW:   LD   HL,(OP2)
          ADD  HL,HL
          LD   DE,(OP1)
          ADD  HL,DE
          CALL DYNADR
          LD   DE,(OP3)
          LD   (HL),D
          INC  HL
          LD   (HL),E
          RET
STOREB:   LD   HL,(OP1)
          LD   DE,(OP2)
          ADD  HL,DE
          CALL DYNADR
          LD   A,(OP3)
          LD   (HL),A
          RET

STORE:    LD   A,(OP1)
          LD   HL,(OP2)
          JP   SKRIVVARIND
LOAD:     LD   A,(OP1)
          CALL LASVARIND
          JP   LAGRA
PUSH_:    LD   HL,(OP1)
          JP   ZPUSH
PULL:     CALL ZPOP
          LD   A,(OP1)
          JP   SKRIVVARIND
POP_:     JP   ZPOP

; ---------------------------------------------------------------------
; Objekten. Ett objekt är 9 bytes: attributen (4), föräldern, syskonet,
; barnet och adressen till egenskaperna (ett ord).

; Objektet HL till en pekare i HL (objekt 0 blir ett tomt objekt).
; Bevarar BC.
OBJEKT:   LD   A,H
          OR   L
          JR   Z,OBJEKT_NOLL
          LD   D,H
          LD   E,L
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,DE
          LD   DE,(OBJBAS)
          ADD  HL,DE
          RET
OBJEKT_NOLL:
          LD   HL,OBJNOLL
          XOR  A
          LD   B,9
OBJNOLL_TOM:
          LD   (HL),A
          INC  HL
          DJNZ OBJNOLL_TOM
          LD   HL,OBJNOLL
          RET

; Objektet A till IX.
OBJIX:    LD   L,A
          LD   H,0
          PUSH BC
          CALL OBJEKT
          POP  BC
          PUSH HL
          POP  IX
          RET
OBJIY:    LD   L,A
          LD   H,0
          PUSH BC
          CALL OBJEKT
          POP  BC
          PUSH HL
          POP  IY
          RET

GET_PARENT:
          LD   A,(OP1)
          CALL OBJIX
          LD   L,(IX+4)
          LD   H,0
          JP   LAGRA
GET_SIBLING:
          LD   A,(OP1)
          CALL OBJIX
          LD   L,(IX+5)
          JR   GET_GREN
GET_CHILD:
          LD   A,(OP1)
          CALL OBJIX
          LD   L,(IX+6)
GET_GREN: LD   H,0
          PUSH HL
          CALL LAGRA
          POP  HL
          LD   A,L
          OR   A
          JP   NZ,GRENSANT
          JP   GRENFALSK
JIN:      LD   A,(OP1)
          CALL OBJIX
          LD   A,(OP2)
          CP   (IX+4)
          JP   Z,GRENSANT
          JP   GRENFALSK

; Ta objektet A ur trädet; IX blir objektet.
TABORT:   LD   (TOBJ),A
          CALL OBJIX
          LD   A,(IX+4)
          OR   A
          RET  Z
          CALL OBJIY            ; föräldern
          LD   A,(TOBJ)
          LD   B,A
          LD   A,(IY+6)
          CP   B
          JR   NZ,TABORT_SOK
          LD   A,(IX+5)         ; det första barnet
          LD   (IY+6),A
          JR   TABORT_KLAR
TABORT_SOK:
          OR   A                ; syskonet före objektet
          JR   Z,TABORT_KLAR
          CALL OBJIY
          LD   A,(IY+5)
          CP   B
          JR   NZ,TABORT_SOK
          LD   A,(IX+5)
          LD   (IY+5),A
TABORT_KLAR:
          LD   (IX+4),0
          LD   (IX+5),0
          RET

REMOVE_OBJ:
          LD   A,(OP1)
          JP   TABORT
INSERT_OBJ:
          LD   A,(OP1)
          CALL TABORT
          LD   A,(OP2)
          LD   (IX+4),A
          CALL OBJIY
          LD   A,(IY+6)
          LD   (IX+5),A
          LD   A,(OP1)
          LD   (IY+6),A
          RET

; Attributet OP2 i objektet OP1: HL pekar på byten och A är biten.
ATTR:     LD   HL,(OP1)
          CALL OBJEKT
          LD   A,(OP2)
          LD   B,A
          RRCA
          RRCA
          RRCA
          AND  3
          LD   E,A
          LD   D,0
          ADD  HL,DE
          LD   A,B
          AND  7
          LD   B,A
          LD   A,$80
          RET  Z
ATTR_SKIFT:
          RRCA
          DJNZ ATTR_SKIFT
          RET
TEST_ATTR:
          CALL ATTR
          AND  (HL)
          JP   NZ,GRENSANT
          JP   GRENFALSK
SET_ATTR: CALL ATTR
          OR   (HL)
          LD   (HL),A
          RET
CLEAR_ATTR:
          CALL ATTR
          CPL
          AND  (HL)
          LD   (HL),A
          RET

; Objektet HL: pekare till dess egenskaper (efter namnet).
EGENSKAPER:
          CALL OBJEKT
          LD   DE,7
          ADD  HL,DE
          LD   A,(HL)
          INC  HL
          LD   L,(HL)
          ADD  A,DYNHI
          LD   H,A
          LD   A,(HL)           ; namnets längd i ord
          ADD  A,A
          INC  A
          LD   E,A
          LD   D,0
          ADD  HL,DE
          RET

; Egenskapens längd ur storleksbyten A.
EGLANGD:  RLCA
          RLCA
          RLCA
          AND  7
          INC  A
          RET

; Egenskapen A i objektet HL: HL pekar på dess storleksbyte; bär om den
; inte finns (de ligger i fallande ordning och slutar med 0).
FINNEG:   LD   C,A
          CALL EGENSKAPER
FINNEG_SLINGA:
          LD   A,(HL)
          AND  $1F
          CP   C
          RET  Z
          RET  C
          LD   A,(HL)
          CALL EGLANGD
          INC  A
          LD   E,A
          LD   D,0
          ADD  HL,DE
          JR   FINNEG_SLINGA

GET_PROP: LD   HL,(OP1)
          LD   A,(OP2)
          CALL FINNEG
          JR   C,GET_PROP_STD
          LD   A,(HL)
          CALL EGLANGD
          INC  HL
          CP   1
          JR   Z,GET_PROP_BYTE
          LD   D,(HL)
          INC  HL
          LD   E,(HL)
          EX   DE,HL
          JP   LAGRA
GET_PROP_BYTE:
          LD   L,(HL)
          LD   H,0
          JP   LAGRA
GET_PROP_STD:
          LD   A,(OP2)          ; standardvärdet
          DEC  A
          ADD  A,A
          LD   L,A
          LD   H,0
          LD   DE,(OBJSTD)
          ADD  HL,DE
          LD   D,(HL)
          INC  HL
          LD   E,(HL)
          EX   DE,HL
          JP   LAGRA

GET_PROP_ADDR:
          LD   HL,(OP1)
          LD   A,(OP2)
          CALL FINNEG
          JR   C,GET_PROP_ADDR0
          INC  HL
          LD   A,H
          SUB  DYNHI
          LD   H,A
          JP   LAGRA
GET_PROP_ADDR0:
          LD   HL,0
          JP   LAGRA

GET_NEXT_PROP:
          LD   HL,(OP1)
          LD   A,(OP2)
          OR   A
          JR   NZ,NEXT_PROP2
          CALL EGENSKAPER
          JR   NEXT_PROP3
NEXT_PROP2:
          CALL FINNEG
          JR   C,EGFEL
          LD   A,(HL)
          CALL EGLANGD
          INC  A
          LD   E,A
          LD   D,0
          ADD  HL,DE
NEXT_PROP3:
          LD   A,(HL)
          AND  $1F
          LD   L,A
          LD   H,0
          JP   LAGRA

PUT_PROP: LD   HL,(OP1)
          LD   A,(OP2)
          CALL FINNEG
          JR   C,EGFEL
          LD   A,(HL)
          CALL EGLANGD
          INC  HL
          LD   DE,(OP3)
          CP   1
          JR   Z,PUT_PROP_BYTE
          LD   (HL),D
          INC  HL
PUT_PROP_BYTE:
          LD   (HL),E
          RET
EGFEL:    LD   HL,T_EGENSKAP
          JP   FEL

GET_PROP_LEN:
          LD   HL,(OP1)
          LD   A,H
          OR   L
          JP   Z,LAGRA
          DEC  HL
          LD   C,0
          CALL LASB
          CALL EGLANGD
          LD   L,A
          LD   H,0
          JP   LAGRA

; ---------------------------------------------------------------------
; Slumptal

RANDOM:   LD   HL,(OP1)
          BIT  7,H
          JR   NZ,RANDOM_FRO
          LD   A,H
          OR   L
          JR   Z,RANDOM_NY
          CALL XORSKIFT
          RES  7,H
          LD   B,H
          LD   C,L
          LD   DE,(OP1)
          CALL UDIV
          INC  HL
          JP   LAGRA
RANDOM_FRO:
          CALL NEGHL            ; förutsägbara tal från fröet -n
          LD   (FRO),HL
          LD   HL,0
          JP   LAGRA
RANDOM_NY:
          LD   A,R
          LD   HL,(FRO)
          XOR  L
          LD   L,A
          LD   (FRO),HL         ; (0 som frö rättar XORSKIFT)
          LD   HL,0
          JP   LAGRA

; Nästa tal (xorshift med 7, 9 och 8) till HL.
XORSKIFT: LD   HL,(FRO)
          LD   A,H
          OR   L
          JR   NZ,XORSKIFT2
          INC  HL
XORSKIFT2:
          LD   A,H
          RRA
          LD   A,L
          RRA
          XOR  H
          LD   H,A
          LD   A,L
          RRA
          LD   A,H
          RRA
          XOR  L
          LD   L,A
          XOR  H
          LD   H,A
          LD   (FRO),HL
          RET

; ---------------------------------------------------------------------
; Texten

PRINT:    CALL HAMTAPC
          CALL ZTEXT
          JP   SETPC
PRINT_RET:
          CALL PRINT
          LD   A,CR
          CALL UTZ
          JP   RTRUE
PRINT_ADDR:
          LD   HL,(OP1)
          LD   C,0
          JP   ZTEXT
PRINT_PADDR:
          LD   HL,(OP1)
          ADD  HL,HL
          LD   C,0
          RL   C
          JP   ZTEXT
PRINT_OBJ:
          LD   HL,(OP1)
SKRIVOBJ: LD   A,H
          OR   L
          RET  Z
          CALL OBJEKT
          LD   DE,7
          ADD  HL,DE
          LD   D,(HL)
          INC  HL
          LD   E,(HL)
          EX   DE,HL
          LD   C,0
          CALL LASB
          OR   A                ; utan namn
          RET  Z
          INC  HL
          JP   ZTEXT
PRINT_CHAR:
          LD   A,(OP1)
          JP   UTZ
NEW_LINE: LD   A,CR
          JP   UTZ
PRINT_NUM:
          LD   HL,(OP1)

; Talet HL med tecken.
SKRIVTAL: BIT  7,H
          JR   Z,SKRIVTAL2
          LD   A,'-'
          CALL UTZ
          CALL NEGHL
SKRIVTAL2:
          LD   DE,SIFFROR+5
          XOR  A
          LD   (DE),A
SKRIVTAL3:
          PUSH DE
          LD   B,H
          LD   C,L
          LD   DE,10
          CALL UDIV
          LD   A,L
          ADD  A,'0'
          POP  DE
          DEC  DE
          LD   (DE),A
          LD   H,B
          LD   L,C
          LD   A,H
          OR   L
          JR   NZ,SKRIVTAL3
          EX   DE,HL
          JP   UTSTR

; Strängen HL (slutar med 0) som text.
UTSTR:    LD   A,(HL)
          OR   A
          RET  Z
          CALL UTZ
          INC  HL
          JR   UTSTR

; Skriv Z-texten på C:HL; C:HL blir adressen efter den. Tre tecken om
; fem bitar i varje ord, och det sista ordet har bit 15 satt.
ZTEXT:    XOR  A
          LD   (ALFABET),A
          LD   (FORKORT),A
          LD   (FLYKT),A
ZTEXT_ORD:
          CALL LASORDINC
          PUSH HL
          PUSH BC
          PUSH DE
          LD   B,3
ZTEXT_TECKEN:
          LD   A,D
          RRA
          RRA
          AND  $1F
          PUSH DE
          PUSH BC
          CALL ZTECKEN
          POP  BC
          POP  DE
          LD   A,5
ZTEXT_SKIFT:
          SLA  E
          RL   D
          DEC  A
          JR   NZ,ZTEXT_SKIFT
          DJNZ ZTEXT_TECKEN
          POP  DE
          POP  BC
          POP  HL
          BIT  7,D
          JR   Z,ZTEXT_ORD
          RET

; Ett Z-tecken A.
ZTECKEN:  LD   C,A
          LD   A,(FLYKT)        ; ett tiobitars ZSCII-tecken i två delar
          OR   A
          JR   Z,ZT_INTEFLYKT
          DEC  A
          JR   NZ,ZT_FLYKTLAG
          LD   A,C
          LD   (FLYKTHOG),A
          LD   A,2
          LD   (FLYKT),A
          RET
ZT_FLYKTLAG:
          XOR  A
          LD   (FLYKT),A
          LD   A,(FLYKTHOG)
          RRCA
          RRCA
          RRCA
          AND  $E0
          OR   C
          JP   UTZ
ZT_INTEFLYKT:
          LD   A,(FORKORT)
          OR   A
          JR   Z,ZT_INTEFORK
          DEC  A                ; förkortning 32 * (z - 1) + tecknet
          RRCA
          RRCA
          RRCA
          ADD  A,C
          LD   L,A
          LD   H,0
          ADD  HL,HL
          LD   DE,(FORKTAB)
          ADD  HL,DE
          XOR  A
          LD   (FORKORT),A
          LD   C,0
          CALL LASORDINC
          EX   DE,HL
          ADD  HL,HL
          LD   C,0
          RL   C
          CALL ZTEXT
          XOR  A
          LD   (ALFABET),A
          LD   (FORKORT),A
          LD   (FLYKT),A
          RET
ZT_INTEFORK:
          LD   A,C
          OR   A
          JR   NZ,ZT_1
          LD   (ALFABET),A
          LD   A,' '
          JP   UTZ
ZT_1:     CP   4
          JR   NC,ZT_2
          LD   (FORKORT),A
          RET
ZT_2:     CP   6
          JR   NC,ZT_3
          SUB  3
          LD   (ALFABET),A
          RET
ZT_3:     LD   A,(ALFABET)
          LD   B,A
          XOR  A
          LD   (ALFABET),A
          LD   A,B
          CP   2
          JR   Z,ZT_A2
          LD   A,C
          ADD  A,'a'-6
          DEC  B
          JP   NZ,UTZ
          ADD  A,'A'-'a'
          JP   UTZ
ZT_A2:    LD   A,C
          CP   6
          JR   NZ,ZT_A2B
          LD   A,1
          LD   (FLYKT),A
          RET
ZT_A2B:   LD   E,C
          LD   D,0
          LD   HL,A2TAB-6
          ADD  HL,DE
          LD   A,(HL)
          JP   UTZ

; Alfabet 2 från Z-tecken 6 (6 är tiobitarstecknet, 7 ny rad).
A2TAB:    DEFB 0,CR,"0123456789.,!?_#'",34,"/",92,"-:()"

; ---------------------------------------------------------------------
; Skärmen

; Ett ZSCII-tecken A till skärmen (eller statusraden). Bevarar allt.
UTZ:      PUSH BC
          PUSH DE
          PUSH HL
          CALL UTZ1
          POP  HL
          POP  DE
          POP  BC
          RET
UTZ1:     LD   C,A
          LD   A,(STATUSLAGE)
          OR   A
          JR   NZ,UT_STATUS
          LD   A,C
          CP   CR
          JR   Z,UT_NYRAD
          CP   ' '
          JR   Z,UT_MELLAN
          RET  C
          CP   127
          RET  NC
          LD   A,(ORDLANGD)     ; ordet väntar tills det är slut
          CP   ORDMAX
          CALL NC,TOMORD
          LD   A,(ORDLANGD)
          LD   E,A
          LD   D,0
          LD   HL,ORDBUF
          ADD  HL,DE
          LD   (HL),C
          INC  A
          LD   (ORDLANGD),A
          RET
UT_NYRAD: CALL TOMORD
          JP   NYRAD
UT_MELLAN:
          CALL TOMORD
          LD   A,(KOLUMN)
          OR   A
          JR   NZ,UT_MELLAN2
          LD   A,(MJUK)         ; inget mellanslag först på en rad som
          OR   A                ; bröts
          RET  NZ
UT_MELLAN2:
          LD   A,(BREDD)
          LD   B,A
          LD   A,(KOLUMN)
          CP   B
          JP   NC,MJUKRAD
          LD   A,' '
          JP   SKRIVTECKEN
UT_STATUS:
          LD   A,C
          CP   ' '
          RET  C
          LD   HL,(STATUSPEK)
          LD   DE,(STATUSSLUT)
          OR   A
          SBC  HL,DE
          RET  NC
          ADD  HL,DE
          LD   (HL),C
          INC  HL
          LD   (STATUSPEK),HL
          RET

; Skriv det väntande ordet, på en ny rad om det inte får plats.
TOMORD:   LD   A,(ORDLANGD)
          OR   A
          RET  Z
          LD   B,A
          LD   A,(KOLUMN)
          OR   A
          JR   Z,TOMORD2
          ADD  A,B
          JR   C,TOMORD1
          LD   C,A
          LD   A,(BREDD)
          CP   C
          JR   NC,TOMORD2
TOMORD1:  PUSH BC
          CALL MJUKRAD
          POP  BC
TOMORD2:  LD   HL,ORDBUF
TOMORD3:  LD   A,(BREDD)        ; längre än raden: bryt mitt i
          LD   C,A
          LD   A,(KOLUMN)
          CP   C
          JR   C,TOMORD4
          PUSH HL
          PUSH BC
          CALL MJUKRAD
          POP  BC
          POP  HL
TOMORD4:  LD   A,(HL)
          PUSH HL
          PUSH BC
          CALL SKRIVTECKEN
          POP  BC
          POP  HL
          INC  HL
          DJNZ TOMORD3
          XOR  A
          LD   (ORDLANGD),A
          RET

; Tecknet A på skärmen, i kolumnen KOLUMN.
SKRIVTECKEN:
          LD   C,A
          CALL KONUT
          LD   HL,KOLUMN
          INC  (HL)
          XOR  A
          LD   (MJUK),A
          RET

MJUKRAD:  CALL NYRAD
          LD   A,1
          LD   (MJUK),A
          RET

; Ny rad. På rad 23 tas rad 1 bort, så att statusraden står kvar. Efter
; en skärm utan inmatning väntar (MER) på en tangent.
NYRAD:    XOR  A
          LD   (KOLUMN),A
          LD   (MJUK),A
          LD   A,(ZRAD)
          CP   23
          JR   NC,NYRAD_RULLA
          INC  A
          LD   (ZRAD),A
          LD   C,CR
          CALL KONUT
          LD   C,LF
          CALL KONUT
          JR   NYRAD_MER
NYRAD_RULLA:
          LD   B,1
          LD   C,0
          CALL GATILL
          LD   C,ESC
          CALL KONUT
          LD   C,'R'
          CALL KONUT
          LD   B,23
          LD   C,0
          CALL GATILL
NYRAD_MER:
          LD   A,(RADER)
          INC  A
          LD   (RADER),A
          CP   TEXTRADER
          RET  C
          XOR  A
          LD   (RADER),A
          LD   HL,T_MER
          CALL KONSTR
          CALL TANGENT
          LD   C,CR
          CALL KONUT
          LD   C,ESC
          CALL KONUT
          LD   C,'T'
          JP   KONUT

; Markören till rad B, kolumn C.
GATILL:   PUSH BC
          LD   C,ESC
          CALL KONUT
          LD   C,'='
          CALL KONUT
          POP  BC
          PUSH BC
          LD   A,B
          ADD  A,32
          LD   C,A
          CALL KONUT
          POP  BC
          LD   A,C
          ADD  A,32
          LD   C,A
          JP   KONUT

; Strängen HL direkt till skärmen.
KONSTR:   LD   A,(HL)
          OR   A
          RET  Z
          LD   C,A
          CALL KONUT
          INC  HL
          JR   KONSTR

; Tecknet C till skärmen. Bevarar BC, DE och HL. En tangent som är nere
; under tiden läggs i kön.
KONUT:    PUSH BC
          PUSH DE
          PUSH HL
          CALL BIOS_KONUT
          CALL POLLA
          POP  HL
          POP  DE
          POP  BC
          RET

; POLLA: en tangent som är nere till kön, så att det som skrivs medan
; spelet svarar eller läser från skivan inte tappas (ABC80 har ingen
; buffert: en tangent som inte läses medan den är nere försvinner).
; Ändrar A och flaggorna.
POLLA:    PUSH BC
          PUSH DE
          PUSH HL
          CALL BIOS_KONST
          OR   A
          JR   Z,POLLA_KLAR
          CALL BIOS_KONIN
          LD   C,A
          LD   A,(KO_IN)
          LD   E,A
          INC  A
          AND  KOSTORLEK-1
          LD   HL,KO_UT
          CP   (HL)
          JR   Z,POLLA_KLAR     ; kön är full
          LD   (KO_IN),A
          LD   D,0
          LD   HL,KO
          ADD  HL,DE
          LD   (HL),C
POLLA_KLAR:
          POP  HL
          POP  DE
          POP  BC
          RET

; Vänta på en tangent (A), ur kön om något står där; fröet till
; slumptalen räknas upp under tiden.
TANGENT:  PUSH BC
          PUSH DE
          PUSH HL
          LD   A,(KO_UT)
          LD   E,A
          LD   A,(KO_IN)
          CP   E
          JR   Z,TANGENT_VANTA  ; kön är tom
          LD   A,E
          INC  A
          AND  KOSTORLEK-1
          LD   (KO_UT),A
          LD   D,0
          LD   HL,KO
          ADD  HL,DE
          LD   A,(HL)
          POP  HL
          POP  DE
          POP  BC
          RET
TANGENT_VANTA:
          LD   HL,(FRO)
          INC  HL
          LD   (FRO),HL
          CALL BIOS_KONST
          OR   A
          JR   Z,TANGENT_VANTA
          CALL BIOS_KONIN
          POP  HL
          POP  DE
          POP  BC
          RET

BIOS_KONST: JP 0                ; adresserna sätts av START
BIOS_KONIN: JP 0
BIOS_KONUT: JP 0

; Statusraden: platsen (global 0) till vänster, poängen och dragen
; (global 1 och 2) till höger, eller klockan om flaggan i headern säger
; att spelet räknar tid.
SHOW_STATUS:
STATUSRAD:
          LD   A,1
          LD   (STATUSLAGE),A
          LD   HL,HOGERBUF      ; först högra delen
          LD   (STATUSPEK),HL
          LD   HL,HOGERBUF+40
          LD   (STATUSSLUT),HL
          LD   A,(DYN+1)
          BIT  1,A
          JR   NZ,STATUS_TID
          LD   HL,T_POANG
          LD   A,(KOLUMNER)
          CP   80
          JR   NC,STATUS_POANG
          LD   HL,T_POANG40
STATUS_POANG:
          CALL UTSTR
          LD   A,17
          CALL LASVAR
          CALL SKRIVTAL
          LD   HL,T_DRAG
          LD   A,(KOLUMNER)
          CP   80
          JR   NC,STATUS_DRAG
          LD   HL,T_DRAG40
STATUS_DRAG:
          CALL UTSTR
          LD   A,18
          CALL LASVAR
          CALL SKRIVTAL
          JR   STATUS_VANSTER
STATUS_TID:
          LD   HL,T_TID
          CALL UTSTR
          LD   A,17
          CALL LASVAR
          CALL SKRIVTAL
          LD   A,':'
          CALL UTZ
          LD   A,18
          CALL LASVAR
          LD   A,L
          CP   10
          JR   NC,STATUS_MINUT
          LD   A,'0'
          CALL UTZ
STATUS_MINUT:
          CALL SKRIVTAL
STATUS_VANSTER:
          LD   HL,STATUSBUF     ; raden blank
          LD   A,(BREDD)
          LD   B,A
STATUS_BLANK:
          LD   (HL),' '
          INC  HL
          DJNZ STATUS_BLANK
          LD   HL,(STATUSPEK)   ; högra delen längst till höger
          LD   DE,HOGERBUF
          OR   A
          SBC  HL,DE
          LD   C,L
          LD   B,0
          LD   A,(BREDD)
          SUB  L
          LD   E,A
          LD   D,0
          LD   HL,STATUSBUF
          ADD  HL,DE
          PUSH HL
          EX   DE,HL
          LD   HL,HOGERBUF
          LD   A,C
          OR   A
          JR   Z,STATUS_PLATS
          LDIR
STATUS_PLATS:
          POP  HL               ; platsen, med ett mellanslag före och
          DEC  HL               ; ett före högra delen
          LD   (STATUSSLUT),HL
          LD   HL,STATUSBUF+1
          LD   (STATUSPEK),HL
          LD   A,16
          CALL LASVAR
          CALL SKRIVOBJ
          XOR  A
          LD   (STATUSLAGE),A
          LD   B,0
          LD   C,0
          CALL GATILL
          LD   HL,STATUSBUF
          LD   A,(BREDD)
          LD   B,A
STATUS_UT:
          LD   C,(HL)
          CALL KONUT
          INC  HL
          DJNZ STATUS_UT
          LD   A,(ZRAD)
          LD   B,A
          LD   A,(KOLUMN)
          LD   C,A
          JP   GATILL

; ---------------------------------------------------------------------
; Inmatningen

; sread: statusraden, en rad till textbufferten (med små bokstäver och
; 0 sist) och orden i den till ordbufferten.
SREAD:    CALL TOMORD
          CALL STATUSRAD
          LD   HL,(OP1)
          CALL DYNADR
          LD   (TEXTPEK),HL
          LD   B,(HL)           ; byte 0 är det längsta + 1
          DEC  B
          LD   A,(BREDD)        ; inte längre än raden
          LD   C,A
          LD   A,(KOLUMN)
          LD   D,A
          LD   A,C
          SUB  D
          CP   B
          JR   NC,SREAD2
          LD   B,A
SREAD2:   INC  HL
          CALL LASRAD
          LD   HL,(TEXTPEK)
          INC  HL
          LD   E,A
          LD   D,0
          ADD  HL,DE
          LD   (HL),0
          CALL NYRAD
          XOR  A
          LD   (RADER),A
          LD   HL,(OP2)
          CALL DYNADR
          LD   (PARSPEK),HL
          JP   ORDDELA

; Läs en rad om högst B tecken till HL; A blir antalet. Stora bokstäver
; blir små.
LASRAD:   LD   C,0
LASRAD_SLINGA:
          CALL TANGENT
          CP   CR
          JR   Z,LASRAD_KLAR
          CP   8
          JR   Z,LASRAD_BAK
          CP   $7F
          JR   Z,LASRAD_BAK
          CP   ' '
          JR   C,LASRAD_SLINGA
          CP   $7F
          JR   NC,LASRAD_SLINGA
          LD   E,A
          LD   A,C
          CP   B
          JR   NC,LASRAD_SLINGA
          LD   A,E
          CP   'A'
          JR   C,LASRAD_LITEN
          CP   'Z'+1
          JR   NC,LASRAD_LITEN
          ADD  A,'a'-'A'
LASRAD_LITEN:
          LD   (HL),A
          INC  HL
          INC  C
          PUSH BC
          PUSH HL
          CALL SKRIVTECKEN
          POP  HL
          POP  BC
          JR   LASRAD_SLINGA
LASRAD_BAK:
          LD   A,C
          OR   A
          JR   Z,LASRAD_SLINGA
          DEC  C
          DEC  HL
          PUSH BC
          LD   C,8
          CALL KONUT
          LD   C,' '
          CALL KONUT
          LD   C,8
          CALL KONUT
          POP  BC
          LD   A,(KOLUMN)
          DEC  A
          LD   (KOLUMN),A
          JR   LASRAD_SLINGA
LASRAD_KLAR:
          LD   A,C
          RET

; Dela textbufferten i ord och slå upp dem i ordlistan. Ett ord är en
; avgränsare (som komma) eller tecknen fram till mellanslag, avgränsare
; eller slutet. Varje ord blir fyra bytes i ordbufferten: adressen i
; ordlistan (0 om det inte finns), längden och platsen i texten.
ORDDELA:  LD   HL,(PARSPEK)
          LD   A,(HL)
          LD   (MAXORD),A
          INC  HL
          LD   (HL),0
          INC  HL
          LD   (PARSUT),HL
          XOR  A
          LD   (ANTORD),A
          LD   HL,(TEXTPEK)
          INC  HL
ORDDELA_SLINGA:
          LD   A,(MAXORD)
          LD   B,A
          LD   A,(ANTORD)
          CP   B
          JR   NC,ORDDELA_KLAR
          LD   A,(HL)
          OR   A
          JR   Z,ORDDELA_KLAR
          CP   ' '
          JR   NZ,ORDDELA_ORD
          INC  HL
          JR   ORDDELA_SLINGA
ORDDELA_ORD:
          LD   (ORDSTART),HL
          CALL ARSEP
          INC  HL
          JR   Z,ORDDELA_SLUT
ORDDELA_LANG:
          LD   A,(HL)
          OR   A
          JR   Z,ORDDELA_SLUT
          CP   ' '
          JR   Z,ORDDELA_SLUT
          CALL ARSEP
          JR   Z,ORDDELA_SLUT
          INC  HL
          JR   ORDDELA_LANG
ORDDELA_SLUT:
          PUSH HL
          LD   DE,(ORDSTART)
          OR   A
          SBC  HL,DE
          LD   A,L
          LD   (ORDLANGD2),A
          CALL KODA
          CALL SLAUPP
          LD   HL,(PARSUT)
          LD   (HL),D
          INC  HL
          LD   (HL),E
          INC  HL
          LD   A,(ORDLANGD2)
          LD   (HL),A
          INC  HL
          PUSH HL
          LD   HL,(ORDSTART)
          LD   DE,(TEXTPEK)
          OR   A
          SBC  HL,DE
          LD   A,L
          POP  HL
          LD   (HL),A
          INC  HL
          LD   (PARSUT),HL
          LD   HL,ANTORD
          INC  (HL)
          POP  HL
          JR   ORDDELA_SLINGA
ORDDELA_KLAR:
          LD   HL,(PARSPEK)
          INC  HL
          LD   A,(ANTORD)
          LD   (HL),A
          RET

; Noll om A är en avgränsare. Bevarar alla register utom F.
ARSEP:    PUSH HL
          PUSH BC
          LD   C,A
          LD   HL,SEPAR
          LD   A,(HL)
          OR   A
          JR   Z,ARSEP_INTE
          LD   B,A
ARSEP_SLINGA:
          INC  HL
          LD   A,(HL)
          CP   C
          JR   Z,ARSEP_KLAR
          DJNZ ARSEP_SLINGA
ARSEP_INTE:
          OR   1
ARSEP_KLAR:
          LD   A,C
          POP  BC
          POP  HL
          RET

; Ordet på (ORDSTART), (ORDLANGD2) tecken, till sex Z-tecken i två ord
; (ZKOD, med den höga byten först, som i ordlistan).
KODA:     LD   HL,ZTECK
          LD   B,6
KODA_FYLL:
          LD   (HL),5
          INC  HL
          DJNZ KODA_FYLL
          XOR  A
          LD   (ZANTAL),A
          LD   HL,(ORDSTART)
          LD   A,(ORDLANGD2)
          LD   B,A
KODA_SLINGA:
          LD   A,(ZANTAL)
          CP   6
          JR   NC,KODA_PACKA
          LD   A,(HL)
          INC  HL
          CP   'a'
          JR   C,KODA_A2
          CP   'z'+1
          JR   NC,KODA_A2
          SUB  'a'-6
          CALL KODA_LAGG
          JR   KODA_NASTA
KODA_A2:  PUSH HL
          PUSH BC
          LD   C,A
          LD   HL,A2TAB+2
          LD   B,24
KODA_SOK: LD   A,(HL)
          CP   C
          JR   Z,KODA_HITTAD
          INC  HL
          DJNZ KODA_SOK
          LD   A,5              ; tiobitarstecknet: 5 6 hög låg
          CALL KODA_LAGG
          LD   A,6
          CALL KODA_LAGG
          LD   A,C
          RLCA
          RLCA
          RLCA
          AND  7
          CALL KODA_LAGG
          LD   A,C
          AND  $1F
          CALL KODA_LAGG
          JR   KODA_A2KLAR
KODA_HITTAD:
          LD   A,5
          CALL KODA_LAGG
          LD   DE,A2TAB-6
          OR   A
          SBC  HL,DE
          LD   A,L
          CALL KODA_LAGG
KODA_A2KLAR:
          POP  BC
          POP  HL
KODA_NASTA:
          DJNZ KODA_SLINGA
KODA_PACKA:
          LD   HL,ZTECK
          CALL KODA_ORD
          LD   (ZKOD),DE
          CALL KODA_ORD
          LD   A,E
          OR   $80
          LD   E,A
          LD   (ZKOD+2),DE
          RET
; Tre Z-tecken på HL till ett ord, med höga byten i E (som i minnet).
KODA_ORD: LD   A,(HL)
          ADD  A,A
          ADD  A,A
          LD   E,A
          INC  HL
          LD   A,(HL)
          RRCA
          RRCA
          RRCA
          LD   C,A
          AND  3
          OR   E
          LD   E,A
          LD   A,C
          AND  $E0
          INC  HL
          OR   (HL)
          LD   D,A
          INC  HL
          RET
; Lägg Z-tecknet A, om det får plats. Bevarar BC, DE och HL.
KODA_LAGG:
          PUSH HL
          PUSH DE
          LD   E,A
          LD   A,(ZANTAL)
          CP   6
          JR   NC,KODA_LAGG2
          LD   HL,ZTECK
          LD   D,0
          PUSH DE
          LD   E,A
          ADD  HL,DE
          POP  DE
          LD   (HL),E
          INC  A
          LD   (ZANTAL),A
KODA_LAGG2:
          POP  DE
          POP  HL
          RET

; Sök ZKOD i ordlistan (som är sorterad); DE blir adressen eller 0.
SLAUPP:   LD   HL,0
          LD   (LAG),HL
          LD   HL,(POSTANTAL)
          DEC  HL
          LD   (HOG),HL
SLAUPP_SLINGA:
          LD   HL,(HOG)         ; slut när LAG > HOG
          LD   DE,(LAG)
          CALL MINDRE
          LD   DE,0
          RET  C
          LD   HL,(LAG)
          LD   DE,(HOG)
          ADD  HL,DE
          SRL  H
          RR   L
          LD   (MITT),HL
          LD   A,(POSTLANGD)
          LD   E,A
          LD   D,0
          CALL MUL16
          LD   DE,(POSTSTART)
          ADD  HL,DE
          PUSH HL
          LD   DE,ZKOD
          LD   C,0
          LD   B,4
SLAUPP_JAMFOR:
          CALL LASB
          EX   DE,HL
          CP   (HL)
          EX   DE,HL
          JR   NZ,SLAUPP_OLIKA
          INC  HL
          INC  DE
          DJNZ SLAUPP_JAMFOR
          POP  DE               ; funnet
          RET
SLAUPP_OLIKA:
          POP  HL
          LD   HL,(MITT)
          JR   C,SLAUPP_UPP     ; posten < ordet
          DEC  HL
          LD   (HOG),HL
          JR   SLAUPP_SLINGA
SLAUPP_UPP:
          INC  HL
          LD   (LAG),HL
          JR   SLAUPP_SLINGA

; ---------------------------------------------------------------------
; Spara, läsa in, börja om, kontrollera och sluta

; Läget sparas i en fil: först en post med programräknaren (som pekar
; på save-instruktionens gren), FP, ZSP och spelets utgåva, serienummer
; och kontrollsumma, sedan Z-stacken och sist det dynamiska minnet.
SAVE:     CALL HAMTAPC
          LD   (LAGE),HL
          LD   A,C
          LD   (LAGE+2),A
          LD   HL,(FP)
          LD   DE,ZSTACK
          OR   A
          SBC  HL,DE
          LD   (LAGE+3),HL
          LD   HL,(ZSP)
          OR   A
          SBC  HL,DE
          LD   (LAGE+5),HL
          CALL SPELID
          LD   DE,LAGE+7
          LD   BC,10
          LDIR
          CALL FILNAMN
          JP   NZ,GRENFALSK
          LD   DE,SPARFCB
          LD   C,19             ; ta bort den gamla
          CALL BDOS
          LD   DE,SPARFCB
          LD   C,22             ; skapa
          CALL BDOS
          INC  A
          JR   Z,SAVE_FEL
          LD   HL,LAGE
          LD   B,1+ZSTACKSTORLEK/128
          LD   C,21
          CALL POSTER
          JR   NZ,SAVE_FEL
          CALL DYNPOSTER
          LD   C,21
          CALL POSTER
          JR   NZ,SAVE_FEL
          LD   DE,SPARFCB
          LD   C,16             ; stäng
          CALL BDOS
          INC  A
          JR   Z,SAVE_FEL
          JP   GRENSANT
SAVE_FEL: LD   HL,T_KAN_INTE_SPARA
          CALL UTSTR
          JP   GRENFALSK

RESTORE:  CALL FILNAMN
          JP   NZ,GRENFALSK
          LD   DE,SPARFCB
          LD   C,15             ; öppna
          CALL BDOS
          INC  A
          JR   Z,RESTORE_FEL
          LD   HL,POSTBUF       ; först bara posten med läget
          LD   B,1
          LD   C,20
          CALL POSTER
          JR   NZ,RESTORE_FEL
          CALL SPELID
          LD   DE,POSTBUF+7
          LD   B,10
RESTORE_ID:
          LD   A,(DE)
          CP   (HL)
          JR   NZ,RESTORE_ANNAT
          INC  HL
          INC  DE
          DJNZ RESTORE_ID
          LD   HL,POSTBUF
          LD   DE,LAGE
          LD   BC,128
          LDIR
          LD   HL,ZSTACK
          LD   B,ZSTACKSTORLEK/128
          LD   C,20
          CALL POSTER
          JR   NZ,RESTORE_FEL
          LD   HL,(DYN+$10)     ; flaggorna 2 behålls
          PUSH HL
          CALL DYNPOSTER
          LD   C,20
          CALL POSTER
          POP  HL
          LD   (DYN+$10),HL
          JR   NZ,RESTORE_FEL
          LD   HL,(LAGE+3)
          LD   DE,ZSTACK
          ADD  HL,DE
          LD   (FP),HL
          LD   HL,(LAGE+5)
          ADD  HL,DE
          LD   (ZSP),HL
          LD   HL,(LAGE)
          LD   A,(LAGE+2)
          LD   C,A
          CALL SETPC
          JP   GRENSANT         ; som om save hade lyckats
RESTORE_ANNAT:
          LD   HL,T_ANNAT_SPEL
          CALL UTSTR
          JP   GRENFALSK
RESTORE_FEL:
          LD   HL,T_KAN_INTE_LASA
          CALL UTSTR
          JP   GRENFALSK

; HL pekar på utgåvan, serienumret och kontrollsumman i headern (10
; bytes, i en kopia).
SPELID:   LD   HL,(DYN+2)
          LD   (SPELIDBUF),HL
          LD   HL,DYN+$12
          LD   DE,SPELIDBUF+2
          LD   BC,6
          LDIR
          LD   HL,(DYN+$1C)
          LD   (SPELIDBUF+8),HL
          LD   HL,SPELIDBUF
          RET

; Det dynamiska minnet: HL = DYN och B = antalet poster.
DYNPOSTER:
          LD   HL,(STATBAS)
          LD   DE,127
          ADD  HL,DE
          ADD  HL,HL
          LD   B,H
          LD   HL,DYN
          RET

; B poster från eller till HL med BDOS-funktionen C (20 läs, 21 skriv)
; i SPARFCB; NZ om det inte gick.
POSTER:   PUSH BC
          PUSH HL
          EX   DE,HL
          LD   C,26
          CALL BDOS
          LD   DE,SPARFCB
          POP  HL
          POP  BC
          PUSH BC
          PUSH HL
          CALL BDOS
          POP  HL
          POP  BC
          OR   A
          RET  NZ
          LD   DE,128
          ADD  HL,DE
          DJNZ POSTER
          RET

; Fråga efter filen (med spelfilens namn och typen SAV om svaret är
; tomt) och gör SPARFCB; NZ om namnet inte går.
FILNAMN:  CALL TOMORD
          LD   HL,SPELFCB       ; förslaget
          LD   DE,SPARFCB
          LD   BC,12
          LDIR
          LD   HL,'S'|('A'<<8)
          LD   (SPARFCB+9),HL
          LD   A,'V'
          LD   (SPARFCB+11),A
          LD   HL,T_FIL
          CALL UTSTR
          LD   HL,SPARFCB
          CALL SKRIVFCB
          LD   HL,T_FIL2
          CALL UTSTR
          CALL TOMORD
          LD   HL,NAMNBUF
          LD   B,14
          LD   A,(BREDD)
          LD   C,A
          LD   A,(KOLUMN)
          LD   D,A
          LD   A,C
          SUB  D
          CP   B
          JR   NC,FILNAMN2
          LD   B,A
FILNAMN2: CALL LASRAD
          PUSH AF
          CALL NYRAD
          XOR  A
          LD   (RADER),A
          POP  AF
          LD   HL,NAMNBUF
          LD   E,A
          LD   D,0
          ADD  HL,DE
          LD   (HL),0
          OR   A
          JR   Z,FILNAMN_KLAR
          LD   HL,NAMNBUF       ; enhet:namn.typ
          LD   A,(NAMNBUF+1)
          CP   ':'
          JR   NZ,FILNAMN_ENHET
          LD   A,(HL)
          SUB  'a'-1
          CP   17
          JR   NC,FILNAMN_FEL
          LD   (SPARFCB),A
          INC  HL
          INC  HL
          JR   FILNAMN_NAMN
FILNAMN_ENHET:
FILNAMN_NAMN:
          LD   DE,SPARFCB+1
          LD   B,8
          CALL FILDEL
          JR   NZ,FILNAMN_FEL
          LD   A,(HL)
          OR   A
          JR   Z,FILNAMN_KLAR
          CP   '.'
          JR   NZ,FILNAMN_FEL
          INC  HL
          LD   DE,SPARFCB+9
          LD   B,3
          CALL FILDEL
          JR   NZ,FILNAMN_FEL
          LD   A,(HL)
          OR   A
          JR   NZ,FILNAMN_FEL
FILNAMN_KLAR:
          LD   HL,SPARFCB+12
          LD   B,24
FILNAMN_NOLL:
          LD   (HL),0
          INC  HL
          DJNZ FILNAMN_NOLL
          XOR  A
          RET
FILNAMN_FEL:
          OR   1
          RET

; Högst B tecken från HL till DE, med stora bokstäver och blanka efter;
; slutar vid '.' eller 0. NZ om det är för långt eller tomt.
FILDEL:   LD   C,B
FILDEL_SLINGA:
          LD   A,(HL)
          OR   A
          JR   Z,FILDEL_SLUT
          CP   '.'
          JR   Z,FILDEL_SLUT
          LD   A,B
          OR   A
          JR   Z,FILDEL_FEL
          LD   A,(HL)
          CP   'a'
          JR   C,FILDEL_STOR
          CP   'z'+1
          JR   NC,FILDEL_STOR
          SUB  'a'-'A'
FILDEL_STOR:
          LD   (DE),A
          INC  HL
          INC  DE
          DEC  B
          JR   FILDEL_SLINGA
FILDEL_SLUT:
          LD   A,B
          CP   C
          JR   Z,FILDEL_FEL
          OR   A
          JR   Z,FILDEL_OK
FILDEL_BLANK:
          LD   A,' '
          LD   (DE),A
          INC  DE
          DJNZ FILDEL_BLANK
FILDEL_OK:
          XOR  A
          RET
FILDEL_FEL:
          OR   1
          RET

; FCB:n HL som enhet:namn.typ.
SKRIVFCB: LD   A,(HL)
          OR   A
          JR   NZ,SKRIVFCB_ENHET
          PUSH HL
          LD   C,25             ; den aktuella enheten
          CALL BDOS
          POP  HL
          INC  A
SKRIVFCB_ENHET:
          ADD  A,'A'-1
          CALL UTZ
          LD   A,':'
          CALL UTZ
          INC  HL
          LD   B,8
          CALL SKRIVFCB_DEL
          LD   A,'.'
          CALL UTZ
          LD   B,3
SKRIVFCB_DEL:
          LD   A,(HL)
          INC  HL
          CP   ' '
          CALL NZ,UTZ
          DJNZ SKRIVFCB_DEL
          RET

; restart: läs det dynamiska minnet igen, men behåll flaggorna 2.
RESTART:  CALL TOMORD
          LD   HL,(DYN+$10)
          PUSH HL
          LD   A,(DYNSIDOR)
          LD   B,A
          LD   DE,0
RESTART_LAS:
          LD   A,E
          ADD  A,DYNHI
          CALL LASSIDA
          INC  DE
          DJNZ RESTART_LAS
          POP  HL
          LD   (DYN+$10),HL
          JP   OMSTART

; verify: summan av filens bytes från $40 till slutet ska vara
; kontrollsumman i headern. Filen läses post för post, eftersom det
; dynamiska minnet har ändrats.
VERIFY:   CALL TOMORD
          LD   HL,(FILLANGD)    ; antal bytes att summera: C:DE
          LD   A,(FILLANGD+2)
          LD   C,A
          LD   DE,$40
          OR   A
          SBC  HL,DE
          JR   NC,VERIFY1
          DEC  C
VERIFY1:  EX   DE,HL
          LD   HL,0
          LD   (SUMMA),HL
          LD   (SPELFCB+33),HL
          XOR  A
          LD   (SPELFCB+35),A
          LD   HL,POSTBUF+$40
          LD   (VERPEK),HL
VERIFY_POST:
          PUSH BC
          PUSH DE
          LD   DE,POSTBUF
          LD   C,26
          CALL BDOS
          LD   DE,SPELFCB
          LD   C,33
          CALL BDOS
          POP  DE
          POP  BC
          OR   A
          JR   NZ,VERIFY_SLUT
          LD   HL,(VERPEK)
VERIFY_BYTE:
          LD   A,C
          OR   D
          OR   E
          JR   Z,VERIFY_SLUT
          PUSH HL
          LD   A,(HL)
          LD   HL,(SUMMA)
          ADD  A,L
          LD   L,A
          JR   NC,VERIFY_ADD
          INC  H
VERIFY_ADD:
          LD   (SUMMA),HL
          POP  HL
          LD   A,D
          OR   E
          DEC  DE
          JR   NZ,VERIFY_DEC
          DEC  C
VERIFY_DEC:
          INC  L
          LD   A,L
          CP   (POSTBUF+128)&255
          JR   NZ,VERIFY_BYTE
          LD   HL,POSTBUF
          LD   (VERPEK),HL
          LD   HL,(SPELFCB+33)
          INC  HL
          LD   (SPELFCB+33),HL
          JR   VERIFY_POST
VERIFY_SLUT:
          LD   HL,(DYN+$1C)
          CALL VAND
          LD   DE,(SUMMA)
          OR   A
          SBC  HL,DE
          JP   Z,GRENSANT
          JP   GRENFALSK

QUIT:     CALL TOMORD
          LD   C,CR
          CALL KONUT
          LD   C,LF
          CALL KONUT
          JP   0

; Ett fel: meddelandet HL och programräknaren, sedan tillbaka till CP/M.
FEL:      PUSH HL
          CALL TOMORD
          LD   HL,T_CRLF
          CALL KONSTR
          LD   HL,T_FEL
          CALL KONSTR
          POP  HL
          CALL KONSTR
          LD   A,(IGANG)
          OR   A
          JR   Z,FEL_SLUT
          LD   HL,T_VID
          CALL KONSTR
          CALL HAMTAPC
          LD   A,C
          ADD  A,'0'
          LD   C,A
          CALL KONUT
          LD   A,H
          CALL HEXBYTE
          LD   A,L
          CALL HEXBYTE
FEL_SLUT: LD   HL,T_CRLF
          CALL KONSTR
          JP   0
HEXBYTE:  PUSH AF
          RRCA
          RRCA
          RRCA
          RRCA
          CALL HEXSIFFRA
          POP  AF
HEXSIFFRA:
          AND  $0F
          ADD  A,'0'
          CP   '9'+1
          JR   C,HEXUT
          ADD  A,'A'-'9'-1
HEXUT:    LD   C,A
          JP   KONUT

; ---------------------------------------------------------------------
; Texterna (] [ \ } { | är Å Ä Ö å ä ö på ABC80)

T_ANVANDNING: DEFB "Anv{ndning: ZTOLK fil (typen Z3 om ingen ges)",0
T_HITTAR_INTE: DEFB "Hittar inte filen",0
T_VERSION:  DEFB "Filen {r inte Z-maskin version 3",0
T_MINNE:    DEFB "F|r lite minne",0
T_OGILTIG:  DEFB "Ok{nd instruktion",0
T_STACK:    DEFB "Stacken {r full",0
T_NOLL:     DEFB "Division med noll",0
T_DYNAMISKT: DEFB "Skriver utanf|r det dynamiska minnet",0
T_EGENSKAP: DEFB "Egenskapen finns inte",0
T_RETUR:    DEFB "Retur fr}n huvudrutinen",0
T_FEL:      DEFB "ZTOLK: ",0
T_VID:      DEFB " vid ",0
T_CRLF:     DEFB CR,LF,0
T_MER:      DEFB "(MER)",0
T_POANG:    DEFB "Score: ",0
T_DRAG:     DEFB "  Moves: ",0
T_POANG40:  DEFB "Score:",0
T_DRAG40:   DEFB " Moves:",0
T_TID:      DEFB "Time: ",0
T_FIL:      DEFB "Fil (",0
T_FIL2:     DEFB "): ",0
T_KAN_INTE_SPARA: DEFB "Kan inte spara.",CR,0
T_KAN_INTE_LASA: DEFB "Kan inte l{sa filen.",CR,0
T_ANNAT_SPEL: DEFB "Filen h|r till ett annat spel.",CR,0

; ---------------------------------------------------------------------
; Variablerna

IGANG:    DEFB 0                ; 1 när spelet har börjat
BREDD:    DEFB 0                ; KOLUMNER - 1
TOPP:     DEFB 0                ; BDOS:ens sida
STATBAS:  DEFW 0
DYNSIDOR: DEFB 0
FORSTA:   DEFB 0                ; första platsen som lånas ut
NASTA:    DEFB 0                ; nästa plats att låna ut
FILLANGD: DEFB 0,0,0
FILSIDOR: DEFW 0
LASPLATS: DEFB 0
GLOBFYS:  DEFW 0
OBJSTD:   DEFW 0
OBJBAS:   DEFW 0
FORKTAB:  DEFW 0
POSTLANGD: DEFB 0
POSTANTAL: DEFW 0
POSTSTART: DEFW 0
PCPEK:    DEFW 0
PCSIDA:   DEFW 0
FP:       DEFW 0
ZSP:      DEFW 0
OPKOD:    DEFB 0
OPANTAL:  DEFB 0
OPPEK:    DEFW 0
OPS:
OP1:      DEFW 0
OP2:      DEFW 0
OP3:      DEFW 0
OP4:      DEFW 0
ANRLAGRA: DEFB 0
ANRHOG:   DEFB 0
ANRPEK:   DEFW 0
ARGPEK:   DEFW 0
ARGKVAR:  DEFB 0
KVOTTECKEN: DEFB 0
RESTTECKEN: DEFB 0
FRO:      DEFW 1
POLLRAKNARE: DEFB 64
KO_IN:    DEFB 0                ; tangentkön: KO_IN där nästa läggs,
KO_UT:    DEFB 0                ; KO_UT där nästa tas
KO:       DEFS KOSTORLEK
TOBJ:     DEFB 0
OBJNOLL:  DEFS 9
ALFABET:  DEFB 0
FORKORT:  DEFB 0
FLYKT:    DEFB 0
FLYKTHOG: DEFB 0
ZRAD:     DEFB 0
KOLUMN:   DEFB 0
RADER:    DEFB 0
MJUK:     DEFB 0
ORDLANGD: DEFB 0
STATUSLAGE: DEFB 0
STATUSPEK: DEFW 0
STATUSSLUT: DEFW 0
TEXTPEK:  DEFW 0
PARSPEK:  DEFW 0
PARSUT:   DEFW 0
MAXORD:   DEFB 0
ANTORD:   DEFB 0
ORDSTART: DEFW 0
ORDLANGD2: DEFB 0
ZANTAL:   DEFB 0
ZTECK:    DEFS 6
ZKOD:     DEFS 4
LAG:      DEFW 0
HOG:      DEFW 0
MITT:     DEFW 0
SUMMA:    DEFW 0
VERPEK:   DEFW 0
SIFFROR:  DEFS 6
SPELIDBUF: DEFS 10
SPELFCB:  DEFS 36
SPARFCB:  DEFS 36
SLUT:

; Minnet efter programmet (inte i filen).
SEPAR     EQU  SLUT             ; antalet avgränsare och dem (32)
ORDBUF    EQU  SEPAR+32         ; ordet som väntar
STATUSBUF EQU  ORDBUF+ORDMAX    ; statusraden (80)
HOGERBUF  EQU  STATUSBUF+80     ; statusradens högra del (40)
NAMNBUF   EQU  HOGERBUF+40      ; filnamnet (16)
POSTBUF   EQU  (NAMNBUF+16+127)&$FF80 ; en post (128), inom en sida
LAGE      EQU  POSTBUF+128      ; läget som sparas (128) och sedan
ZSTACK    EQU  LAGE+128         ; Z-stacken, i en följd
ZSTACKSLUT EQU ZSTACK+ZSTACKSTORLEK
STACK     EQU  ZSTACKSLUT+64+256 ; Z80:ns stack
SIDKARTA  EQU  (STACK+255)&$FF00 ; 512 sidor
SLOTLAG   EQU  SIDKARTA+512
SLOTHOG   EQU  SLOTLAG+256
DYN       EQU  SLOTHOG+256      ; spelets minne, från sida 0
DYNHI     EQU  DYN>>8

          END
