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
EXIT:   LD   E,(IX+0)
        LD   D,(IX+1)
        INC  IX
        INC  IX
        JP   NEXT

;W EXECUTE EXECUTE
EXECUTE: LD  H,B
        LD   L,C
        POP  BC
        JP   (HL)

;W BYE BYE
; Tillbaka till BASIC med TOS som CALL:s värde.
BYE:    LD   H,B
        LD   L,C
        LD   SP,(SAVESP)
        RET

;W LIT LIT
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
QBRAN:  LD   A,B
        OR   C
        POP  BC
        JR   Z,BRANCH
        INC  DE
        INC  DE
        JP   NEXT

;W (DO) XDO
; Returstacken: (IX) = index, (IX+2) = gräns.
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
IDX:     PUSH BC
        LD   C,(IX+0)
        LD   B,(IX+1)
        JP   NEXT

;W UNLOOP UNLOOP
; Ta bort index och gräns, före EXIT inne i en DO-slinga.
UNLOOP: INC  IX
        INC  IX
        INC  IX
        INC  IX
        JP   NEXT

;W >R TOR
TOR:    DEC  IX
        LD   (IX+0),B
        DEC  IX
        LD   (IX+0),C
        POP  BC
        JP   NEXT

;W R> RFROM
RFROM:  PUSH BC
        LD   C,(IX+0)
        LD   B,(IX+1)
        INC  IX
        INC  IX
        JP   NEXT

;W R@ RFETCH IDX
RFETCH: JP   IDX

;W DUP DUP
DUP:    PUSH BC
        JP   NEXT

;W DROP DROP
DROP:   POP  BC
        JP   NEXT

;W SWAP SWAP
SWAP:   POP  HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W OVER OVER
OVER:   POP  HL
        PUSH HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W ROT ROT
ROT:    POP  HL
        EX   (SP),HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W + PLUS
PLUS:   POP  HL
        ADD  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W - MINUS
MINUS:  POP  HL
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W AND ANDW
ANDW:    POP  HL
        LD   A,C
        AND  L
        LD   C,A
        LD   A,B
        AND  H
        LD   B,A
        JP   NEXT

;W OR ORW
ORW:    POP  HL
        LD   A,C
        OR   L
        LD   C,A
        LD   A,B
        OR   H
        LD   B,A
        JP   NEXT

;W NEGATE NEGATE
NEGATE: LD   HL,0
        OR   A
        SBC  HL,BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W 1+ ONEP
ONEP:   INC  BC
        JP   NEXT

;W 1- ONEM
ONEM:   DEC  BC
        JP   NEXT

;W 2* TWOST
TWOST:  SLA  C
        RL   B
        JP   NEXT

;W 0= ZEQU
ZEQU:   LD   A,B
        OR   C
        LD   BC,0
        JP   NZ,NEXT
        DEC  BC             ; sant = -1
        JP   NEXT

;W 0< ZLESS
ZLESS:  LD   A,B
        ADD  A,A            ; teckenbiten till CY
        SBC  A,A
        LD   B,A
        LD   C,A
        JP   NEXT

;W = EQUAL
EQUAL:    POP  HL
        OR   A
        SBC  HL,BC
        LD   BC,0
        JP   NZ,NEXT
        DEC  BC
        JP   NEXT

;W < LESS
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
ULESS:  POP  HL
        OR   A
        SBC  HL,BC
        SBC  A,A
        LD   B,A
        LD   C,A
        JP   NEXT

;W @ FETCH
FETCH:  LD   H,B
        LD   L,C
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JP   NEXT

;W ! STORE
STORE:  LD   H,B
        LD   L,C
        POP  BC
        LD   (HL),C
        INC  HL
        LD   (HL),B
        POP  BC
        JP   NEXT

;W C@ CFETCH
CFETCH: LD   A,(BC)
        LD   C,A
        LD   B,0
        JP   NEXT

;W C! CSTORE
CSTORE: LD   H,B
        LD   L,C
        POP  BC
        LD   (HL),C
        POP  BC
        JP   NEXT

;W (S") XSQ
; Texten står i tråden: en längdbyte och tecknen. Ger adress och längd.
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
DOVAR:  POP  HL
        PUSH BC
        LD   B,H
        LD   C,L
        JP   NEXT

;W (CON) DOCON
; CONSTANT: CALL DOCON följt av värdet.
DOCON:  POP  HL
        PUSH BC
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JP   NEXT

;W SAME SAME
; ( a1 a2 n -- f ) Sant om de n bytena på a1 och a2 är lika.
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
RESET:  LD   SP,(S0)
        LD   HL,CANARY
        EX   (SP),HL
        LD   BC,0
        LD   IX,(SAVESP)
        JP   NEXT

;W EMIT EMIT
; Tecknet läggs på stacken och skrivs med ROM:ens PRTXT.
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
TYPE:   POP  HL
        PUSH DE
        CALL PRTXT
        POP  DE
        POP  BC
        JP   NEXT

;W KEY KEY
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
