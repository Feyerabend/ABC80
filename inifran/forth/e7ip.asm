; Exempel 7: en minimal Forth. Skrivet för boken, inte ur någon krets.
;
; Kärnan, direkt trådad (DTC). Korskompilatorn e7fc.py läser den här
; filen och Forth-källan e7.fs och skriver e7.asm. Allt före den första
; ;W-raden kommer alltid med; varje ;W-rad börjar ett ord:
;   ;W namn ETIKETT [ord som det behöver ...]
; och ordet tas med bara om något program använder det.
;
; Registren:
;   SP  datastacken (Z80-stacken)     BC  översta värdet (TOS)
;   IX  returstacken, växer nedåt     DE  IP, nästa adress i tråden
;   HL  arbetsregister                IY  rörs inte (ROM:ens, $FE16)
;
; Ett ord är maskinkod. Ett kolonord börjar med CALL DOCOL och följs
; av adresserna till de ord det består av.
;
; Från BASIC: Z=CALL(a,n) kör ingång n (0 om n saknas), a = ORG:
; 16384 i kretsen, 49152 i RAM. Det värde som ligger överst på stacken
; när ordet slutar blir CALL:s värde. Kärnan förutsätter ingen adress;
; e7fc.py byter ORG nedan.

PRTXT:  EQU  $000B          ; ROM: skriv BC tecken från HL
GETKEY: EQU  $0002          ; ROM: vänta på tangent, A = tecknet
INLINE: EQU  $0005          ; ROM: läs en rad till HL, högst C tecken
SAVESP: EQU  $7C78          ; BASIC:s SP, i bildminnet där det inte syns
HEAP:   EQU  $FE20          ; BASIC: första lediga byte efter variablerna
S0:     EQU  $7C7A          ; datastackens botten = kanariefågelns adress
CANARY: EQU  $5AA5          ; värdet mellan returstacken och datastacken

        ORG  $4000          ; $C000 i e7r.asm och e7i.asm

        JP   COLD

; Kallstart: returstacken direkt under BASIC:s stack, datastacken 64
; bytes längre ned. Mellan dem en kanariefågel: skrivs den över har
; returstacken vuxit för långt (?STACK). TOS = ingångens adress, och
; tråden BOOT kör den; EXECUTE tar då det andra CANARY-värdet som TOS.
COLD:   LD   (SAVESP),SP
        LD   IX,0
        ADD  IX,SP
        LD   HL,-64
        ADD  HL,SP
        LD   SP,HL
        LD   HL,CANARY
        PUSH HL
        LD   (S0),SP
        PUSH HL
        LD   HL,ENTRY       ; ingångstabellen, skrivs av e7fc.py
        ADD  HL,DE
        ADD  HL,DE
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        LD   DE,BOOT
; NEXT: hämta nästa adress i tråden och hoppa dit.
NEXT:   EX   DE,HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        EX   DE,HL
        JP   (HL)

BOOT:   DEFW EXECUTE,BYE

; DOCOL: lägg IP på returstacken; tråden börjar efter CALL DOCOL,
; vars returadress CALL lade på datastacken. Returstacken får inte nå
; kanariefågeln: IX >= S0 + 2.
DOCOL:  DEC  IX
        LD   (IX+0),D
        DEC  IX
        LD   (IX+0),E
        POP  DE
        PUSH DE
        PUSH IX
        POP  DE
        LD   HL,(S0)
        INC  HL
        INC  HL
        EX   DE,HL
        OR   A
        SBC  HL,DE          ; IX - (S0 + 2)
        POP  DE
        JP   NC,NEXT

; En stack utanför sina gränser (DOCOL, BRANCH): töm båda och kör
; tråden ABORTT, som e7fc.py skriver: tolkens ABORT, annars BYE med
; -1 som CALL:s värde.
STKERR: LD   SP,(S0)
        LD   HL,CANARY
        EX   (SP),HL
        LD   BC,-1
        LD   IX,(SAVESP)
        LD   DE,ABORTT
        JP   NEXT

;W EXIT EXIT
H_EXIT: DEFW 0
        DEFB 4
        DEFM "EXIT"
EXIT:   LD   E,(IX+0)
        LD   D,(IX+1)
        INC  IX
        INC  IX
        JP   NEXT

;W EXECUTE EXECUTE
H_EXECUTE: DEFW H_EXIT
        DEFB 7
        DEFM "EXECUTE"
EXECUTE: LD  H,B
        LD   L,C
        POP  BC
        JP   (HL)

;W BYE BYE
; Tillbaka till BASIC med TOS som CALL:s värde.
H_BYE: DEFW H_EXECUTE
        DEFB 3
        DEFM "BYE"
BYE:    LD   H,B
        LD   L,C
        LD   SP,(SAVESP)
        RET

;W LIT LIT
H_LIT: DEFW H_BYE
        DEFB 3
        DEFM "LIT"
LIT:    PUSH BC
        EX   DE,HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        EX   DE,HL
        JP   NEXT

;W BRANCH BRANCH
; Alla slingor går tillbaka hit, så här prövas stackarna: datastacken
; inte tömd förbi S0 och inte nere vid BASIC:s variabler (HEAP + 64),
; returstacken (>R i en slinga) som i DOCOL.
H_BRANCH: DEFW H_LIT
        DEFB 6
        DEFM "BRANCH"
BRANCH: LD   HL,(S0)
        OR   A
        SBC  HL,SP
        JP   C,STKERR
        PUSH DE
        LD   HL,-64
        ADD  HL,SP
        LD   DE,(HEAP)
        OR   A
        SBC  HL,DE
        JP   C,STKERR
        PUSH IX
        POP  DE
        LD   HL,(S0)
        INC  HL
        INC  HL
        EX   DE,HL
        SBC  HL,DE          ; C = 0 här
        POP  DE
        JP   C,STKERR
        EX   DE,HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        JP   NEXT

;W 0BRANCH QBRAN BRANCH
H_QBRAN: DEFW H_BRANCH
        DEFB 7
        DEFM "0BRANCH"
QBRAN:  LD   A,B
        OR   C
        POP  BC
        JR   Z,BRANCH
        INC  DE
        INC  DE
        JP   NEXT

;W (DO) XDO
; Returstacken: (IX) = index, (IX+2) = gräns.
H_XDO: DEFW H_QBRAN
        DEFB 4
        DEFB 40,68,79,41 ; (DO)
XDO:    POP  HL
        DEC  IX
        LD   (IX+0),H
        DEC  IX
        LD   (IX+0),L
        DEC  IX
        LD   (IX+0),B
        DEC  IX
        LD   (IX+0),C
        POP  BC
        JP   NEXT

;W (LOOP) XLOOP BRANCH
H_XLOOP: DEFW H_XDO
        DEFB 6
        DEFB 40,76,79,79,80,41 ; (LOOP)
XLOOP:  LD   L,(IX+0)
        LD   H,(IX+1)
        INC  HL
        LD   (IX+0),L
        LD   (IX+1),H
        LD   A,L
        CP   (IX+2)
        JP   NZ,BRANCH
        LD   A,H
        CP   (IX+3)
        JP   NZ,BRANCH
        LD   HL,4           ; klart: ta bort index och gräns
        EX   DE,HL
        ADD  IX,DE
        EX   DE,HL
        INC  DE
        INC  DE
        JP   NEXT

;W I IDX
H_IDX: DEFW H_XLOOP
        DEFB 1
        DEFM "I"
IDX:     PUSH BC
        LD   C,(IX+0)
        LD   B,(IX+1)
        JP   NEXT

;W UNLOOP UNLOOP
; Ta bort index och gräns, före EXIT inne i en DO-slinga.
H_UNLOOP: DEFW H_IDX
        DEFB 6
        DEFM "UNLOOP"
UNLOOP: INC  IX
        INC  IX
        INC  IX
        INC  IX
        JP   NEXT

;W >R TOR
H_TOR: DEFW H_UNLOOP
        DEFB 2
        DEFM ">R"
TOR:    DEC  IX
        LD   (IX+0),B
        DEC  IX
        LD   (IX+0),C
        POP  BC
        JP   NEXT

;W R> RFROM
H_RFROM: DEFW H_TOR
        DEFB 2
        DEFM "R>"
RFROM:  PUSH BC
        LD   C,(IX+0)
        LD   B,(IX+1)
        INC  IX
        INC  IX
        JP   NEXT

;W R@ RFETCH IDX
H_RFETCH: DEFW H_RFROM
        DEFB 2
        DEFM "R@"
RFETCH: JP   IDX

;W DUP DUP
H_DUP: DEFW H_RFETCH
        DEFB 3
        DEFM "DUP"
DUP:    PUSH BC
        JP   NEXT

;W DROP DROP
H_DROP: DEFW H_DUP
        DEFB 4
        DEFM "DROP"
DROP:   POP  BC
        JP   NEXT

;W SWAP SWAP
H_SWAP: DEFW H_DROP
        DEFB 4
        DEFM "SWAP"
SWAP:   POP  HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W OVER OVER
H_OVER: DEFW H_SWAP
        DEFB 4
        DEFM "OVER"
OVER:   POP  HL
        PUSH HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W ROT ROT
H_ROT: DEFW H_OVER
        DEFB 3
        DEFM "ROT"
ROT:    POP  HL
        EX   (SP),HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W + PLUS
H_PLUS: DEFW H_ROT
        DEFB 1
        DEFM "+"
PLUS:   POP  HL
        ADD  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W - MINUS
H_MINUS: DEFW H_PLUS
        DEFB 1
        DEFM "-"
MINUS:  POP  HL
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W AND ANDW
H_ANDW: DEFW H_MINUS
        DEFB 3
        DEFM "AND"
ANDW:    POP  HL
        LD   A,C
        AND  L
        LD   C,A
        LD   A,B
        AND  H
        LD   B,A
        JP   NEXT

;W OR ORW
H_ORW: DEFW H_ANDW
        DEFB 2
        DEFM "OR"
ORW:    POP  HL
        LD   A,C
        OR   L
        LD   C,A
        LD   A,B
        OR   H
        LD   B,A
        JP   NEXT

;W NEGATE NEGATE
H_NEGATE: DEFW H_ORW
        DEFB 6
        DEFM "NEGATE"
NEGATE: LD   HL,0
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W 1+ ONEP
H_ONEP: DEFW H_NEGATE
        DEFB 2
        DEFM "1+"
ONEP:   INC  BC
        JP   NEXT

;W 1- ONEM
H_ONEM: DEFW H_ONEP
        DEFB 2
        DEFM "1-"
ONEM:   DEC  BC
        JP   NEXT

;W 2* TWOST
H_TWOST: DEFW H_ONEM
        DEFB 2
        DEFM "2*"
TWOST:  SLA  C
        RL   B
        JP   NEXT

;W 0= ZEQU
H_ZEQU: DEFW H_TWOST
        DEFB 2
        DEFM "0="
ZEQU:   LD   A,B
        OR   C
        LD   BC,0
        JP   NZ,NEXT
        DEC  BC             ; sant = -1
        JP   NEXT

;W 0< ZLESS
H_ZLESS: DEFW H_ZEQU
        DEFB 2
        DEFM "0<"
ZLESS:  LD   A,B
        ADD  A,A            ; teckenbiten till CY
        SBC  A,A
        LD   B,A
        LD   C,A
        JP   NEXT

;W = EQUAL
H_EQUAL: DEFW H_ZLESS
        DEFB 1
        DEFM "="
EQUAL:    POP  HL
        OR   A
        SBC  HL,BC
        LD   BC,0
        JP   NZ,NEXT
        DEC  BC
        JP   NEXT

;W < LESS
H_LESS: DEFW H_EQUAL
        DEFB 1
        DEFM "<"
LESS:   POP  HL
        OR   A
        SBC  HL,BC
        LD   BC,0
        JP   PE,LESS1       ; spill: tecknet är omvänt
        JP   P,NEXT
        DEC  BC
        JP   NEXT
LESS1:  JP   M,NEXT
        DEC  BC
        JP   NEXT

;W U< ULESS
H_ULESS: DEFW H_LESS
        DEFB 2
        DEFM "U<"
ULESS:  POP  HL
        OR   A
        SBC  HL,BC
        SBC  A,A
        LD   B,A
        LD   C,A
        JP   NEXT

;W @ FETCH
H_FETCH: DEFW H_ULESS
        DEFB 1
        DEFM "@"
FETCH:  LD   H,B
        LD   L,C
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JP   NEXT

;W ! STORE
H_STORE: DEFW H_FETCH
        DEFB 1
        DEFM "!"
STORE:  LD   H,B
        LD   L,C
        POP  BC
        LD   (HL),C
        INC  HL
        LD   (HL),B
        POP  BC
        JP   NEXT

;W C@ CFETCH
H_CFETCH: DEFW H_STORE
        DEFB 2
        DEFM "C@"
CFETCH: LD   A,(BC)
        LD   C,A
        LD   B,0
        JP   NEXT

;W C! CSTORE
H_CSTORE: DEFW H_CFETCH
        DEFB 2
        DEFM "C!"
CSTORE: LD   H,B
        LD   L,C
        POP  BC
        LD   (HL),C
        POP  BC
        JP   NEXT

;W (S") XSQ
; Texten står i tråden: en längdbyte och tecknen. Ger adress och längd.
H_XSQ: DEFW H_CSTORE
        DEFB 4
        DEFB 40,83,34,41 ; (S")
XSQ:    PUSH BC
        LD   A,(DE)
        LD   C,A
        LD   B,0
        INC  DE
        PUSH DE
        EX   DE,HL
        ADD  HL,BC
        EX   DE,HL
        JP   NEXT

;W (VAR) DOVAR
; Ett ord som skapats med VARIABLE börjar med CALL DOVAR, och
; returadressen är adressen till värdet.
H_DOVAR: DEFW H_XSQ
        DEFB 5
        DEFB 40,86,65,82,41 ; (VAR)
DOVAR:  POP  HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W (CON) DOCON
; CONSTANT: CALL DOCON följt av värdet.
H_DOCON: DEFW H_DOVAR
        DEFB 5
        DEFB 40,67,79,78,41 ; (CON)
DOCON:  POP  HL
        PUSH BC
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JP   NEXT

;W SAME SAME
; ( a1 a2 n -- f ) Sant om de n bytena på a1 och a2 är lika.
H_SAME: DEFW H_DOCON
        DEFB 4
        DEFM "SAME"
SAME:   POP  HL             ; a2
        EX   (SP),HL        ; HL = a1, a2 på stacken
        EX   DE,HL          ; DE = a1, HL = IP
        EX   (SP),HL        ; HL = a2, IP på stacken
SAME1:  LD   A,B
        OR   C
        JR   Z,SAME2        ; alla lika
        LD   A,(DE)
        CP   (HL)
        JR   NZ,SAME3
        INC  DE
        INC  HL
        DEC  BC
        JR   SAME1
SAME2:  DEC  BC             ; BC = 0 -> -1, sant
        JR   SAME4
SAME3:  LD   BC,0
SAME4:  POP  DE
        JP   NEXT

;W (LOOKUP) XLOOK
; ( a n h -- h' | 0 ) Sök namnet a n i ordlistan, från huvudet h och
; bakåt längs länkarna. h' = huvudet med samma namn, 0 = finns inte.
H_XLOOK: DEFW H_SAME
        DEFB 8
        DEFB 40,76,79,79,75,85,80,41 ; (LOOKUP)
XLOOK:  POP  HL             ; n
        EX   (SP),HL        ; HL = a, n på stacken
        EX   DE,HL          ; DE = a, HL = IP
        EX   (SP),HL        ; HL = n, IP på stacken
        PUSH HL             ; n överst
XL1:    LD   A,B
        OR   C
        JR   Z,XL9          ; listans slut, BC = 0
        LD   H,B
        LD   L,C
        INC  HL
        INC  HL
        LD   A,(HL)
        AND  $7F            ; namnets längd
        EX   (SP),HL
        CP   L              ; = n?
        EX   (SP),HL
        JR   NZ,XL3
        PUSH BC
        PUSH DE
        LD   B,A
XL2:    INC  HL             ; jämför tecken för tecken
        LD   A,(DE)
        CP   (HL)
        JR   NZ,XL4
        INC  DE
        DJNZ XL2
        POP  DE             ; lika: BC = huvudet
        POP  BC
        JR   XL9
XL4:    POP  DE
        POP  BC
XL3:    LD   H,B            ; nästa huvud
        LD   L,C
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JR   XL1
XL9:    POP  HL
        POP  DE
        JP   NEXT

;W ?STACK QSTACK
; ( -- f ) Sant om datastacken har tömts förbi S0 eller om
; kanariefågeln under returstacken har skrivits över.
H_QSTACK: DEFW H_XLOOK
        DEFB 6
        DEFM "?STACK"
QSTACK: PUSH BC
        LD   HL,(S0)
        LD   C,(HL)
        INC  HL
        LD   B,(HL)         ; BC = det som står på S0
        DEC  HL
        DEC  HL
        DEC  HL             ; S0 - 2 = SP när stacken är tom (efter PUSH)
        OR   A
        SBC  HL,SP
        JR   C,QST1         ; SP högre: för många tagna
        LD   HL,CANARY
        SBC  HL,BC
        LD   BC,0
        JP   Z,NEXT
QST1:   LD   BC,-1
        JP   NEXT

;W RESET RESET
; Töm båda stackarna och sätt tillbaka kanariefågeln. Bara från QUIT,
; som aldrig går tillbaka till den som anropade den.
H_RESET: DEFW H_QSTACK
        DEFB 5
        DEFM "RESET"
RESET:  LD   SP,(S0)
        LD   HL,CANARY
        EX   (SP),HL
        LD   BC,0
        LD   IX,(SAVESP)
        JP   NEXT

;W EMIT EMIT
; Tecknet läggs på stacken och skrivs med ROM:ens PRTXT.
H_EMIT: DEFW H_RESET
        DEFB 4
        DEFM "EMIT"
EMIT:   PUSH BC
        LD   HL,0
        ADD  HL,SP
        PUSH DE
        LD   BC,1
        CALL PRTXT
        POP  DE
        POP  BC
        POP  BC
        JP   NEXT

;W TYPE TYPE
H_TYPE: DEFW H_EMIT
        DEFB 4
        DEFM "TYPE"
TYPE:   POP  HL
        PUSH DE
        CALL PRTXT
        POP  DE
        POP  BC
        JP   NEXT

;W KEY KEY
H_KEY: DEFW H_TYPE
        DEFB 3
        DEFM "KEY"
KEY:    PUSH BC
        PUSH DE
        PUSH IX
        CALL GETKEY
        POP  IX
        POP  DE
        LD   C,A
        LD   B,0
        JP   NEXT

;W ACCEPT ACCEPT
; ( a n -- n2 ) Läs en rad med ROM:ens INLINE, som ekar och låter
; raden rättas; n2 = antal tecken, utan CR:en som INLINE lägger sist.
H_ACCEPT: DEFW H_KEY
        DEFB 6
        DEFM "ACCEPT"
ACCEPT: POP  HL
        PUSH HL
        PUSH DE
        CALL INLINE         ; HL = CR:en efter raden
        POP  DE
        POP  BC
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W U. UDOT
; Talet skrivs med ROM:ens PRNUM i en buffert på stacken, sedan ett
; blanktecken, och bufferten med PRTXT.
H_UDOT: DEFW H_ACCEPT
        DEFB 2
        DEFM "U."
UDOT:   PUSH DE
        PUSH HL             ; 6 bytes buffert
        PUSH HL
        PUSH HL
        LD   HL,0
        ADD  HL,SP
        EX   DE,HL          ; DE = bufferten
        LD   H,B
        LD   L,C
        CALL PRNUM          ; DE = efter siffrorna
        LD   A,' '
        LD   (DE),A
        INC  DE
        LD   HL,0
        ADD  HL,SP          ; HL = bufferten
        EX   DE,HL
        OR   A
        SBC  HL,DE
        LD   B,H
        LD   C,L            ; BC = längden
        EX   DE,HL
        CALL PRTXT
        POP  HL
        POP  HL
        POP  HL
        POP  DE
        POP  BC
        JP   NEXT

; ROM:ens PRNUM ligger på olika adresser i de två versionerna
; (exempel 1).
PRNUM:  CALL VERSEL
        DEFW $1862          ; 11273
        DEFW $1860          ; 9913

; VERSEL hoppar till den första adressen efter CALL i 11273 och till
; den andra i 9913 (byten på $0019). Inga register ändras.
VERSEL: EX   (SP),HL
        PUSH AF
        LD   A,($0019)
        CP   $BE            ; 11273?
        JR   Z,V1
        INC  HL
        INC  HL
V1:     LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        POP  AF
        EX   (SP),HL
        RET

; Orden i Forth (från e7.fs och e7i.fs)

; : CR
H_F_CR: DEFW H_UDOT
        DEFB 2
        DEFM "CR"
F_CR: CALL DOCOL
        DEFW LIT,13
        DEFW EMIT ; EMIT
        DEFW LIT,10
        DEFW EMIT ; EMIT
        DEFW EXIT ; EXIT

; : .
H_F__2E: DEFW H_F_CR
        DEFB 1
        DEFM "."
F__2E: CALL DOCOL
        DEFW DUP ; DUP
        DEFW ZLESS ; 0<
        DEFW QBRAN ; 0BRANCH
        DEFW F__2E_L1
        DEFW LIT,45
        DEFW EMIT ; EMIT
        DEFW NEGATE ; NEGATE
F__2E_L1:
        DEFW UDOT ; U.
        DEFW EXIT ; EXIT

; : MAIN
H_F_MAIN: DEFW H_F__2E
        DEFB 4
        DEFM "MAIN"
F_MAIN: CALL DOCOL
        DEFW LIT,1
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW F__2E ; .
        DEFW F_CR ; CR
        DEFW LIT,5
        DEFW LIT,0
        DEFW XDO ; (DO)
F_MAIN_L2:
        DEFW IDX ; I
        DEFW F__2E ; .
        DEFW XLOOP ; (LOOP)
        DEFW F_MAIN_L2
        DEFW F_CR ; CR
        DEFW LIT,-42
        DEFW F__2E ; .
        DEFW XSQ
        DEFB 3
        DEFM "HEJ"
        DEFW TYPE ; TYPE
        DEFW F_CR ; CR
        DEFW EXIT ; EXIT

; : TIO
H_F_TIO: DEFW H_F_MAIN
        DEFB 3
        DEFM "TIO"
F_TIO: CALL DOCOL
        DEFW LIT,7
        DEFW LIT,3
        DEFW PLUS ; +
        DEFW EXIT ; EXIT

; : MATCH
H_F_MATCH: DEFW H_F_TIO
        DEFB 5
        DEFM "MATCH"
F_MATCH: CALL DOCOL
        DEFW LIT,31868
        DEFW FETCH ; @
        DEFW LIT,31870
        DEFW FETCH ; @
        DEFW SAME ; SAME
        DEFW EXIT ; EXIT

; : LINE?
H_F_LINE_3F: DEFW H_F_MATCH
        DEFB 5
        DEFM "LINE?"
F_LINE_3F: CALL DOCOL
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,2
        DEFW MINUS ; -
        DEFW LIT,31870
        DEFW FETCH ; @
        DEFW MINUS ; -
        DEFW DUP ; DUP
        DEFW LIT,1
        DEFW LESS ; <
        DEFW QBRAN ; 0BRANCH
        DEFW F_LINE_3F_L3
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW LIT,0
        DEFW EXIT ; EXIT
F_LINE_3F_L3:
        DEFW TOR ; >R
        DEFW LIT,3
        DEFW PLUS ; +
        DEFW LIT,0
        DEFW SWAP ; SWAP
        DEFW DUP ; DUP
        DEFW RFROM ; R>
        DEFW PLUS ; +
        DEFW SWAP ; SWAP
        DEFW XDO ; (DO)
F_LINE_3F_L4:
        DEFW IDX ; I
        DEFW F_MATCH ; MATCH
        DEFW ORW ; OR
        DEFW XLOOP ; (LOOP)
        DEFW F_LINE_3F_L4
        DEFW EXIT ; EXIT

; : FIND
H_F_FIND: DEFW H_F_LINE_3F
        DEFB 4
        DEFM "FIND"
F_FIND: CALL DOCOL
        DEFW LIT,65056
        DEFW FETCH ; @
        DEFW LIT,31868
        DEFW STORE ; !
        DEFW LIT,31868
        DEFW FETCH ; @
        DEFW LIT,40
        DEFW ACCEPT ; ACCEPT
        DEFW LIT,31870
        DEFW STORE ; !
        DEFW F_CR ; CR
        DEFW LIT,0
        DEFW LIT,31870
        DEFW FETCH ; @
        DEFW ZEQU ; 0=
        DEFW QBRAN ; 0BRANCH
        DEFW F_FIND_L5
        DEFW EXIT ; EXIT
F_FIND_L5:
        DEFW LIT,65052
        DEFW FETCH ; @
F_FIND_L6:
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,1
        DEFW EQUAL ; =
        DEFW ZEQU ; 0=
        DEFW QBRAN ; 0BRANCH
        DEFW F_FIND_L7
        DEFW DUP ; DUP
        DEFW F_LINE_3F ; LINE?
        DEFW QBRAN ; 0BRANCH
        DEFW F_FIND_L8
        DEFW DUP ; DUP
        DEFW ONEP ; 1+
        DEFW FETCH ; @
        DEFW UDOT ; U.
        DEFW SWAP ; SWAP
        DEFW ONEP ; 1+
        DEFW SWAP ; SWAP
F_FIND_L8:
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW PLUS ; +
        DEFW BRANCH ; BRANCH
        DEFW F_FIND_L6
F_FIND_L7:
        DEFW DROP ; DROP
        DEFW F_CR ; CR
        DEFW EXIT ; EXIT

; : SPACE
H_F_SPACE: DEFW H_F_FIND
        DEFB 5
        DEFM "SPACE"
F_SPACE: CALL DOCOL
        DEFW LIT,32
        DEFW EMIT ; EMIT
        DEFW EXIT ; EXIT

; : ?DUP
H_F__3FDUP: DEFW H_F_SPACE
        DEFB 4
        DEFM "?DUP"
F__3FDUP: CALL DOCOL
        DEFW DUP ; DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F__3FDUP_L9
        DEFW DUP ; DUP
F__3FDUP_L9:
        DEFW EXIT ; EXIT

; : HERE
H_F_HERE: DEFW H_F__3FDUP
        DEFB 4
        DEFM "HERE"
F_HERE: CALL DOCOL
        DEFW LIT,31994
        DEFW FETCH ; @
        DEFW EXIT ; EXIT

; : ,
H_F__2C: DEFW H_F_HERE
        DEFB 1
        DEFM ","
F__2C: CALL DOCOL
        DEFW F_HERE ; HERE
        DEFW STORE ; !
        DEFW F_HERE ; HERE
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW LIT,31994
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : C,
H_F_C_2C: DEFW H_F__2C
        DEFB 2
        DEFM "C,"
F_C_2C: CALL DOCOL
        DEFW F_HERE ; HERE
        DEFW CSTORE ; C!
        DEFW F_HERE ; HERE
        DEFW ONEP ; 1+
        DEFW LIT,31994
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : ALLOT
H_F_ALLOT: DEFW H_F_C_2C
        DEFB 5
        DEFM "ALLOT"
F_ALLOT: CALL DOCOL
        DEFW LIT,31994
        DEFW FETCH ; @
        DEFW PLUS ; +
        DEFW LIT,31994
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : TIB
H_F_TIB: DEFW H_F_ALLOT
        DEFB 3
        DEFM "TIB"
F_TIB: CALL DOCOL
        DEFW LIT,65052
        DEFW FETCH ; @
        DEFW LIT,80
        DEFW MINUS ; -
        DEFW EXIT ; EXIT

; : END?
H_F_END_3F: DEFW H_F_TIB
        DEFB 4
        DEFM "END?"
F_END_3F: CALL DOCOL
        DEFW F_TIB ; TIB
        DEFW LIT,32122
        DEFW FETCH ; @
        DEFW PLUS ; +
        DEFW ULESS ; U<
        DEFW ZEQU ; 0=
        DEFW EXIT ; EXIT

; : PARSE-NAME
H_F_PARSE_2DNAME: DEFW H_F_END_3F
        DEFB 10
        DEFM "PARSE-NAME"
F_PARSE_2DNAME: CALL DOCOL
        DEFW F_TIB ; TIB
        DEFW LIT,32120
        DEFW FETCH ; @
        DEFW PLUS ; +
F_PARSE_2DNAME_L10:
        DEFW DUP ; DUP
        DEFW F_END_3F ; END?
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_2DNAME_L11
        DEFW LIT,0
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_2DNAME_L12
F_PARSE_2DNAME_L11:
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,32
        DEFW EQUAL ; =
F_PARSE_2DNAME_L12:
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_2DNAME_L13
        DEFW ONEP ; 1+
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_2DNAME_L10
F_PARSE_2DNAME_L13:
        DEFW DUP ; DUP
F_PARSE_2DNAME_L14:
        DEFW DUP ; DUP
        DEFW F_END_3F ; END?
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_2DNAME_L15
        DEFW LIT,0
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_2DNAME_L16
F_PARSE_2DNAME_L15:
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,32
        DEFW EQUAL ; =
        DEFW ZEQU ; 0=
F_PARSE_2DNAME_L16:
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_2DNAME_L17
        DEFW ONEP ; 1+
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_2DNAME_L14
F_PARSE_2DNAME_L17:
        DEFW DUP ; DUP
        DEFW F_TIB ; TIB
        DEFW MINUS ; -
        DEFW LIT,32120
        DEFW STORE ; !
        DEFW OVER ; OVER
        DEFW MINUS ; -
        DEFW EXIT ; EXIT

; : PARSE
H_F_PARSE: DEFW H_F_PARSE_2DNAME
        DEFB 5
        DEFM "PARSE"
F_PARSE: CALL DOCOL
        DEFW TOR ; >R
        DEFW F_TIB ; TIB
        DEFW LIT,32120
        DEFW FETCH ; @
        DEFW PLUS ; +
        DEFW DUP ; DUP
F_PARSE_L18:
        DEFW DUP ; DUP
        DEFW F_END_3F ; END?
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_L19
        DEFW LIT,0
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_L20
F_PARSE_L19:
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW RFETCH ; R@
        DEFW EQUAL ; =
        DEFW ZEQU ; 0=
F_PARSE_L20:
        DEFW QBRAN ; 0BRANCH
        DEFW F_PARSE_L21
        DEFW ONEP ; 1+
        DEFW BRANCH ; BRANCH
        DEFW F_PARSE_L18
F_PARSE_L21:
        DEFW RFROM ; R>
        DEFW DROP ; DROP
        DEFW OVER ; OVER
        DEFW MINUS ; -
        DEFW DUP ; DUP
        DEFW LIT,32120
        DEFW FETCH ; @
        DEFW PLUS ; +
        DEFW ONEP ; 1+
        DEFW LIT,32120
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : LOOKUP
H_F_LOOKUP: DEFW H_F_PARSE
        DEFB 6
        DEFM "LOOKUP"
F_LOOKUP: CALL DOCOL
        DEFW LIT,31996
        DEFW FETCH ; @
        DEFW XLOOK ; (LOOKUP)
        DEFW EXIT ; EXIT

; : >XT
H_F__3EXT: DEFW H_F_LOOKUP
        DEFB 3
        DEFM ">XT"
F__3EXT: CALL DOCOL
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,127
        DEFW ANDW ; AND
        DEFW PLUS ; +
        DEFW ONEP ; 1+
        DEFW EXIT ; EXIT

; : IMM?
H_F_IMM_3F: DEFW H_F__3EXT
        DEFB 4
        DEFM "IMM?"
F_IMM_3F: CALL DOCOL
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW CFETCH ; C@
        DEFW LIT,128
        DEFW ANDW ; AND
        DEFW EXIT ; EXIT

; : 10*
H_F_10_2A: DEFW H_F_IMM_3F
        DEFB 3
        DEFM "10*"
F_10_2A: CALL DOCOL
        DEFW DUP ; DUP
        DEFW TWOST ; 2*
        DEFW TWOST ; 2*
        DEFW PLUS ; +
        DEFW TWOST ; 2*
        DEFW EXIT ; EXIT

; : NUMBER?
H_F_NUMBER_3F: DEFW H_F_10_2A
        DEFB 7
        DEFM "NUMBER?"
F_NUMBER_3F: CALL DOCOL
        DEFW OVER ; OVER
        DEFW CFETCH ; C@
        DEFW LIT,45
        DEFW EQUAL ; =
        DEFW DUP ; DUP
        DEFW TOR ; >R
        DEFW QBRAN ; 0BRANCH
        DEFW F_NUMBER_3F_L22
        DEFW ONEM ; 1-
        DEFW SWAP ; SWAP
        DEFW ONEP ; 1+
        DEFW SWAP ; SWAP
F_NUMBER_3F_L22:
        DEFW DUP ; DUP
        DEFW ZEQU ; 0=
        DEFW QBRAN ; 0BRANCH
        DEFW F_NUMBER_3F_L23
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW RFROM ; R>
        DEFW DROP ; DROP
        DEFW LIT,0
        DEFW EXIT ; EXIT
F_NUMBER_3F_L23:
        DEFW LIT,0
        DEFW SWAP ; SWAP
        DEFW LIT,0
        DEFW XDO ; (DO)
F_NUMBER_3F_L24:
        DEFW OVER ; OVER
        DEFW IDX ; I
        DEFW PLUS ; +
        DEFW CFETCH ; C@
        DEFW LIT,48
        DEFW MINUS ; -
        DEFW DUP ; DUP
        DEFW LIT,10
        DEFW ULESS ; U<
        DEFW ZEQU ; 0=
        DEFW QBRAN ; 0BRANCH
        DEFW F_NUMBER_3F_L25
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW UNLOOP ; UNLOOP
        DEFW RFROM ; R>
        DEFW DROP ; DROP
        DEFW LIT,0
        DEFW EXIT ; EXIT
F_NUMBER_3F_L25:
        DEFW SWAP ; SWAP
        DEFW F_10_2A ; 10*
        DEFW PLUS ; +
        DEFW XLOOP ; (LOOP)
        DEFW F_NUMBER_3F_L24
        DEFW SWAP ; SWAP
        DEFW DROP ; DROP
        DEFW RFROM ; R>
        DEFW QBRAN ; 0BRANCH
        DEFW F_NUMBER_3F_L26
        DEFW NEGATE ; NEGATE
F_NUMBER_3F_L26:
        DEFW LIT,-1
        DEFW EXIT ; EXIT

; : ERROR
H_F_ERROR: DEFW H_F_NUMBER_3F
        DEFB 5
        DEFM "ERROR"
F_ERROR: CALL DOCOL
        DEFW LIT,32122
        DEFW FETCH ; @
        DEFW LIT,32120
        DEFW STORE ; !
        DEFW LIT,31992
        DEFW FETCH ; @
        DEFW QBRAN ; 0BRANCH
        DEFW F_ERROR_L27
        DEFW LIT,31998
        DEFW FETCH ; @
        DEFW LIT,31994
        DEFW STORE ; !
F_ERROR_L27:
        DEFW LIT,0
        DEFW LIT,31992
        DEFW STORE ; !
        DEFW LIT,-1
        DEFW LIT,32124
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : NOTFOUND
H_F_NOTFOUND: DEFW H_F_ERROR
        DEFB 8
        DEFM "NOTFOUND"
F_NOTFOUND: CALL DOCOL
        DEFW TYPE ; TYPE
        DEFW XSQ
        DEFB 2
        DEFM " ?"
        DEFW TYPE ; TYPE
        DEFW F_ERROR ; ERROR
        DEFW EXIT ; EXIT

; : INTERPRET
H_F_INTERPRET: DEFW H_F_NOTFOUND
        DEFB 9
        DEFM "INTERPRET"
F_INTERPRET: CALL DOCOL
F_INTERPRET_L28:
        DEFW F_PARSE_2DNAME ; PARSE-NAME
        DEFW DUP ; DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F_INTERPRET_L29
        DEFW OVER ; OVER
        DEFW OVER ; OVER
        DEFW F_LOOKUP ; LOOKUP
        DEFW F__3FDUP ; ?DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F_INTERPRET_L30
        DEFW ROT ; ROT
        DEFW ROT ; ROT
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW DUP ; DUP
        DEFW F_IMM_3F ; IMM?
        DEFW LIT,31992
        DEFW FETCH ; @
        DEFW ZEQU ; 0=
        DEFW ORW ; OR
        DEFW QBRAN ; 0BRANCH
        DEFW F_INTERPRET_L31
        DEFW F__3EXT ; >XT
        DEFW EXECUTE ; EXECUTE
        DEFW BRANCH ; BRANCH
        DEFW F_INTERPRET_L32
F_INTERPRET_L31:
        DEFW F__3EXT ; >XT
        DEFW F__2C ; ,
F_INTERPRET_L32:
        DEFW BRANCH ; BRANCH
        DEFW F_INTERPRET_L33
F_INTERPRET_L30:
        DEFW OVER ; OVER
        DEFW OVER ; OVER
        DEFW F_NUMBER_3F ; NUMBER?
        DEFW QBRAN ; 0BRANCH
        DEFW F_INTERPRET_L34
        DEFW ROT ; ROT
        DEFW ROT ; ROT
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW LIT,31992
        DEFW FETCH ; @
        DEFW QBRAN ; 0BRANCH
        DEFW F_INTERPRET_L35
        DEFW LIT,LIT ; ' LIT
        DEFW F__2C ; ,
        DEFW F__2C ; ,
F_INTERPRET_L35:
        DEFW BRANCH ; BRANCH
        DEFW F_INTERPRET_L36
F_INTERPRET_L34:
        DEFW F_NOTFOUND ; NOTFOUND
F_INTERPRET_L36:
F_INTERPRET_L33:
        DEFW BRANCH ; BRANCH
        DEFW F_INTERPRET_L28
F_INTERPRET_L29:
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW EXIT ; EXIT

; : QUIT
H_F_QUIT: DEFW H_F_INTERPRET
        DEFB 4
        DEFM "QUIT"
F_QUIT: CALL DOCOL
F_QUIT_L37:
        DEFW F_TIB ; TIB
        DEFW LIT,79
        DEFW ACCEPT ; ACCEPT
        DEFW LIT,32122
        DEFW STORE ; !
        DEFW LIT,0
        DEFW LIT,32120
        DEFW STORE ; !
        DEFW LIT,0
        DEFW LIT,32124
        DEFW STORE ; !
        DEFW F_SPACE ; SPACE
        DEFW F_INTERPRET ; INTERPRET
        DEFW QSTACK ; ?STACK
        DEFW QBRAN ; 0BRANCH
        DEFW F_QUIT_L38
        DEFW XSQ
        DEFB 7
        DEFM "STACK ?"
        DEFW TYPE ; TYPE
        DEFW F_ERROR ; ERROR
F_QUIT_L38:
        DEFW LIT,32124
        DEFW FETCH ; @
        DEFW QBRAN ; 0BRANCH
        DEFW F_QUIT_L39
        DEFW RESET ; RESET
        DEFW BRANCH ; BRANCH
        DEFW F_QUIT_L40
F_QUIT_L39:
        DEFW LIT,31992
        DEFW FETCH ; @
        DEFW ZEQU ; 0=
        DEFW QBRAN ; 0BRANCH
        DEFW F_QUIT_L41
        DEFW XSQ
        DEFB 2
        DEFM "OK"
        DEFW TYPE ; TYPE
F_QUIT_L41:
F_QUIT_L40:
        DEFW F_CR ; CR
        DEFW BRANCH ; BRANCH
        DEFW F_QUIT_L37
        DEFW EXIT ; EXIT

; : ABORT
H_F_ABORT: DEFW H_F_QUIT
        DEFB 5
        DEFM "ABORT"
F_ABORT: CALL DOCOL
        DEFW XSQ
        DEFB 7
        DEFM "STACK ?"
        DEFW TYPE ; TYPE
        DEFW F_ERROR ; ERROR
        DEFW F_CR ; CR
        DEFW F_QUIT ; QUIT
        DEFW EXIT ; EXIT

; : FORTH
H_F_FORTH: DEFW H_F_ABORT
        DEFB 5
        DEFM "FORTH"
F_FORTH: CALL DOCOL
        DEFW LIT,SLUT
        DEFW LIT,31994
        DEFW STORE ; !
        DEFW LIT,LATEST
        DEFW LIT,31996
        DEFW STORE ; !
        DEFW LIT,0
        DEFW LIT,31992
        DEFW STORE ; !
        DEFW F_QUIT ; QUIT
        DEFW EXIT ; EXIT

; : S,
H_F_S_2C: DEFW H_F_FORTH
        DEFB 2
        DEFM "S,"
F_S_2C: CALL DOCOL
F_S_2C_L42:
        DEFW DUP ; DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F_S_2C_L43
        DEFW OVER ; OVER
        DEFW CFETCH ; C@
        DEFW F_C_2C ; C,
        DEFW ONEM ; 1-
        DEFW SWAP ; SWAP
        DEFW ONEP ; 1+
        DEFW SWAP ; SWAP
        DEFW BRANCH ; BRANCH
        DEFW F_S_2C_L42
F_S_2C_L43:
        DEFW DROP ; DROP
        DEFW DROP ; DROP
        DEFW EXIT ; EXIT

; : HEADER
H_F_HEADER: DEFW H_F_S_2C
        DEFB 6
        DEFM "HEADER"
F_HEADER: CALL DOCOL
        DEFW F_HERE ; HERE
        DEFW LIT,31998
        DEFW STORE ; !
        DEFW LIT,31996
        DEFW FETCH ; @
        DEFW F__2C ; ,
        DEFW F_PARSE_2DNAME ; PARSE-NAME
        DEFW DUP ; DUP
        DEFW F_C_2C ; C,
        DEFW F_S_2C ; S,
        DEFW EXIT ; EXIT

; : REVEAL
H_F_REVEAL: DEFW H_F_HEADER
        DEFB 6
        DEFM "REVEAL"
F_REVEAL: CALL DOCOL
        DEFW LIT,31998
        DEFW FETCH ; @
        DEFW LIT,31996
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : IMMEDIATE
H_F_IMMEDIATE: DEFW H_F_REVEAL
        DEFB 9
        DEFM "IMMEDIATE"
F_IMMEDIATE: CALL DOCOL
        DEFW LIT,31996
        DEFW FETCH ; @
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW DUP ; DUP
        DEFW CFETCH ; C@
        DEFW LIT,128
        DEFW ORW ; OR
        DEFW SWAP ; SWAP
        DEFW CSTORE ; C!
        DEFW EXIT ; EXIT

; : :
H_F__3A: DEFW H_F_IMMEDIATE
        DEFB 1
        DEFM ":"
F__3A: CALL DOCOL
        DEFW F_HEADER ; HEADER
        DEFW LIT,205
        DEFW F_C_2C ; C,
        DEFW LIT,DOCOL
        DEFW F__2C ; ,
        DEFW LIT,-1
        DEFW LIT,31992
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : ; IMMEDIATE
H_F__3B: DEFW H_F__3A
        DEFB 129
        DEFB 59 ; ;
F__3B: CALL DOCOL
        DEFW LIT,EXIT ; ' EXIT
        DEFW F__2C ; ,
        DEFW F_REVEAL ; REVEAL
        DEFW LIT,0
        DEFW LIT,31992
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : VARIABLE
H_F_VARIABLE: DEFW H_F__3B
        DEFB 8
        DEFM "VARIABLE"
F_VARIABLE: CALL DOCOL
        DEFW F_HEADER ; HEADER
        DEFW LIT,205
        DEFW F_C_2C ; C,
        DEFW LIT,DOVAR
        DEFW F__2C ; ,
        DEFW LIT,0
        DEFW F__2C ; ,
        DEFW F_REVEAL ; REVEAL
        DEFW EXIT ; EXIT

; : CONSTANT
H_F_CONSTANT: DEFW H_F_VARIABLE
        DEFB 8
        DEFM "CONSTANT"
F_CONSTANT: CALL DOCOL
        DEFW F_HEADER ; HEADER
        DEFW LIT,205
        DEFW F_C_2C ; C,
        DEFW LIT,DOCON
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW F_REVEAL ; REVEAL
        DEFW EXIT ; EXIT

; : '
H_F__27: DEFW H_F_CONSTANT
        DEFB 1
        DEFM "'"
F__27: CALL DOCOL
        DEFW F_PARSE_2DNAME ; PARSE-NAME
        DEFW F_LOOKUP ; LOOKUP
        DEFW DUP ; DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F__27_L44
        DEFW F__3EXT ; >XT
F__27_L44:
        DEFW EXIT ; EXIT

; : LITERAL IMMEDIATE
H_F_LITERAL: DEFW H_F__27
        DEFB 135
        DEFM "LITERAL"
F_LITERAL: CALL DOCOL
        DEFW LIT,LIT ; ' LIT
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : IF IMMEDIATE
H_F_IF: DEFW H_F_LITERAL
        DEFB 130
        DEFM "IF"
F_IF: CALL DOCOL
        DEFW LIT,QBRAN ; ' 0BRANCH
        DEFW F__2C ; ,
        DEFW F_HERE ; HERE
        DEFW LIT,0
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : THEN IMMEDIATE
H_F_THEN: DEFW H_F_IF
        DEFB 132
        DEFM "THEN"
F_THEN: CALL DOCOL
        DEFW F_HERE ; HERE
        DEFW SWAP ; SWAP
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : ELSE IMMEDIATE
H_F_ELSE: DEFW H_F_THEN
        DEFB 132
        DEFM "ELSE"
F_ELSE: CALL DOCOL
        DEFW LIT,BRANCH ; ' BRANCH
        DEFW F__2C ; ,
        DEFW F_HERE ; HERE
        DEFW LIT,0
        DEFW F__2C ; ,
        DEFW SWAP ; SWAP
        DEFW F_HERE ; HERE
        DEFW SWAP ; SWAP
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : BEGIN IMMEDIATE
H_F_BEGIN: DEFW H_F_ELSE
        DEFB 133
        DEFM "BEGIN"
F_BEGIN: CALL DOCOL
        DEFW F_HERE ; HERE
        DEFW EXIT ; EXIT

; : UNTIL IMMEDIATE
H_F_UNTIL: DEFW H_F_BEGIN
        DEFB 133
        DEFM "UNTIL"
F_UNTIL: CALL DOCOL
        DEFW LIT,QBRAN ; ' 0BRANCH
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : AGAIN IMMEDIATE
H_F_AGAIN: DEFW H_F_UNTIL
        DEFB 133
        DEFM "AGAIN"
F_AGAIN: CALL DOCOL
        DEFW LIT,BRANCH ; ' BRANCH
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : WHILE IMMEDIATE
H_F_WHILE: DEFW H_F_AGAIN
        DEFB 133
        DEFM "WHILE"
F_WHILE: CALL DOCOL
        DEFW LIT,QBRAN ; ' 0BRANCH
        DEFW F__2C ; ,
        DEFW F_HERE ; HERE
        DEFW LIT,0
        DEFW F__2C ; ,
        DEFW SWAP ; SWAP
        DEFW EXIT ; EXIT

; : REPEAT IMMEDIATE
H_F_REPEAT: DEFW H_F_WHILE
        DEFB 134
        DEFM "REPEAT"
F_REPEAT: CALL DOCOL
        DEFW LIT,BRANCH ; ' BRANCH
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW F_HERE ; HERE
        DEFW SWAP ; SWAP
        DEFW STORE ; !
        DEFW EXIT ; EXIT

; : DO IMMEDIATE
H_F_DO: DEFW H_F_REPEAT
        DEFB 130
        DEFM "DO"
F_DO: CALL DOCOL
        DEFW LIT,XDO ; ' (DO)
        DEFW F__2C ; ,
        DEFW F_HERE ; HERE
        DEFW EXIT ; EXIT

; : LOOP IMMEDIATE
H_F_LOOP: DEFW H_F_DO
        DEFB 132
        DEFM "LOOP"
F_LOOP: CALL DOCOL
        DEFW LIT,XLOOP ; ' (LOOP)
        DEFW F__2C ; ,
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : ." IMMEDIATE
H_F__2E_22: DEFW H_F_LOOP
        DEFB 130
        DEFB 46,34 ; ."
F__2E_22: CALL DOCOL
        DEFW LIT,32120
        DEFW FETCH ; @
        DEFW ONEP ; 1+
        DEFW LIT,32120
        DEFW STORE ; !
        DEFW LIT,XSQ ; ' (S")
        DEFW F__2C ; ,
        DEFW LIT,34
        DEFW F_PARSE ; PARSE
        DEFW DUP ; DUP
        DEFW F_C_2C ; C,
        DEFW F_S_2C ; S,
        DEFW LIT,TYPE ; ' TYPE
        DEFW F__2C ; ,
        DEFW EXIT ; EXIT

; : WORDS
H_F_WORDS: DEFW H_F__2E_22
        DEFB 5
        DEFM "WORDS"
F_WORDS: CALL DOCOL
        DEFW LIT,31996
        DEFW FETCH ; @
F_WORDS_L45:
        DEFW DUP ; DUP
        DEFW QBRAN ; 0BRANCH
        DEFW F_WORDS_L46
        DEFW DUP ; DUP
        DEFW LIT,3
        DEFW PLUS ; +
        DEFW OVER ; OVER
        DEFW LIT,2
        DEFW PLUS ; +
        DEFW CFETCH ; C@
        DEFW LIT,127
        DEFW ANDW ; AND
        DEFW TYPE ; TYPE
        DEFW F_SPACE ; SPACE
        DEFW FETCH ; @
        DEFW BRANCH ; BRANCH
        DEFW F_WORDS_L45
F_WORDS_L46:
        DEFW DROP ; DROP
        DEFW F_CR ; CR
        DEFW EXIT ; EXIT

; Ingångarna: CALL(ORG,n) kör ord n
ENTRY:
        DEFW F_MAIN ; 0 MAIN
        DEFW F_TIO ; 1 TIO
        DEFW F_FIND ; 2 FIND
        DEFW F_FORTH ; 3 FORTH
        DEFW F_QUIT ; 4 QUIT

; Efter ett stackfel (STKERR)
ABORTT: DEFW F_ABORT
LATEST: EQU H_F_WORDS
BILDSLUT:
; Kretsen kan inte skrivas: nya ord läggs i RAM
SLUT:   EQU $C000
