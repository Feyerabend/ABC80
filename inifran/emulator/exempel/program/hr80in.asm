; hr80in.asm -- grafikkortets svar på V24, för HR80.BAS och FONSTER2.BAS
; med ABC80:s egen ROM, ett exempel till ABC80 inifrån (grafikkortet i
; HR-80:s anda och dess fönster, utanför boken)
;
; Ligger direkt efter hr80ut.asm, på $FFBB, i de 128 byte på $FF80 som
; bruksanvisningen lämnar till egen maskinkod, och anropas med
; Z=CALL(65467). Rutinen skickar ESC ? med SKICKA och läser de fyra
; byte som kortet svarar med till SVAR, $FFFC-$FFFF (65532-65535), där
; BASIC tar dem med PEEK: slaget ('K', 'L', 'F', 'S', 'R' eller 0 för
; inget), fönstret och två tal.
;
; ABC80 tar emot V24 i programvaran: RxD är PIO B (port $3A) bit 0, och
; varje bit läses när den kommer. Avbrotten är därför av medan svaret
; kommer, och kortet sänder bara när det får frågan (en millisekund
; efter). Startbiten letas upp i en slinga om 48 T-cykler, och varje bit
; läses mitt i: först 1,5 bit efter startbitens flank, sedan var 310:e
; T-cykel (9600 baud är 312 vid 2,9952 MHz). NMI-klockan går inte att
; stänga av och tar omkring 110 T; kommer den mitt i en byte flyttas de
; bitar som återstår så mycket, men de läses fortfarande inom sin bit.
; Kommer ingen startbit på omkring en sekund (inget kort) slutar
; rutinen, och SVAR börjar då med 0.
;
; UFD-DOS kopierar sin korttabell till $FFC0-$FFE0 och har UFDUN på
; $FFF9, så rutinen går inte med UFD-DOS. ABC-DOS (exempeldisketten)
; lämnar $FFB0-$FFFF orört.

SKICKA  EQU $FF80               ; hr80ut.asm: tecknet i E
SVAR    EQU $FFFC               ; fyra byte

        ORG $FFBB

HAMTA:  LD   E,27               ; ESC
        CALL SKICKA
        LD   E,'?'
        CALL SKICKA
        LD   HL,SVAR
        LD   (HL),0             ; inget, om inget svar kommer
        LD   D,4                ; fyra byte
        DI
NASTA:  LD   BC,0               ; 65 536 varv, omkring en sekund
START:  IN   A,($3A)            ; 11
        RRA                     ; 4: RxD till CY
        JR   NC,FUNNEN          ; 7 / 12: startbiten
        DEC  BC                 ; 6
        LD   A,B                ; 4
        OR   C                  ; 4
        JR   NZ,START           ; 12: 48 per varv
        JR   KLAR
FUNNEN: LD   A,7                ; 7: till mitten av den första biten
DROJ1:  DEC  A                  ; 4
        JR   NZ,DROJ1           ; 12 / 7
        LD   B,8                ; 7
BIT:    LD   A,17               ; 7: 17 x 16 - 5 = 267
DROJ2:  DEC  A                  ; 4
        JR   NZ,DROJ2           ; 12 / 7
        IN   A,($3A)            ; 11
        RRA                     ; 4
        RR   C                  ; 8: den lägsta biten först
        DJNZ BIT                ; 13: 7+267+11+4+8+13 = 310
        LD   (HL),C
        INC  HL
        LD   A,15               ; in i stoppbiten, så att en 0 i den
DROJ3:  DEC  A                  ; sista biten inte tas för nästa
        JR   NZ,DROJ3           ; startbit
        DEC  D
        JR   NZ,NASTA
KLAR:   EI
        RET
