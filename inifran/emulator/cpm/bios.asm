; bios.asm -- BIOS:en till CP/M 2.2 på ABC80 med UNI-80, till boken ABC80 inifrån
;
; Vår egen BIOS; ingenting här kommer från MyAB. CCP och BDOS är Digital
; Researchs CP/M 2.2 (den fria utgåvan, se cpm/systemskiva.py), översatta
; för 63 KB: CCP på $E000, BDOS på $E806 och BIOS:en på $F600.
;
; Minnet i CP/M-kartan (1) är kortets 64 KB:
;   $0000-$00FF  sidan noll (hoppen till BIOS och BDOS, FCB, bufferten)
;   $0100-$DFFF  programmen (TPA, 56 KB)
;   $E000-$F5FF  CCP och BDOS, som läses om från skivan vid varmstart
;   $F600-$FF6F  BIOS:en med buffertarna
;   $FF70-$FFFF  det gemensamma minnet, samma i alla kartor (uni80.h)
;
; Bara skärmen och RAM-skivan ligger i ABC80:s eget minne, och det syns
; bara i systemkartan (2). En kort rutin i det gemensamma minnet,
; SYSFLYTT, byter till systemkartan, flyttar ett block med LDIR och
; byter tillbaka; all annan kod körs i CP/M-kartan. Stacken byts till
; det gemensamma minnet under tiden, för i systemkartan kommer NMI var
; 20:e ms, och ROM:ens NMI-rutin (klockan) lägger fyra bytes på
; stacken. Portarna syns i alla kartor, så tangentbordet, skivkortet
; och V24 sköts direkt härifrån. Avbrotten är av hela tiden.
;
; Enheterna:
;   konsolen  tangentbordet på port $38 (bit 7 = en tangent är nere),
;             skärmen 24 rader om 40 tecken, eller 80 med TKN80, med
;             ett urval av ADM-3A:s koder (se KONUT)
;   A: B:     FD2 på kort 45, enhet 0 och 1: 40 spår om 8 sektorer à
;             256 bytes. CP/M:s sektorer om 128 bytes, 16 per spår, är
;             halvor av dem. Spår 0-3 är systemet (startblocket i spår 0
;             sektor 0, se cpm/start.asm, sedan CCP, BDOS och BIOS:en),
;             spår 4-39 är 72 block om 1 KB med 64 katalogposter.
;   C:        RAM-skivan i ABC80:s eget RAM från den lägsta sida som går
;             att skriva ($C000 med 16 KB, $8000 med 32 KB) till $FBFF:
;             15 eller 31 block om 1 KB, 32 katalogposter. Den töms vid
;             kallstart. $FC00-$FFFF lämnas åt ROM:en (NMI-klockan).
;   LST: PUN: V24 på PIO B, som BASIC:s PR: (karna/skrivarkort.h): en
;             OUT per bit på bit 3, 1 200 baud.
;   RDR:      ger alltid ^Z (slut).

KARTPORT  EQU  $EF              ; UNI-80:s port för kartan
CPMKARTA  EQU  1
SYSKARTA  EQU  2

CCP       EQU  $E000
BDOS      EQU  $E806
SYSSEKTORER EQU 22              ; CCP och BDOS, $1600 bytes i sektorer om 256
IOBYTE    EQU  3
AKTUELL   EQU  4                ; aktuella enheten (och användaren)
TBUFF     EQU  $80              ; standardbufferten

FD2       EQU  45               ; skivkortet

TANGENTER EQU  $38              ; PIO A: koden, bit 7 = nere
PIOA_STYR EQU  $39
V24       EQU  $3A              ; PIO B: bit 3 TxD ut, bit 1 CTS in (0 = klar)
PIOB_STYR EQU  $3B

BILD40    EQU  $7C00            ; ABC80:s bildminne, 1 KB
BILD80    EQU  $5800            ; TKN80:s, 2 KB
RAMSLUT   EQU  $FC00            ; RAM-skivan slutar under den här adressen

; Det gemensamma minnet ($FF70-$FFFF).
SYSFLYTT  EQU  $FF70            ; rutinen (FLYTTKOD kopieras hit)
GEM_SP    EQU  $FF90            ; stackpekaren under tiden
GEM_BYTE  EQU  $FF92            ; en byte till eller från ABC80:s minne
GEM_STACK EQU  $FFA0            ; stacken under tiden, växer nedåt
RAMBUF    EQU  $FFC0            ; 64 bytes till RAM-skivan

          ORG  $F600

; De 17 ingångarna i CP/M 2.2, i den ordning BDOS:en väntar sig.
          JP   KALLSTART        ; BOOT
VARMHOPP: JP   VARMSTART        ; WBOOT
          JP   KONSTAT          ; CONST
          JP   KONIN            ; CONIN
          JP   KONUT            ; CONOUT
          JP   SKRIVARE         ; LIST
          JP   SKRIVARE         ; PUNCH, samma V24
          JP   LASARE           ; READER
          JP   HEM              ; HOME
          JP   VALJ             ; SELDSK
          JP   SPAR             ; SETTRK
          JP   SEKTOR           ; SETSEC
          JP   DMA              ; SETDMA
          JP   LAS              ; READ
          JP   SKRIV            ; WRITE
          JP   SKRIVARSTATUS    ; LISTST
          JP   OVERSATT         ; SECTRAN

; ----------------------------------------------------------------------
; Start

KALLSTART:
          DI
          LD   SP,BIOSSTACK
          LD   A,$03            ; PIO A: inga avbrott från tangentbordet
          OUT  (PIOA_STYR),A
          LD   A,$CF            ; PIO B: bitläge, bit 0-2 och 7 in
          OUT  (PIOB_STYR),A
          LD   A,$87
          OUT  (PIOB_STYR),A
          LD   A,$18            ; TxD i vila
          OUT  (V24),A
          LD   HL,FLYTTKOD      ; SYSFLYTT till det gemensamma minnet
          LD   DE,SYSFLYTT
          LD   BC,FLYTTLANGD
          LDIR
          CALL SKARMSTART
          CALL RAMSTART
          XOR  A
          LD   (IOBYTE),A
          LD   (AKTUELL),A
          LD   HL,HALSNING
          CALL TEXT
          JR   GA

VARMSTART:
          DI
          LD   SP,BIOSSTACK
          XOR  A
          LD   (BUF_GILTIG),A   ; skivan kan ha bytts
OMLAS:    LD   HL,CCP           ; CCP och BDOS, sektor 1 och framåt på A:
          LD   DE,1*32          ; spår 0, sektor 1 (sektor * 32)
          LD   B,SYSSEKTORER
SYSSEKTOR:
          PUSH BC
          XOR  A
          LD   (ENHET),A
          CALL FD2LAS
          POP  BC
          JR   NZ,SYSFEL
          LD   A,E              ; nästa sektor, och efter sektor 7 nästa spår
          ADD  A,32
          LD   E,A
          JR   NC,SAMMASPAR
          INC  D
SAMMASPAR:
          DJNZ SYSSEKTOR
          JR   GA
SYSFEL:   LD   HL,SYSFELTEXT
          CALL TEXT
          CALL KONIN
          JR   OMLAS

; Sidan noll och till CCP med den aktuella enheten i C.
GA:       LD   A,$C3            ; JP
          LD   ($0000),A
          LD   HL,VARMHOPP
          LD   ($0001),HL
          LD   ($0005),A
          LD   HL,BDOS
          LD   ($0006),HL
          LD   BC,TBUFF
          CALL DMA
          LD   A,(AKTUELL)
          AND  $0F              ; enheten; finns den inte, A:
          CP   3
          JR   C,ENHETFINNS
          XOR  A
ENHETFINNS:
          LD   C,A
          LD   A,(AKTUELL)
          AND  $F0
          OR   C
          LD   (AKTUELL),A
          LD   C,A
          JP   CCP

HALSNING: DEFB 13,10,"CP/M 2.2 p} ABC80 med UNI-80",13,10,0
SYSFELTEXT:
          DEFB 13,10,"Kan inte l{sa systemet p} A:, tryck en tangent",0

; TEXT: skriv texten på (HL), som slutar med 0.
TEXT:     LD   A,(HL)
          OR   A
          RET  Z
          PUSH HL
          LD   C,A
          CALL KONUT
          POP  HL
          INC  HL
          JR   TEXT

; ----------------------------------------------------------------------
; Det gemensamma minnet

; FLYTTKOD körs på SYSFLYTT: flytta BC bytes från (HL) till (DE) i
; systemkartan, med LDIR. Ändrar A, BC, DE och HL som LDIR. Koden har
; inga hopp och kan därför assembleras här och köras där.
FLYTTKOD: LD   (GEM_SP),SP
          LD   SP,GEM_STACK
          LD   A,SYSKARTA
          OUT  (KARTPORT),A
          LDIR
          LD   A,CPMKARTA
          OUT  (KARTPORT),A
          LD   SP,(GEM_SP)
          RET
FLYTTLANGD EQU $-FLYTTKOD

; LASSYS: A = byten på (HL) i systemkartan. Ändrar inget annat.
LASSYS:   PUSH BC
          PUSH DE
          PUSH HL
          LD   DE,GEM_BYTE
          LD   BC,1
          CALL SYSFLYTT
          POP  HL
          POP  DE
          POP  BC
          LD   A,(GEM_BYTE)
          RET

; SKRIVSYS: skriv A på (HL) i systemkartan. Ändrar inget annat.
SKRIVSYS: PUSH AF
          PUSH BC
          PUSH DE
          PUSH HL
          LD   (GEM_BYTE),A
          EX   DE,HL
          LD   HL,GEM_BYTE
          LD   BC,1
          CALL SYSFLYTT
          POP  HL
          POP  DE
          POP  BC
          POP  AF
          RET

; FYLLSYS: fyll BC bytes (minst 1) från (HL) med A i systemkartan.
; Ändrar AF, BC, DE och HL.
FYLLSYS:  CALL SKRIVSYS
          DEC  BC
          LD   A,B
          OR   C
          RET  Z
          LD   D,H
          LD   E,L
          INC  DE
          JP   SYSFLYTT         ; LDIR som för byten vidare

; ----------------------------------------------------------------------
; Tangentbordet

; KOLLA: har en ny tangent tryckts ned sedan förra gången? Då hamnar
; koden i TANGENT och HAR_TANGENT blir 1. Ändrar A.
KOLLA:    IN   A,(TANGENTER)
          OR   A
          JP   P,UPPE
          LD   A,(NERE)
          OR   A
          RET  NZ               ; samma tangent, fortfarande nere
          LD   A,1
          LD   (NERE),A
          LD   (HAR_TANGENT),A
          IN   A,(TANGENTER)
          AND  $7F
          LD   (TANGENT),A
          RET
UPPE:     XOR  A
          LD   (NERE),A
          RET

KONSTAT:  CALL KOLLA
          LD   A,(HAR_TANGENT)
          OR   A
          RET  Z
          LD   A,$FF
          RET

KONIN:    CALL KOLLA
          LD   A,(HAR_TANGENT)
          OR   A
          JR   Z,KONIN
          XOR  A
          LD   (HAR_TANGENT),A
          LD   A,(TANGENT)
          RET

; ----------------------------------------------------------------------
; Skärmen
;
; Koderna (som ADM-3A, och ESC T, ESC Y och ESC * som TeleVideo):
;   CR ($0D)       till radens början
;   LF ($0A)       ned en rad; på sista raden flyttas skärmen upp
;   BS ($08)       vänster
;   ^L ($0C)       höger
;   ^K ($0B)       upp
;   ^Z ($1A)       radera skärmen, markören överst till vänster
;   ^^ ($1E)       markören överst till vänster
;   ESC = r c      markören till rad r - 32, kolumn c - 32
;   ESC T          radera till radens slut
;   ESC Y          radera till skärmens slut
;   ESC *          radera skärmen
;   ESC E          infoga en tom rad där markören står; raderna under
;                  flyttas ned och den sista försvinner
;   ESC R          radera raden där markören står; raderna under flyttas
;                  upp och den sista blir tom
;   (ESC E och ESC R lämnar markören först på raden)
; Andra koder under $20 och $7F syns inte. $20-$7E skrivs som de är; i
; ABC80:s teckensats är [\]{|} ÄÖÅäöå. Efter sista kolumnen fortsätter
; texten på nästa rad. Markören är bit 7 i tecknet där den står.

; SKARMSTART: 40 eller 80 tecken, radernas adresser, tom skärm.
SKARMSTART:
          IN   A,(4)            ; TKN80: 80 tecken, om kortet finns
          LD   B,2              ; två prov: TKN80:s $5C00 är ABC80:s $7C00
TKN80PROV:
          LD   A,B
          RRCA                  ; $01 och $80
          LD   HL,BILD80+$400
          CALL SKRIVSYS
          LD   C,A
          LD   HL,BILD40
          CALL LASSYS
          CP   C
          JR   NZ,UTAN_TKN80
          DJNZ TKN80PROV
          LD   HL,BILD80
          LD   DE,$100          ; avståndet mellan rad 0, 1 ... 7
          LD   A,80
          JR   RADADRESSER
UTAN_TKN80:
          LD   HL,BILD40
          LD   DE,$80
          LD   A,40
RADADRESSER:
          LD   (BILDMINNE),HL
          LD   (KOLUMNER),A
          LD   IX,RADTABELL     ; rad r: bildminnet + (r & 7) * DE
          LD   C,3              ;        + (r >> 3) * kolumnerna
TREDJEDEL:
          PUSH HL
          LD   B,8
ATTARADER:
          LD   (IX+0),L
          LD   (IX+1),H
          INC  IX
          INC  IX
          ADD  HL,DE
          DJNZ ATTARADER
          POP  HL
          LD   A,(KOLUMNER)
          ADD  A,L
          LD   L,A
          JR   NC,NASTA_TREDJEDEL
          INC  H
NASTA_TREDJEDEL:
          DEC  C
          JR   NZ,TREDJEDEL
          CALL RENSA
          JP   MARKOR_PA

; KONUT: skriv tecknet i C.
KONUT:    CALL MARKOR_AV
          LD   A,C
          CALL TECKEN
          JP   MARKOR_PA

TECKEN:   LD   B,A
          LD   A,(ESCLAGE)
          OR   A
          JP   NZ,ESCAPE
          LD   A,B
          CP   $20
          JR   C,STYRKOD
          CP   $7F
          RET  Z
          CALL ADRESS           ; ett vanligt tecken
          CALL SKRIVSYS
          LD   A,(KOLUMNER)
          LD   B,A
          LD   A,(KOL)
          INC  A
          LD   (KOL),A
          CP   B
          RET  C
          XOR  A                ; sista kolumnen: nästa rad
          LD   (KOL),A
          JR   NYRAD

STYRKOD:  CP   13
          JR   Z,VAGNRETUR
          CP   10
          JR   Z,NYRAD
          CP   8
          JR   Z,VANSTER
          CP   12
          JR   Z,HOGER
          CP   11
          JR   Z,UPP
          CP   26
          JR   Z,RENSA
          CP   30
          JR   Z,HEMMA
          CP   27
          RET  NZ
          LD   A,1
          LD   (ESCLAGE),A
          RET

VAGNRETUR:
          XOR  A
          LD   (KOL),A
          RET

NYRAD:    LD   A,(RAD)
          CP   23
          JR   NC,RULLA
          INC  A
          LD   (RAD),A
          RET

VANSTER:  LD   A,(KOL)
          OR   A
          RET  Z
          DEC  A
          LD   (KOL),A
          RET

HOGER:    LD   A,(KOLUMNER)
          LD   B,A
          LD   A,(KOL)
          INC  A
          CP   B
          RET  NC
          LD   (KOL),A
          RET

UPP:      LD   A,(RAD)
          OR   A
          RET  Z
          DEC  A
          LD   (RAD),A
          RET

RENSA:    LD   HL,(BILDMINNE)   ; hela bildminnet, 1 eller 2 KB
          LD   BC,$400
          LD   A,(KOLUMNER)
          CP   80
          JR   NZ,RENSA_ALLT
          LD   B,$08
RENSA_ALLT:
          LD   A,' '
          CALL FYLLSYS
HEMMA:    XOR  A
          LD   (RAD),A
          LD   (KOL),A
          RET

; RULLA: skärmen en rad upp, rad 1-23 till rad 0-22 och sista raden
; tom. RADERA_RAD gör samma sak från markörens rad (ESC R).
RULLA:    XOR  A
          JR   UPP_FRAN
RADERA_RAD:
          XOR  A
          LD   (KOL),A
          LD   A,(RAD)
UPP_FRAN: LD   C,A              ; raden A + 1 till A och så vidare
          ADD  A,A
          LD   E,A
          LD   D,0
          LD   IX,RADTABELL
          ADD  IX,DE
          LD   A,23
          SUB  C
          JR   Z,TOM_SISTA
          LD   B,A
RULLA_RAD:
          PUSH BC
          LD   E,(IX+0)
          LD   D,(IX+1)
          LD   L,(IX+2)
          LD   H,(IX+3)
          CALL RADKOPIA
          INC  IX
          INC  IX
          POP  BC
          DJNZ RULLA_RAD
TOM_SISTA:
          LD   L,(IX+0)
          LD   H,(IX+1)
TOM_RADEN:
          LD   A,(KOLUMNER)
          LD   C,A
          LD   B,0
          LD   A,' '
          JP   FYLLSYS

; INFOGA_RAD: raderna från markörens en ned, den sista försvinner, och
; markörens rad tom (ESC E).
INFOGA_RAD:
          XOR  A
          LD   (KOL),A
          LD   IX,RADTABELL+2*23
          LD   A,(RAD)
          LD   C,A
          LD   A,23
          SUB  C
          JR   Z,TOM_SISTA
          LD   B,A
NED_RAD:  PUSH BC
          LD   E,(IX+0)
          LD   D,(IX+1)
          LD   L,(IX-2)
          LD   H,(IX-1)
          CALL RADKOPIA
          DEC  IX
          DEC  IX
          POP  BC
          DJNZ NED_RAD
          JR   TOM_SISTA

; RADKOPIA: en rad från (HL) till (DE) i bildminnet.
RADKOPIA: LD   A,(KOLUMNER)
          LD   C,A
          LD   B,0
          JP   SYSFLYTT

; ESC och det som följer.
ESCAPE:   CP   2
          JR   Z,ESC_RAD
          CP   3
          JR   Z,ESC_KOL
          XOR  A                ; tecknet efter ESC
          LD   (ESCLAGE),A
          LD   A,B
          CP   '='
          JR   Z,ESC_LIKA
          CP   'T'
          JR   Z,RADSLUT
          CP   'Y'
          JR   Z,SKARMSLUT
          CP   '*'
          JP   Z,RENSA
          CP   'E'
          JP   Z,INFOGA_RAD
          CP   'R'
          JP   Z,RADERA_RAD
          RET
ESC_LIKA: LD   A,2
          LD   (ESCLAGE),A
          RET
ESC_RAD:  LD   A,B
          SUB  32
          LD   (ESCRAD),A
          LD   A,3
          LD   (ESCLAGE),A
          RET
ESC_KOL:  XOR  A
          LD   (ESCLAGE),A
          LD   A,(ESCRAD)       ; utanför skärmen: kanten
          CP   24
          JR   C,ESC_RADEN
          LD   A,23
ESC_RADEN:
          LD   (RAD),A
          LD   A,(KOLUMNER)
          LD   C,A
          LD   A,B
          SUB  32
          CP   C
          JR   C,ESC_KOLUMNEN
          LD   A,C
          DEC  A
ESC_KOLUMNEN:
          LD   (KOL),A
          RET

; Radera till radens slut.
RADSLUT:  CALL ADRESS
          LD   A,(KOL)
          LD   B,A
          LD   A,(KOLUMNER)
          SUB  B
          LD   C,A
          LD   B,0
          LD   A,' '
          JP   FYLLSYS

; Radera till skärmens slut: radens slut och raderna under.
SKARMSLUT:
          CALL RADSLUT
          LD   A,(RAD)
TOM_RAD:  INC  A
          CP   24
          RET  NC
          PUSH AF
          ADD  A,A
          LD   E,A
          LD   D,0
          LD   HL,RADTABELL
          ADD  HL,DE
          LD   A,(HL)
          INC  HL
          LD   H,(HL)
          LD   L,A
          LD   A,(KOLUMNER)
          LD   C,A
          LD   B,0
          LD   A,' '
          CALL FYLLSYS
          POP  AF
          JR   TOM_RAD

; ADRESS: HL = markörens plats i bildminnet. Ändrar DE.
ADRESS:   PUSH AF
          LD   A,(RAD)
          ADD  A,A
          LD   E,A
          LD   D,0
          LD   HL,RADTABELL
          ADD  HL,DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          LD   A,(KOL)
          LD   L,A
          LD   H,0
          ADD  HL,DE
          POP  AF
          RET

MARKOR_PA:
          CALL ADRESS
          CALL LASSYS
          OR   $80
          JP   SKRIVSYS

MARKOR_AV:
          PUSH BC
          CALL ADRESS
          CALL LASSYS
          AND  $7F
          CALL SKRIVSYS
          POP  BC
          RET

; ----------------------------------------------------------------------
; V24: LST: och PUN:

; SKRIVARE: tecknet i C på V24, 8 bitar utan paritet, en stoppbit.
SKRIVARE: LD   A,$08            ; vila
          CALL BIT_UT
          XOR  A                ; startbiten
          CALL BIT_UT
          LD   B,8
DATABIT:  LD   A,C
          RRCA
          LD   C,A
          AND  $80              ; biten till bit 3
          RRCA
          RRCA
          RRCA
          RRCA
          CALL BIT_UT
          DJNZ DATABIT
          LD   A,$08            ; stoppbiten
          CALL BIT_UT
          LD   A,$18            ; tecknet är slut
          OUT  (V24),A
          RET

; BIT_UT: A på V24 och vänta en bittid, 1/1 200 s = 2 496 T.
BIT_UT:   OUT  (V24),A
          LD   A,152            ; 152 * 16 T och ca 60 T runt omkring
BITTID:   DEC  A
          JR   NZ,BITTID
          RET

; SKRIVARSTATUS: $FF om skrivaren är klar (CTS, bit 1 = 0), annars 0.
SKRIVARSTATUS:
          IN   A,(V24)
          AND  2
          LD   A,$FF
          RET  Z
          XOR  A
          RET

LASARE:   LD   A,$1A            ; ^Z: inget att läsa
          RET

; ----------------------------------------------------------------------
; Skivorna

HEM:      LD   BC,0
SPAR:     LD   (SPARET),BC
          RET

SEKTOR:   LD   (SEKTORN),BC
          RET

DMA:      LD   (DMAADR),BC
          RET

; Inga översättningstabeller: den logiska sektorn är den fysiska.
OVERSATT: LD   H,B
          LD   L,C
          RET

; VALJ: enheten i C. HL = dess diskparameterhuvud, eller 0 om den inte
; finns.
VALJ:     LD   HL,0
          LD   A,C
          CP   2
          JR   C,FD2ENHET
          JR   NZ,VALJ_SLUT     ; D: och uppåt
          LD   A,(RAMBLOCK)     ; C: bara med RAM-skiva
          OR   A
          RET  Z
          LD   HL,DPH_C
          LD   A,2
          JR   VALD
FD2ENHET: LD   HL,DPH_A
          OR   A
          JR   Z,VALD
          LD   HL,DPH_B
VALD:     LD   (VALD_ENHET),A
VALJ_SLUT:
          RET

; LAS och SKRIV: A = 0 om det gick bra, 1 vid fel.
LAS:      LD   A,(VALD_ENHET)
          CP   2
          JP   Z,RAMLAS
          CALL HAMTA
          JR   NZ,FEL
          CALL HALVAN
          LD   DE,(DMAADR)
          LD   BC,128
          LDIR
          XOR  A
          RET

; Skrivningen går genast till skivan (ingen fördröjd skrivning, så att
; inget blir kvar i bufferten när programmet eller emulatorn slutar).
SKRIV:    LD   A,(VALD_ENHET)
          CP   2
          JP   Z,RAMSKRIV
          CALL HAMTA            ; den andra halvan av sektorn behövs
          JR   NZ,FEL
          CALL HALVAN
          EX   DE,HL
          LD   HL,(DMAADR)
          LD   BC,128
          LDIR
          CALL FYSISK
          LD   HL,BUFFERT
          CALL FD2SKRIV
          RET  Z                ; A = 0
FEL:      XOR  A
          LD   (BUF_GILTIG),A
          INC  A
          RET

; HALVAN: HL = halvan av bufferten där CP/M-sektorn står.
HALVAN:   LD   HL,BUFFERT
          LD   A,(SEKTORN)
          RRCA
          RET  NC
          LD   L,128            ; BUFFERT börjar på en jämn sida
          RET

; HAMTA: se till att den fysiska sektorn för spåret och sektorn finns i
; BUFFERT. Z om den gör det, NZ vid läsfel.
HAMTA:    CALL FYSISK
          LD   A,(BUF_GILTIG)
          OR   A
          JR   Z,HAMTA_LAS
          LD   A,(ENHET)
          LD   HL,BUF_ENHET
          CP   (HL)
          JR   NZ,HAMTA_LAS
          LD   HL,(BUF_SEKTOR)  ; L = sektorn * 32, H = spåret
          OR   A
          SBC  HL,DE
          RET  Z
HAMTA_LAS:
          XOR  A
          LD   (BUF_GILTIG),A
          LD   HL,BUFFERT
          CALL FD2LAS
          RET  NZ
          LD   A,(ENHET)
          LD   (BUF_ENHET),A
          LD   (BUF_SEKTOR),DE
          LD   A,1
          LD   (BUF_GILTIG),A
          XOR  A
          RET

; FYSISK: ENHET = den valda enheten, D = spåret och E = den fysiska
; sektorn * 32 (CP/M-sektorn / 2 * 32).
FYSISK:   LD   A,(VALD_ENHET)
          LD   (ENHET),A
          LD   A,(SPARET)
          LD   D,A
          LD   A,(SEKTORN)
          AND  $0E
          ADD  A,A
          ADD  A,A
          ADD  A,A
          ADD  A,A
          LD   E,A
          RET

; FD2: OUT 1 väljer kortet, OUT 2 (C1) ger ett nytt kommando, fyra
; bytes på port 0: 3 (läs) eller $0C (skriv), enheten, spåret, sektorn
; * 32. IN 1 är statusen: bit 1 = 0 medan kommandot tas emot, bit 2 = 0
; under dataöverföringen, bit 0 = redo för en byte, bit 7 = klart
; (slutstatusen på IN 0, 0 = rätt). Se karna/skivkort.h.

; FD2LAS: läs sektorn D (spår), E (sektor * 32) på ENHET till (HL).
; Z om det gick bra. Ändrar A, B, C och HL.
FD2LAS:   LD   A,3
          CALL KOMMANDO
          RET  NZ
LASVANTA: IN   A,(1)
          BIT  7,A              ; klart utan data, alltså fel
          JR   NZ,SLUTSTATUS
          BIT  2,A
          JR   NZ,LASVANTA
          LD   B,0              ; 256 bytes
LASBYTE:  IN   A,(1)
          RRCA
          JR   NC,LASBYTE
          IN   A,(0)
          LD   (HL),A
          INC  HL
          DJNZ LASBYTE
          JR   KLART

; FD2SKRIV: skriv (HL) till sektorn D, E på ENHET. Som FD2LAS.
FD2SKRIV: LD   A,$0C
          CALL KOMMANDO
          RET  NZ
SKRIVVANTA:
          IN   A,(1)
          BIT  7,A
          JR   NZ,SLUTSTATUS
          BIT  2,A
          JR   NZ,SKRIVVANTA
          LD   B,0
SKRIVBYTE:
          IN   A,(1)
          RRCA
          JR   NC,SKRIVBYTE
          LD   A,(HL)
          OUT  (0),A
          INC  HL
          DJNZ SKRIVBYTE
KLART:    IN   A,(1)
          BIT  7,A
          JR   Z,KLART
SLUTSTATUS:
          IN   A,(0)
          OR   A
          RET

; KOMMANDO: välj FD2 och skicka kommandot A för ENHET, D och E. Z om
; kortet tog emot det, NZ om det inte svarar. Ändrar A, B och C.
KOMMANDO: LD   C,A
          LD   A,FD2
          OUT  (1),A
          OUT  (2),A            ; C1: nytt kommando
          PUSH HL
          LD   HL,0             ; tidsgräns, 65 536 varv
TAREMOT:  IN   A,(1)
          BIT  1,A
          JR   Z,SKICKA
          DEC  HL
          LD   A,H
          OR   L
          JR   NZ,TAREMOT
          POP  HL
          INC  A                ; inget svar: NZ
          RET
SKICKA:   POP  HL
          LD   A,C
          OUT  (0),A
          LD   A,(ENHET)
          OUT  (0),A
          LD   A,D
          OUT  (0),A
          LD   A,E
          OUT  (0),A
          XOR  A
          RET

; ----------------------------------------------------------------------
; RAM-skivan

; RAMSTART: leta upp ABC80:s RAM från $8000 och uppåt i steg om 1 KB;
; RAM-skivan är det från den första sidan som går att skriva till
; RAMSLUT. Töm katalogen (block 0) med $E5.
RAMSTART: LD   HL,$8000
RAMPROV:  CALL LASSYS
          LD   C,A
          CPL
          CALL SKRIVSYS
          LD   B,A
          CALL LASSYS
          CP   B
          LD   A,C
          CALL SKRIVSYS         ; tillbaka, om det var RAM
          JR   Z,RAMFUNNET
          LD   DE,$400
          ADD  HL,DE
          LD   A,H
          CP   RAMSLUT/256
          JR   C,RAMPROV
RAMFUNNET:
          LD   (RAMBAS),HL
          LD   A,RAMSLUT/256    ; antalet block: (RAMSLUT - HL) / 1 KB
          SUB  H
          RRCA
          RRCA
          AND  $3F
          CP   2                ; katalogen och minst ett block till
          JR   NC,RAMSTOR
          XOR  A
RAMSTOR:  LD   (RAMBLOCK),A
          OR   A
          RET  Z
          DEC  A
          LD   (DPB_C+5),A      ; DSM = antalet block - 1
          LD   BC,$400
          LD   A,$E5
          JP   FYLLSYS          ; HL = RAMBAS

; RAMADRESS: HL = RAMBAS + SPARET * 1 KB + SEKTORN * 128.
RAMADRESS:
          LD   A,(SPARET)       ; spåren är 8 sektorer = 1 KB
          ADD  A,A
          ADD  A,A
          LD   H,A
          LD   L,0
          LD   A,(SEKTORN)
          RRCA                  ; * 128: bit 0 till bit 7, bit 1-2 till H
          LD   B,A
          AND  $80
          LD   L,A
          LD   A,B
          AND  $03
          ADD  A,H
          LD   H,A
          LD   DE,(RAMBAS)
          ADD  HL,DE
          RET

; RAMLAS: 128 bytes från RAM-skivan till DMA, i två halvor genom
; RAMBUF i det gemensamma minnet.
RAMLAS:   CALL RAMADRESS
          LD   DE,(DMAADR)
          CALL RAMHALVA_IN
          CALL RAMHALVA_IN
          XOR  A
          RET
RAMHALVA_IN:                    ; HL = RAM-skivan, DE = DMA; båda + 64
          PUSH DE
          LD   DE,RAMBUF
          LD   BC,64
          CALL SYSFLYTT
          POP  DE
          PUSH HL
          LD   HL,RAMBUF
          LD   BC,64
          LDIR
          POP  HL
          RET

RAMSKRIV: CALL RAMADRESS
          EX   DE,HL
          LD   HL,(DMAADR)
          CALL RAMHALVA_UT
          CALL RAMHALVA_UT
          XOR  A
          RET
RAMHALVA_UT:                    ; HL = DMA, DE = RAM-skivan; båda + 64
          PUSH DE
          LD   DE,RAMBUF
          LD   BC,64
          LDIR
          POP  DE
          PUSH HL
          LD   HL,RAMBUF
          LD   BC,64
          CALL SYSFLYTT
          POP  HL
          RET

; ----------------------------------------------------------------------
; Diskparametrarna

; Diskparameterhuvudena: översättning, tre ord åt BDOS:en, katalog-
; bufferten, parameterblocket, kontrollsummorna och blockkartan.
DPH_A:    DEFW 0,0,0,0,KATALOGBUF,DPB_FD2,CSV_A,ALV_A
DPH_B:    DEFW 0,0,0,0,KATALOGBUF,DPB_FD2,CSV_B,ALV_B
DPH_C:    DEFW 0,0,0,0,KATALOGBUF,DPB_C,0,ALV_C

; FD2: 16 sektorer per spår, block om 1 KB (BSH 3, BLM 7, EXM 0), 72
; block, 64 katalogposter i block 0-1, 16 bytes kontrollsumma, 4 spår
; system.
DPB_FD2:  DEFW 16
          DEFB 3,7,0
          DEFW 71,63
          DEFB $C0,0
          DEFW 16,4

; RAM-skivan: 8 sektorer per spår (1 KB), högst 31 block (DSM sätts av
; RAMSTART), 32 katalogposter i block 0, ingen kontrollsumma (fast
; skiva), inga systemspår.
DPB_C:    DEFW 8
          DEFB 3,7,0
          DEFW 0,31
          DEFB $80,0
          DEFW 0,0

SLUT_PA_SKIVAN EQU $            ; härifrån sparas inget på skivan

; ----------------------------------------------------------------------
; Variablerna och buffertarna (läses inte in från skivan)

BUFFERT   EQU  (SLUT_PA_SKIVAN+255)&$FF00   ; en sektor från FD2, jämn sida
KATALOGBUF EQU BUFFERT+256      ; 128 bytes åt BDOS:en
CSV_A     EQU  KATALOGBUF+128
CSV_B     EQU  CSV_A+16
ALV_A     EQU  CSV_B+16         ; 72 block: 9 bytes
ALV_B     EQU  ALV_A+9
ALV_C     EQU  ALV_B+9          ; 31 block: 4 bytes
RADTABELL EQU  ALV_C+4          ; 24 ord: radernas början i bildminnet
BILDMINNE EQU  RADTABELL+48
KOLUMNER  EQU  BILDMINNE+2
RAD       EQU  KOLUMNER+1
KOL       EQU  RAD+1
ESCLAGE   EQU  KOL+1            ; 0, 1 efter ESC, 2 efter ESC =, 3 efter raden
ESCRAD    EQU  ESCLAGE+1
NERE      EQU  ESCRAD+1         ; tangenten är nere
HAR_TANGENT EQU NERE+1          ; TANGENT är inte läst
TANGENT   EQU  HAR_TANGENT+1
SPARET    EQU  TANGENT+1        ; ord
SEKTORN   EQU  SPARET+2         ; ord
DMAADR    EQU  SEKTORN+2
VALD_ENHET EQU DMAADR+2
ENHET     EQU  VALD_ENHET+1     ; i kommandot till FD2
BUF_GILTIG EQU ENHET+1
BUF_ENHET EQU  BUF_GILTIG+1
BUF_SEKTOR EQU BUF_ENHET+1      ; spåret och sektorn * 32, som DE
RAMBAS    EQU  BUF_SEKTOR+2
RAMBLOCK  EQU  RAMBAS+2         ; 0: ingen RAM-skiva
BIOSSTACK EQU  RAMBLOCK+1+64    ; 64 bytes
BIOSSLUT  EQU  BIOSSTACK

          END
