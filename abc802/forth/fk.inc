; Forth för ABC 802. Kärnan är bokens exempel 7 (ABC80, band 2,
; e7k.inc), flyttad till RAM på $8000 och anpassad till BASIC II för
; ABC 802 och DOS:en.
;
; Kärnan, direkt trådad (DTC). Korskompilatorn ffc.py läser den här
; filen och Forth-källorna och skriver forth.asm. Allt före den första
; ;W-raden kommer alltid med; varje ;W-rad börjar ett ord:
;   ;W namn ETIKETT [ord som det behöver ...]
; och ordet tas med bara om något program använder det.
;
; Registren:
;   SP  datastacken (Z80-stacken)     BC  översta värdet (TOS)
;   IX  returstacken, växer nedåt     DE  IP, nästa adress i tråden
;   HL  arbetsregister                IY  rörs inte (ROM:ens, $FF00)
;
; Ett ord är maskinkod. Ett kolonord börjar med CALL DOCOL och följs
; av adresserna till de ord det består av.
;
; Från BASIC: Z=CALL(32768,n) kör ingång n (0 om n saknas). Det värde
; som ligger överst på stacken när ordet slutar blir CALL:s värde.
;
; Minnet: kärnan och ordlistan på $8000-$AFFF. LADDAD, som DOS:ens
; DRUN hoppar till när filen är inläst, flyttar BASIC:s BOTRAM till
; UPP, så att COMMON, programmet och variablerna ligger ovanför.
; Radbufferten TIB är de 80 bytena under UPP.

PRTXT:  EQU  $000B          ; ROM: skriv BC tecken från HL
GETKEY: EQU  $0002          ; ROM: vänta på tangent, A = tecknet
INLINE: EQU  $0005          ; ROM: läs en rad till HL, högst C tecken
HEAP:   EQU  $FF0A          ; BASIC: FREPTR, första lediga byte efter variablerna
BOTRAM: EQU  $FF0C          ; BASIC: första RAM-adressen för BASIC
COMEND: EQU  $FF30          ; BASIC: COMMON-områdets slut
VARBEG: EQU  $FF2E          ; BASIC: variablernas början
CANARY: EQU  $5AA5          ; värdet mellan returstacken och datastacken
UPP:    EQU  $B800          ; BASIC:s nya BOTRAM

        ORG  $8000

        JP   COLD
SAVESP: DEFW 0              ; BASIC:s SP
S0:     DEFW 0              ; datastackens botten = kanariefågelns adress

; Filens startadress: DRUN hoppar hit när FORTH.ABS är inläst.
; Variablerna börjar också på UPP; NEW efteråt ordnar resten.
LADDAD: LD   HL,UPP
        LD   (BOTRAM),HL
        LD   (COMEND),HL
        LD   (HEAP),HL
        LD   (VARBEG),HL
        LD   HL,0
        RET

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
        LD   A,D            ; CALL(32768,VAROOT(A$)): en adress, inte
        OR   A              ; ett ingångsnummer. Den läggs på stacken,
        JR   Z,COLD1        ; och ingång 3, (STR), tolkar strängen.
        PUSH DE
        LD   DE,3
COLD1:  LD   HL,ENTRY       ; ingångstabellen, skrivs av ffc.py
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
; tråden ABORTT, som ffc.py skriver: tolkens ABORT, annars BYE med
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

;W DEPTH DEPTH
; ( -- n ) Antal tal på datastacken. Tom stack: SP = S0 efter PUSH BC
; ger S0 - SP = 2.
DEPTH:  PUSH BC
        LD   HL,(S0)
        OR   A
        SBC  HL,SP
        SRL  H
        RR   L
        DEC  HL
        LD   B,H
        LD   C,L
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
; Siffrorna skrivs bakifrån i NUMBUF (delat med 10), sedan ett
; blanktecken, och bufferten med PRTXT.
UDOT:   PUSH DE
        LD   H,B
        LD   L,C
        LD   DE,NUMBUF+6
        DEC  DE
        LD   A,' '
        LD   (DE),A
UDOT1:  CALL DIV10          ; HL = HL / 10, A = resten
        ADD  A,'0'
        DEC  DE
        LD   (DE),A
        LD   A,H
        OR   L
        JR   NZ,UDOT1
        LD   HL,NUMBUF+6
        OR   A
        SBC  HL,DE
        LD   B,H
        LD   C,L            ; BC = längden
        EX   DE,HL
        CALL PRTXT
        POP  DE
        POP  BC
        JP   NEXT

DIV10:  LD   B,16
        XOR  A
DIV101: ADD  HL,HL
        RLA
        CP   10
        JR   C,DIV102
        SUB  10
        INC  L
DIV102: DJNZ DIV101
        RET

NUMBUF: DEFB 0,0,0,0,0,0

; Orden nedan är nya för ABC 802. *, U/MOD, J och CMOVE är tagna ur
; forthaid (abc80/utanfor/forthaid/fak.inc).

;W * STAR
; ( n1 n2 -- n3 ) De låga 16 bitarna av produkten.
STAR:   POP  HL
        PUSH DE
        EX   DE,HL          ; DE = n1
        LD   HL,0
        LD   A,16
STAR1:  ADD  HL,HL
        SLA  C
        RL   B
        JR   NC,STAR2
        ADD  HL,DE
STAR2:  DEC  A
        JR   NZ,STAR1
        POP  DE
        LD   B,H
        LD   C,L
        JP   NEXT

;W U/MOD UDIVM
; ( u1 u2 -- rest kvot ) Utan tecken. Med u2 = 0 blir kvoten $FFFF och
; resten u1.
UDIVM:  POP  HL             ; u1
        PUSH DE             ; IP
        LD   D,B
        LD   E,C            ; DE = u2
        CALL DIV16
        POP  DE
        PUSH HL             ; resten
        JP   NEXT

; DIV16: HL / DE utan tecken, BC = kvoten, HL = resten. Ändrar A.
DIV16:  LD   A,H
        LD   C,L            ; AC = u1, blir kvoten
        LD   HL,0           ; resten
        LD   B,16
DIV1:   SLA  C
        RLA
        ADC  HL,HL
        JR   C,DIV2         ; 17 bitar: större än u2
        SBC  HL,DE          ; CY = 0 här
        JR   NC,DIV3
        ADD  HL,DE          ; gick inte: tillbaka
        JR   DIV4
DIV2:   CCF                 ; CY = 0
        SBC  HL,DE
DIV3:   INC  C              ; en etta i kvoten
DIV4:   DJNZ DIV1
        LD   B,A
        RET

NEGHL:  XOR  A
        SUB  L
        LD   L,A
        SBC  A,A
        SUB  H
        LD   H,A
        RET

;W /MOD SDIVM U/MOD
; ( n1 n2 -- rest kvot ) Med tecken: kvoten avrundas mot noll och
; resten får täljarens tecken, som BASIC:s heltal.
SDIVM:  POP  HL             ; n1
        PUSH DE
        LD   A,H
        XOR  B
        PUSH AF             ; bit 7: kvotens tecken
        LD   A,H
        PUSH AF             ; bit 7: restens tecken
        BIT  7,H
        CALL NZ,NEGHL
        EX   DE,HL
        LD   H,B
        LD   L,C
        BIT  7,H
        CALL NZ,NEGHL
        EX   DE,HL          ; HL = |n1|, DE = |n2|
        CALL DIV16
        POP  AF
        ADD  A,A
        CALL C,NEGHL
        POP  AF
        ADD  A,A
        JR   NC,SDIV1
        XOR  A
        SUB  C
        LD   C,A
        SBC  A,A
        SUB  B
        LD   B,A
SDIV1:  POP  DE
        PUSH HL
        JP   NEXT

;W XOR XORW
XORW:   POP  HL
        LD   A,C
        XOR  L
        LD   C,A
        LD   A,B
        XOR  H
        LD   B,A
        JP   NEXT

;W PICK PICK
; ( xu ... x0 u -- xu ... x0 xu ) 0 PICK = DUP.
PICK:   LD   H,B
        LD   L,C
        ADD  HL,HL
        ADD  HL,SP
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        JP   NEXT

;W 2DUP TWODUP
TWODUP: POP  HL
        PUSH HL
        PUSH BC
        PUSH HL
        JP   NEXT

;W 2DROP TWODRP
TWODRP: POP  BC
        POP  BC
        JP   NEXT

;W LEAVE LEAVE
; Indexet blir gränsen - 1, så att slingan slutar vid nästa LOOP
; (resten av varvet körs).
LEAVE:  LD   L,(IX+2)
        LD   H,(IX+3)
        DEC  HL
        LD   (IX+0),L
        LD   (IX+1),H
        JP   NEXT

;W (+LOOP) XPLOOP BRANCH
; ( n -- ) Steget n läggs till indexet. Med n >= 0 slutar slingan när
; indexet >= gränsen, med n < 0 när det är < gränsen (med tecken).
XPLOOP: PUSH DE
        LD   L,(IX+0)
        LD   H,(IX+1)
        ADD  HL,BC
        LD   (IX+0),L
        LD   (IX+1),H
        LD   E,(IX+2)
        LD   D,(IX+3)
        OR   A
        SBC  HL,DE          ; mindre än gränsen: S xor V
        LD   A,B            ; stegets tecken
        POP  DE
        POP  BC
        JP   PE,XPL1
        JP   M,XPLT
        JR   XPGE
XPL1:   JP   P,XPLT
XPGE:   BIT  7,A            ; index >= gränsen: klart om steget >= 0
        JP   NZ,BRANCH
        JR   XPEND
XPLT:   BIT  7,A            ; index < gränsen: klart om steget < 0
        JP   Z,BRANCH
XPEND:  LD   HL,4
        EX   DE,HL
        ADD  IX,DE
        EX   DE,HL
        INC  DE
        INC  DE
        JP   NEXT

;W P@ PFETCH
; ( port -- byte ) IN från porten.
PFETCH: IN   C,(C)
        LD   B,0
        JP   NEXT

;W P! PSTORE
; ( byte port -- ) OUT till porten.
PSTORE: POP  HL
        OUT  (C),L
        POP  BC
        JP   NEXT

;W J JDX
; Den yttre DO-slingans index (som I, fyra byte längre upp).
JDX:    PUSH BC
        LD   C,(IX+4)
        LD   B,(IX+5)
        JP   NEXT

;W CMOVE CMOVE
; ( från till n -- ) n byte framåt, med LDIR; till får ligga före från.
CMOVE:  EX   DE,HL          ; HL = IP
        POP  DE             ; till
        EX   (SP),HL        ; HL = från, IP på stacken
        LD   A,B
        OR   C
        JR   Z,CMOV1
        LDIR
CMOV1:  POP  DE
        POP  BC
        JP   NEXT

;W CMOVE> CMOVEU
; ( från till n -- ) n byte bakåt från slutet, med LDDR; till får
; ligga efter från.
CMOVEU: EX   DE,HL          ; HL = IP
        POP  DE             ; till
        EX   (SP),HL        ; HL = från, IP på stacken
        LD   A,B
        OR   C
        JR   Z,CMOVU1
        DEC  BC
        ADD  HL,BC          ; från + n - 1
        EX   DE,HL
        ADD  HL,BC          ; till + n - 1
        EX   DE,HL
        INC  BC
        LDDR
CMOVU1: POP  DE
        POP  BC
        JP   NEXT

;W FILL FILL
; ( adr n c -- ) n byte med värdet c.
FILL:   LD   A,C            ; c
        POP  BC             ; n
        POP  HL             ; adr
FILL1:  INC  B              ; BC = 0 ger inget
        DEC  B
        JR   NZ,FILL2
        INC  C
        DEC  C
        JR   Z,FILL3
FILL2:  LD   (HL),A
        INC  HL
        DEC  BC
        JR   FILL1
FILL3:  POP  BC
        JP   NEXT

;W SCAN SCAN
; ( adr n c -- adr2 n2 ) Första c bland n byte från adr, med CPIR:
; adr2 är dess adress och n2 det som är kvar från den. Inget c: adr2 =
; adr + n och n2 = 0.
SCAN:   LD   A,C
        LD   (SCANC),A
        POP  BC             ; n
        POP  HL             ; adr
        LD   A,B
        OR   C
        JR   Z,SCAN9
        LD   A,(SCANC)
        CPIR
        JR   NZ,SCAN9
        DEC  HL
        INC  BC
SCAN9:  PUSH HL
        JP   NEXT
SCANC:  DEFB 0

;W -SCAN MSCAN SCAN
; ( adr n c -- n2 ) Sista c bland n byte från adr, med CPDR: n2 är
; antalet byte till och med det, 0 om inget c finns. Ordbrytningen:
; adr 81 32 -SCAN ger radens längd till och med sista blanksteget.
MSCAN:  LD   A,C
        LD   (SCANC),A
        POP  BC             ; n
        POP  HL             ; adr
        LD   A,B
        OR   C
        JR   Z,MSCAN9
        ADD  HL,BC
        DEC  HL             ; adr + n - 1
        LD   A,(SCANC)
        CPDR
        JR   NZ,MSCAN9
        INC  BC
MSCAN9: JP   NEXT

; RAM-skivan: de undre 32 KB RAM, $0000-$77FF (30 KB), som MEM: i BASIC
; använder. ROM:ens LDIR7F ($7FFD) är LDIR och RET sist i ROM:en; medan
; instruktionen hämtas där syns RAM:et på $0000-$77FF i stället för
; ROM:en. Från och till får vara
; var som helst utom $7800-$7FFF (bildminnet), som då är ROM.

LDIR7F: EQU  $7FFD

;W RMOVE RMOVE
; ( från till n -- ) Som CMOVE, men $0000-$77FF är RAM-skivan.
RMOVE:  EX   DE,HL          ; HL = IP
        POP  DE             ; till
        EX   (SP),HL        ; HL = från, IP på stacken
        LD   A,B
        OR   C
        CALL NZ,LDIR7F
        POP  DE
        POP  BC
        JP   NEXT

;W RC@ RCFET
; ( adr -- c ) En byte från RAM-skivan.
RCFET:  PUSH DE
        LD   H,B
        LD   L,C
        LD   DE,RTMP
        LD   BC,1
        CALL LDIR7F
        POP  DE
        LD   A,(RTMP)
        LD   C,A
        LD   B,0
        JP   NEXT
RTMP:   DEFB 0

;W RC! RCSTO RC@
; ( c adr -- ) En byte till RAM-skivan.
RCSTO:  POP  HL             ; c
        LD   A,L
        LD   (RTMP),A
        PUSH DE
        LD   D,B
        LD   E,C
        LD   HL,RTMP
        LD   BC,1
        CALL LDIR7F
        POP  DE
        POP  BC
        JP   NEXT

; Grafiken: mosaikpunkter som SET DOT och CLR DOT i BASIC II, rad y
; 0-71 uppifrån och kolumn x 0-77 (0-157 med 80 tecken). Första rutan
; på raden är grafikkodens (GR i f.fs), så x räknas två punkter in, som
; i TXPOINT. ROM:ens DOTADR ger cellens adress och punktens bit; en
; punkt utanför skärmen hoppas över (BASIC ger fel 176).

DOTADR: EQU  $04C0          ; ROM: BC = rad, HL = kolumn -> HL, A
CONWID: EQU  $FF54          ; CON: radlängd (40 eller 80)

;W PLOT PLOT
; ( x y -- ) Tänd punkten.
PLOT:   POP  HL             ; x, BC = y
        CALL DOTOK
        JR   C,PLOT9
        PUSH DE
        CALL DOTADR
        DEFB $00            ; rad och kolumn uppifrån
        OR   (HL)
        OR   $20
        LD   (HL),A
        POP  DE
PLOT9:  POP  BC
        JP   NEXT

; DOTOK: CY om rad BC eller kolumn HL är utanför skärmen; annars
; HL = kolumnen + 2.
DOTOK:  LD   A,B
        OR   H
        SCF
        RET  NZ
        LD   A,L
        ADD  A,2
        RET  C
        LD   L,A
        LD   A,C
        CP   72
        CCF
        RET  C
        LD   A,(CONWID)
        ADD  A,A
        DEC  A
        CP   L              ; CY om kolumnen > 2 x radlängd - 1
        RET

;W UNPLOT UNPLOT PLOT
; ( x y -- ) Släck punkten.
UNPLOT: POP  HL
        CALL DOTOK
        JR   C,PLOT9
        PUSH DE
        CALL DOTADR
        DEFB $00
        CPL
        AND  (HL)
        OR   $20
        LD   (HL),A
        POP  DE
        JR   PLOT9

;W POINT? POINTQ PLOT
; ( x y -- f ) Sant om punkten är tänd (falskt utanför skärmen).
POINTQ: POP  HL
        CALL DOTOK
        JR   C,POINT9
        PUSH DE
        CALL DOTADR
        DEFB $00
        AND  (HL)
        AND  $DF
        POP  DE
        LD   BC,0
        JP   Z,NEXT
        DEC  BC
        JP   NEXT
POINT9: LD   BC,0
        JP   NEXT

;W (FLT) FLT
; Flyttalen: BASIC II:s rutiner i ROM:en, med DOUBLE, 8 bytes. Först
; exponenten (+$80; 0 = talet 0), sedan tecknet i bit 7 och mantissan
; (ettan underförstådd), högsta byten först. Talen ligger på en egen
; stack, FSTK, som växer nedåt: FSP pekar på det översta talet och det
; under ligger 8 bytes högre, som på Z80-stacken. Rutinerna vill ha
; talen där, under returadressen, så två tal flyttas dit med en LDIR och
; resultatet tillbaka med en till. Under anropet är talens längd
; ($FF1E, IY+$1E) 8, efteråt den BASIC hade (4 med SINGLE). Rutinerna
; ändrar IX (returstacken) och de alternativa registren, så BC, DE och
; IX sparas i minnet (FENTER, FLEAVE).
;
; Spill, division med 0 och en flyttalsstack som är tom eller full ger
; FERR <> 0. Ordet gör då ingenting, eller lämnar det ROM:en ger
; (FTOIR: 32767); FERR nollas inte av orden, utan av den som läser den.

FADD:   EQU  $4F41          ; ROM: + (8 bytes: DADD)
FSUB:   EQU  $4F39          ; ROM: - (tecknet byts, sedan FADD)
FMUL:   EQU  $4FEA          ; ROM: * (DMUL)
FDIV:   EQU  $505F          ; ROM: / (DDIV)
FCOMP:  EQU  $4C61          ; ROM: jämför, CY = mindre, Z = lika
ITOFS:  EQU  $4CC0          ; ROM: heltal -> flyttal
FTOIR:  EQU  $4CF3          ; ROM: flyttal -> heltal, avrundat
TXTNUM: EQU  $47F3          ; ROM: text (HL) -> flyttal (DE)
FTODEC: EQU  $5153          ; ROM: flyttal -> decimal form
FLEN:   EQU  $FF1E          ; IY+$1E: flyttalens längd, 4 eller 8

; (FLT) gör inget; här ligger det som F-orden delar.

FLT:    JP   NEXT

FSTK:   DEFS 128            ; 16 tal
FSTKE:
FSP:    DEFW FSTKE
FERRV:  DEFW 0
FSBC:   DEFW 0
FSDE:   DEFW 0
FSIX:   DEFW 0
FSSP:   DEFW 0
FSLEN:  DEFB 0
FMASK:  DEFB 0
FROUT:  DEFW 0
FTXT:   DEFS 32             ; texten till >FLOAT
FDEC:   DEFS 24             ; talet i decimal form, ur (F.)

FENTER: LD   (FSBC),BC
        LD   (FSDE),DE
        LD   (FSIX),IX
        LD   A,(FLEN)
        LD   (FSLEN),A
        LD   A,8
        LD   (FLEN),A
        RET

FLEAVE: LD   A,(FSLEN)
        LD   (FLEN),A
        LD   BC,(FSBC)
        LD   DE,(FSDE)
        LD   IX,(FSIX)
        JP   NEXT

; FNEED: minst A bytes (8 eller 16) på flyttalsstacken; HL = FSP.
; FROOM: plats för ett tal till; HL = FSP. Annars FERR, och ordet slutar.
FNEED:  LD   HL,FSTKE
        LD   DE,(FSP)
        OR   A
        SBC  HL,DE          ; HL = bytes på stacken
        LD   E,A
        LD   D,0
        SBC  HL,DE
        JR   C,FSTKF
        LD   HL,(FSP)
        RET
FROOM:  LD   HL,(FSP)
        LD   DE,FSTK+8
        OR   A
        SBC  HL,DE
        JR   C,FSTKF
        LD   HL,(FSP)
        RET
FSTKF:  POP  HL             ; ordets återhopp bort
        CALL FERR1
        JP   FLEAVE

; FERR1: ett fel.
FERR1:  LD   HL,-1
        LD   (FERRV),HL
        RET

; FCALL: kör rutinen i FROUT, med en returadress på stacken.
FCALL:  LD   HL,(FROUT)
        JP   (HL)

;W F+ FPLUS (FLT)
; ( F: r1 r2 -- r3 ) r1 + r2. F- F* F/ på samma sätt (F2OP).
FPLUS:  LD   HL,FADD
F2OP:   CALL FENTER
        LD   (FROUT),HL
        LD   A,16
        CALL FNEED          ; HL = FSP
        EX   DE,HL
        LD   HL,-16
        ADD  HL,SP
        LD   SP,HL
        EX   DE,HL          ; båda talen till Z80-stacken
        LD   BC,16
        LDIR
        CALL FCALL
        CALL C,FERR1        ; CY: spill (eller division med 0)
        LD   HL,(FSP)       ; resultatet i det undres plats
        LD   DE,8
        ADD  HL,DE
        LD   (FSP),HL
        EX   DE,HL
        LD   HL,0
        ADD  HL,SP
        LD   BC,8
        LDIR
        LD   SP,HL
        JP   FLEAVE

;W F- FMINUS F+
FMINUS: LD   HL,FSUB
        JP   F2OP

;W F* FSTAR F+
FSTAR:  LD   HL,FMUL
        JP   F2OP

;W F/ FSLASH F+
FSLASH: LD   HL,FDIV
        JP   F2OP

;W F< FLESS (FLT)
; ( F: r1 r2 -- ) ( -- f ) Sant om r1 < r2. F= på samma sätt med
; Z-flaggan i stället för CY (FCMPW).
FLESS:  LD   H,$01          ; CY
FCMPW:  CALL FENTER
        LD   A,H
        LD   (FMASK),A
        LD   A,16
        CALL FNEED
        EX   DE,HL
        LD   HL,-16
        ADD  HL,SP
        LD   SP,HL
        EX   DE,HL
        LD   BC,16
        LDIR
        CALL FCOMP          ; talen ligger kvar
        PUSH AF
        POP  HL             ; L = flaggorna
        LD   A,(FMASK)
        AND  L
        LD   BC,0
        JR   Z,FCMPW1
        DEC  BC
FCMPW1: LD   HL,16
        ADD  HL,SP
        LD   SP,HL
        LD   HL,(FSP)
        LD   DE,16
        ADD  HL,DE
        LD   (FSP),HL
FPUSHB: LD   HL,(FSBC)      ; det gamla TOS ned, BC som nytt TOS
        PUSH HL
        LD   (FSBC),BC
        JP   FLEAVE

;W F= FEQU F<
FEQU:   LD   H,$40          ; Z
        JP   FCMPW

;W FDROP FDROP (FLT)
; ( F: r -- )
FDROP:  CALL FENTER
        LD   A,8
        CALL FNEED
        LD   DE,8
        ADD  HL,DE
        LD   (FSP),HL
        JP   FLEAVE

;W FDUP FDUP (FLT)
; ( F: r -- r r )
FDUP:   CALL FENTER
        LD   A,8
        CALL FNEED
        CALL FROOM          ; HL = FSP
        PUSH HL
        LD   DE,-8
        ADD  HL,DE
        LD   (FSP),HL
        EX   DE,HL
        POP  HL
        LD   BC,8
        LDIR
        JP   FLEAVE

;W FOVER FOVER (FLT)
; ( F: r1 r2 -- r1 r2 r1 )
FOVER:  CALL FENTER
        LD   A,16
        CALL FNEED
        CALL FROOM
        PUSH HL
        LD   DE,-8
        ADD  HL,DE
        LD   (FSP),HL
        EX   DE,HL
        POP  HL
        LD   BC,8
        ADD  HL,BC
        LDIR
        JP   FLEAVE

;W FSWAP FSWAP (FLT)
; ( F: r1 r2 -- r2 r1 )
FSWAP:  CALL FENTER
        LD   A,16
        CALL FNEED
        LD   D,H
        LD   E,L
        LD   BC,8
        EX   DE,HL
        ADD  HL,BC
        EX   DE,HL          ; DE = FSP + 8
        LD   B,8
FSWAP1: LD   A,(DE)
        LD   C,(HL)
        LD   (HL),A
        LD   A,C
        LD   (DE),A
        INC  HL
        INC  DE
        DJNZ FSWAP1
        JP   FLEAVE

;W FNEGATE FNEG (FLT)
; ( F: r -- -r ) Tecknet byts, utom på 0.
FNEG:   CALL FENTER
        LD   A,8
        CALL FNEED
        LD   A,(HL)
        OR   A
        JP   Z,FLEAVE
        INC  HL
        LD   A,(HL)
        XOR  $80
        LD   (HL),A
        JP   FLEAVE

;W FABS FABS (FLT)
; ( F: r -- |r| )
FABS:   CALL FENTER
        LD   A,8
        CALL FNEED
        INC  HL
        RES  7,(HL)
        JP   FLEAVE

;W F@ FFETCH (FLT)
; ( a -- ) ( F: -- r ) De 8 bytena på a.
FFETCH: CALL FENTER
        CALL FROOM
        LD   DE,-8
        ADD  HL,DE
        LD   (FSP),HL
        EX   DE,HL
        LD   HL,(FSBC)
        LD   BC,8
        LDIR
FPOPB:  POP  HL             ; nästa tal blir TOS
        LD   (FSBC),HL
        JP   FLEAVE

;W F! FSTORE F@
; ( a -- ) ( F: r -- ) Talet till de 8 bytena på a.
FSTORE: CALL FENTER
        LD   A,8
        CALL FNEED
        LD   DE,(FSBC)
        LD   BC,8
        LDIR
        LD   (FSP),HL
        JP   FPOPB

;W S>F STOF F@
; ( n -- ) ( F: -- r ) Heltalet med tecken.
STOF:   CALL FENTER
        CALL FROOM
        LD   HL,(FSBC)
        PUSH HL
        CALL ITOFS          ; talet på Z80-stacken
        LD   HL,(FSP)
        LD   DE,-8
        ADD  HL,DE
        LD   (FSP),HL
        EX   DE,HL
        LD   HL,0
        ADD  HL,SP
        LD   BC,8
        LDIR
        LD   SP,HL
        JP   FPOPB

;W F>S FTOS F<
; ( -- n ) ( F: r -- ) Avrundat; utanför -65535..65535 32767 och FERR.
; Från 32768 blir det ett tal utan tecken (som U.).
FTOS:   CALL FENTER
        LD   A,8
        CALL FNEED
        EX   DE,HL
        LD   HL,-8
        ADD  HL,SP
        LD   SP,HL
        EX   DE,HL
        LD   BC,8
        LDIR
        LD   (FSP),HL
        CALL FTOIR
        CALL C,FERR1
        POP  BC
        JP   FPUSHB

;W >FLOAT TOFLT F<
; ( a n -- f ) ( F: -- r om f ) Texten som tal, som INPUT läser det;
; falskt om den inte är ett tal (högst 30 tecken) eller stacken är full.
TOFLT:  CALL FENTER
        POP  HL             ; a
        LD   (FSBC),HL      ; ... till svaret: f ersätter a och n
        LD   A,B
        OR   A
        JR   NZ,TOFLT9
        LD   A,C
        OR   A
        JR   Z,TOFLT9
        CP   31
        JR   NC,TOFLT9
        LD   HL,(FSP)
        LD   DE,FSTK+8
        OR   A
        SBC  HL,DE
        JR   C,TOFLT9
        LD   HL,(FSBC)
        LD   DE,FTXT
        LDIR
        LD   A,13
        LD   (DE),A
        LD   (FSSP),SP
        LD   HL,(FSP)
        LD   DE,-8
        ADD  HL,DE
        EX   DE,HL          ; DE = det nya talets plats
        LD   HL,FTXT
        CALL TXTNUM         ; HL = efter talet
        LD   SP,(FSSP)
        JR   C,TOFLT9
        LD   A,(HL)
        CP   13             ; hela texten
        JR   NZ,TOFLT9
        LD   HL,(FSP)
        LD   DE,-8
        ADD  HL,DE
        LD   (FSP),HL
        LD   HL,-1
        JR   TOFLT8
TOFLT9: LD   HL,0
TOFLT8: LD   (FSBC),HL
        JP   FLEAVE

;W (F.) PFDOT F<
; ( -- a ) ( F: r -- ) Talet i decimal form på a: antalet siffror (bit
; 7 = tecknet), tiopotensen e (talet = 0.siffrorna * 10^e, -128..127)
; och siffrorna i ASCII, högst 22 och utan nollor sist (FTODEC).
PFDOT:  CALL FENTER
        LD   A,8
        CALL FNEED
        LD   (FSSP),SP
        EX   DE,HL
        LD   HL,-8
        ADD  HL,SP
        LD   SP,HL
        EX   DE,HL
        LD   BC,8
        LDIR
        LD   (FSP),HL
        CALL FTODEC
        LD   HL,0
        ADD  HL,SP
        LD   A,(HL)
        AND  $7F
        ADD  A,2
        LD   C,A
        LD   B,0
        LD   DE,FDEC
        LDIR
        LD   SP,(FSSP)
        LD   BC,FDEC
        JP   FPUSHB

;W FERR FERRW (FLT)
; ( -- a ) Sant (-1) efter ett fel med flyttalen; nollas med 0 FERR !.
FERRW:  PUSH BC
        LD   BC,FERRV
        JP   NEXT

;W FDEPTH FDEPTH (FLT)
; ( -- n ) Antal tal på flyttalsstacken.
FDEPTH: PUSH BC
        LD   HL,FSTKE
        LD   BC,(FSP)
        OR   A
        SBC  HL,BC
        SRL  H
        RR   L
        SRL  H
        RR   L
        SRL  H
        RR   L
        LD   B,H
        LD   C,L
        JP   NEXT

;W FCLEAR FCLEAR (FLT)
; ( F: ... -- ) Töm flyttalsstacken.
FCLEAR: LD   HL,FSTKE
        LD   (FSP),HL
        JP   NEXT
