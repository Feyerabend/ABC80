; bios.asm -- BIOS:en till CP/M 2.2 på ABC 802, till ABC 802 inifrån
;
; Vår egen BIOS; ingenting här kommer från MyAB. Den bygger på BIOS:en
; till ABC80 (abc80/emulator/cpm/bios.asm): samma skärmkoder och samma
; rutiner för kort 44. CCP och BDOS är Digital Researchs CP/M 2.2 (den
; fria utgåvan, se cpm/dr/), översatta för 64 KB: CCP på $E000, BDOS på
; $E806 och BIOS:en på $F600. Laddaren i BASICINI.SYS (cpm/start.asm)
; läser in systemet och hoppar till BOOT.
;
; Minnet: DTR på DART B (WR5 bit 7, LRS*) ger 64 KB RAM, och så står det
; hela tiden utom när BIOS:en skriver på skärmen:
;   $0000-$00FF  sidan noll (hoppen till BIOS och BDOS, FCB, bufferten)
;   $0100-$DFFF  programmen (TPA, 56 KB)
;   $E000-$F5FF  CCP och BDOS, som läses om från skivan vid varmstart
;   $F600-$FFFF  BIOS:en med buffertarna
; Bildminnet ($7800-$7FFF, 2 KB) syns bara utan DTR, och då är
; $0000-$77FF ROM:en. KONUT byter därför till en egen stack över $8000,
; slår av DTR, skriver och slår på DTR igen. All BIOS:ens kod och alla
; dess variabler ligger över $8000, där RAM:et syns i båda lägena.
;
; Avbrotten är av hela tiden, och BOOT stänger av dem vid källan
; (DART, SIO och CTC): ROM:ens IM 2-tabell på $FFB0 finns inte längre,
; och ett program som gör EI ska inte hoppa ut i det okända.
;
; Enheterna:
;   konsolen  tangentbordet på DART B (port $22, RR0 bit 0 = ett tecken
;             har kommit), skärmen 24 rader om 80 tecken med 6845:s
;             markör och ett urval av ADM-3A:s koder (se KONUT)
;   A: B:     kort 44 enhet 0 och 1 (MF0 och MF1): 1 280 sektorer à 256
;             bytes. CP/M ser 80 spår om 32 sektorer (16 av kortets), där
;             spår 0-4 är systemet: DOS:ens del (katalogen och
;             BASICINI.SYS) och CCP, BDOS och BIOS:en i sektor 40-71.
;             Spår 5-79 är 150 block om 2 KB med 128 katalogposter.
;   LST: PUN: finns inte än; tecknen försvinner.
;   RDR:      ger alltid ^Z (slut).

CCP       EQU  $E000
BDOS      EQU  $E806
SYSSEKTOR EQU  40               ; CCP och BDOS från sektor 40 (cpm/skiva.py)
SYSSEKTORER EQU 22              ; $1600 bytes i sektorer om 256
IOBYTE    EQU  3
AKTUELL   EQU  4                ; aktuella enheten (och användaren)
TBUFF     EQU  $80              ; standardbufferten

MF        EQU  44               ; skivkortet

DARTA_STYR EQU $21
DARTB_DATA EQU $22              ; tangentbordet
DARTB_STYR EQU $23
SIOA_STYR EQU  $41
SIOB_STYR EQU  $43
CTC       EQU  $60              ; kanal 0-3 på $60-$63
CRTC_ADR  EQU  $38              ; 6845, skrivs med A3 = 1
CRTC_DATA EQU  $39

WR5_ROM   EQU  $68              ; DTR = 0: ROM:en och bildminnet; RTS = 0: 80 tecken
WR5_RAM   EQU  $E8              ; DTR = 1: 64 KB RAM

BILD      EQU  $7800            ; bildminnet, 24 rader om 80 tecken i följd
KOLUMNER  EQU  80
RADER     EQU  24

          ORG  $F600

; De 17 ingångarna i CP/M 2.2, i den ordning BDOS:en väntar sig.
          JP   KALLSTART        ; BOOT
VARMHOPP: JP   VARMSTART        ; WBOOT
          JP   KONSTAT          ; CONST
          JP   KONIN            ; CONIN
          JP   KONUT            ; CONOUT
          JP   SKRIVARE         ; LIST
          JP   SKRIVARE         ; PUNCH
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

; Skärmens kolumner, för SKRIV (cpm/skriv/), som läser dem ur minnet som
; på ABC80, där de är 40 eller 80; Makefile hämtar adressen ur bios.lst.
BREDDEN:  DEFB KOLUMNER

; ----------------------------------------------------------------------
; Start

KALLSTART:
          DI
          LD   SP,BIOSSTACK
          LD   HL,AVBROTT_AV    ; WR1 = 0 i DART A och B och SIO A och B
AVBROTT:  LD   C,(HL)
          INC  C
          DEC  C
          JR   Z,CTC_AV
          INC  HL
          LD   A,1
          OUT  (C),A
          XOR  A
          OUT  (C),A
          JR   AVBROTT
CTC_AV:   LD   BC,4*256+CTC     ; CTC:ns fyra kanaler: återställ
          LD   A,$03
CTC_KANAL:
          OUT  (C),A
          INC  C
          DJNZ CTC_KANAL
          CALL SKARMSTART
          CALL BSTART
          XOR  A
          LD   (IOBYTE),A
          LD   (AKTUELL),A
          LD   HL,HALSNING
          CALL TEXT
          JR   GA

AVBROTT_AV:
          DEFB DARTA_STYR,DARTB_STYR,SIOA_STYR,SIOB_STYR,0

VARMSTART:
          DI
          LD   SP,BIOSSTACK
          XOR  A
          LD   (BUF_GILTIG),A   ; skivan kan ha bytts
          LD   (ENHET),A
OMLAS:    LD   HL,CCP           ; CCP och BDOS, sektor 40 och framåt på A:
          LD   DE,SYSSEKTOR
          LD   B,SYSSEKTORER
SYSLAS:   PUSH BC
          PUSH DE
          CALL LOPANDE
          CALL MFLAS
          POP  DE
          POP  BC
          JR   NZ,SYSFEL
          INC  DE
          DJNZ SYSLAS
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
          LD   C,A
          PUSH BC
          CALL VALJ
          POP  BC
          LD   A,H
          OR   L
          LD   A,C
          JR   NZ,ENHETFINNS
          XOR  A
ENHETFINNS:
          LD   C,A
          LD   A,(AKTUELL)
          AND  $F0
          OR   C
          LD   (AKTUELL),A
          LD   C,A
          JP   CCP

HALSNING: DEFB 26,"CP/M 2.2 p} ABC 802, 64 KB",13,10,0
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
; Tangentbordet

; Tangentbordets processor skickar koden på DART B när en tangent
; trycks ned (servicemanualen s. 84-88). Avbrotten är av, så RR0 bit 0
; säger om ett tecken har kommit.
KONSTAT:  IN   A,(DARTB_STYR)
          AND  1
          RET  Z
          LD   A,$FF
          RET

KONIN:    IN   A,(DARTB_STYR)
          RRCA
          JR   NC,KONIN
          IN   A,(DARTB_DATA)
          PUSH AF
          LD   A,$30            ; WR0: återställ fel, som ROM:ens KEYERR
          OUT  (DARTB_STYR),A
          POP  AF
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
; Andra koder under $20 och $7F syns inte (koderna under $20 är
; grafik- och blinkkoder i teckengeneratorn). $20-$7E skrivs som de är;
; i ABC 802:s teckensats är [\]{|} ÄÖÅäöå. Efter sista kolumnen
; fortsätter texten på nästa rad. Markören är 6845:s (R14/R15), en
; blinkande understrykning.

; SKARMSTART: radernas adresser, markören och tom skärm.
SKARMSTART:
          LD   HL,BILD
          LD   DE,KOLUMNER
          LD   IX,RADTABELL
          LD   B,RADER
RADADRESS:
          LD   (IX+0),L
          LD   (IX+1),H
          INC  IX
          INC  IX
          ADD  HL,DE
          DJNZ RADADRESS
          XOR  A
          LD   (ESCLAGE),A
          LD   BC,10*256+$69    ; R10: markören blinkar (1/32), linje 9
          CALL CRTC
          LD   BC,11*256+$09    ; R11: till linje 9
          CALL CRTC
          LD   C,26             ; tom skärm
          JP   KONUT

; CRTC: skriv C i 6845:s register B.
CRTC:     LD   A,B
          OUT  (CRTC_ADR),A
          LD   A,C
          OUT  (CRTC_DATA),A
          RET

; KONUT: skriv tecknet i C. Stacken byts till SKARMSTACK, eftersom den
; som anropar kan ha sin under $8000, och den finns inte utan DTR.
KONUT:    LD   (ANV_SP),SP
          LD   SP,SKARMSTACK
          LD   A,5              ; DART B WR5: DTR av, bildminnet syns
          OUT  (DARTB_STYR),A
          LD   A,WR5_ROM
          OUT  (DARTB_STYR),A
          LD   A,C
          CALL TECKEN
          LD   A,5              ; DTR på igen
          OUT  (DARTB_STYR),A
          LD   A,WR5_RAM
          OUT  (DARTB_STYR),A
          CALL ADRESS           ; markören dit nästa tecken kommer
          LD   BC,14*256
          LD   C,H
          CALL CRTC
          LD   BC,15*256
          LD   C,L
          CALL CRTC
          LD   SP,(ANV_SP)
          RET

TECKEN:   LD   B,A
          LD   A,(ESCLAGE)
          OR   A
          JP   NZ,ESCAPE
          LD   A,B
          CP   $20
          JR   C,STYRKOD
          CP   $7F
          RET  NC
          CALL ADRESS           ; ett vanligt tecken
          LD   (HL),A
          LD   A,(KOL)
          INC  A
          LD   (KOL),A
          CP   KOLUMNER
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
          CP   RADER-1
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

HOGER:    LD   A,(KOL)
          INC  A
          CP   KOLUMNER
          RET  NC
          LD   (KOL),A
          RET

UPP:      LD   A,(RAD)
          OR   A
          RET  Z
          DEC  A
          LD   (RAD),A
          RET

RENSA:    LD   HL,BILD          ; hela bildminnet, 2 KB
          LD   BC,$800
          LD   A,' '
          CALL FYLL
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
UPP_FRAN: LD   C,A              ; raderna A + 1 ... 23 till A ... 22
          CALL RADENS
          LD   A,RADER-1
          SUB  C
          JR   Z,TOM_RADEN
          PUSH HL
          CALL GANGER80
          POP  DE               ; dit
          LD   HL,KOLUMNER
          ADD  HL,DE            ; därifrån, en rad längre ned
          LDIR
          EX   DE,HL            ; HL = den sista raden
TOM_RADEN:
          LD   BC,KOLUMNER
          LD   A,' '
          JP   FYLL

; INFOGA_RAD: raderna från markörens en ned, den sista försvinner, och
; markörens rad tom (ESC E).
INFOGA_RAD:
          XOR  A
          LD   (KOL),A
          LD   A,(RAD)
          LD   C,A
          LD   A,RADER-1
          SUB  C
          JR   Z,SISTA_TOM
          CALL GANGER80         ; bakifrån, från rad 22:s slut till 23:s
          LD   HL,BILD+RADER*KOLUMNER-1-KOLUMNER
          LD   DE,BILD+RADER*KOLUMNER-1
          LDDR
SISTA_TOM:
          LD   A,(RAD)
          CALL RADENS
          JR   TOM_RADEN

; GANGER80: BC = A * 80 (A rader). Ändrar HL.
GANGER80: LD   L,A
          LD   H,0
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,HL            ; * 16
          LD   B,H
          LD   C,L
          ADD  HL,HL
          ADD  HL,HL            ; * 64
          ADD  HL,BC
          LD   B,H
          LD   C,L
          RET

; RADENS: HL = början av rad A i bildminnet. Ändrar DE.
RADENS:   ADD  A,A
          LD   E,A
          LD   D,0
          LD   HL,RADTABELL
          ADD  HL,DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          EX   DE,HL
          RET

; FYLL: fyll BC bytes (minst 1) från (HL) med A. Ändrar BC, DE och HL.
FYLL:     LD   (HL),A
          DEC  BC
          LD   D,A
          LD   A,B
          OR   C
          LD   A,D
          RET  Z
          LD   D,H
          LD   E,L
          INC  DE
          LDIR
          RET

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
          CP   RADER
          JR   C,ESC_RADEN
          LD   A,RADER-1
ESC_RADEN:
          LD   (RAD),A
          LD   A,B
          SUB  32
          CP   KOLUMNER
          JR   C,ESC_KOLUMNEN
          LD   A,KOLUMNER-1
ESC_KOLUMNEN:
          LD   (KOL),A
          RET

; Radera till radens slut.
RADSLUT:  CALL ADRESS
          LD   A,(KOL)
          LD   B,A
          LD   A,KOLUMNER
          SUB  B
          LD   C,A
          LD   B,0
          LD   A,' '
          JP   FYLL

; Radera till skärmens slut: allt från markören till bildminnets slut.
SKARMSLUT:
          CALL ADRESS
          EX   DE,HL
          LD   HL,BILD+$800
          OR   A
          SBC  HL,DE
          LD   B,H
          LD   C,L
          EX   DE,HL
          LD   A,' '
          JP   FYLL

; ADRESS: HL = markörens plats i bildminnet. Ändrar DE.
ADRESS:   PUSH AF
          LD   A,(RAD)
          CALL RADENS
          LD   A,(KOL)
          LD   E,A
          LD   D,0
          ADD  HL,DE
          POP  AF
          RET

; ----------------------------------------------------------------------
; LST:, PUN: och RDR:

SKRIVARE: RET                   ; ingen skrivare än

SKRIVARSTATUS:
          LD   A,$FF            ; alltid klar
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
          RET  NC               ; C: och uppåt
          LD   HL,DPH_A
          OR   A
          JR   Z,VALD
          LD   A,(B_FINNS)      ; B: bara med en skiva i MF1
          OR   A
          LD   HL,0
          RET  Z
          LD   HL,DPH_B
          LD   A,1
VALD:     LD   (VALD_ENHET),A
          RET

; LAS och SKRIV: A = 0 om det gick bra, 1 vid fel.
LAS:      CALL HAMTA
          JR   NZ,FEL
          CALL HALVAN
          LD   DE,(DMAADR)
          LD   BC,128
          LDIR
          XOR  A
          RET

; Skrivningen går genast till skivan (ingen fördröjd skrivning, så att
; inget blir kvar i bufferten när programmet eller emulatorn slutar).
SKRIV:    CALL HAMTA            ; den andra halvan av sektorn behövs
          JR   NZ,FEL
          CALL HALVAN
          EX   DE,HL
          LD   HL,(DMAADR)
          LD   BC,128
          LDIR
          CALL FYSISK
          LD   HL,BUFFERT
          CALL MFSKRIV
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
          LD   A,(VALD_ENHET)
          LD   HL,BUF_ENHET
          CP   (HL)
          JR   NZ,HAMTA_LAS
          LD   HL,(BUF_SEKTOR)
          OR   A
          SBC  HL,DE
          RET  Z
HAMTA_LAS:
          XOR  A
          LD   (BUF_GILTIG),A
          LD   HL,BUFFERT
          CALL MFLAS
          RET  NZ
          LD   A,(VALD_ENHET)
          LD   (BUF_ENHET),A
          LD   (BUF_SEKTOR),DE
          LD   A,1
          LD   (BUF_GILTIG),A
          XOR  A
          RET

; FYSISK: ENHET för den valda enheten, och sektorn i DE som kortet vill
; ha den. Kortets sektor N = spåret * 16 + CP/M-sektorn / 2 ska skickas
; som 32 * (N / 4) + (N AND 3). Med spåret T och s = CP/M-sektorn / 2
; blir det D = T / 2 och E = (T AND 1) * 128 + (s / 4) * 32 + (s AND 3).
FYSISK:   LD   A,(VALD_ENHET)
          LD   (ENHET),A
          LD   A,(SPARET)
          SRL  A                ; C = T AND 1
          LD   D,A
          LD   A,(SEKTORN)
          RRA                   ; (T AND 1) * 128 + s
          LD   B,A
          AND  $0C              ; s / 4 till bit 5-6
          ADD  A,A
          ADD  A,A
          ADD  A,A
          LD   E,A
          LD   A,B
          AND  $83
          OR   E
          LD   E,A
          RET

; LOPANDE: sektor DE i följd (0-1279) som kortet vill ha den, 32 * (DE
; / 4) + (DE AND 3), i DE. Ändrar A.
LOPANDE:  LD   A,E
          AND  3
          PUSH AF
          LD   A,E
          AND  $FC
          LD   E,A
          EX   DE,HL
          ADD  HL,HL
          ADD  HL,HL
          ADD  HL,HL
          EX   DE,HL
          POP  AF
          OR   E
          LD   E,A
          RET

; Kort 44: OUT 1 väljer kortet, OUT 2 (C1) ger ett nytt kommando, fyra
; bytes på port 0: 3 (läs) eller $0C (skriv), enheten, sektorns höga och
; låga byte. IN 1 är statusen: bit 1 = 0 medan kommandot tas emot, bit
; 2 = 0 under dataöverföringen, bit 0 = redo för en byte, bit 7 = klart
; (slutstatusen på IN 0, 0 = rätt). Se karna/skivkort.c.

; MFLAS: läs sektorn D, E på ENHET till (HL). Z om det gick bra.
; Ändrar A, B, C och HL.
MFLAS:    LD   A,3
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

; MFSKRIV: skriv (HL) till sektorn D, E på ENHET. Som MFLAS.
MFSKRIV:  LD   A,$0C
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

; KOMMANDO: välj kort 44 och skicka kommandot A för ENHET, D och E. Z
; om kortet tog emot det, NZ om det inte svarar. Ändrar A, B och C.
KOMMANDO: LD   C,A
          LD   A,MF
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

; BSTART: finns en skiva i MF1? B_FINNS = 1 om sektor 0 går att läsa.
BSTART:   XOR  A
          LD   (B_FINNS),A
          LD   (BUF_GILTIG),A
          INC  A
          LD   (ENHET),A
          LD   DE,0
          LD   HL,BUFFERT
          CALL MFLAS
          LD   A,0
          LD   (ENHET),A
          RET  NZ
          INC  A
          LD   (B_FINNS),A
          RET

; ----------------------------------------------------------------------
; Diskparametrarna

; Diskparameterhuvudena: översättning, tre ord åt BDOS:en, katalog-
; bufferten, parameterblocket, kontrollsummorna och blockkartan.
DPH_A:    DEFW 0,0,0,0,KATALOGBUF,DPB_MF,CSV_A,ALV_A
DPH_B:    DEFW 0,0,0,0,KATALOGBUF,DPB_MF,CSV_B,ALV_B

; 32 sektorer per spår, block om 2 KB (BSH 4, BLM 15, EXM 1), 150 block,
; 128 katalogposter i block 0-1, 32 bytes kontrollsumma, 5 spår system.
DPB_MF:   DEFW 32
          DEFB 4,15,1
          DEFW 149,127
          DEFB $C0,0
          DEFW 32,5

SLUT_PA_SKIVAN EQU $            ; härifrån sparas inget på skivan

; ----------------------------------------------------------------------
; Variablerna och buffertarna (läses inte in från skivan)

BUFFERT   EQU  (SLUT_PA_SKIVAN+255)&$FF00   ; en sektor, jämn sida
KATALOGBUF EQU BUFFERT+256      ; 128 bytes åt BDOS:en
CSV_A     EQU  KATALOGBUF+128
CSV_B     EQU  CSV_A+32
ALV_A     EQU  CSV_B+32         ; 150 block: 19 bytes
ALV_B     EQU  ALV_A+19
RADTABELL EQU  ALV_B+19         ; 24 ord: radernas början i bildminnet
RAD       EQU  RADTABELL+2*RADER
KOL       EQU  RAD+1
ESCLAGE   EQU  KOL+1            ; 0, 1 efter ESC, 2 efter ESC =, 3 efter raden
ESCRAD    EQU  ESCLAGE+1
ANV_SP    EQU  ESCRAD+1         ; anroparens stack under KONUT
SPARET    EQU  ANV_SP+2         ; ord
SEKTORN   EQU  SPARET+2         ; ord
DMAADR    EQU  SEKTORN+2
VALD_ENHET EQU DMAADR+2
ENHET     EQU  VALD_ENHET+1     ; i kommandot till kortet
B_FINNS   EQU  ENHET+1          ; 1: B: finns
BUF_GILTIG EQU B_FINNS+1
BUF_ENHET EQU  BUF_GILTIG+1     ; VALD_ENHET
BUF_SEKTOR EQU BUF_ENHET+1      ; sektorn som DE i kommandot
SKARMSTACK EQU BUF_SEKTOR+2+32  ; 32 bytes åt KONUT
BIOSSTACK EQU  SKARMSTACK+32    ; 32 bytes, vid start och varmstart
BIOSSLUT  EQU  BIOSSTACK

          END
