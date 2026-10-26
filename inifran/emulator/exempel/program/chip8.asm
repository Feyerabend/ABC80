; chip8.asm -- CHIP-8 i Z80-kod för BASIC II på ABC80, ett exempel till
; ABC80 inifrån
;
; Samma tolk som text_b2/CHIP8.BAS, men i maskinkod, så att spelet går i
; den fart CHIP-8 är tänkt för. gor.sh assemblerar den och gör laddaren
; text_b2/CHIP8Z.BAS, som lägger koden på $9000 och startar den med
; CALL(36864). BASIC II lägger programmet högst upp, under MEMTOP ($F268
; med DOS:en), och variablerna nedifrån $8000; laddaren har få variabler,
; så $9000-$AFFF är ledigt så länge den kör. Koden går tillbaka till BASIC
; med RET när BREAK (Ctrl-C) trycks.
;
; Minnet:
;   $9000-       koden, typsnittet och spelet
;   $9F00-$9FFF  tolkens variabler (VARS)
;   $A000-$AFFF  CHIP-8:s 4 KB: typsnittet på 0, spelet på $200
;
; Skärmen: CHIP-8:s 64 x 32 punkter är 32 x 11 mosaiktecken, på textrad
; 1-11 och kolumn 4-35, som i BASIC-versionen. Punkterna ändras direkt i
; bildminnet; det som står där är också det som DXYN prövar för VF.
;
; Tangenterna: BASIC II:s avbrott för tangentbordet (KEYINT) lägger varje
; tangent i LASTKY och sätter bit 7 i KEYFLG, så inget tryck går förlorat.
; En tangent räknas som nedtryckt i HALLTID bilder efter trycket och så
; länge bit 7 i port 56 är satt (tangenten är nere). Upprepningen håller
; den nere i emulatorn, där bit 7 bara är satt en kort stund per tryck.
;
; Tiden: NMI räknar upp $FFF5 50 gånger i sekunden (den slår om vid 50,
; klockan i BASIC II). Varje gång den har ändrats räknas timrarna ned och
; tolken får PERBILD nya instruktioner.

SCR     EQU $7C00               ; bildminnet, 24 rader x 40
KEYFLG  EQU $FFE2               ; bit 7: ny tangent (KEYINT)
LASTKY  EQU $FFE3               ; den senaste tangenten
BRKPTR  EQU $FF85               ; pekare till BREAK-biten
TICKS   EQU $FFF5               ; räknas upp av NMI, 50 Hz
PORT_KB EQU $38                 ; tangentbordet, bit 7 = nere
PORT_SND EQU 6                  ; ljudgeneratorn SN76477
LJUD_PA EQU 9                   ; tonen för ljudtimern, som i CHIP8.BAS
LJUD_AV EQU 0
BREAK   EQU 3                   ; Ctrl-C

MEM     EQU $A000               ; CHIP-8:s minne, 4 KB
PERBILD EQU 12                  ; instruktioner per 1/50 s, 600 i sekunden
HALLTID EQU 6                   ; bilder som en tangent räknas som nere

VARS    EQU $9F00
VREG    EQU VARS                ; V0-VF, 16 byte (sidjusterat)
STACK   EQU VARS+$20            ; 16 returadresser
SPKOD   EQU VARS+$40            ; CHIP-8:s stackdjup 0-16
PC8     EQU VARS+$42            ; CHIP-8:s PC
IREG    EQU VARS+$44            ; I
DT      EQU VARS+$46            ; fördröjningstimern
ST      EQU VARS+$47            ; ljudtimern
SENAST  EQU VARS+$48            ; TICKS när den lästes senast
KVAR    EQU VARS+$49            ; instruktioner kvar i den här bilden
NERE    EQU VARS+$4A            ; tangenten som är nere, $FF = ingen
HALLT   EQU VARS+$4B            ; bilder kvar som den räknas som nere
NYTT    EQU VARS+$4C            ; ny tangent till FX0A, $FF = ingen
XPOS    EQU VARS+$4D            ; DXYN: VX AND 63
YPOS    EQU VARS+$4E            ; DXYN: VY AND 31
SPSPAR  EQU VARS+$50            ; SP från BASIC
SLUMP   EQU VARS+$52            ; slumptalets tillstånd, aldrig 0
YTAB    EQU VARS+$60            ; 33 x 3 byte: radens adress, 2 * raden i tecknet

        ORG $9000

START:  LD HL,VARS              ; alla variabler 0
        LD DE,VARS+1
        LD BC,$5F
        LD (HL),0
        LDIR
        LD (SPSPAR),SP
        CALL SKARM
        CALL MINNE
        LD HL,$200
        LD (PC8),HL
        LD A,$FF
        LD (NERE),A
        LD (NYTT),A
        LD A,R                  ; slumpen börjar på något som varierar
        LD L,A
        LD A,(TICKS)
        LD H,A
        SET 0,L
        LD (SLUMP),HL
        CALL YBYGG
        XOR A
        OUT (PORT_SND),A
SLINGA: CALL TID
        LD HL,KVAR
        LD A,(HL)
        OR A
        JR Z,SLINGA             ; bilden är slut: vänta på nästa
        DEC (HL)
        CALL STEG
        JR SLINGA

; Tillbaka till BASIC, med vilket djup som helst på stacken.
SLUT:   XOR A
        OUT (PORT_SND),A
        LD (KEYFLG),A
        LD SP,(SPSPAR)
        LD HL,(BRKPTR)          ; Ctrl-C har satt BREAK-biten, som BASIC
        LD A,H                  ; annars ser efter nästa sats
        OR L
        JR Z,SLUT1
        RES 0,(HL)
SLUT1:  LD HL,0
        RET

; ---------------------------------------------------------------- tiden

; TID ser efter nya tangenter och om bilden har bytts.
TID:    LD A,(KEYFLG)
        RLA
        CALL C,TANGENT
        LD A,(TICKS)
        LD HL,SENAST
        CP (HL)
        RET Z
        LD (HL),A
        LD A,PERBILD
        LD (KVAR),A
        LD HL,DT
        LD A,(HL)
        OR A
        JR Z,TID1
        DEC (HL)
TID1:   INC HL                  ; ST
        LD A,(HL)
        OR A
        JR Z,TID2
        DEC (HL)
        JR NZ,TID2
        XOR A                   ; tonen av
        OUT (PORT_SND),A
TID2:   IN A,(PORT_KB)
        RLA
        JR NC,TID3
        LD A,HALLTID            ; nere: räknas som nere ett tag till
        LD (HALLT),A
        RET
TID3:   LD HL,HALLT
        LD A,(HL)
        OR A
        RET Z
        DEC (HL)
        RET NZ
        LD A,$FF                ; släppt
        LD (NERE),A
        RET

; TANGENT tar tangenten ur LASTKY. BREAK går tillbaka till BASIC, och
; tangenterna på knappsatsen blir NERE och NYTT.
;     1 2 3 C        1 2 3 4
;     4 5 6 D   <--  Q W E R
;     7 8 9 E        A S D F
;     A 0 B F        Z X C V
TANGENT: XOR A
        LD (KEYFLG),A
        LD A,(LASTKY)
        AND $7F
        CP BREAK
        JP Z,SLUT
        CP 'a'
        JR C,TANG1
        CP 'z'+1
        JR NC,TANG1
        SUB 32                  ; versal
TANG1:  LD HL,KNAPPAR
        LD BC,16
        CPIR
        RET NZ                  ; inte på knappsatsen
        LD A,15                 ; BC = 15 - platsen
        SUB C
        LD (NERE),A
        LD (NYTT),A
        LD A,HALLTID
        LD (HALLT),A
        RET

KNAPPAR: DEFB "X123QWEASDZC4RFV"  ; tangenten för CHIP-8:s 0-F

; ---------------------------------------------------------------- tolken

; STEG hämtar en instruktion, B = första byten och C = andra, och hoppar
; till rutinen för den höga halvbyten i B.
STEG:   LD HL,(PC8)
        CALL ADR
        LD B,(HL)
        INC HL
        LD C,(HL)
        LD HL,(PC8)
        INC HL
        INC HL
        LD A,H
        AND $0F
        LD H,A
        LD (PC8),HL
        LD A,B
        RRCA
        RRCA
        RRCA
        AND $1E                 ; 2 * höga halvbyten
        LD E,A
        LD D,0
        LD HL,OPTAB
        ADD HL,DE
        LD A,(HL)
        INC HL
        LD H,(HL)
        LD L,A
        JP (HL)

OPTAB:  DEFW OP0,OP1,OP2,OP3,OP4,OP5,OP6,OP7
        DEFW OP8,OP9,OPA,OPB,OPC,OPD,OPE,OPF

; ADR: HL = MEM + (HL AND $FFF).
ADR:    LD A,H
        AND $0F
        OR MEM/256
        LD H,A
        RET

; VX: HL = adressen till VX (X = B AND 15).
VX:     LD A,B
        AND $0F
        LD L,A
        LD H,VREG/256
        RET

; VY: DE = adressen till VY (Y = C / 16).
VY:     LD A,C
        RRCA
        RRCA
        RRCA
        RRCA
        AND $0F
        LD E,A
        LD D,VREG/256
        RET

; HOPPA: hoppa över nästa instruktion.
HOPPA:  LD HL,(PC8)
        INC HL
        INC HL
        LD A,H
        AND $0F
        LD H,A
        LD (PC8),HL
        RET

; NNN: HL = B AND 15, C.
NNN:    LD A,B
        AND $0F
        LD H,A
        LD L,C
        RET

; 00E0 töm skärmen, 00EE återvänd; 0NNN gör ingenting.
OP0:    LD A,B
        OR A
        RET NZ
        LD A,C
        CP $E0
        JP Z,TOM
        CP $EE
        RET NZ
        LD A,(SPKOD)
        OR A
        RET Z                   ; inget att återvända till
        DEC A
        LD (SPKOD),A
        ADD A,A
        ADD A,STACK-VARS
        LD L,A
        LD H,VARS/256
        LD A,(HL)
        INC HL
        LD H,(HL)
        LD L,A
        LD (PC8),HL
        RET

; 1NNN hoppa.
OP1:    CALL NNN
        LD (PC8),HL
        RET

; 2NNN anropa.
OP2:    LD A,(SPKOD)
        CP 16
        JR NC,OP1               ; stacken full: bara hoppet
        INC A
        LD (SPKOD),A
        DEC A
        ADD A,A
        ADD A,STACK-VARS
        LD L,A
        LD H,VARS/256
        LD DE,(PC8)
        LD (HL),E
        INC HL
        LD (HL),D
        JR OP1

; 3XNN hoppa över om VX = NN.
OP3:    CALL VX
        LD A,(HL)
        CP C
        RET NZ
        JR HOPPA

; 4XNN hoppa över om VX <> NN.
OP4:    CALL VX
        LD A,(HL)
        CP C
        RET Z
        JR HOPPA

; 5XY0 hoppa över om VX = VY.
OP5:    CALL VX
        CALL VY
        LD A,(DE)
        CP (HL)
        RET NZ
        JR HOPPA

; 6XNN VX = NN.
OP6:    CALL VX
        LD (HL),C
        RET

; 7XNN VX = VX + NN, VF orörd.
OP7:    CALL VX
        LD A,(HL)
        ADD A,C
        LD (HL),A
        RET

; 8XYN räkning med VX och VY. VF sätts sist, så att den gäller också när
; X är F. 8XY6 och 8XYE flyttar VY, som på COSMAC VIP och i Octo.
OP8:    CALL VX
        CALL VY
        LD A,C
        AND $0F
        JR Z,OP80
        DEC A
        JR Z,OP81
        DEC A
        JR Z,OP82
        DEC A
        JR Z,OP83
        DEC A
        JR Z,OP84
        DEC A
        JR Z,OP85
        DEC A
        JR Z,OP86
        DEC A
        JR Z,OP87
        CP $0E-7
        RET NZ
        LD A,(DE)               ; 8XYE: VX = VY * 2, VF = biten ut
        ADD A,A
        JR VFUT
OP80:   LD A,(DE)
        LD (HL),A
        RET
OP81:   LD A,(DE)
        OR (HL)
        LD (HL),A
        RET
OP82:   LD A,(DE)
        AND (HL)
        LD (HL),A
        RET
OP83:   LD A,(DE)
        XOR (HL)
        LD (HL),A
        RET
OP84:   LD A,(DE)               ; VF = minnessiffran
        ADD A,(HL)
        JR VFUT
OP85:   LD A,(HL)               ; VF = 1 om inget lån
        EX DE,HL
        SUB (HL)
        EX DE,HL
        CCF
        JR VFUT
OP86:   LD A,(DE)               ; VX = VY / 2, VF = biten ut
        SRL A
        JR VFUT
OP87:   LD A,(DE)               ; VX = VY - VX
        SUB (HL)
        CCF
; VFUT: VX = A och VF = carry.
VFUT:   LD (HL),A
        LD A,0
        RLA
        LD (VREG+15),A
        RET

; 9XY0 hoppa över om VX <> VY.
OP9:    CALL VX
        CALL VY
        LD A,(DE)
        CP (HL)
        RET Z
        JP HOPPA

; ANNN I = NNN.
OPA:    CALL NNN
        LD (IREG),HL
        RET

; BNNN hoppa till NNN + V0.
OPB:    CALL NNN
        LD A,(VREG)
        ADD A,L
        LD L,A
        JR NC,OPB1
        INC H
OPB1:   LD A,H
        AND $0F
        LD H,A
        LD (PC8),HL
        RET

; CXNN VX = slumptal AND NN. Xorshift med 16 bitar.
OPC:    LD HL,(SLUMP)
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
        AND C
        PUSH AF
        CALL VX
        POP AF
        LD (HL),A
        RET

; EX9E hoppa över om tangenten VX är nere, EXA1 om den inte är det.
OPE:    CALL VX
        LD A,(NERE)
        CP (HL)
        LD A,C
        JR Z,OPE1
        CP $A1                  ; inte nere
        RET NZ
        JP HOPPA
OPE1:   CP $9E                  ; nere
        RET NZ
        JP HOPPA

; FXNN timrarna, tangenten, I och minnet.
OPF:    CALL VX
        LD A,C
        CP $07
        JR Z,OPF07
        CP $0A
        JR Z,OPF0A
        CP $15
        JR Z,OPF15
        CP $18
        JR Z,OPF18
        CP $1E
        JR Z,OPF1E
        CP $29
        JR Z,OPF29
        CP $33
        JR Z,OPF33
        CP $55
        JR Z,OPF55
        CP $65
        JP Z,OPF65
        RET
OPF07:  LD A,(DT)               ; VX = DT
        LD (HL),A
        RET
OPF0A:  LD A,(NYTT)             ; vänta på en tangent: gör om
        CP $FF                  ; instruktionen tills den kommer
        JR Z,OPF0A1
        LD (HL),A
        LD A,$FF
        LD (NYTT),A
        RET
OPF0A1: LD HL,(PC8)
        DEC HL
        DEC HL
        LD A,H
        AND $0F
        LD H,A
        LD (PC8),HL
        RET
OPF15:  LD A,(HL)               ; DT = VX
        LD (DT),A
        RET
OPF18:  LD A,(HL)               ; ST = VX, tonen på
        LD (ST),A
        OR A
        RET Z
        LD A,LJUD_PA
        OUT (PORT_SND),A
        RET
OPF1E:  LD E,(HL)               ; I = I + VX
        LD D,0
        LD HL,(IREG)
        ADD HL,DE
        LD A,H
        AND $0F
        LD H,A
        LD (IREG),HL
        RET
OPF29:  LD A,(HL)               ; I = siffran VX i typsnittet
        AND $0F
        LD L,A
        ADD A,A
        ADD A,A
        ADD A,L
        LD L,A
        LD H,0
        LD (IREG),HL
        RET
OPF33:  LD A,(HL)               ; VX som tre siffror på I
        LD HL,(IREG)
        CALL ADR
        LD B,0
OPF331: CP 100
        JR C,OPF332
        SUB 100
        INC B
        JR OPF331
OPF332: LD (HL),B
        CALL NASTA
        LD B,0
OPF333: CP 10
        JR C,OPF334
        SUB 10
        INC B
        JR OPF333
OPF334: LD (HL),B
        CALL NASTA
        LD (HL),A
        RET
OPF55:  LD A,B                  ; V0-VX till I, I = I + X + 1
        AND $0F
        INC A
        LD B,A
        LD DE,VREG
        LD HL,(IREG)
OPF551: PUSH HL
        CALL ADR
        LD A,(DE)
        LD (HL),A
        POP HL
        INC HL
        INC DE
        DJNZ OPF551
        JR OPF652
OPF65:  LD A,B                  ; V0-VX från I, I = I + X + 1
        AND $0F
        INC A
        LD B,A
        LD DE,VREG
        LD HL,(IREG)
OPF651: PUSH HL
        CALL ADR
        LD A,(HL)
        LD (DE),A
        POP HL
        INC HL
        INC DE
        DJNZ OPF651
OPF652: LD A,H
        AND $0F
        LD H,A
        LD (IREG),HL
        RET

; NASTA: HL = nästa byte i CHIP-8:s minne, runt vid 4 KB. A orörd.
NASTA:  INC HL
        PUSH AF
        CALL ADR
        POP AF
        RET

; ---------------------------------------------------------------- skärmen

; DXYN rita N rader ur I på VX, VY med XOR. VF = 1 om någon punkt släcks.
; Utanför kanten klipps figuren; bara början tas runt.
OPD:    CALL VX
        LD A,(HL)
        AND 63
        LD (XPOS),A
        CALL VY
        LD A,(DE)
        AND 31
        LD (YPOS),A
        XOR A
        LD (VREG+15),A
        LD A,C
        AND $0F
        RET Z
        LD B,A                  ; B = rader, högst ned till rad 31
        LD A,(YPOS)
        ADD A,B
        SUB 32
        JR C,OPD1
        JR Z,OPD1
        LD C,A                  ; för många: dra bort dem
        LD A,B
        SUB C
        LD B,A
OPD1:   LD HL,(IREG)
        LD A,(YPOS)
        LD C,A
OPD2:   PUSH HL
        PUSH BC
        CALL ADR
        LD D,(HL)               ; D = radens punkter
        LD A,(XPOS)
        LD B,A
OPD3:   LD A,D
        OR A
        JR Z,OPD5               ; inga fler punkter på raden
        SLA D
        JR NC,OPD4
        CALL PUNKT
OPD4:   INC B
        LD A,B
        CP 64
        JR C,OPD3               ; till högerkanten
OPD5:   POP BC
        POP HL
        INC HL
        INC C
        DJNZ OPD2
        RET

; PUNKT vänder punkten B (0-63), C (0-31) och sätter VF om den var tänd.
; Punktraden C är tecknets rad C/3 + 1 och raden C MOD 3 i tecknet,
; ur YTAB; punktkolumnen B är kolumn B/2 + 4 och halvan B AND 1.
PUNKT:  PUSH BC
        PUSH DE
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
        LD D,(HL)               ; DE = radens kolumn 4
        INC HL
        LD A,B
        AND 1
        ADD A,(HL)              ; 2 * raden i tecknet + halvan
        LD HL,MASKER
        ADD A,L
        LD L,A
        JR NC,PUNKT1
        INC H
PUNKT1: LD C,(HL)               ; C = punktens bit
        LD A,B
        SRL A
        ADD A,E
        LD E,A
        JR NC,PUNKT2
        INC D
PUNKT2: LD A,(DE)
        LD B,A
        AND C
        JR Z,PUNKT3
        LD A,1
        LD (VREG+15),A
PUNKT3: LD A,B
        XOR C
        LD (DE),A
        POP DE
        POP BC
        RET

; Bitarna i ett mosaiktecken: vänster och höger på rad 0, 1 och 2.
MASKER: DEFB 1,2,4,8,16,64

; Radernas adresser i bildminnet (rad R: (R AND 7) * 128 + (R / 8) * 40),
; kolumn 4, för raderna 1-11.
RADER:  DEFW SCR+1*128+4,SCR+2*128+4,SCR+3*128+4,SCR+4*128+4
        DEFW SCR+5*128+4,SCR+6*128+4,SCR+7*128+4,SCR+40+4
        DEFW SCR+40+128+4,SCR+40+2*128+4,SCR+40+3*128+4

; YBYGG gör YTAB: för varje punktrad radens adress och 2 * raden i tecknet.
YBYGG:  LD HL,RADER
        LD DE,YTAB
        LD B,11
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

; 00E0: blanka mosaiktecken på raderna 1-11, kolumn 4-35.
TOM:    LD HL,RADER
        LD B,11
TOM1:   LD E,(HL)
        INC HL
        LD D,(HL)
        INC HL
        PUSH BC
        LD B,32
        LD A,' '
TOM2:   LD (DE),A
        INC DE
        DJNZ TOM2
        POP BC
        DJNZ TOM1
        RET

; SKARM tömmer skärmen, gör raderna 1-11 till grafikrader och skriver
; texterna.
SKARM:  LD HL,SCR
        LD DE,SCR+1
        LD BC,1023
        LD (HL),' '
        LDIR
        LD HL,RADER
        LD B,11
SKARM1: LD E,(HL)
        INC HL
        LD D,(HL)
        INC HL
        DEC DE
        DEC DE
        DEC DE
        DEC DE
        LD A,$97                ; grafik från kolumn 1
        LD (DE),A
        DJNZ SKARM1
        LD HL,TEXTER
SKARM2: LD E,(HL)
        INC HL
        LD D,(HL)
        INC HL
        LD A,D
        OR E
        RET Z
SKARM3: LD A,(HL)
        INC HL
        OR A
        JR Z,SKARM2
        LD (DE),A
        INC DE
        JR SKARM3

TEXTER: DEFW SCR+4
        DEFB "CHIP-8 ON ABC80",0
        DEFW SCR+5*128+40
        DEFB "KEYPAD  1234 QWER ASDF ZXCV",0
        DEFW SCR+6*128+40
        DEFB "GAME    MINI LIGHTS OUT (TOBIASVL, CC0)",0
        DEFW SCR+7*128+40
        DEFB "STOP    CTRL-C",0
        DEFW 0

; MINNE nollar CHIP-8:s minne och lägger typsnittet på 0 och spelet på $200.
MINNE:  LD HL,MEM
        LD DE,MEM+1
        LD BC,$FFF
        LD (HL),0
        LDIR
        LD HL,TYPSN
        LD DE,MEM
        LD BC,SPEL-TYPSN
        LDIR
        LD HL,SPEL
        LD DE,MEM+$200
        LD BC,SPELSLUT-SPEL
        LDIR
        RET

; De 16 siffrorna, 5 byte var.
TYPSN:  DEFB $F0,$90,$90,$90,$F0, $20,$60,$20,$20,$70
        DEFB $F0,$10,$F0,$80,$F0, $F0,$10,$F0,$10,$F0
        DEFB $90,$90,$F0,$10,$10, $F0,$80,$F0,$10,$F0
        DEFB $F0,$80,$F0,$90,$F0, $F0,$10,$20,$40,$40
        DEFB $F0,$90,$F0,$90,$F0, $F0,$90,$F0,$10,$F0
        DEFB $F0,$90,$F0,$90,$90, $E0,$90,$E0,$90,$E0
        DEFB $F0,$80,$80,$80,$F0, $E0,$90,$90,$90,$E0
        DEFB $F0,$80,$F0,$80,$F0, $F0,$80,$F0,$80,$80

; Mini Lights Out av tobiasvl, ur John Earnests CHIP-8-arkiv (CC0). De två
; summerna på titelbilden, $0204 och $023E, är gjorda till 8000 (V0 = V0)
; som spelets README råder till med en riktig summer, som i CHIP8.BAS.
SPEL:   DEFB $00,$E0,$60,$32,$80,$00,$A3,$C8,$6A,$11,$6B,$08
        DEFB $6C,$07,$DA,$B7,$FC,$1E,$7A,$08,$DA,$B7,$FC,$1E
        DEFB $7A,$08,$DA,$B7,$FC,$1E,$7A,$08,$DA,$B7,$6A,$11
        DEFB $7B,$08,$FC,$1E,$7A,$08,$DA,$B7,$FC,$1E,$7A,$08
        DEFB $DA,$B7,$FC,$1E,$7A,$08,$DA,$B1,$60,$32,$F0,$15
        DEFB $4F,$01,$80,$00,$A3,$B8,$FE,$65,$E0,$A1,$12,$AE
        DEFB $E1,$A1,$12,$AE,$E2,$A1,$12,$AE,$E3,$A1,$12,$AE
        DEFB $E4,$A1,$12,$AE,$E5,$A1,$12,$AE,$E6,$A1,$12,$AE
        DEFB $E7,$A1,$12,$AE,$E8,$A1,$12,$AE,$E9,$A1,$12,$AE
        DEFB $EA,$A1,$12,$AE,$EB,$A1,$12,$AE,$EC,$A1,$12,$AE
        DEFB $ED,$A1,$12,$AE,$EE,$A1,$12,$AE,$6F,$0F,$EF,$A1
        DEFB $12,$AE,$FF,$07,$3F,$00,$12,$44,$A4,$24,$6A,$F8
        DEFB $6B,$F1,$7B,$0F,$4B,$2D,$12,$AC,$7A,$08,$4A,$40
        DEFB $12,$A8,$4B,$1E,$DA,$B2,$3B,$1E,$DA,$BF,$12,$98
        DEFB $6A,$F8,$12,$92,$12,$38,$6D,$00,$FD,$18,$A4,$1D
        DEFB $CC,$FF,$CE,$FF,$8B,$C0,$8B,$E2,$4B,$00,$12,$AE
        DEFB $00,$E0,$6A,$08,$6B,$F8,$4B,$00,$8C,$E0,$4B,$10
        DEFB $8C,$E0,$7B,$08,$4B,$20,$12,$EE,$7A,$08,$4A,$30
        DEFB $12,$EA,$DA,$B7,$8C,$C6,$3F,$01,$12,$E8,$A4,$24
        DEFB $DA,$B8,$A4,$1D,$12,$D4,$6A,$08,$12,$C6,$23,$50
        DEFB $F3,$0A,$13,$66,$23,$50,$7D,$01,$4D,$64,$12,$00
        DEFB $23,$50,$A3,$E1,$6A,$08,$6B,$08,$7B,$08,$4B,$30
        DEFB $13,$1E,$7A,$08,$4A,$30,$13,$1A,$DA,$B1,$DA,$B1
        DEFB $4F,$01,$12,$F0,$13,$0A,$6A,$08,$13,$04,$00,$E0
        DEFB $23,$50,$A3,$F3,$6A,$14,$6B,$08,$6C,$07,$DA,$B7
        DEFB $FC,$1E,$7A,$08,$DA,$B7,$FC,$1E,$7A,$08,$DA,$B7
        DEFB $6A,$0C,$7B,$08,$FC,$1E,$7A,$08,$DA,$B7,$FC,$1E
        DEFB $7A,$08,$DA,$B7,$FC,$1E,$7A,$08,$DA,$B7,$12,$38
        DEFB $A4,$33,$FD,$33,$F2,$65,$6A,$34,$6B,$0E,$F1,$29
        DEFB $DA,$B5,$7A,$05,$F2,$29,$DA,$B5,$00,$EE,$80,$30
        DEFB $A3,$B8,$F0,$1E,$F0,$65,$8A,$00,$8B,$00,$6C,$03
        DEFB $8A,$C2,$8A,$AE,$8A,$AE,$8A,$AE,$7A,$10,$6C,$1C
        DEFB $8B,$C2,$8B,$BE,$A3,$E1,$DA,$B1,$DA,$B1,$3F,$00
        DEFB $13,$94,$60,$0A,$F0,$18,$12,$F0,$A4,$24,$DA,$B8
        DEFB $83,$A0,$7A,$F8,$4A,$08,$7A,$E0,$DA,$B8,$8A,$30
        DEFB $7A,$08,$4A,$30,$7A,$E0,$DA,$B8,$8A,$30,$7B,$F8
        DEFB $DA,$B8,$7B,$10,$DA,$B8,$12,$F4,$0D,$00,$01,$02
        DEFB $04,$05,$06,$08,$09,$0A,$0C,$0E,$03,$07,$0B,$0F
        DEFB $84,$84,$84,$84,$84,$84,$F4,$71,$C9,$81,$99,$89
        DEFB $C9,$71,$17,$11,$11,$F1,$11,$11,$11,$CE,$11,$10
        DEFB $0E,$01,$11,$0E,$71,$D9,$89,$89,$89,$D9,$70,$17
        DEFB $11,$11,$11,$11,$11,$E1,$C0,$82,$45,$29,$11,$11
        DEFB $11,$10,$F1,$99,$09,$09,$09,$99,$F0,$08,$08,$08
        DEFB $08,$08,$08,$F0,$44,$44,$2A,$2A,$2A,$11,$11,$52
        DEFB $53,$92,$92,$92,$12,$12,$10,$10,$90,$90,$50,$30
        DEFB $10,$00,$7E,$7E,$7E,$7E,$7E,$7E,$FF,$FF,$FF,$FF
        DEFB $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
SPELSLUT:

        END
