; abc80.asm -- BBC BASIC (Z80) på ABC80 med UNI-80, till boken ABC80 inifrån
;
; Sidan $0100-$01FF i BBCBASIC.COM är den del som R. T. Russell lämnar
; åt den som anpassar BASIC till en dator (hans DIST.Z80). Det här är vår
; version av den, för vår BIOS (cpm/bios.asm); resten av BBCBASIC.COM är
; oförändrad (se LASMIG.md). Makefile lägger sidan över den första sidan
; i BBCBASIC.COM och sätter först in BIOS:ens variabler KOLUMNER, RAD och
; KOL med adresserna ur cpm/bios.lst.
;
; Hopptabellen på $0100 och tabellen på $01F0 ligger där BASIC väntar
; sig dem. Ändrat mot DIST.Z80:
;   CLS     ^Z, som raderar skärmen (BIOS:ens ADM-3A-koder)
;   PCSR    ESC = rad+32 kolumn+32
;   GCSR    markören ur BIOS:ens RAD och KOL, för POS och VPOS
;   TICKS   CP/M-kartan har ingen klocka (NMI kommer bara i systemkartan,
;           uni80.h), så TIME räknas fram: varje läsning väntar 10 ms
;           och ökar räknaren med 1. BASIC läser den medan den väntar på
;           en tangent, så där går TIME som en klocka, och en väntan som
;           REPEAT UNTIL TIME>T eller INKEY(100) tar ungefär rätt tid;
;           medan ett program räknar står den still.
;   INIT    WIDTH-byten på $01F4 sätts till 40 eller 80 efter skärmen.
;           Kontrollerna av Z80 och CP/M 2 är borta (det är alltid
;           båda här), för koden ska få plats under $01F0.
;   tangenterna  markören åt höger med ABC80:s högerpil ($09).

CPM       EQU  5
COLD      EQU  $0200

          ORG  $0100

; Hopptabellen: BASIC anropar datorns delar härigenom.
          JP   INIT
CLRSCN:   JP   CLS              ; radera skärmen
PUTCSR:   JP   PCSR             ; flytta markören
GETCSR:   JP   GCSR             ; var markören står
PUTIME:   JP   PTIME            ; sätt TIME
GETIME:   JP   GTIME            ; läs TIME
GETKEY:   JP   INKEY            ; en tangent inom en tid
BYE:      JP   0                ; tillbaka till CP/M

; BDOS: CP/M-anrop som behåller IX och IY.
BDOS:     PUSH IX
          PUSH IY
          CALL CPM
          POP  IY
          POP  IX
          RET

; INIT: sätter bredden och startar BASIC.
INIT:     LD   A,(KOLUMNER)
          LD   (BREDD),A
          JP   COLD

; GTIME: DEHL = TIME (hundradelar). Ändrar A, DE, HL och flaggorna.
GTIME:    PUSH BC
          CALL TICKS
          LD   BC,(OFFLO)
          ADD  HL,BC
          EX   DE,HL
          LD   BC,(OFFHI)
          ADC  HL,BC
          EX   DE,HL
          POP  BC
          RET

; PTIME: TIME = DEHL. Ändrar A, DE, HL och flaggorna.
PTIME:    PUSH BC
          PUSH DE
          PUSH HL
          CALL TICKS
          LD   B,H
          LD   C,L
          POP  HL
          OR   A
          SBC  HL,BC
          LD   (OFFLO),HL
          LD   B,D
          LD   C,E
          POP  HL
          SBC  HL,BC
          LD   (OFFHI),HL
          POP  BC
          RET

; TICKS: väntar 10 ms och ger DEHL = räknaren + 1. 1 152 varv om 26 T
; är 29 952 T, 10 ms vid 2,9952 MHz. Ändrar A, BC, DE, HL och flaggorna.
TICKS:    LD   BC,1152
VANTA:    DEC  BC
          LD   A,B
          OR   C
          JR   NZ,VANTA
          LD   HL,(RAKNARE)
          LD   DE,(RAKNARE+2)
          INC  HL
          LD   A,H
          OR   L
          JR   NZ,TICKS2
          INC  DE
TICKS2:   LD   (RAKNARE),HL
          LD   (RAKNARE+2),DE
          RET

; INKEY: väntar högst HL hundradelar på en tangent. Carry: A = tangenten,
; annars tog tiden slut. Ändrar A, DE, HL och flaggorna.
INKEY:    PUSH BC
          PUSH HL
          CALL TICKS
          POP  DE
          ADD  HL,DE
INKEY1:   PUSH HL
          LD   C,6
          LD   E,$FF
          CALL BDOS
          POP  HL
          OR   A
          SCF
          JR   NZ,INKEY2
          PUSH HL
          CALL TICKS
          POP  DE
          SBC  HL,DE
          EX   DE,HL
          JR   C,INKEY1
INKEY2:   POP  BC
          RET

; CLS: radera skärmen. Ändrar A, DE, HL och flaggorna.
CLS:      LD   A,$1A
          JR   COUT

; PCSR: markören till kolumn DE och rad HL (0 överst till vänster).
PCSR:     LD   A,27
          CALL COUT
          LD   A,'='
          CALL COUT
          LD   A,L
          ADD  A,32
          CALL COUT
          LD   A,E
          ADD  A,32
          JR   COUT

; GCSR: DE = kolumnen (POS), HL = raden (VPOS).
GCSR:     LD   A,(KOL)
          LD   E,A
          LD   D,0
          LD   A,(RAD)
          LD   L,A
          LD   H,0
          RET

; COUT: tecknet i A till konsolen. Ändrar A och flaggorna.
COUT:     PUSH BC
          PUSH DE
          PUSH HL
          LD   E,A
          LD   C,2
          CALL BDOS
          POP  HL
          POP  DE
          POP  BC
          RET

RAKNARE:  DEFW 0,0              ; TICKS

          DEFS $01F0-$         ; fel om koden går förbi $01F0

; Tabellen som BASIC läser.
OFFLO:    DEFW 0                ; TIME minus räknaren, låga ordet
OFFHI:    DEFW 0                ; och det höga
BREDD:    DEFB 80               ; skärmens bredd, sätts av INIT
          DEFB 'G' & $1F        ; markören upp
          DEFB 'O' & $1F        ; markören ned
          DEFB 'F' & $1F        ; radens början
          DEFB 'N' & $1F        ; radens slut
          DEFB 'X' & $1F        ; radera till radens slut
          DEFB $08              ; vänster och radera
          DEFB 'U' & $1F        ; radera till radens början
          DEFB 'J' & $1F        ; markören vänster
          DEFB $09              ; markören höger: ABC80:s högerpil
          DEFB 'R' & $1F        ; radera tecknet
          DEFB 'Q' & $1F        ; infoga eller skriv över

          END
