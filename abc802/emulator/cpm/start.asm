; start.asm -- laddaren i BASICINI.SYS, CP/M 2.2 på ABC 802, till ABC 802 inifrån
;
; Vår egen laddare; ingenting här kommer från MyAB. DOS v.20 i ROM:en
; öppnar BASICINI.SYS vid starten (OPNINI, se listningen i ../../listning/)
; och läser post 0 till kanal 0:s sektorbuffert på $F500. Är sektorns
; byte 3 (postens första byte) JP, körs posten från $F503. Laddaren är
; alltså posten, högst 253 bytes.
;
; Gången:
;   1. Sektorerna SYSSEKTOR och framåt på kort 44 enhet 0 (MF0, skivan
;      som laddaren kom från), ANTAL stycken, läses till LADDA ($8000),
;      som alltid är RAM.
;   2. Fyra instruktioner (FLYTT) kopieras till $A000 och körs där: DTR
;      på DART B (WR5 $E8, LRS*) ger 64 KB RAM, systemet flyttas till
;      $E000 och uppåt, och BIOS:ens BOOT på $F600 tar över. Laddaren
;      själv ligger i det som skrivs över, därav kopian.
; Går läsningen fel återvänder laddaren till DOS:en med HL = $6F00, och
; maskinen startar BASIC som utan BASICINI.SYS.
;
; Sektorerna räknas i följd i avbilden (0-1279). Kort 44 vill ha
; sektor N som 32 * (N / 4) + (N AND 3), i två bytes (karna/skivkort.c).
; Kommandot är fyra bytes på port 0 efter OUT 1 (kortet) och OUT 2 (C1,
; nytt kommando): 3 (läs), enheten, sektorns höga och låga byte. IN 1 är
; statusen: bit 1 = 0 medan kommandot tas emot, bit 2 = 0 under data,
; bit 0 = en byte kan läsas, bit 7 = klart (slutstatusen på IN 0, 0 =
; rätt).

MF        EQU  44               ; skivkortet
SYSSEKTOR EQU  40               ; systemets första sektor (cpm/skiva.py)
ANTAL     EQU  32               ; sektorer, 8 KB: $E000-$FFFF
LADDA     EQU  $8000
SYSTEM    EQU  $E000
BOOT      EQU  $F600
TRAMPOLIN EQU  $A000

          ORG  $F503

          JP   START            ; DOS:en kör posten bara om den börjar med JP

START:    DI
          LD   HL,LADDA
          LD   DE,SYSSEKTOR
          LD   B,ANTAL
NASTA:    PUSH BC
          CALL LAS
          POP  BC
          JR   NZ,FEL
          INC  DE
          DJNZ NASTA
          LD   HL,FLYTT
          LD   DE,TRAMPOLIN
          LD   BC,FLYTTLANGD
          LDIR
          JP   TRAMPOLIN

FEL:      LD   HL,$6F00         ; tillbaka till DOS:en och BASIC
          RET

; FLYTT körs på TRAMPOLIN. Utan hopp, så den går att köra var som helst.
FLYTT:    LD   A,5              ; DART B WR5: DTR på, 64 KB RAM
          OUT  ($23),A
          LD   A,$E8
          OUT  ($23),A
          LD   HL,LADDA
          LD   DE,SYSTEM
          LD   BC,ANTAL*256
          LDIR
          JP   BOOT
FLYTTLANGD EQU $-FLYTT

; LAS: läs sektor DE på MF0 till (HL) och framåt. Z om det gick bra,
; HL efter sektorn. Ändrar A, B och C.
LAS:      PUSH HL
          LD   A,E              ; HL = 32 * (DE / 4) + (DE AND 3)
          AND  3
          LD   C,A
          LD   A,E
          AND  $FC
          LD   L,A
          LD   H,D
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,HL
          LD   A,L
          OR   C
          LD   L,A
          LD   A,MF
          OUT  (1),A
          OUT  (2),A            ; C1: nytt kommando
          LD   B,0              ; tidsgräns, 256 varv
TAREMOT:  IN   A,(1)
          BIT  1,A
          JR   Z,SKICKA
          DJNZ TAREMOT
          POP  HL
          INC  B                ; inget svar: NZ
          RET
SKICKA:   LD   A,3              ; läs
          OUT  (0),A
          XOR  A                ; enhet 0
          OUT  (0),A
          LD   A,H
          OUT  (0),A
          LD   A,L
          OUT  (0),A
          POP  HL
VANTA:    IN   A,(1)
          BIT  7,A              ; klart utan data, alltså fel
          JR   NZ,STATUS
          BIT  2,A
          JR   NZ,VANTA
          LD   B,0              ; 256 bytes
BYTE:     IN   A,(1)
          RRCA
          JR   NC,BYTE
          IN   A,(0)
          LD   (HL),A
          INC  HL
          DJNZ BYTE
KLART:    IN   A,(1)
          BIT  7,A
          JR   Z,KLART
STATUS:   IN   A,(0)
          OR   A
          RET

SLUT      EQU  $
