; BASICINI.SYS för forth.img: Forth startar direkt.
;
; DOS:en öppnar BASICINI.SYS när maskinen startar och läser post 0 till
; sektorbufferten på $F500 (OPNINI). Börjar posten (sektorns byte 3) med
; JP körs koden när DOSINI är klar, med HL = $6F00, som ska tillbaka till
; EXTSCN. Koden laddar FORTH.ABS (DRUN, LADDAD flyttar BASIC till
; $B800 och kommer tillbaka hit), stänger av feltexterna ur filen
; (ERRTXV = 0, felen blir nummer som utan BASICINI.SYS) och låter AUTOST
; peka på KOMMANDO. När BASIC är klar kopierar ROM:en 160 bytes därifrån
; till radbufferten och utför raden, utan att visa den: skärmen töms,
; texten skrivs ut och Forth startar. BYE går till BASIC med Forth kvar.

; Med PROGRAM EQU 1 (gor.sh gör det till program.img) startar inte Forth:
; skärmen visar programmen på skivan, och BASIC väntar med Forth laddat.
PROGRAM EQU  0
DRUN    EQU  $601E
AUTOST  EQU  $FF83
ERRTXV  EQU  $FFAC
FLYTT   EQU  $B700          ; under $B800, där BASIC börjar efter LADDAD

; DRUN laddar alltid på kanal 0, med bufferten på $F500 där den här
; koden står. Därför flyttas resten först till FLYTT och körs där;
; adresserna i den är X-KOPIA+FLYTT.
        ORG  $F503
        JP   START
        DEFS 16,$FF             ; bytes 6-21: INIBUF, används inte

START:  PUSH HL
        PUSH IX
        PUSH IY
        LD   HL,KOPIA
        LD   DE,FLYTT
        LD   BC,SLUT-KOPIA
        LDIR
        JP   FLYTT

KOPIA:  LD   DE,NAMN-KOPIA+FLYTT
        LD   C,$FF              ; vilken enhet som helst
        CALL DRUN
        LD   HL,0
        LD   (ERRTXV),HL
        LD   HL,KOMMANDO-KOPIA+FLYTT
        LD   (AUTOST),HL
        POP  IY
        POP  IX
        POP  HL
        RET

NAMN:   DEFM "FORTH   ABS"

; ABC:s svenska tecken: { = ä, | = ö, } = å.
; Med 241 bytes i BASICINI.SYS kom Error 20 vid starten, med 225 gick
; det: raden här hålls kort, och ; står för PRINT.
KOMMANDO:
        IF PROGRAM
        DEFM ";CHR$(12);"
        DEFB 34
        DEFM "Programskivan till ABC 802. Forth {r laddat."
        DEFB 34
        DEFM ":;:;"
        DEFB 34
        DEFM "RUN ORD     ordbehandlaren"
        DEFB 34
        DEFM ":;"
        DEFB 34
        DEFM "RUN KALKYL  kalkylprogrammet"
        DEFB 34
        DEFM ":;"
        DEFB 34
        DEFM "RUN VERK    text och kalkyl"
        DEFB 34
        DEFM ":;"
        DEFB 13
        ELSE
        DEFM "PRINT CHR$(12);"
        DEFB 34
        DEFM "Forth f|r ABC 802. BYE g}r till BASIC (RUN ORD, DEMO, MASK)"
        DEFB 34
        DEFM ":PRINT "
        DEFB 34
        DEFM "och Z=CALL(32768,1) tillbaka."
        DEFB 34
        DEFM ":PRINT:Z=CALL(32768)"
        DEFB 13
        ENDIF
SLUT:
