; start.asm -- start-PROM:en på UNI-80 i emulatorn, till boken ABC80 inifrån
;
; Vår egen start: MyAB:s PROM finns inte bevarad, och ingenting här
; kommer från MyAB. Emulatorn lägger kretsen i kortet (karna/uni80.h)
; och startar i startkartan (3), som när RESET hålls nere på den riktiga
; maskinen. Processorn börjar då på $0000 i kretsen.
;
; Gången:
;   1. Kretsen kopierar sig själv till kortets RAM på $0800 och fortsätter
;      där; i startkartan är $0800-$7FFF kortets RAM.
;   2. CP/M-kartan (1): hela minnet är kortets RAM, och kretsen syns inte.
;   3. Startblocket, spår 0 sektor 0 på skivenheten FD2 (kort 45, enhet
;      0), läses till BUFFERT. Det ska börja så här:
;          +0  "CP/M"
;          +4  laddadressen (ord)
;          +6  antalet sektorer som följer (byte, 1-255)
;          +7  startadressen (ord)
;   4. Sektorerna efter startblocket, sektor 1 och framåt (spår 0
;      sektor 1-7, spår 1 sektor 0-7 ...), läses till laddadressen och
;      uppåt, 256 bytes var.
;   5. Hopp till startadressen, fortfarande i CP/M-kartan, med
;      avbrotten av.
; Laddningen får inte röra $0800-$0AFF (kretsens kopia, stacken och
; bufferten).
;
; Går något fel (inget kort 45, ingen skiva, ett läsfel, inget "CP/M")
; startar maskinen som ABC80 i systemkartan (2): fem bytes i det
; gemensamma minnet ($FF70-$FFFF, samma i alla kartor) byter karta och
; hoppar till $0000.
;
; FD2 (karna/skivkort.h): OUT 1 väljer kortet, OUT 2 (C1) ger ett nytt
; kommando, som är fyra bytes på port 0: 3 (läs), enheten, spåret och
; sektorn * 32. IN 1 är statusen: bit 1 = 0 medan kommandot tas emot,
; bit 2 = 0 när data kommer, bit 0 = en byte kan läsas, bit 7 = klart
; (slutstatusen på IN 0, 0 = rätt).

KARTPORT  EQU  $EF              ; UNI-80:s port för kartan
CPMKARTA  EQU  1
SYSKARTA  EQU  2
FD2       EQU  45               ; skivkortet
ENHET     EQU  0                ; A:
STACK     EQU  $0A00            ; växer nedåt i $0900-$09FF
BUFFERT   EQU  $0A00            ; startblocket, $0A00-$0AFF
TILLBAKA  EQU  $FFF0            ; i det gemensamma minnet

; Startblockets fält i bufferten.
LADDA     EQU  BUFFERT+4
ANTAL     EQU  BUFFERT+6
HOPPA     EQU  BUFFERT+7

          ORG  $0800            ; där koden körs; kretsen ligger på $0000

; På $0000 i kretsen. Bara instruktioner som inte beror på var de står,
; tills hoppet.
START:    DI
          LD   HL,0
          LD   DE,START
          LD   BC,LANGD
          LDIR
          JP   VIDARE

VIDARE:   LD   SP,STACK
          LD   A,CPMKARTA
          OUT  (KARTPORT),A

; Startblocket.
          LD   HL,BUFFERT
          LD   DE,0             ; spår 0, sektor 0
          CALL LAS
          JR   NZ,ABC80
          LD   HL,BUFFERT
          LD   DE,MARKE
          LD   B,4
JAMFOR:   LD   A,(DE)
          CP   (HL)
          JR   NZ,ABC80
          INC  HL
          INC  DE
          DJNZ JAMFOR

; Sektorerna. HL = dit, DE = spår och sektor * 32, B = antal kvar.
          LD   A,(ANTAL)
          OR   A
          JR   Z,ABC80
          LD   B,A
          LD   HL,(LADDA)
          LD   DE,1*32          ; spår 0, sektor 1
NASTA:    PUSH BC
          CALL LAS
          POP  BC
          JR   NZ,ABC80
          LD   A,E              ; nästa sektor: sektor * 32 räknas upp,
          ADD  A,32             ; och efter sektor 7 nästa spår
          LD   E,A
          JR   NC,SAMMA
          INC  D
SAMMA:    DJNZ NASTA
          LD   HL,(HOPPA)
          JP   (HL)

; Något gick fel: ABC80 startar som vanligt.
ABC80:    LD   HL,TILL_ABC80
          LD   DE,TILLBAKA
          LD   BC,5
          LDIR
          LD   A,SYSKARTA
          JP   TILLBAKA
TILL_ABC80: OUT (KARTPORT),A    ; körs på TILLBAKA
          JP   0

MARKE:    DEFM "CP/M"

; LAS: läs sektorn D (spår), E (sektor * 32) på ENHET till (HL) och
; uppåt. Efteråt HL + 256 och Z om allt gick rätt; NZ om kortet inte
; svarar eller slutstatusen inte är 0. Ändrar A, B, C.
LAS:      LD   A,FD2
          OUT  (1),A            ; välj kortet
          OUT  (2),A            ; C1: nytt kommando
          LD   BC,0             ; tidsgräns, 65 536 varv
TAR_EMOT: IN   A,($01)
          BIT  1,A              ; 0: kommandot tas emot
          JR   Z,KOMMANDO
          DEC  BC
          LD   A,B
          OR   C
          JR   NZ,TAR_EMOT
          INC  A                ; inget svar: NZ
          RET
KOMMANDO: LD   A,3              ; läs från skivan och skicka bufferten
          OUT  ($00),A
          LD   A,ENHET
          OUT  ($00),A
          LD   A,D
          OUT  ($00),A
          LD   A,E
          OUT  ($00),A
VANTA:    IN   A,($01)
          BIT  7,A              ; klart utan data, alltså fel
          JR   NZ,SLUT
          BIT  2,A              ; 0 när data kommer
          JR   NZ,VANTA
          LD   B,0              ; 256 bytes
BYTE:     IN   A,($01)
          RRCA                  ; bit 0: en byte kan läsas
          JR   NC,BYTE
          IN   A,($00)
          LD   (HL),A
          INC  HL
          DJNZ BYTE
KLART:    IN   A,($01)
          BIT  7,A
          JR   Z,KLART
SLUT:     IN   A,($00)          ; slutstatusen
          OR   A
          RET

LANGD     EQU  $-START
          END
