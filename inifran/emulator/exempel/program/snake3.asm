; snake3.asm -- SNAKE3 i Z80-kod för BASIC II på ABC80, ett exempel till
; ABC80 inifrån
;
; Samma spel som text_b2/SNAKE3.BAS, men i maskinkod. gor.sh assemblerar
; det och gör laddaren text_b2/SNAKE3Z.BAS, som lägger koden på $9000,
; startar den med CALL(36864) och skriver poängen som koden lämnar i HL.
; Minnet är som för chip8.asm: koden på $9000, variablerna på $9F00 och
; ormen på $A000, ledigt mellan BASIC-variablerna och laddaren.
;
; Spelplanen är skärmens 24 rader i grafikläget, punktrad 0-71 och
; punktkolumn 2-79 (kolumn 0 och 1 är tecknet $97 som slår på grafiken),
; med en ram runt. Ormen börjar med fem punkter och växer med en för varje
; bit mat. Den som kör in i ramen eller i sig själv har förlorat.
; Tangenterna: A vänster, S höger, W upp, Z ned, BREAK (Ctrl-C) avbryter.
; De tas ur KEYFLG och LASTKY, som BASIC II:s avbrott fyller, och läggs i
; en kö, så att två snabba tryck mellan två steg båda gäller.

SCR     EQU $7C00               ; bildminnet, 24 rader x 40
KEYFLG  EQU $FFE2               ; bit 7: ny tangent (KEYINT)
LASTKY  EQU $FFE3               ; den senaste tangenten
BRKPTR  EQU $FF85               ; pekare till BREAK-biten
TICKS   EQU $FFF5               ; räknas upp av NMI, 50 Hz
BREAK   EQU 3                   ; Ctrl-C

FART    EQU 3                   ; bilder (1/50 s) per steg
LANGD   EQU 5                   ; ormen från början
MAXLEN  EQU 500                 ; som Smax i SNAKE3.BAS
RADMAX  EQU 71                  ; ramens nedre rad
KOLMIN  EQU 2                   ; ramens vänstra kolumn
KOLMAX  EQU 79                  ; ramens högra kolumn

ORM     EQU $A000               ; ormen, 1024 punkter (rad, kolumn)
VARS    EQU $9F00
HUVUD   EQU VARS                ; huvudets plats i ORM, 0-1023
SVANS   EQU VARS+2              ; svansens plats
LANG    EQU VARS+4              ; ormens längd
POANG   EQU VARS+6              ; poängen
RIKT    EQU VARS+8              ; 1 vänster, 2 ned, 3 höger, 4 upp
MATR    EQU VARS+9              ; matens rad
MATK    EQU VARS+10             ; matens kolumn
SENAST  EQU VARS+11             ; TICKS när den lästes senast
VANTA   EQU VARS+12             ; bilder kvar till nästa steg
KON     EQU VARS+13             ; antal tangenter i kön, 0-4
KOPLATS EQU VARS+14             ; kön, 4 riktningar
SPSPAR  EQU VARS+18             ; SP från BASIC
SLUMP   EQU VARS+20             ; slumptalets tillstånd, aldrig 0
YTAB    EQU VARS+$20            ; 72 x 3 byte: radens adress, 2 * raden i tecknet

        ORG $9000

START:  LD HL,VARS              ; alla variabler 0
        LD DE,VARS+1
        LD BC,$1F
        LD (HL),0
        LDIR
        LD (SPSPAR),SP
        LD A,R                  ; slumpen börjar på något som varierar
        LD L,A
        LD A,(TICKS)
        LD H,A
        SET 0,L
        LD (SLUMP),HL
        XOR A
        LD (KEYFLG),A
        CALL YBYGG
        CALL SKARM
        CALL RAM
        CALL ORMEN
        CALL MAT
        LD A,(TICKS)
        LD (SENAST),A
        LD A,FART
        LD (VANTA),A
SLINGA: CALL TID
        JR NZ,SLINGA
        CALL STEG
        JR NC,SLINGA
        LD HL,OVER              ; förlorat
        LD DE,SCR+2*128+40+10   ; rad 10, kolumn 10
        LD BC,OVERSLUT-OVER
        LDIR
        LD B,100                ; två sekunder
FORL1:  CALL VANTBILD
        DJNZ FORL1
; Tillbaka till BASIC med poängen i HL, med vilket djup som helst på
; stacken.
SLUT:   LD SP,(SPSPAR)
        XOR A
        LD (KEYFLG),A
        LD HL,(BRKPTR)          ; Ctrl-C har satt BREAK-biten, som BASIC
        LD A,H                  ; annars ser efter nästa sats
        OR L
        JR Z,SLUT1
        RES 0,(HL)
SLUT1:  LD HL,(POANG)
        RET

OVER:   DEFB $87,"G A M E   O V E R",$97
OVERSLUT:

; ---------------------------------------------------------------- tiden

; TID ser efter nya tangenter och räknar bilderna. Z när det är dags för
; nästa steg.
TID:    LD A,(KEYFLG)
        RLA
        CALL C,TANGENT
        LD A,(TICKS)
        LD HL,SENAST
        CP (HL)
        JR Z,TID1               ; samma bild
        LD (HL),A
        LD HL,VANTA
        DEC (HL)
        RET NZ
        LD (HL),FART            ; Z från DEC
        RET
TID1:   OR 1                    ; NZ
        RET

; VANTBILD väntar en bild. B orörd.
VANTBILD: LD A,(TICKS)
VANTB1: LD HL,TICKS
        CP (HL)
        JR Z,VANTB1
        RET

; TANGENT tar tangenten ur LASTKY. BREAK går tillbaka till BASIC; A, S, W
; och Z läggs i kön som riktningen 1, 3, 4 och 2.
TANGENT: XOR A
        LD (KEYFLG),A
        LD A,(LASTKY)
        AND $7F
        CP BREAK
        JR Z,SLUT
        OR $20                  ; gemen
        LD C,1
        CP 'a'
        JR Z,TANG1
        LD C,3
        CP 's'
        JR Z,TANG1
        LD C,4
        CP 'w'
        JR Z,TANG1
        LD C,2
        CP 'z'
        RET NZ
TANG1:  LD HL,KON
        LD A,(HL)
        CP 4
        RET NC                  ; kön är full
        INC (HL)
        INC HL                  ; KOPLATS
        ADD A,L
        LD L,A
        JR NC,TANG2
        INC H
TANG2:  LD (HL),C
        RET

; ---------------------------------------------------------------- spelet

; STEG flyttar ormen ett steg. Carry om den körde in i något.
STEG:   LD HL,KON               ; en riktning ur kön
        LD A,(HL)
        OR A
        JR Z,STEG2
        DEC (HL)
        LD HL,KOPLATS
        LD A,(HL)
        PUSH AF
        LD DE,KOPLATS           ; flytta fram resten
        INC HL
        LD BC,3
        LDIR
        POP AF
        LD C,A                  ; C = den nya riktningen
        LD A,(RIKT)
        SUB C
        JR NC,STEG1
        NEG
STEG1:  CP 2                    ; rakt bakåt går inte
        JR Z,STEG2
        LD A,C
        LD (RIKT),A
STEG2:  LD HL,(HUVUD)           ; B, C = huvudet
        CALL PLATS
        LD C,(HL)
        INC HL
        LD B,(HL)
        LD A,(RIKT)
        DEC A
        JR NZ,STEG3
        DEC B                   ; vänster
        JR STEG6
STEG3:  DEC A
        JR NZ,STEG4
        INC C                   ; ned
        JR STEG6
STEG4:  DEC A
        JR NZ,STEG5
        INC B                   ; höger
        JR STEG6
STEG5:  DEC C                   ; upp
STEG6:  LD A,(MATR)
        CP C
        JR NZ,STEG7
        LD A,(MATK)
        CP B
        JR Z,STEG9              ; maten
STEG7:  CALL PROVA
        SCF
        RET NZ                  ; ramen eller ormen
STEG7B: PUSH BC                 ; svansen bort
        LD HL,(SVANS)
        PUSH HL
        CALL PLATS
        LD C,(HL)
        INC HL
        LD B,(HL)
        CALL SLACK
        POP HL
        INC HL
        LD A,H
        AND 3
        LD H,A
        LD (SVANS),HL
        POP BC
STEG8:  LD HL,(HUVUD)           ; huvudet fram
        INC HL
        LD A,H
        AND 3
        LD H,A
        LD (HUVUD),HL
        CALL PLATS
        LD (HL),C
        INC HL
        LD (HL),B
        CALL SATT
        OR A                    ; NC
        RET
STEG9:  LD HL,(POANG)           ; äter: ormen växer, svansen står kvar
        INC HL
        LD (POANG),HL
        LD HL,(LANG)
        LD DE,MAXLEN
        OR A
        SBC HL,DE
        JR C,STEG10
        PUSH BC                 ; full längd: ny mat, och svansen flyttas
        CALL MAT
        POP BC
        JR STEG7B
STEG10: ADD HL,DE
        INC HL
        LD (LANG),HL
        PUSH BC
        CALL MAT
        POP BC
        JR STEG8

; PLATS: HL = adressen till punkt HL i ORM.
PLATS:  ADD HL,HL
        LD DE,ORM
        ADD HL,DE
        RET

; ORMEN lägger de fem första punkterna i kolumn 39, rad 1-5, med huvudet
; på rad 5 på väg åt vänster, som i SNAKE3.BAS.
ORMEN:  LD HL,ORM
        LD BC,(KOLMAX/2)*256+1  ; B = kolumnen, C = raden
        LD A,LANGD
ORMEN1: LD (HL),C
        INC HL
        LD (HL),B
        INC HL
        CALL SATT
        INC C
        DEC A
        JR NZ,ORMEN1
        LD HL,LANGD-1
        LD (HUVUD),HL
        LD HL,LANGD
        LD (LANG),HL
        LD A,1
        LD (RIKT),A
        RET

; MAT lägger maten på en ledig punkt, rad 5-65 och kolumn 10-68.
MAT:    LD E,61
        CALL SLUMPT
        ADD A,5
        LD C,A
        LD E,59
        CALL SLUMPT
        ADD A,10
        LD B,A
        CALL PROVA
        JR NZ,MAT
MAT1:   LD A,C
        LD (MATR),A
        LD A,B
        LD (MATK),A
        JP SATT

; SLUMPT: A = slumptal 0 till E - 1. Xorshift med 16 bitar, och sedan
; (slumptal * E) / 256.
SLUMPT: LD HL,(SLUMP)
        LD A,H
        RRA
        LD A,L
        RRA
        XOR H
        LD H,A
        LD A,L
        RRA
        LD A,H
        RRA
        XOR L
        LD L,A
        XOR H
        LD H,A
        LD (SLUMP),HL
        PUSH BC
        LD HL,0
        LD D,H
        LD B,8
SLUMPT1: ADD HL,HL
        RLA
        JR NC,SLUMPT2
        ADD HL,DE
SLUMPT2: DJNZ SLUMPT1
        LD A,H
        POP BC
        RET

; ---------------------------------------------------------------- skärmen

; PADR: DE = tecknet med punkten C (rad 0-71), B (kolumn 0-79) och
; A = punktens bit. Punktraden C är tecknets rad C/3 och raden C MOD 3 i
; tecknet, ur YTAB; punktkolumnen B är kolumn B/2 och halvan B AND 1.
PADR:   PUSH HL
        LD L,C
        LD H,0
        LD E,L
        LD D,H
        ADD HL,HL
        ADD HL,DE               ; 3 * C
        LD DE,YTAB
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)               ; DE = radens början
        INC HL
        LD A,B
        AND 1
        ADD A,(HL)              ; 2 * raden i tecknet + halvan
        LD HL,MASKER
        ADD A,L
        LD L,A
        JR NC,PADR1
        INC H
PADR1:  LD A,B
        SRL A
        ADD A,E
        LD E,A
        JR NC,PADR2
        INC D
PADR2:  LD A,(HL)
        POP HL
        RET

; SATT tänder punkten C, B; SLACK släcker den; PROVA ger NZ om den är
; tänd. BC och HL orörda.
SATT:   PUSH AF
        PUSH DE
        CALL PADR
        EX DE,HL
        OR (HL)
        LD (HL),A
        EX DE,HL
        POP DE
        POP AF
        RET

SLACK:  PUSH DE
        CALL PADR
        EX DE,HL
        CPL
        AND (HL)
        LD (HL),A
        EX DE,HL
        POP DE
        RET

PROVA:  PUSH DE
        CALL PADR
        EX DE,HL
        AND (HL)
        EX DE,HL
        POP DE
        RET

; Bitarna i ett mosaiktecken: vänster och höger på rad 0, 1 och 2.
MASKER: DEFB 1,2,4,8,16,64

; Radernas adresser i bildminnet: rad R är (R AND 7) * 128 + (R / 8) * 40.
RADER:  DEFW SCR,SCR+128,SCR+2*128,SCR+3*128
        DEFW SCR+4*128,SCR+5*128,SCR+6*128,SCR+7*128
        DEFW SCR+40,SCR+40+128,SCR+40+2*128,SCR+40+3*128
        DEFW SCR+40+4*128,SCR+40+5*128,SCR+40+6*128,SCR+40+7*128
        DEFW SCR+80,SCR+80+128,SCR+80+2*128,SCR+80+3*128
        DEFW SCR+80+4*128,SCR+80+5*128,SCR+80+6*128,SCR+80+7*128

; YBYGG gör YTAB: för varje punktrad radens adress och 2 * raden i tecknet.
YBYGG:  LD HL,RADER
        LD DE,YTAB
        LD B,24
YBYGG1: LD C,0
YBYGG2: LD A,(HL)
        LD (DE),A
        INC DE
        INC HL
        LD A,(HL)
        LD (DE),A
        INC DE
        DEC HL
        LD A,C
        LD (DE),A
        INC DE
        ADD A,2
        LD C,A
        CP 6
        JR C,YBYGG2
        INC HL
        INC HL
        DJNZ YBYGG1
        RET

; SKARM tömmer skärmen och gör alla 24 rader till grafikrader.
SKARM:  LD HL,SCR
        LD DE,SCR+1
        LD BC,1023
        LD (HL),' '
        LDIR
        LD HL,RADER
        LD B,24
SKARM1: LD E,(HL)
        INC HL
        LD D,(HL)
        INC HL
        LD A,$97                ; grafik från kolumn 1
        LD (DE),A
        DJNZ SKARM1
        RET

; RAM ritar ramen: rad 0 och RADMAX, kolumn KOLMIN och KOLMAX.
RAM:    LD B,KOLMIN
RAM1:   LD C,0
        CALL SATT
        LD C,RADMAX
        CALL SATT
        INC B
        LD A,B
        CP KOLMAX+1
        JR C,RAM1
        LD C,0
RAM2:   LD B,KOLMIN
        CALL SATT
        LD B,KOLMAX
        CALL SATT
        INC C
        LD A,C
        CP RADMAX+1
        JR C,RAM2
        RET

        END
