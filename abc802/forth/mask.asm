; Masken i maskinkod: samma spel och samma sätt som SNAKE1.BAS i
; ../prov och Forth-masken i mask.bas, så att tiderna går att jämföra.
;
; Rad 0-71 och kolumn 2-158 som SET DOT i BASIC; ramen på rad 0 och 71
; och kolumn 2 och 158. Masken är fälten RA (rader) och CA (kolumner),
; huvudet först. Varje steg: läs tangenten (a z s w), flytta huvudet,
; ät eller krocka, lägg huvudet sist, skifta allt ett steg (LDDR) och
; rita hela masken igen.
;
; MASKMK.ABS laddas till $A800, i Forths ordbok (HERE ligger långt
; under), så BASIC:s minne rörs inte. Z=CALL(43008,V) spelar med V
; avbrott (1/93,75 s) väntan per steg och ger poängen; antalet steg
; ligger på 43011-43012.

DOTADR: EQU  $04C0          ; ROM: BC = rad, HL = kolumn -> HL, A
PRTXT:  EQU  $000B          ; ROM: skriv BC tecken från HL
KEYFLG: EQU  $FFE2          ; bit 7: en tangent har kommit
LASTKY: EQU  $FFE3          ; tangentens kod
SEK:    EQU  $FFF4          ; klockans sekunder
TICK:   EQU  $FFF5          ; avbrotten, 93,75 per sekund
SKARM:  EQU  $7800
RA:     EQU  $AB00          ; 501 bytes
CA:     EQU  RA+501         ; 501 bytes, slutar $AEEA

        ORG  $A800

        JP   SPEL
NSTEG:  DEFW 0              ; $A803: antalet steg
LADDAD: LD   HL,0           ; DRUN hoppar hit när filen är inläst
        RET

T:      DEFW 0              ; maskens längd
P:      DEFW 0              ; poängen
SEED:   DEFW 0
CU:     DEFB 0              ; riktningen: 1 vänster, 2 ned, 3 höger, 4 upp
FX:     DEFB 0              ; maten
FY:     DEFB 0
R1:     DEFB 0              ; svansen, som släcks i nästa steg
C1:     DEFB 0
V:      DEFB 0
FF:     DEFB 12

SPEL:   LD   A,E
        LD   (V),A
        LD   A,(TICK)       ; fröet från klockan, som RANDOMIZE 0
        LD   L,A
        LD   A,(SEK)
        LD   H,A
        LD   (SEED),HL
        CALL GR
        CALL RAM
        LD   HL,RA          ; masken: rad 5, 4, 3, 2, 1 i kolumn 79
        LD   DE,CA
        LD   B,5
        LD   A,5
INIT:   LD   (HL),A
        INC  HL
        EX   DE,HL
        LD   (HL),79
        INC  HL
        EX   DE,HL
        DEC  A
        DJNZ INIT
        LD   HL,5
        LD   (T),HL
        LD   HL,0
        LD   (P),HL
        LD   (NSTEG),HL
        LD   A,1
        LD   (CU),A
        LD   (R1),A
        LD   A,79
        LD   (C1),A
        CALL MAT

LOOP:   CALL TANG
        LD   A,(RA)         ; B = huvudets rad, C = kolumn
        LD   B,A
        LD   A,(CA)
        LD   C,A
        LD   A,(CU)
        DEC  A
        JR   NZ,LOOP2
        DEC  C
        JR   LOOP5
LOOP2:  DEC  A
        JR   NZ,LOOP3
        INC  B
        JR   LOOP5
LOOP3:  DEC  A
        JR   NZ,LOOP4
        INC  C
        JR   LOOP5
LOOP4:  DEC  B
LOOP5:  LD   A,(FX)
        CP   B
        JR   NZ,TEST
        LD   A,(FY)
        CP   C
        JR   NZ,TEST
        CALL ATA
        JR   FLYTT
TEST:   CALL TSTD
        JR   NZ,SLUT        ; en tänd punkt: krock
FLYTT:  LD   HL,(T)         ; huvudet sist, RA(T-1) och CA(T-1)
        DEC  HL
        PUSH HL
        LD   DE,RA
        ADD  HL,DE
        LD   (HL),B
        POP  HL
        LD   DE,CA
        ADD  HL,DE
        LD   (HL),C
        CALL SKIFT
        CALL RITA
        LD   HL,(NSTEG)
        INC  HL
        LD   (NSTEG),HL
        CALL VANTA
        JR   LOOP
SLUT:   LD   HL,(P)
        RET

; Tangenten utan att vänta, som INKEY i Forth; inte rakt bakåt.
TANG:   LD   A,(KEYFLG)
        BIT  7,A
        RET  Z
        AND  $7F
        LD   (KEYFLG),A
        LD   A,(LASTKY)
        LD   HL,CU
        CP   'a'
        JR   NZ,TANG2
        LD   A,(HL)
        CP   3
        RET  Z
        LD   (HL),1
        RET
TANG2:  CP   's'
        JR   NZ,TANG3
        LD   A,(HL)
        CP   1
        RET  Z
        LD   (HL),3
        RET
TANG3:  CP   'z'
        JR   NZ,TANG4
        LD   A,(HL)
        CP   4
        RET  Z
        LD   (HL),2
        RET
TANG4:  CP   'w'
        RET  NZ
        LD   A,(HL)
        CP   2
        RET  Z
        LD   (HL),4
        RET

; Ät: huvudet på RA(T), masken en längre (högst 500), ny mat. BC kvar.
ATA:    LD   HL,(T)
        PUSH HL
        LD   DE,RA
        ADD  HL,DE
        LD   (HL),B
        POP  HL
        PUSH HL
        LD   DE,CA
        ADD  HL,DE
        LD   (HL),C
        POP  HL
        LD   DE,500
        OR   A
        SBC  HL,DE
        JR   NC,ATA2
        LD   HL,(T)
        INC  HL
        LD   (T),HL
ATA2:   LD   HL,(P)
        INC  HL
        LD   (P),HL
        PUSH BC
        CALL MAT
        POP  BC
        RET

; Skifta: det sista (det nya huvudet) först, resten ett steg bakåt.
SKIFT:  LD   HL,(T)
        DEC  HL
        LD   DE,RA
        ADD  HL,DE
        LD   A,(HL)
        PUSH AF
        LD   HL,(T)
        DEC  HL
        LD   DE,CA
        ADD  HL,DE
        LD   A,(HL)
        PUSH AF
        LD   BC,(T)
        LD   HL,RA-1
        ADD  HL,BC
        LD   D,H
        LD   E,L
        INC  DE
        LDDR
        LD   BC,(T)
        LD   HL,CA-1
        ADD  HL,BC
        LD   D,H
        LD   E,L
        INC  DE
        LDDR
        POP  AF
        LD   (CA),A
        POP  AF
        LD   (RA),A
        RET

; Rita: släck förra svansen, tänd hela masken och maten.
RITA:   LD   A,(R1)
        LD   B,A
        LD   A,(C1)
        LD   C,A
        CALL CLRD
        LD   HL,RA
        LD   DE,CA
        LD   BC,(T)
RITA2:  PUSH BC
        LD   B,(HL)
        LD   A,(DE)
        LD   C,A
        CALL SETD
        POP  BC
        INC  HL
        INC  DE
        DEC  BC
        LD   A,B
        OR   C
        JR   NZ,RITA2
        DEC  HL
        DEC  DE
        LD   A,(HL)
        LD   (R1),A
        LD   A,(DE)
        LD   (C1),A
        LD   A,(FX)
        LD   B,A
        LD   A,(FY)
        LD   C,A
        JP   SETD

; Vänta V avbrott.
VANTA:  LD   A,(V)
        OR   A
        RET  Z
        LD   B,A
VANTA2: LD   A,(TICK)
        LD   C,A
VANTA3: LD   A,(TICK)
        CP   C
        JR   Z,VANTA3
        DJNZ VANTA2
        RET

; Maten: rad 5-65 och kolumn 10-147, som i SNAKE1.
MAT:    LD   E,61
        CALL RND
        ADD  A,5
        LD   (FX),A
        LD   E,138
        CALL RND
        ADD  A,10
        LD   (FY),A
        RET

; RND: A = 0..E-1 ur den höga byten av SEED = SEED * 31421 + 6927.
RND:    PUSH DE
        LD   HL,(SEED)
        LD   DE,31421
        CALL MUL16
        LD   DE,6927
        ADD  HL,DE
        LD   (SEED),HL
        POP  DE
        LD   A,H
        LD   D,0            ; HL = A * E, svaret i H
        LD   HL,0
        LD   B,8
RND2:   ADD  HL,HL
        RLA
        JR   NC,RND3
        ADD  HL,DE
RND3:   DJNZ RND2
        LD   A,H
        RET

; HL = HL * DE (de låga 16 bitarna).
MUL16:  LD   B,H
        LD   C,L
        LD   HL,0
        LD   A,16
MUL2:   ADD  HL,HL
        SLA  E
        RL   D
        JR   NC,MUL3
        ADD  HL,BC
MUL3:   DEC  A
        JR   NZ,MUL2
        RET

; Grafiken: töm skärmen och sätt koden 151 först på varje rad.
GR:     LD   HL,FF
        LD   BC,1
        CALL PRTXT
        LD   HL,SKARM
        LD   DE,80
        LD   B,24
GR2:    LD   (HL),151
        ADD  HL,DE
        DJNZ GR2
        RET

; Ramen.
RAM:    LD   C,2
RAM2:   LD   B,0
        CALL SETD
        LD   B,71
        CALL SETD
        INC  C
        LD   A,C
        CP   159
        JR   NZ,RAM2
        LD   B,0
RAM3:   LD   C,2
        CALL SETD
        LD   C,158
        CALL SETD
        INC  B
        LD   A,B
        CP   72
        JR   NZ,RAM3
        RET

; SETD, CLRD och TSTD: punkten på rad B, kolumn C, med ROM:ens DOTADR.
; BC, DE och HL är kvar; TSTD ger NZ om punkten är tänd.
SETD:   PUSH BC
        PUSH DE
        PUSH HL
        CALL DOTPOS
        OR   (HL)
        OR   $20
        LD   (HL),A
        JR   DOTUT
CLRD:   PUSH BC
        PUSH DE
        PUSH HL
        CALL DOTPOS
        CPL
        AND  (HL)
        OR   $20
        LD   (HL),A
        JR   DOTUT
TSTD:   PUSH BC
        PUSH DE
        PUSH HL
        CALL DOTPOS
        AND  (HL)
        AND  $DF
DOTUT:  POP  HL
        POP  DE
        POP  BC
        RET
DOTPOS: LD   L,C
        LD   H,0
        LD   C,B
        LD   B,0
        CALL DOTADR
        DEFB $00            ; rad och kolumn uppifrån
        RET
SLUTKOD:
