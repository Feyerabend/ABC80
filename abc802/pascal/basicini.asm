; BASICINI.SYS för pascal.img: Pascal startar direkt.
;
; Som forth/basicini.asm: DOS:en läser post 0 till $F500 när maskinen
; startar och kör koden här, med HL = $6F00 som ska tillbaka. Koden
; flyttar resten till FLYTT, eftersom DRUN läser på kanal 0 med
; bufferten där den här koden står, laddar PASCAL.ABS (DRUN, LADDAD
; flyttar BASIC till $ED00 och kommer tillbaka hit), stänger av
; feltexterna ur filen och låter AUTOST peka på KOMMANDO, som BASIC
; utför när den är klar: skärmen töms, texten skrivs och tolken frågar
; efter programmet.
DRUN    EQU  $601E
AUTOST  EQU  $FF83
ERRTXV  EQU  $FFAC
FLYTT   EQU  $EC00          ; under $ED00, där BASIC börjar efter LADDAD,
                            ; och över PASCAL.ABS ($8000-$8FFF)

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

NAMN:   DEFM "PASCAL  ABS"

; ABC:s svenska tecken: { = ä, | = ö, } = å. Raden hålls kort (se
; forth/basicini.asm: med 241 bytes i BASICINI.SYS kom Error 20).
KOMMANDO:
        DEFM "PRINT CHR$(12);"
        DEFB 34
        DEFM "Pascal f|r ABC 802. PC kompilerar, t.ex. PC.PAS till PC.PK."
        DEFB 34
        DEFM ":PRINT "
        DEFB 34
        DEFM "Z=CALL(32768) k|r ett program igen."
        DEFB 34
        DEFM ":PRINT:Z=CALL(32768)"
        DEFB 13
SLUT:
