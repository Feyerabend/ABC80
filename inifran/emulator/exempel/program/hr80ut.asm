; hr80ut.asm -- ett tecken till V24, för HR80.BAS med ABC80:s egen ROM,
; ett exempel till ABC80 inifrån (grafikkortet i HR-80:s anda, utanför
; boken)
;
; ABC80:s ROM har ingen utskrift på V24, så HR80.BAS lägger den här
; rutinen på $FF80, i de 128 byte som bruksanvisningen lämnar till egen
; maskinkod, och skickar varje tecken med Z=CALL(65408,T). EXCALL lämnar
; T i DE.
;
; Sändningen är som BASIC II:s PROUT: PIO B (port $3A) bit 3 är TxD
; (1 = vila), bit 4 är 0 medan tecknet går och 1 efteråt, och bit 1 = 1
; säger att mottagaren inte är klar. Startbit 0, 8 databitar med den
; lägsta först, en stoppbit, 9600 baud: 312 T-cykler per bit med
; ABC80:s 2,9952 MHz. Bara bit 3 och 4 ändras: bit 5 och 6 (kassettens
; motor och skrivsignal) skrivs tillbaka som de lästes, och bit 0-2 och 7
; är ingångar som en OUT inte når. Tangentbordet sitter på PIO A (port
; $38), som rutinen inte rör. Avbrotten är av medan tecknet går, så att
; bitarna blir lika långa. Är inget kort inkopplat väntar rutinen på
; klar; CTRL-C (BRKFLG) går då tillbaka utan att skicka, och BASIC stannar
; efter CALL.

BRKFLG  EQU $FE07               ; skilt från 0 efter CTRL-C (KEYINT)

        ORG $FF80

SKICKA: LD   C,E                ; C = tecknet
        LD   B,10               ; start + 8 bitar + stopp
        IN   A,($3A)
        OR   $08                ; vila (1), bit 4 = 0
        AND  $EF
        OUT  ($3A),A
VANTA:  LD   A,(BRKFLG)
        AND  A                  ; CTRL-C?
        JR   NZ,SLUT
        IN   A,($3A)
        AND  $02                ; mottagaren klar? (CY = 0: startbiten)
        JR   NZ,VANTA
        DI
BIT:    IN   A,($3A)            ; 11
        JR   C,ETT              ; 7 / 12
        AND  $F7                ; 7: bit 3 = 0
        JR   UT                 ; 12
ETT:    OR   $08                ; 7: bit 3 = 1
        NOP                     ; 4 } som AND och JR
        NOP                     ; 4 }
UT:     OUT  ($3A),A            ; 11
        LD   A,15               ; 7: vänta, 15 x 16 - 5 = 235
DROJ:   DEC  A                  ; 4
        JR   NZ,DROJ            ; 12 / 7
        SCF                     ; 4
        RR   C                  ; 8: nästa bit, ettor in (stoppbiten)
        DJNZ BIT                ; 13: 11+7+7+12+11+7+235+4+8+13 = 315
SLUT:   IN   A,($3A)
        OR   $10                ; bit 4 = 1: tecknet är slut
        OUT  ($3A),A
        EI
        LD   HL,0
        RET
