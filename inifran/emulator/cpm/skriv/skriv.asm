; skriv.asm -- SKRIV, en skärmeditor för CP/M på ABC80 med UNI-80, till boken ABC80 inifrån
;
;     SKRIV fil.typ
;
; En egen editor för hela skärmen, med WordStars tangenter (Ctrl-J visar
; dem; ABC80 har inget ^, där står Ü). ESC och en bokstav gör samma sak
; som Ctrl och bokstaven.
; Rad 0-22 är texten och rad 23 statusraden: filen, * om den är ändrad,
; raden och kolumnen. Filen läses in i minnet i sin helhet; den sparas
; som fil.$$$, som byter namn till fil.typ när den är skriven, och den
; gamla blir fil.BAK, som med ED.
;
; Texten ligger i en buffert med en lucka (gap buffer): det som står
; före markören ligger från TEXT till LUCKA, det som står efter från
; LUCKSLUT till TEXTSLUT, och det som skrivs hamnar i luckan. I minnet
; slutar raderna med LF; CR tas bort vid inläsningen och sätts tillbaka
; när filen sparas. Ovanför TEXTSLUT ligger klippbufferten (2 KB) med
; de rader som ^Y har raderat, för ^U.
;
; Skärmen sköts genom BIOS:en (cpm/bios.asm) direkt, inte genom BDOS:en:
; ESC = rad kolumn, ESC T (resten av raden), ESC E och ESC R (infoga och
; radera en rad, som rullar texten utan att allt ritas om). Raderna som
; ska ritas om är markerade i RITAS. Tangenter som kommer medan skärmen
; ritas läggs i en kö, och ritandet väntar då tills de är behandlade,
; så att inget tappas när man skriver fort.
;
; Makefile sätter först in BIOS:ens KOLUMNER (40 eller 80) ur
; cpm/bios.lst, som för BBC BASIC.
;
; Rader som är längre än 255 kolumner visas bara till kolumn 255.

BDOS      EQU  5
FCB       EQU  $5C              ; filnamnet som CCP har tolkat
DMABUF    EQU  $80
LF        EQU  10
CR        EQU  13
TAB       EQU  9
ESC       EQU  27
EOF       EQU  $1A              ; ^Z, filens slut
DEL       EQU  $7F
TEXTRADER EQU  23               ; rad 0-22 är texten, 23 statusraden
MITTEN    EQU  11
KLIPPSTORLEK EQU 2048
SOKLANGD  EQU  32

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
          LD   A,(KOLUMNER)
          LD   (BREDD),A
          LD   HL,FCB+1         ; ett filnamn, utan ? och *
          LD   A,(HL)
          CP   ' '
          JR   Z,ANVANDNING
          LD   B,11
NAMNPROV: LD   A,(HL)
          CP   '?'
          JR   Z,ANVANDNING
          INC  HL
          DJNZ NAMNPROV
          LD   HL,FCB           ; enheten, namnet och typen
          LD   DE,FILNAMN
          LD   BC,12
          LDIR
          LD   A,(FILNAMN)      ; enheten: 0 är den aktuella
          OR   A
          JR   NZ,ENHET_KLAR
          LD   C,25
          CALL BDOS
          INC  A
          LD   (FILNAMN),A
ENHET_KLAR:
          LD   A,(7)            ; BDOS:ens sida; CCP under den får
          LD   H,A              ; skrivas över (CP/M läser in den igen)
          LD   L,0
          LD   DE,-KLIPPSTORLEK
          ADD  HL,DE
          LD   (TEXTSLUT),HL
          LD   (LUCKSLUT),HL
          LD   HL,TEXT
          LD   (LUCKA),HL
          LD   HL,T_START
          LD   (MEDDELANDE),HL
          CALL LADDA
          CALL TILL_BORJAN
          XOR  A
          LD   (ANDRAD),A
          LD   A,26             ; radera skärmen
          CALL UT
          CALL MARKERA_ALLA
          JR   SLINGA

ANVANDNING:
          LD   HL,T_ANVANDNING
          CALL UTTEXT
          JP   0

; ----------------------------------------------------------------------
; Huvudslingan

SLINGA:   CALL KOLUMN           ; markörens kolumn
          LD   (KOLNR),A
          LD   A,(VERTIKAL)     ; efter upp och ned behålls den önskade
          OR   A
          JR   NZ,BEHALL
          LD   A,(KOLNR)
          LD   (ONSKAD),A
BEHALL:   XOR  A
          LD   (VERTIKAL),A
          CALL SIDLED
          CALL POLLA
          CALL KO_TOM
          JR   NZ,TANGENT       ; fler tangenter väntar: rita sedan
          CALL RITA
          JR   NZ,TANGENT
          CALL STATUS
          LD   A,(SKRAD)        ; markören dit den hör
          LD   D,A
          LD   A,(VANSTERKANT)
          LD   B,A
          LD   A,(KOLNR)
          SUB  B
          LD   E,A
          CALL POS
TANGENT:  CALL HAMTA
          LD   (DENNA),A
          LD   HL,0             ; meddelandet syns tills nästa tangent
          LD   (MEDDELANDE),HL
          LD   A,(VARNAD)       ; ^KQ två gånger i följd
          LD   (VARNAD_FORUT),A
          XOR  A
          LD   (VARNAD),A
          LD   A,(DENNA)
          CALL KOMMANDO
          LD   A,(DENNA)
          LD   (SENASTE),A
          JR   SLINGA

; KOMMANDO: tangenten i A.
KOMMANDO: CP   DEL
          JP   Z,K_BAKAT
          CP   ' '
          JP   NC,K_TECKEN
          LD   L,A
          LD   H,0
          ADD  HL,HL
          LD   DE,STYRTABELL
          ADD  HL,DE
          LD   E,(HL)
          INC  HL
          LD   D,(HL)
          EX   DE,HL
          JP   (HL)

STYRTABELL:
          DEFW INGET            ; ^@
          DEFW K_ORDVANSTER     ; ^A  ett ord vänster
          DEFW INGET            ; ^B
          DEFW K_SIDNED         ; ^C  en sida ned
          DEFW K_HOGER          ; ^D  höger
          DEFW K_UPP            ; ^E  upp
          DEFW K_ORDHOGER       ; ^F  ett ord höger
          DEFW K_RADERA         ; ^G  radera tecknet
          DEFW K_VANSTER        ; ^H  vänster (ABC80:s vänsterpil)
          DEFW K_HOGER          ; ^I  höger (ABC80:s högerpil)
          DEFW K_HJALP          ; ^J  hjälpen
          DEFW K_FIL            ; ^K  spara och sluta
          DEFW K_SOKIGEN        ; ^L  sök igen
          DEFW K_RETUR          ; ^M  RETURN
          DEFW K_DELA           ; ^N  dela raden
          DEFW INGET            ; ^O
          DEFW K_ORDAGRANT      ; ^P  nästa tecken som det är
          DEFW K_SNABB          ; ^Q  ^QS ^QD ^QR ^QC ^QF ^QY
          DEFW K_SIDUPP         ; ^R  en sida upp
          DEFW K_VANSTER        ; ^S  vänster
          DEFW K_ORDRADERA      ; ^T  radera ordet
          DEFW K_KLISTRA        ; ^U  raderna från ^Y tillbaka
          DEFW K_SKRIVOVER      ; ^V  infoga eller skriv över
          DEFW K_RULLAUPP       ; ^W  rulla upp
          DEFW K_NED            ; ^X  ned
          DEFW K_RADRADERA      ; ^Y  radera raden
          DEFW K_RULLANED       ; ^Z  rulla ned
          DEFW K_ESC            ; ESC  och en bokstav, som Ctrl och den
          DEFW INGET,INGET,INGET,INGET

INGET:    RET

; ESC och en bokstav gör samma sak som Ctrl och bokstaven (ESC K X är
; ^KX), för webbläsare som tar Ctrl-W, Ctrl-N och Ctrl-T själva.
K_ESC:    LD   HL,T_ESC
          CALL PREFIX
          CP   '@'
          RET  C
          AND  $1F
          LD   (DENNA),A        ; ^Y efter ESC Y samlas också
          JP   KOMMANDO

; ----------------------------------------------------------------------
; Förflyttningarna

K_UPP:    CALL UPP_RAD
          LD   A,1
          LD   (VERTIKAL),A
          RET  C
          JP   SKARM_UPP

K_NED:    CALL NED_RAD
          LD   A,1
          LD   (VERTIKAL),A
          RET  C
          JP   SKARM_NED

K_VANSTER:
          CALL VANSTER1
          RET  C
          CP   LF
          RET  NZ
          JP   SKARM_UPP

K_HOGER:  CALL HOGER1
          RET  C
          CP   LF
          RET  NZ
          JP   SKARM_NED

; Ett ord: tecken som inte är blanka (mellanslag, TAB, radslut).
K_ORDHOGER:
          CALL EFTER            ; förbi ordet
          RET  C
          CP   '!'
          JR   C,ORDHOGER_BLANK
          CALL K_HOGER
          JR   K_ORDHOGER
ORDHOGER_BLANK:                 ; och förbi det blanka efter
          CALL EFTER
          RET  C
          CP   '!'
          RET  NC
          CALL K_HOGER
          JR   ORDHOGER_BLANK

K_ORDVANSTER:
          CALL FORE             ; förbi det blanka
          RET  C
          CP   '!'
          JR   NC,ORDVANSTER_ORD
          CALL K_VANSTER
          JR   K_ORDVANSTER
ORDVANSTER_ORD:                 ; och till ordets början
          CALL FORE
          RET  C
          CP   '!'
          RET  C
          CALL K_VANSTER
          JR   ORDVANSTER_ORD

K_SIDUPP: LD   B,TEXTRADER-1
SIDUPP:   PUSH BC
          CALL UPP_RAD
          POP  BC
          JR   C,SIDA_KLAR
          DJNZ SIDUPP
          JR   SIDA_KLAR

K_SIDNED: LD   B,TEXTRADER-1
SIDNED:   PUSH BC
          CALL NED_RAD
          POP  BC
          JR   C,SIDA_KLAR
          DJNZ SIDNED
SIDA_KLAR:
          LD   A,1
          LD   (VERTIKAL),A
          JP   MARKERA_ALLA

; ^W: texten en rad ned, om det finns en rad ovanför skärmen.
K_RULLAUPP:
          CALL RADPEKARE
          LD   A,(SKRAD)
          INC  A
          LD   B,A
RULLAUPP_PROV:
          CALL FORRA_RADEN
          RET  C
          DJNZ RULLAUPP_PROV
          LD   A,(SKRAD)
          INC  A
          CP   TEXTRADER
          JR   C,RULLAUPP_RAD
          CALL UPP_RAD          ; markören var på sista raden
          LD   A,1
          LD   (VERTIKAL),A
          LD   A,TEXTRADER-1
RULLAUPP_RAD:
          LD   (SKRAD),A
          CALL NAGOT_MARKERAT
          JP   NZ,MARKERA_ALLA
          LD   DE,0
          CALL POS
          CALL INFOGA_RAD
          XOR  A
          JP   MARKERA

; ^Z: texten en rad upp.
K_RULLANED:
          LD   A,(SKRAD)
          OR   A
          JR   NZ,RULLANED_RAD
          CALL NED_RAD          ; markören var på första raden
          LD   A,1
          LD   (VERTIKAL),A
          RET  C
          LD   A,1
RULLANED_RAD:
          DEC  A
          LD   (SKRAD),A
          CALL NAGOT_MARKERAT
          JP   NZ,MARKERA_ALLA
          LD   DE,0
          CALL POS
          CALL RADERA_RAD
          LD   A,TEXTRADER-1
          JP   MARKERA

K_BORJAN: CALL TILL_BORJAN
          XOR  A
          LD   (SKRAD),A
          JP   MARKERA_ALLA

K_SLUTET: CALL TILL_SLUTET
          LD   A,TEXTRADER-3
          LD   (SKRAD),A
          JP   MARKERA_ALLA

; SKARM_UPP och SKARM_NED: markören har gått till raden ovanför eller
; under. På första och sista raden rullar texten en rad.
SKARM_UPP:
          LD   A,(SKRAD)
          OR   A
          JR   Z,SKARM_UPP_RULLA
          DEC  A
          LD   (SKRAD),A
          RET
SKARM_UPP_RULLA:
          CALL NAGOT_MARKERAT   ; raderna som väntar flyttas inte med
          JP   NZ,MARKERA_ALLA
          LD   DE,0
          CALL POS
          CALL INFOGA_RAD
          XOR  A
          JP   MARKERA

SKARM_NED:
          LD   A,(SKRAD)
          CP   TEXTRADER-1
          JR   NC,SKARM_NED_RULLA
          INC  A
          LD   (SKRAD),A
          RET
SKARM_NED_RULLA:
          CALL NAGOT_MARKERAT
          JP   NZ,MARKERA_ALLA
          LD   DE,0
          CALL POS
          CALL RADERA_RAD
          LD   A,TEXTRADER-1
          JP   MARKERA

; ----------------------------------------------------------------------
; Ändringarna

K_TECKEN: LD   C,A
          LD   A,(SKRIVOVER)    ; skriv över: tecknet efter bort, utom
          OR   A                ; radslutet
          JR   Z,TECKEN_IN
          CALL EFTER
          JR   C,TECKEN_IN
          CP   LF
          CALL NZ,TA_BORT
TECKEN_IN:
          LD   A,C
          CALL INFOGA
          RET  C
RAD_ANDRAD:
          LD   A,(SKRAD)
          JP   MARKERA

K_ORDAGRANT:
          LD   HL,T_P
          CALL PREFIX
          CP   CR
          JR   Z,K_RETUR
          CP   LF
          JR   Z,K_RETUR
          JR   K_TECKEN

K_SKRIVOVER:
          LD   A,(SKRIVOVER)
          XOR  1
          LD   (SKRIVOVER),A
          RET

; RETURN: raden delas, och den nya raden infogas på skärmen.
K_RETUR:  CALL NAGOT_MARKERAT
          PUSH AF
          LD   A,LF
          CALL INFOGA
          JR   C,RETUR_FULLT
          POP  AF
          JR   NZ,RETUR_ALLA
          LD   A,(SKRAD)
          CP   TEXTRADER-1
          JR   NC,RETUR_SIST
          INC  A
          LD   (SKRAD),A
          LD   D,A
          LD   E,0
          CALL POS
          CALL INFOGA_RAD
          LD   A,(SKRAD)
          CALL MARKERA
          DEC  A
          JP   MARKERA
RETUR_SIST:                     ; på sista raden: allt en rad upp
          LD   DE,0
          CALL POS
          CALL RADERA_RAD
          LD   A,TEXTRADER-2
          CALL MARKERA
          INC  A
          JP   MARKERA
RETUR_ALLA:
          CALL MARKERA_ALLA
          JP   SKARM_NED
RETUR_FULLT:
          POP  AF
          RET

; ^N: som RETURN, men markören står kvar.
K_DELA:   LD   HL,(LUCKA)
          PUSH HL
          CALL K_RETUR
          POP  DE
          LD   HL,(LUCKA)
          OR   A
          SBC  HL,DE
          RET  Z
          JP   K_VANSTER

; ^G: radera tecknet där markören står; på radslutet kommer nästa rad
; upp.
K_RADERA: CALL EFTER
          RET  C
          CP   LF
          JR   Z,RADERA_RADSLUTET
          CALL TA_BORT
          JP   RAD_ANDRAD
RADERA_RADSLUTET:
          CALL NAGOT_MARKERAT
          PUSH AF
          CALL TA_BORT
          POP  AF
          JP   NZ,MARKERA_ALLA
          LD   A,(SKRAD)
          INC  A
          CP   TEXTRADER
          JP   NC,RAD_ANDRAD
          LD   D,A
          LD   E,0
          CALL POS
          CALL RADERA_RAD
          LD   A,TEXTRADER-1
          CALL MARKERA
          JP   RAD_ANDRAD

; DEL: radera tecknet före markören.
K_BAKAT:  CALL FORE
          RET  C
          CP   LF
          JR   Z,BAKAT_RADSLUT
          LD   HL,(LUCKA)
          DEC  HL
          LD   (LUCKA),HL
          CALL ANDRAT
          JP   RAD_ANDRAD
BAKAT_RADSLUT:
          CALL K_VANSTER
          JR   K_RADERA

; ^T: radera ordet och det blanka efter det, eller radslutet.
K_ORDRADERA:
          CALL EFTER
          RET  C
          CP   LF
          JR   Z,K_RADERA
ORDRADERA_ORD:
          CALL EFTER
          JR   C,ORDRADERA_KLAR
          CP   '!'
          JR   C,ORDRADERA_BLANK
          CALL TA_BORT
          JR   ORDRADERA_ORD
ORDRADERA_BLANK:
          CALL EFTER
          JR   C,ORDRADERA_KLAR
          CP   ' '
          JR   Z,ORDRADERA_TA
          CP   TAB
          JR   NZ,ORDRADERA_KLAR
ORDRADERA_TA:
          CALL TA_BORT
          JR   ORDRADERA_BLANK
ORDRADERA_KLAR:
          JP   RAD_ANDRAD

; ^Y: radera raden. Flera ^Y i följd samlas i klippbufferten.
K_RADRADERA:
          LD   A,(SENASTE)
          CP   'Y'-64
          JR   Z,RADRADERA_SAMLA
          LD   HL,0
          LD   (KLIPPLANGD),HL
RADRADERA_SAMLA:
          CALL RADBORJAN
          CALL NAGOT_MARKERAT
          PUSH AF
          LD   B,0              ; 1 när radslutet är raderat
RADRADERA_TECKEN:
          CALL EFTER
          JR   C,RADRADERA_KLAR
          CALL KLIPP_IN
          CALL TA_BORT
          CP   LF
          JR   NZ,RADRADERA_TECKEN
          INC  B
RADRADERA_KLAR:
          POP  AF
          JP   NZ,MARKERA_ALLA
          LD   A,B
          OR   A
          JP   Z,RAD_ANDRAD     ; sista raden, utan radslut
          LD   A,(SKRAD)
          LD   D,A
          LD   E,0
          CALL POS
          CALL RADERA_RAD
          LD   A,TEXTRADER-1
          JP   MARKERA

; ^QY: radera resten av raden (till klippbufferten).
K_RADRESTEN:
          LD   HL,0
          LD   (KLIPPLANGD),HL
RADRESTEN_TECKEN:
          CALL EFTER
          JP   C,RAD_ANDRAD
          CP   LF
          JP   Z,RAD_ANDRAD
          CALL KLIPP_IN
          CALL TA_BORT
          JR   RADRESTEN_TECKEN

; ^U: klippbufferten in där markören står; markören står kvar.
K_KLISTRA:
          LD   BC,(KLIPPLANGD)
          LD   A,B
          OR   C
          RET  Z
          LD   HL,(LUCKSLUT)
          LD   DE,(LUCKA)
          OR   A
          SBC  HL,DE
          OR   A
          SBC  HL,BC
          JP   C,MINNET_FULLT
          LD   HL,(LUCKSLUT)
          OR   A
          SBC  HL,BC
          LD   (LUCKSLUT),HL
          EX   DE,HL
          LD   HL,(TEXTSLUT)
          LDIR
          CALL ANDRAT
          LD   A,(SKRAD)
          JP   MARKERA_FRAN

; KLIPP_IN: A sist i klippbufferten, om den inte är full. Ändrar DE
; och HL.
KLIPP_IN: PUSH AF
          LD   HL,(KLIPPLANGD)
          LD   DE,KLIPPSTORLEK
          OR   A
          SBC  HL,DE
          JR   NC,KLIPP_FULL
          ADD  HL,DE
          INC  HL
          LD   (KLIPPLANGD),HL
          DEC  HL
          LD   DE,(TEXTSLUT)
          ADD  HL,DE
          POP  AF
          LD   (HL),A
          RET
KLIPP_FULL:
          POP  AF
          RET

; ----------------------------------------------------------------------
; Sökningen

; ^QF: skriv det som ska sökas på statusraden; RETURN direkt söker
; samma sak som förra gången.
K_SOK:    LD   DE,TEXTRADER*256
          CALL POS
          LD   HL,T_SOK
          CALL UTTEXT
          CALL RADERA_RADSLUT
          LD   C,0              ; antalet tecken
SOK_TECKEN:
          CALL HAMTA
          CP   CR
          JR   Z,SOK_KLAR
          CP   ESC
          RET  Z
          CP   8
          JR   Z,SOK_BAKAT
          CP   DEL
          JR   Z,SOK_BAKAT
          CP   ' '
          JR   C,SOK_TECKEN
          LD   B,A
          LD   A,C
          CP   SOKLANGD
          JR   NC,SOK_TECKEN
          LD   HL,SOKNY
          LD   E,C
          LD   D,0
          ADD  HL,DE
          LD   (HL),B
          INC  C
          LD   A,B
          CALL UT
          JR   SOK_TECKEN
SOK_BAKAT:
          LD   A,C
          OR   A
          JR   Z,SOK_TECKEN
          DEC  C
          LD   A,8
          CALL UT
          LD   A,' '
          CALL UT
          LD   A,8
          CALL UT
          JR   SOK_TECKEN
SOK_KLAR: LD   A,C
          OR   A
          JR   Z,K_SOKIGEN
          LD   (SOKANTAL),A
          LD   B,0
          LD   HL,SOKNY
          LD   DE,SOKTEXT
          LDIR

; ^L: sök framåt från tecknet efter markören.
K_SOKIGEN:
          LD   A,(SOKANTAL)
          OR   A
          RET  Z
          LD   HL,(LUCKSLUT)
SOK_PLATS:
          INC  HL
          PUSH HL               ; ryms texten före TEXTSLUT?
          LD   A,(SOKANTAL)
          LD   E,A
          LD   D,0
          ADD  HL,DE
          EX   DE,HL
          LD   HL,(TEXTSLUT)
          OR   A
          SBC  HL,DE
          POP  HL
          JR   C,SOK_INTE
          PUSH HL
          LD   DE,SOKTEXT
          LD   A,(SOKANTAL)
          LD   B,A
SOK_JAMFOR:
          LD   A,(DE)
          CP   (HL)
          JR   NZ,SOK_OLIKA
          INC  HL
          INC  DE
          DJNZ SOK_JAMFOR
SOK_OLIKA:
          POP  HL
          JR   NZ,SOK_PLATS
          LD   DE,(LUCKSLUT)    ; funnet: markören dit
          OR   A
          SBC  HL,DE
          LD   B,H
          LD   C,L
          LD   HL,(RADNR)
          PUSH HL
          CALL FLYTTA_HOGER
          POP  DE
          LD   HL,(RADNR)       ; rader framåt
          OR   A
          SBC  HL,DE
          LD   A,H
          OR   A
          JR   NZ,SOK_LANGT
          LD   A,(SKRAD)
          ADD  A,L
          JR   C,SOK_LANGT
          CP   TEXTRADER
          JR   NC,SOK_LANGT
          LD   (SKRAD),A
          RET
SOK_LANGT:
          LD   A,MITTEN
          LD   (SKRAD),A
          JP   MARKERA_ALLA
SOK_INTE: LD   HL,T_INTE
          LD   (MEDDELANDE),HL
          RET

; ----------------------------------------------------------------------
; ^K och ^Q

K_FIL:    LD   HL,T_K
          CALL PREFIX
          AND  $1F
          CP   'S'&$1F
          JP   Z,SPARA
          CP   'X'&$1F
          JR   Z,SPARA_SLUTA
          CP   'D'&$1F
          JR   Z,SPARA_SLUTA
          CP   'Q'&$1F
          RET  NZ
          LD   A,(ANDRAD)
          OR   A
          JR   Z,SLUTA
          LD   A,(VARNAD_FORUT)
          OR   A
          JR   NZ,SLUTA
          LD   A,1
          LD   (VARNAD),A
          LD   HL,T_VARNING
          LD   (MEDDELANDE),HL
          RET
SPARA_SLUTA:
          CALL SPARA
          RET  C
SLUTA:    LD   A,26
          CALL UT
          JP   0

K_SNABB:  LD   HL,T_Q
          CALL PREFIX
          AND  $1F
          CP   'S'&$1F
          JP   Z,RADBORJAN
          CP   'D'&$1F
          JP   Z,RADSLUTET
          CP   'R'&$1F
          JP   Z,K_BORJAN
          CP   'C'&$1F
          JP   Z,K_SLUTET
          CP   'F'&$1F
          JP   Z,K_SOK
          CP   'Y'&$1F
          JP   Z,K_RADRESTEN
          RET

; PREFIX: visa texten på (HL) på statusraden och vänta på nästa
; tangent, i A.
PREFIX:   LD   DE,TEXTRADER*256
          CALL POS
          CALL UTTEXT
          CALL RADERA_RADSLUT
          JP   HAMTA

; ^J: hjälpen, tills en tangent trycks ned.
K_HJALP:  LD   A,26
          CALL UT
          LD   HL,T_HJALP
          CALL UTTEXT
          CALL HAMTA
          LD   A,26
          CALL UT
          JP   MARKERA_ALLA

; ----------------------------------------------------------------------
; Bufferten

; FORE: A = tecknet före markören, C om markören står först.
FORE:     PUSH HL
          PUSH DE
          LD   HL,(LUCKA)
          LD   DE,TEXT
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          SCF
          JR   Z,FORE_SLUT
          DEC  HL
          LD   A,(HL)
          OR   A
FORE_SLUT:
          POP  DE
          POP  HL
          RET

; EFTER: A = tecknet där markören står, C om markören står sist.
EFTER:    PUSH HL
          PUSH DE
          LD   HL,(LUCKSLUT)
          LD   DE,(TEXTSLUT)
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          SCF
          JR   Z,EFTER_SLUT
          LD   A,(HL)
          OR   A
EFTER_SLUT:
          POP  DE
          POP  HL
          RET

; VANSTER1 och HOGER1: markören ett tecken, och tecknet den går förbi
; i A; C om det inte går. RADNR följer med.
VANSTER1: CALL FORE
          RET  C
          PUSH HL
          LD   HL,(LUCKA)
          DEC  HL
          LD   (LUCKA),HL
          LD   HL,(LUCKSLUT)
          DEC  HL
          LD   (LUCKSLUT),HL
          LD   (HL),A
          CP   LF
          JR   NZ,VANSTER1_KLAR
          LD   HL,(RADNR)
          DEC  HL
          LD   (RADNR),HL
VANSTER1_KLAR:
          POP  HL
          OR   A
          RET

HOGER1:   CALL EFTER
          RET  C
          PUSH HL
          LD   HL,(LUCKSLUT)
          INC  HL
          LD   (LUCKSLUT),HL
          LD   HL,(LUCKA)
          LD   (HL),A
          INC  HL
          LD   (LUCKA),HL
          CP   LF
          JR   NZ,HOGER1_KLAR
          LD   HL,(RADNR)
          INC  HL
          LD   (RADNR),HL
HOGER1_KLAR:
          POP  HL
          OR   A
          RET

RADBORJAN:
          CALL FORE
          RET  C
          CP   LF
          RET  Z
          CALL VANSTER1
          JR   RADBORJAN

; RADSLUTET: till radslutet; C om texten tar slut utan LF.
RADSLUTET:
          CALL EFTER
          RET  C
          CP   LF
          RET  Z
          CALL HOGER1
          JR   RADSLUTET

; UPP_RAD och NED_RAD: till raden ovanför eller under, så nära den
; önskade kolumnen som det går; C om den inte finns.
UPP_RAD:  CALL RADBORJAN
          CALL FORE
          JR   NC,UPP_FINNS
          CALL GA_TILL
          SCF
          RET
UPP_FINNS:
          CALL VANSTER1
          CALL RADBORJAN
          CALL GA_TILL
          OR   A
          RET

NED_RAD:  CALL RADSLUTET
          JR   NC,NED_FINNS
          CALL RADBORJAN
          CALL GA_TILL
          SCF
          RET
NED_FINNS:
          CALL HOGER1
          CALL GA_TILL
          OR   A
          RET

; GA_TILL: från radens början till den önskade kolumnen (ONSKAD).
GA_TILL:  LD   B,0
GA_TILL_TECKEN:
          CALL EFTER
          RET  C
          CP   LF
          RET  Z
          LD   C,B
          CALL STEG
          LD   A,(ONSKAD)
          CP   B
          JR   C,GA_TILL_FORBI
          CALL HOGER1
          JR   GA_TILL_TECKEN
GA_TILL_FORBI:
          LD   B,C
          RET

; STEG: B = kolumnen efter tecknet A i kolumnen B (TAB till nästa
; åttondel), högst 255.
STEG:     CP   TAB
          JR   Z,STEG_TAB
          INC  B
          RET  NZ
          DEC  B
          RET
STEG_TAB: LD   A,B
          OR   7
          INC  A
          JR   NZ,STEG_KLAR
          DEC  A
STEG_KLAR:
          LD   B,A
          RET

; RADPEKARE: HL = början av markörens rad. Ändrar A och DE.
RADPEKARE:
          LD   HL,(LUCKA)
          LD   DE,TEXT
RADPEKARE_BAKAT:
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          RET  Z
          DEC  HL
          LD   A,(HL)
          CP   LF
          JR   NZ,RADPEKARE_BAKAT
          INC  HL
          RET

; KOLUMN: A = markörens kolumn.
KOLUMN:   CALL RADPEKARE
          LD   B,0
KOLUMN_TECKEN:
          LD   DE,(LUCKA)
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          LD   A,B
          RET  Z
          LD   A,(HL)
          INC  HL
          CALL STEG
          JR   KOLUMN_TECKEN

; FORRA_RADEN: HL från början av en rad före markören till början av
; raden före; C om det inte finns någon. Ändrar A och DE.
FORRA_RADEN:
          LD   DE,TEXT
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          SCF
          RET  Z
          DEC  HL               ; radslutet på raden före
FORRA_TECKEN:
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          RET  Z
          DEC  HL
          LD   A,(HL)
          CP   LF
          JR   NZ,FORRA_TECKEN
          INC  HL
          OR   A
          RET

; NASTA: A = tecknet på HL och HL till nästa, förbi luckan; C när
; texten är slut.
NASTA:    PUSH DE
          LD   DE,(LUCKA)
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          JR   NZ,NASTA_PROV
          LD   HL,(LUCKSLUT)
NASTA_PROV:
          LD   DE,(TEXTSLUT)
          OR   A
          SBC  HL,DE
          ADD  HL,DE
          POP  DE
          SCF
          RET  Z
          LD   A,(HL)
          INC  HL
          OR   A
          RET

; HOPPA_RAD: HL till nästa rads början (eller textens slut).
HOPPA_RAD:
          CALL NASTA
          RET  C
          CP   LF
          JR   NZ,HOPPA_RAD
          RET

; INFOGA: A in där markören står; C om minnet är fullt. Ändrar inte
; DE och HL.
INFOGA:   PUSH HL
          PUSH DE
          LD   HL,(LUCKA)
          LD   DE,(LUCKSLUT)
          OR   A
          SBC  HL,DE
          JR   Z,INFOGA_FULLT
          ADD  HL,DE
          LD   (HL),A
          INC  HL
          LD   (LUCKA),HL
          CP   LF
          JR   NZ,INFOGA_KLAR
          LD   HL,(RADNR)
          INC  HL
          LD   (RADNR),HL
INFOGA_KLAR:
          POP  DE
          POP  HL
          JR   ANDRAT
INFOGA_FULLT:
          POP  DE
          POP  HL
          JR   MINNET_FULLT
ANDRAT:   PUSH AF
          LD   A,1
          LD   (ANDRAD),A
          POP  AF
          OR   A
          RET
MINNET_FULLT:
          LD   HL,T_FULLT
          LD   (MEDDELANDE),HL
          SCF
          RET

; TA_BORT: radera tecknet där markören står (det finns). Ändrar inte A.
TA_BORT:  LD   HL,(LUCKSLUT)
          INC  HL
          LD   (LUCKSLUT),HL
          JR   ANDRAT

TILL_BORJAN:
          LD   HL,1
          LD   (RADNR),HL
          LD   HL,(LUCKA)
          LD   DE,TEXT
          OR   A
          SBC  HL,DE
          LD   B,H
          LD   C,L
          LD   A,B
          OR   C
          RET  Z
          LD   HL,(LUCKA)
          DEC  HL
          LD   DE,(LUCKSLUT)
          DEC  DE
          LDDR
          INC  DE
          LD   (LUCKSLUT),DE
          LD   HL,TEXT
          LD   (LUCKA),HL
          RET

TILL_SLUTET:
          LD   HL,(TEXTSLUT)
          LD   DE,(LUCKSLUT)
          OR   A
          SBC  HL,DE
          LD   B,H
          LD   C,L

; FLYTTA_HOGER: markören BC tecken framåt (de finns), med RADNR.
FLYTTA_HOGER:
          LD   A,B
          OR   C
          RET  Z
          PUSH BC
          LD   HL,(LUCKSLUT)    ; radsluten räknas med CPIR
          LD   DE,0
          LD   A,LF
FLYTTA_RAKNA:
          CPIR
          JR   NZ,FLYTTA_RAKNAT
          INC  DE
          JP   PE,FLYTTA_RAKNA
FLYTTA_RAKNAT:
          LD   HL,(RADNR)
          ADD  HL,DE
          LD   (RADNR),HL
          POP  BC
          LD   HL,(LUCKSLUT)
          LD   DE,(LUCKA)
          LDIR
          LD   (LUCKSLUT),HL
          LD   (LUCKA),DE
          RET

; ----------------------------------------------------------------------
; Skärmen

; RITA: rita de markerade raderna. NZ om en tangent kom emellan; då
; står resten kvar som markerat.
RITA:     LD   HL,RITAS
          LD   B,0
RITA_SOK: LD   A,(HL)
          OR   A
          JR   NZ,RITA_FORSTA
          INC  HL
          INC  B
          LD   A,B
          CP   TEXTRADER
          JR   C,RITA_SOK
          XOR  A
          RET
RITA_FORSTA:
          LD   A,B
          LD   (RITARAD),A
          OR   A                ; allt ritas om: markörens rad kan ha
          CALL Z,TOPPFIX        ; för få rader ovanför sig
          CALL RADPEKARE        ; HL = den första markerade raden
          LD   A,(SKRAD)
          LD   B,A
          LD   A,(RITARAD)
          SUB  B
          JR   Z,RITA_RAD
          JR   NC,RITA_FRAMAT
          NEG
          LD   B,A
RITA_BAKAT:
          CALL FORRA_RADEN
          DJNZ RITA_BAKAT
          JR   RITA_RAD
RITA_FRAMAT:
          LD   B,A
RITA_HOPPA:
          CALL HOPPA_RAD
          DJNZ RITA_HOPPA
RITA_RAD: LD   A,(RITARAD)
          LD   E,A
          LD   D,0
          PUSH HL
          LD   HL,RITAS
          ADD  HL,DE
          LD   A,(HL)
          LD   (HL),0
          POP  HL
          OR   A
          JR   Z,RITA_INTE
          LD   A,(RITARAD)
          CALL RITA_EN
          CALL POLLA
          CALL KO_TOM
          RET  NZ
          JR   RITA_NASTA
RITA_INTE:
          CALL HOPPA_RAD
RITA_NASTA:
          LD   A,(RITARAD)
          INC  A
          LD   (RITARAD),A
          CP   TEXTRADER
          JR   C,RITA_RAD
          XOR  A
          RET

; TOPPFIX: SKRAD högst antalet rader före markörens.
TOPPFIX:  CALL RADPEKARE
          LD   A,(SKRAD)
          OR   A
          RET  Z
          LD   B,A
          LD   C,0
TOPPFIX_RAD:
          CALL FORRA_RADEN
          JR   C,TOPPFIX_KLAR
          INC  C
          DJNZ TOPPFIX_RAD
TOPPFIX_KLAR:
          LD   A,C
          LD   (SKRAD),A
          RET

; RITA_EN: raden från HL på skärmraden A, från VANSTERKANT; HL till
; nästa rad.
RITA_EN:  LD   D,A
          LD   E,0
          CALL POS
          LD   BC,0             ; B kolumnen i texten, C tecknen på skärmen
RITA_TECKEN:
          CALL NASTA
          JR   C,RITA_SLUT
          CP   LF
          JR   Z,RITA_SLUT
          CP   TAB
          JR   Z,RITA_TAB
          CP   ' '
          JR   NC,RITA_SYNS
          LD   A,'?'            ; andra styrtecken
RITA_SYNS:
          CALL VISA
          INC  B
          JR   NZ,RITA_TECKEN
          DEC  B
          JR   RITA_TECKEN
RITA_TAB: LD   A,' '
          CALL VISA
          INC  B
          JR   Z,RITA_TAB_SIST
          LD   A,B
          AND  7
          JR   NZ,RITA_TAB
          JR   RITA_TECKEN
RITA_TAB_SIST:
          DEC  B
          JR   RITA_TECKEN
RITA_SLUT:
          LD   A,(BREDD)        ; hela raden skriven: BIOS:en står redan
          CP   C                ; på nästa, så ingen ESC T
          RET  Z
          JP   RADERA_RADSLUT

; VISA: A på skärmen om kolumnen B syns och raden inte är full.
VISA:     PUSH AF
          LD   A,(BREDD)
          CP   C
          JR   Z,VISA_INTE
          LD   A,(VANSTERKANT)
          LD   E,A
          LD   A,B
          SUB  E
          JR   C,VISA_INTE
          POP  AF
          CALL UT
          INC  C
          RET
VISA_INTE:
          POP  AF
          RET

; SIDLED: flytta vänsterkanten om markörens kolumn inte syns.
SIDLED:   LD   A,(VANSTERKANT)
          LD   B,A
          LD   A,(KOLNR)
          SUB  B
          JR   C,SIDLED_FLYTTA
          LD   C,A
          LD   A,(BREDD)
          DEC  A
          CP   C
          RET  NC
SIDLED_FLYTTA:
          LD   A,(BREDD)
          SRL  A
          LD   B,A
          LD   A,(KOLNR)
          SUB  B
          JR   NC,SIDLED_KANT
          XOR  A
SIDLED_KANT:
          AND  $F8
          LD   (VANSTERKANT),A
          JP   MARKERA_ALLA

; MARKERA: skärmraden A ska ritas om. Ändrar inte A.
MARKERA:  CP   TEXTRADER
          RET  NC
          PUSH HL
          PUSH DE
          LD   E,A
          LD   D,0
          LD   HL,RITAS
          ADD  HL,DE
          LD   (HL),1
          POP  DE
          POP  HL
          RET

MARKERA_ALLA:
          XOR  A
MARKERA_FRAN:                   ; rad A och alla under
          CALL MARKERA
          INC  A
          CP   TEXTRADER
          JR   C,MARKERA_FRAN
          RET

; NAGOT_MARKERAT: NZ om någon rad väntar på att ritas.
NAGOT_MARKERAT:
          PUSH HL
          PUSH BC
          LD   HL,RITAS
          LD   B,TEXTRADER
          XOR  A
NAGOT_RAD:
          OR   (HL)
          INC  HL
          DJNZ NAGOT_RAD
          POP  BC
          POP  HL
          OR   A
          RET

; STATUS: meddelandet, eller filen, * om den är ändrad, raden och
; kolumnen.
STATUS:   LD   DE,TEXTRADER*256
          CALL POS
          LD   HL,(MEDDELANDE)
          LD   A,H
          OR   L
          JR   Z,STATUS_FIL
          CALL UTTEXT
          JP   RADERA_RADSLUT
STATUS_FIL:
          LD   A,(FILNAMN)
          ADD  A,'A'-1
          CALL UT
          LD   A,':'
          CALL UT
          LD   HL,FILNAMN+1
          LD   B,8
          CALL UTNAMN
          LD   A,'.'
          CALL UT
          LD   B,3
          CALL UTNAMN
          LD   A,(ANDRAD)
          OR   A
          LD   A,' '
          JR   Z,STATUS_ANDRAD
          LD   A,'*'
STATUS_ANDRAD:
          CALL UT
          LD   HL,T_RAD
          CALL UTTEXT
          LD   HL,(RADNR)
          CALL UTTAL
          LD   HL,T_KOL
          CALL UTTEXT
          LD   A,(KOLNR)
          LD   L,A
          LD   H,0
          INC  HL
          CALL UTTAL
          LD   A,(SKRIVOVER)
          OR   A
          LD   HL,T_OVER
          CALL NZ,UTTEXT
          JP   RADERA_RADSLUT

; UTNAMN: B tecken från HL, utan mellanslagen.
UTNAMN:   LD   A,(HL)
          AND  $7F
          CP   ' '
          CALL NZ,UT
          INC  HL
          DJNZ UTNAMN
          RET

; UTTAL: HL som decimaltal.
UTTAL:    LD   E,0              ; 1 när en siffra är skriven
          LD   BC,-10000
          CALL SIFFRA
          LD   BC,-1000
          CALL SIFFRA
          LD   BC,-100
          CALL SIFFRA
          LD   BC,-10
          CALL SIFFRA
          LD   A,L
          ADD  A,'0'
          JP   UT
SIFFRA:   LD   A,'0'-1
SIFFRA_RAKNA:
          INC  A
          ADD  HL,BC
          JR   C,SIFFRA_RAKNA
          SBC  HL,BC
          CP   '0'
          JR   NZ,SIFFRA_SKRIV
          INC  E
          DEC  E
          RET  Z
SIFFRA_SKRIV:
          LD   E,1
          JP   UT

; UTTEXT: texten på HL, som slutar med 0.
UTTEXT:   LD   A,(HL)
          OR   A
          RET  Z
          CALL UT
          INC  HL
          JR   UTTEXT

; POS: markören till rad D, kolumn E.
POS:      LD   A,ESC
          CALL UT
          LD   A,'='
          CALL UT
          LD   A,D
          ADD  A,32
          CALL UT
          LD   A,E
          ADD  A,32
          JP   UT

RADERA_RADSLUT:
          LD   A,'T'
          JR   ESCKOD
INFOGA_RAD:
          LD   A,'E'
          JR   ESCKOD
RADERA_RAD:
          LD   A,'R'
ESCKOD:   PUSH AF
          LD   A,ESC
          CALL UT
          POP  AF

; UT: tecknet A till skärmen genom BIOS:en, och en tangent som är nere
; till kön (en tangent är nere i 40 ms, kortare tid än en rad med 80
; tecken tar). Ändrar inget annat än A och flaggorna.
UT:       PUSH BC
          PUSH DE
          PUSH HL
          LD   C,A
          CALL BIOS_KONUT
          POP  HL
          POP  DE
          POP  BC
          JP   POLLA

BIOS_KONST: JP 0                ; adresserna sätts av START
BIOS_KONIN: JP 0
BIOS_KONUT: JP 0

; ----------------------------------------------------------------------
; Tangenterna

; POLLA: en tangent som är nere till kön. Ändrar A och flaggorna.
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
          AND  15
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

; KO_TOM: Z om kön är tom.
KO_TOM:   PUSH HL
          LD   A,(KO_IN)
          LD   HL,KO_UT
          CP   (HL)
          POP  HL
          RET

; HAMTA: A = nästa tangent, ur kön eller från tangentbordet.
HAMTA:    CALL KO_TOM
          JR   Z,HAMTA_VANTA
          PUSH DE
          PUSH HL
          LD   A,(KO_UT)
          LD   E,A
          INC  A
          AND  15
          LD   (KO_UT),A
          LD   D,0
          LD   HL,KO
          ADD  HL,DE
          LD   A,(HL)
          POP  HL
          POP  DE
          RET
HAMTA_VANTA:
          PUSH BC
          PUSH DE
          PUSH HL
          CALL BIOS_KONIN
          POP  HL
          POP  DE
          POP  BC
          RET

; ----------------------------------------------------------------------
; Filen

; GOR_FCB: ett FCB på DE med filens namn, och typen på (HL) om HL inte
; är 0.
GOR_FCB:  PUSH HL
          LD   HL,FILNAMN
          LD   BC,12
          LDIR
          LD   B,24
          XOR  A
GOR_FCB_NOLL:
          LD   (DE),A
          INC  DE
          DJNZ GOR_FCB_NOLL
          POP  HL
          LD   A,H
          OR   L
          RET  Z
          EX   DE,HL            ; typen på FCB + 9
          LD   BC,-27
          ADD  HL,BC
          EX   DE,HL
          LD   BC,3
          LDIR
          RET

; LADDA: läs filen till luckan, utan CR och fram till ^Z.
LADDA:    LD   DE,FCB_FIL
          LD   HL,0
          CALL GOR_FCB
          LD   DE,FCB_FIL
          LD   C,15             ; öppna
          CALL BDOS
          INC  A
          JR   NZ,LADDA_FINNS
          LD   HL,T_NY
          LD   (MEDDELANDE),HL
          RET
LADDA_FINNS:
          LD   DE,DMABUF
          LD   C,26
          CALL BDOS
LADDA_POST:
          LD   DE,FCB_FIL
          LD   C,20             ; läs nästa post
          CALL BDOS
          OR   A
          RET  NZ
          LD   HL,DMABUF
          LD   B,128
LADDA_BYTE:
          LD   A,(HL)
          INC  HL
          CP   EOF
          RET  Z
          CP   CR
          JR   Z,LADDA_NASTA
          CALL INFOGA
          JR   C,LADDA_STOR
LADDA_NASTA:
          DJNZ LADDA_BYTE
          JR   LADDA_POST
LADDA_STOR:
          LD   HL,T_STOR
          CALL UTTEXT
          JP   0

; SPARA: skriv filen som namn.$$$, radera namn.BAK, byt namn.typ till
; namn.BAK och namn.$$$ till namn.typ. C om det inte gick.
SPARA:    LD   DE,FCB_NY
          LD   HL,T_TMP
          CALL GOR_FCB
          LD   DE,FCB_NY
          LD   C,19             ; radera
          CALL BDOS
          LD   DE,FCB_NY
          LD   C,22             ; skapa
          CALL BDOS
          INC  A
          LD   HL,T_KATALOG
          JP   Z,SPARA_FEL
          LD   DE,DMABUF
          LD   C,26
          CALL BDOS
          LD   HL,DMABUF
          LD   (SPARPEKARE),HL
          LD   HL,TEXT
SPARA_TECKEN:
          CALL NASTA
          JR   C,SPARA_SLUT
          CP   LF
          JR   NZ,SPARA_BYTE
          LD   A,CR
          CALL SPARBYTE
          JR   C,SPARA_FULL
          LD   A,LF
SPARA_BYTE:
          CALL SPARBYTE
          JR   C,SPARA_FULL
          JR   SPARA_TECKEN
SPARA_SLUT:                     ; ^Z till postens slut
          LD   A,EOF
          CALL SPARBYTE
          JR   C,SPARA_FULL
          LD   A,(SPARPEKARE)
          CP   DMABUF
          JR   NZ,SPARA_SLUT
          LD   DE,FCB_NY
          LD   C,16             ; stäng
          CALL BDOS
          INC  A
          LD   HL,T_SKIVFEL
          JR   Z,SPARA_FEL
          LD   DE,FCB_BAK
          LD   HL,T_BAK
          CALL GOR_FCB
          LD   DE,FCB_BAK
          LD   C,19
          CALL BDOS
          LD   DE,FCB_BYT       ; namn.typ till namn.BAK
          LD   HL,0
          CALL GOR_FCB
          LD   HL,FCB_BAK
          LD   DE,FCB_BYT+16
          LD   BC,16
          LDIR
          LD   DE,FCB_BYT
          LD   C,23             ; byt namn
          CALL BDOS
          LD   HL,FCB_NY        ; namn.$$$ till namn.typ
          LD   DE,FCB_BYT
          LD   BC,16
          LDIR
          LD   DE,FCB_BYT+16
          LD   HL,0
          CALL GOR_FCB
          LD   DE,FCB_BYT
          LD   C,23
          CALL BDOS
          XOR  A
          LD   (ANDRAD),A
          LD   HL,T_SPARAD
          LD   (MEDDELANDE),HL
          OR   A
          RET
SPARA_FULL:
          LD   DE,FCB_NY
          LD   C,16
          CALL BDOS
          LD   DE,FCB_NY
          LD   C,19
          CALL BDOS
          LD   HL,T_SKIVFULL
SPARA_FEL:
          LD   (MEDDELANDE),HL
          SCF
          RET

; SPARBYTE: A till posten; en full post skrivs. C om skivan är full.
SPARBYTE: PUSH HL
          LD   HL,(SPARPEKARE)
          LD   (HL),A
          INC  HL
          LD   (SPARPEKARE),HL
          LD   A,H              ; posten är $80-$FF
          OR   A
          JR   Z,SPARBYTE_KLAR
          LD   HL,DMABUF
          LD   (SPARPEKARE),HL
          LD   DE,FCB_NY
          LD   C,21             ; skriv nästa post
          CALL BDOS
          OR   A
          JR   Z,SPARBYTE_KLAR
          POP  HL
          SCF
          RET
SPARBYTE_KLAR:
          POP  HL
          OR   A
          RET

; ----------------------------------------------------------------------
; Texterna (ABC80:s teckensats: { | } är ä ö å, [ ] är Ä Å)

T_ANVANDNING:
          DEFB "SKRIV fil.typ",13,10,0
T_START:  DEFB "Ctrl-J visar tangenterna",0
T_NY:     DEFB "Ny fil, Ctrl-J visar tangenterna",0
T_STOR:   DEFB "Filen ryms inte i minnet",13,10,0
T_FULLT:  DEFB "Minnet {r fullt",0
T_SPARAD: DEFB "Sparad",0
T_KATALOG:DEFB "Katalogen {r full, inte sparad",0
T_SKIVFULL:
          DEFB "Skivan {r full, inte sparad",0
T_SKIVFEL:DEFB "Kunde inte st{nga filen",0
T_VARNING:DEFB "Inte sparad! Ctrl-KQ igen slutar {nd}",0
T_INTE:   DEFB "Hittades inte",0
T_SOK:    DEFB "S|k: ",0
T_K:      DEFB "Ctrl-K",0
T_Q:      DEFB "Ctrl-Q",0
T_P:      DEFB "Ctrl-P",0
T_ESC:    DEFB "ESC",0
T_RAD:    DEFB " rad ",0
T_KOL:    DEFB " kol ",0
T_OVER:   DEFB " |ver",0
T_TMP:    DEFB "$$$"
T_BAK:    DEFB "BAK"
T_HJALP:  DEFB "SKRIV, med Ctrl nedtryckt (WordStar):",13,10,13,10
          DEFB "E X S D  upp ned v{nster h|ger",13,10
          DEFB "A F      ett ord v{nster, h|ger",13,10
          DEFB "R C      en sida upp, ned",13,10
          DEFB "W Z      rulla en rad upp, ned",13,10
          DEFB "QS QD    radens b|rjan, slut",13,10
          DEFB "QR QC    filens b|rjan, slut",13,10
          DEFB "G        radera tecknet (och DEL)",13,10
          DEFB "T Y      radera ordet, raden",13,10
          DEFB "QY       radera resten av raden",13,10
          DEFB "U        raderna fr}n Y tillbaka",13,10
          DEFB "N        dela raden",13,10
          DEFB "V        infoga eller skriv |ver",13,10
          DEFB "P        n{sta tangent som tecken",13,10
          DEFB "QF L     s|k, s|k igen",13,10
          DEFB "KS       spara",13,10
          DEFB "KX KD    spara och sluta",13,10
          DEFB "KQ       sluta utan att spara",13,10
          DEFB "J        den h{r hj{lpen",13,10,13,10
          DEFB "ESC (Ctrl-[) och bokstav: som med Ctrl",13,10
          DEFB "Pilarna g}r ocks}. Tryck p} en tangent.",0

; ----------------------------------------------------------------------
; Variablerna

LUCKA:    DEFW 0                ; luckans början, där markören står
LUCKSLUT: DEFW 0                ; första tecknet efter markören
TEXTSLUT: DEFW 0                ; textens slut, klippbufferten börjar
RADNR:    DEFW 1                ; markörens rad i texten, från 1
MEDDELANDE: DEFW 0              ; texten på statusraden, eller 0
KLIPPLANGD: DEFW 0
SPARPEKARE: DEFW 0
BREDD:    DEFB 40               ; skärmens kolumner
SKRAD:    DEFB 0                ; markörens rad på skärmen
KOLNR:    DEFB 0                ; markörens kolumn i texten
ONSKAD:   DEFB 0                ; kolumnen upp och ned försöker hålla
VANSTERKANT: DEFB 0             ; textens kolumn längst till vänster
VERTIKAL: DEFB 0                ; 1 efter upp och ned
ANDRAD:   DEFB 0                ; 1 om texten är ändrad sedan den sparades
SKRIVOVER: DEFB 0               ; 1: skriv över, 0: infoga
VARNAD:   DEFB 0
VARNAD_FORUT: DEFB 0
DENNA:    DEFB 0                ; tangenten som behandlas
SENASTE:  DEFB 0                ; tangenten före
RITARAD:  DEFB 0
SOKANTAL: DEFB 0
KO_IN:    DEFB 0
KO_UT:    DEFB 0
KO:       DEFS 16
RITAS:    DEFS TEXTRADER        ; 1: raden ska ritas om
FILNAMN:  DEFS 12               ; enheten (1 = A:), namnet och typen
FCB_FIL:  DEFS 36
FCB_NY:   DEFS 36
FCB_BAK:  DEFS 36
FCB_BYT:  DEFS 52               ; det gamla namnet och, på +16, det nya
SOKTEXT:  DEFS SOKLANGD
SOKNY:    DEFS SOKLANGD
          DEFS 128
STACK     EQU  $
TEXT      EQU  $                ; texten börjar här

          END
