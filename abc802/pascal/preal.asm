; preal.asm -- REAL i pvm.asm (PASCAL.md, avsnitt 4.4 och 8.4): limmet
; mellan p-koden och flyttalspaketet FPP ur BBC BASIC. bygg.sh lägger
; ihop limmet, symbolerna ur pvm.lst och fpp/fpp.asm till FPP.BIN, som
; tolken laddar till FPPADR när p-kodfilen säger att programmet använder
; REAL. Opkoderna $A0-$BB går genom RVEK i tolken till RGLUE med A =
; opkoden.
;
; Ett REAL är tre ord, bytes L' H' L H C 0 i minnet och på p-stacken:
; FPP:s HLH'L'C, mantissan med tecknet i H:s högsta bit och exponenten i
; C. Andra talet till en operation ligger i DED'E'B. FPP går, som
; standardprocedurerna, på BASIC:s stack (SAVESP), och p-stacken nås
; genom PSP. Fel i FPP blir körfel 2 (division med noll) eller 8.

FPPADR  EQU  $DD00

        ORG  FPPADR
        DEFW RGLUE          ; tolken hoppar hit genom RVEK
        DEFW FPP            ; pvm.py anropar FPP direkt

RGLUE:  LD   (TMPIP),DE
        LD   (MPS),IX
        LD   (PSP),SP
        LD   SP,(SAVESP)
        SUB  $A0
        ADD  A,A
        LD   L,A
        LD   H,0
        LD   BC,RTAB
        ADD  HL,BC
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        JP   (HL)

RTAB:   DEFW R_LDCR,R_INDR,R_STOR,R_FLT,R_FLO,R_TRC,R_RND,R_ADR
        DEFW R_SBR,R_MPR,R_DVR,R_NGR,R_ABR,R_SQRR,R_EQR,R_NER
        DEFW R_LSR,R_LER,R_GTR,R_GER,R_SQT,R_SIN,R_COS,R_ATN
        DEFW R_LNR,R_EXR,R_WRR,R_RDR

; Tillbaka till p-koden: RX utan värde, RXH med ordet HL, LGXX med
; talet HLH'L'C på p-stacken.
LGXX:   CALL LGX
RX:     LD   SP,(PSP)
        LD   DE,(TMPIP)
        LD   IX,(MPS)
        JP   NEXT
RXH:    LD   SP,(PSP)
        PUSH HL
        LD   DE,(TMPIP)
        LD   IX,(MPS)
        JP   NEXT

; TAX: HLH'L'C = talet överst på p-stacken, som tas bort. B, DE och D'E'
; rörs inte.
TAX:    LD   HL,(PSP)
        LD   A,(HL)
        INC  HL
        EXX
        LD   L,A
        EXX
        LD   A,(HL)
        INC  HL
        EXX
        LD   H,A
        EXX
        LD   A,(HL)
        INC  HL
        PUSH AF
        LD   A,(HL)
        INC  HL
        LD   C,(HL)
        INC  HL
        INC  HL
        LD   (PSP),HL
        LD   H,A
        POP  AF
        LD   L,A
        RET

; LGX: talet HLH'L'C till p-stacken
LGX:    EX   DE,HL
        LD   HL,(PSP)
        DEC  HL
        LD   (HL),0
        DEC  HL
        LD   (HL),C
        DEC  HL
        LD   (HL),D
        DEC  HL
        LD   (HL),E
        EXX
        PUSH HL
        EXX
        POP  DE
        DEC  HL
        LD   (HL),D
        DEC  HL
        LD   (HL),E
        LD   (PSP),HL
        RET

; PUSHW: ordet HL till p-stacken
PUSHW:  EX   DE,HL
        LD   HL,(PSP)
        DEC  HL
        LD   (HL),D
        DEC  HL
        LD   (HL),E
        LD   (PSP),HL
        RET

; FCALL: FPP med operationen A; körfel om FPP säger fel
FCALL:  CALL FPP
        RET  NC
        CP   18
        LD   A,2
        JR   Z,FERR
RFEL8:   LD   A,8
FERR:   LD   DE,(TMPIP)
        JP   KORFEL

; Två tal: det högra till DED'E'B, det vänstra till HLH'L'C
R_ADR:  LD   A,11
        JR   BIN
R_SBR:  LD   A,13
        JR   BIN
R_MPR:  LD   A,10
        JR   BIN
R_DVR:  LD   A,15
BIN:    CALL BIN2
        JP   LGXX
BIN2:   PUSH AF
        CALL TAX
        EX   DE,HL
        EXX
        EX   DE,HL
        EXX
        LD   B,C
        CALL TAX
        POP  AF
        JP   FCALL

; Jämförelserna: FPP ger -1 eller 0, Pascal 1 eller 0
R_EQR:  LD   A,9
        JR   JFR
R_NER:  LD   A,6
        JR   JFR
R_LSR:  LD   A,8
        JR   JFR
R_LER:  LD   A,5
        JR   JFR
R_GTR:  LD   A,12
        JR   JFR
R_GER:  LD   A,7
JFR:    CALL BIN2
        EXX
        LD   A,L
        EXX
        AND  1
        LD   L,A
        LD   H,0
        JP   RXH

; Ett tal
R_ABR:  LD   A,16
        JR   EN
R_SQT:  LD   A,30
        JR   EN
R_SIN:  LD   A,29
        JR   EN
R_COS:  LD   A,20
        JR   EN
R_ATN:  LD   A,19
        JR   EN
R_LNR:  LD   A,24
        JR   EN
R_EXR:  LD   A,22
EN:     PUSH AF
        CALL TAX
        POP  AF
        CALL FCALL
        JP   LGXX

R_NGR:  CALL TAX            ; tecknet byts, utom för noll
        LD   A,C
        OR   A
        JP   Z,LGXX
        LD   A,H
        XOR  $80
        LD   H,A
        JP   LGXX

R_SQRR: CALL TAX            ; x*x
        LD   D,H
        LD   E,L
        EXX
        LD   D,H
        LD   E,L
        EXX
        LD   B,C
        LD   A,10
        CALL FCALL
        JP   LGXX

; LDCR n text: talet ur texten i p-koden, med VAL och FLOAT
R_LDCR: LD   HL,(TMPIP)
        LD   C,(HL)
        LD   B,0
        INC  HL
        LD   DE,RBUF
        LDIR
        LD   (TMPIP),HL
        EX   DE,HL
        LD   (HL),13
VALX:   LD   IX,RBUF
        LD   A,36
        CALL FCALL
        LD   A,39
        CALL FCALL
        JP   LGXX

R_INDR: CALL POPA           ; adr -- r
        PUSH HL
        LD   HL,(PSP)
        LD   BC,-6
        ADD  HL,BC
        LD   (PSP),HL
        EX   DE,HL
        POP  HL
        LD   BC,6
        LDIR
        JP   RX

R_STOR: LD   HL,(PSP)       ; adr r --
        LD   BC,6
        ADD  HL,BC
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        PUSH HL
        LD   HL,(PSP)
        LDIR
        POP  HL
        LD   (PSP),HL
        JP   RX

; FLTHL: HL som tal i HLH'L'C
FLTHL:  PUSH HL
        EXX
        POP  HL
        EXX
        LD   A,H
        RLA
        SBC  A,A
        LD   H,A
        LD   L,A
        LD   C,0
        LD   A,39
        JP   FCALL

R_FLT:  CALL POPA           ; i -- r
        CALL FLTHL
        JP   LGXX

R_FLO:  LD   HL,(PSP)       ; i r -- r r
        LD   DE,RTMP
        LD   BC,6
        LDIR
        LD   (PSP),HL
        CALL POPA
        CALL FLTHL
        CALL LGX
        LD   HL,(PSP)
        LD   BC,-6
        ADD  HL,BC
        LD   (PSP),HL
        EX   DE,HL
        LD   HL,RTMP
        LD   BC,6
        LDIR
        JP   RX

; TRUNC och ROUND: FIX, och körfel 8 om talet inte ryms i ett ord
R_RND:  CALL TAX            ; r -- i: x + 0.5 eller x - 0.5 först
        LD   A,C
        OR   A
        JR   Z,TRC1
        LD   A,H
        AND  $80
        LD   D,A
        LD   E,0
        EXX
        LD   DE,0
        EXX
        LD   B,$7F
        LD   A,11
        CALL FCALL
        JR   TRC1
R_TRC:  CALL TAX            ; r -- i
TRC1:   LD   A,38
        CALL FCALL
        EXX
        LD   A,H
        EXX
        RLA
        SBC  A,A
        CP   H
        JP   NZ,RFEL8
        CP   L
        JP   NZ,RFEL8
        EXX
        PUSH HL
        EXX
        POP  HL
        JP   RXH

; WRR f r b d --: STR till RBUF, G med 9 siffror när d < 0 och annars F
; med d decimaler (högst 10); sedan som WRS.
R_WRR:  CALL POPA
        LD   BC,9           ; B = läget (0 G), C = siffrorna
        BIT  7,H
        JR   NZ,WRR2
        LD   C,10
        LD   A,H
        OR   A
        JR   NZ,WRR1
        LD   A,L
        CP   C
        JR   NC,WRR1
        LD   C,A
WRR1:   LD   B,2
WRR2:   LD   (RFMT+1),BC
        CALL POPA
        LD   (RBRED),HL
        CALL TAX
        LD   DE,RBUF
        LD   IX,RFMT
        LD   A,37
        CALL FCALL
        EX   DE,HL
        LD   DE,RBUF
        OR   A
        SBC  HL,DE
        PUSH HL
        EX   DE,HL
        CALL PUSHW
        POP  HL
        CALL PUSHW
        LD   HL,(RBRED)
        CALL PUSHW
        JP   C_WRS

; RDR f -- r: blanka, radslut och tab först; sedan högst 30 tecken av
; 0-9 + - . E (e blir E), och VAL
R_RDR:  CALL POPA
        CALL LASFIL
RDR1:   CALL TITTA
        JR   C,RDR3
        CP   32
        JR   Z,RDR2
        CP   10
        JR   Z,RDR2
        CP   9
        JR   NZ,RDR3
RDR2:   CALL TA
        JR   RDR1
RDR3:   LD   HL,RBUF
RDR4:   LD   (RPOS),HL
        LD   DE,RBUF+30
        OR   A
        SBC  HL,DE
        JR   NC,RDR6
        CALL TITTA
        JR   C,RDR6
        CP   'e'
        JR   NZ,RDR5
        LD   A,'E'
RDR5:   LD   HL,RTKN
        LD   BC,14
        CPIR
        JR   NZ,RDR6
        LD   HL,(RPOS)
        LD   (HL),A
        CALL TA
        LD   HL,(RPOS)
        INC  HL
        JR   RDR4
RDR6:   LD   HL,(RPOS)
        LD   (HL),13
        LD   DE,RBUF
        OR   A
        SBC  HL,DE
        JP   Z,F5
        JP   VALX
RTKN:   DEFM "0123456789+-.E"

RFMT:   DEFB 0,9,0,0        ; @% till STR: bredd, siffror, läge (0 G, 2 F)
RBRED:  DEFW 0
RPOS:   DEFW 0
RTMP:   DEFS 6,0
RBUF:   DEFS 40,0

; Här följer symbolerna ur pvm.lst och sedan fpp/fpp.asm (bygg.sh).
