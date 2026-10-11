; utanför boken: ABC80, V=V+X och V=V-X som en superinstruktion i VAR0
; Samma VAR0X som a_var (variabeln och dess värde i en kod), och när
; VAR0 ska lägga adressen på stacken ser den också efter om satsen är
;     V=V+X  eller  V=V-X
; med X en konstant eller en variabel utan index, alla av samma typ
; (heltal eller flyttal). Koden genereras som förut:
;     heltal   C1 n b lo hi | C1 n b lo hi D5 | C7 k k  eller  C1 n b a a D5 | F5/F8 | B7
;     flyttal  C0 n b lo hi | C0 n b lo hi D4 | C3 5 byte eller C0 n b a a D4 | F4/F7 | B6
; Varje kod läses i tur, så mönstret känns bara igen från en kods början.
; Stämmer det görs satsen här och VAR0X slutar som ASGNI eller ASGNF:
; RET med HL = variabelns adress, B = typ och DE efter tilldelningen.
; Heltal räknas med ADD/SBC som ADDI/SUBI (utan spilltest). Flyttal
; läggs på stacken som när koderna körs och räknas av FADD eller FSUB
; i ROM:en; variabeln skrivs först när rutinen kommit tillbaka, så ett
; fel lämnar den orörd som förut. Stämmer det inte går VAR0X vidare som
; förut, med HL och DE som de var. En DEF FN-parameter (adress < $100 i
; koden) kan inte vara med: DEF FN-kroppen är ett uttryck (CMPTYX) utan
; tilldelning, och utanför den har ingen variabel en adress < $100.
; Den som tar emot RET (RUNSTM, FOR, THEN-delen) läser inte A, C eller
; flaggorna. EXPTAB1:s tre poster för VAR0 pekar hit (LAPPAR i lappa.py).
        ORG  $5193
VAR0X:  EX   DE,HL
        INC  HL
        INC  HL                 ; RDDE1, på plats
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        EX   DE,HL              ; HL = variabelns adress, DE = koden
        LD   A,H
        AND  A
        CALL Z,$33AB            ; FNPAR: < $100, DEF FN-parameter
        LD   A,(DE)
        CP   $D5
        JR   Z,GETIX
        CP   $D4
        JR   Z,GETFX
        CP   $C1
        JP   Z,SUPI
        CP   $C0
        JP   Z,SUPF
NEJ0:   RST  $28                ; adressen på stacken, som förut
GETIX:  INC  DE                 ; GETI, på plats
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        RST  $28
GETFX:  INC  DE
        LD   B,H                ; GETF efter POP BC
        LD   C,L
        JP   $29A2

; Heltal. HL = variabeln, DE pekar på C1 efter den.
        ORG  $5A93
SUPI:   PUSH DE                 ; koden, om mönstret inte stämmer
        EX   DE,HL              ; DE = variabeln, HL = koden
        INC  HL
        INC  HL
        INC  HL
        LD   A,(HL)             ; samma variabel?
        CP   E
        JR   NZ,NEJI
        INC  HL
        LD   A,(HL)
        CP   D
        JR   NZ,NEJI
        INC  HL
        LD   A,(HL)
        CP   $D5                ; GETI
        JR   NZ,NEJI
        INC  HL
        LD   A,(HL)
        INC  HL
        CP   $C7                ; CONSTI
        JR   NZ,SUPIV
        LD   C,(HL)             ; BC = konstanten
        INC  HL
        LD   B,(HL)
        JR   SUPIO
SUPIV:  CP   $C1                ; eller en heltalsvariabel med GETI
        JR   NZ,NEJI
        INC  HL
        INC  HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)             ; BC = dess adress
        INC  HL
        LD   A,(HL)
        CP   $D5
        JR   NZ,NEJI
        PUSH HL
        LD   H,B                ; BC = värdet
        LD   L,C
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        POP  HL
SUPIO:  INC  HL
        INC  HL
        LD   A,(HL)
        CP   $B7                ; ASGNI
        JR   NZ,NEJI
        DEC  HL
        LD   A,(HL)             ; ADDI eller SUBI
        INC  HL
        INC  HL                 ; HL = efter tilldelningen
        CP   $F5
        JP   Z,SUPIA
        CP   $F8
        JP   Z,SUPIA
NEJI:   POP  HL                 ; koden
        EX   DE,HL              ; HL = variabeln
        RST  $28

        ORG  $5D93
SUPIA:  EX   (SP),HL            ; koden efter satsen på stacken
        EX   DE,HL              ; HL = variabeln
        LD   E,(HL)
        INC  HL
        LD   D,(HL)             ; DE = värdet
        EX   DE,HL
        CP   $F5
        JR   NZ,SUPIS
        ADD  HL,BC              ; ADDI
        JR   SUPIW
SUPIS:  AND  A
        SBC  HL,BC              ; SUBI
SUPIW:  EX   DE,HL
        LD   (HL),D
        DEC  HL
        LD   (HL),E             ; HL = variabeln, som efter ASGNI
        POP  DE
        LD   B,$01
        RET

; Flyttal. HL = variabelns exponent, DE pekar på C0 efter den.
        ORG  $5B93
SUPF:   PUSH DE
        EX   DE,HL
        INC  HL
        INC  HL
        INC  HL
        LD   A,(HL)             ; samma variabel?
        CP   E
        JR   NZ,NEJF
        INC  HL
        LD   A,(HL)
        CP   D
        JR   NZ,NEJF
        INC  HL
        LD   A,(HL)
        CP   $D4                ; GETF
        JR   NZ,NEJF
        INC  HL
        LD   A,(HL)
        CP   $C3                ; CONSTF
        JR   NZ,SUPFV
        LD   BC,$0005
        ADD  HL,BC
        LD   B,H                ; BC = konstantens exponent i koden
        LD   C,L
        JR   SUPFO
SUPFV:  CP   $C0                ; eller en flyttalsvariabel med GETF
        JR   NZ,NEJF
        INC  HL
        INC  HL
        INC  HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)             ; BC = dess exponent
        INC  HL
        LD   A,(HL)
        CP   $D4
        JR   NZ,NEJF
SUPFO:  INC  HL
        INC  HL
        LD   A,(HL)
        CP   $B6                ; ASGNF
        JR   NZ,NEJF
        DEC  HL
        LD   A,(HL)             ; ADDF eller SUBF
        INC  HL
        INC  HL                 ; HL = efter tilldelningen
        CP   $F4
        JR   Z,SUPFA
        CP   $F7
        JR   NZ,NEJF
        LD   IX,$3460           ; FSUB
        JP   SUPFR
SUPFA:  LD   IX,$33F7           ; FADD
        JP   SUPFR
NEJF:   POP  HL
        EX   DE,HL
        RST  $28

; Som FARITH: vänster operand (variabeln) under höger på stacken.
        ORG  $5C93
SUPFR:  EX   (SP),HL            ; koden efter satsen på stacken
        PUSH DE                 ; variabelns exponent
        LD   HL,$FFF6           ; tio byte: SP+0-4 höger, SP+5-9 vänster
        ADD  HL,SP
        LD   SP,HL
        PUSH BC                 ; höger operands exponent
        LD   BC,$0009
        ADD  HL,BC
        EX   DE,HL              ; DE = SP+9, HL = variabeln
        LD   BC,$0005
        LDDR                    ; vänster; DE = SP+4 efteråt
        POP  HL
        LD   C,$05
        LDDR                    ; höger
        LD   HL,$0009
        ADD  HL,SP
        LD   D,H                ; DE = BC = vänster, HL = höger
        LD   E,L
        LD   B,H
        LD   C,L
        LD   HL,$0004
        ADD  HL,SP
        CALL $2A91              ; JPIX: FADD eller FSUB
        LD   HL,$000A
        ADD  HL,SP
        LD   E,(HL)             ; DE = variabelns exponent
        INC  HL
        LD   D,(HL)
        LD   HL,$0009
        ADD  HL,SP
        LD   BC,$0005
        LDDR                    ; resultatet till variabeln
        LD   HL,$000A
        ADD  HL,SP
        LD   SP,HL
        POP  HL                 ; HL = variabeln, som efter ASGNF
        POP  DE
        LD   B,$00
        RET
