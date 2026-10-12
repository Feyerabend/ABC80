; comal.asm -- utanför boken: COMAL:s styrstrukturer i ABC80-BASIC:en
; (gamla ROM:en, abc80old.rom) med en krets på $4000-$5FFF.
; bygg.py lägger bytena under $4000 i en kopia av ROM:en och resten i
; kretsen. Se LASMIG.md.
;
; De nya satserna är satser i tabell 2: $86 följd av en kod $96-. ROM:en
; letar upp tabell 2 (STKW2, STTAB2, STEXE2) på fyra ställen, och de
; pekar nu på kopior här med de nya satserna efter de gamla. En sats som
; behöver en adress har en eller två platser $B9 lo hi, ett element som
; SKPELM, LNKREF och DETOK redan hoppar över (FOR-posten).
;
;   WHILE v           $86 $96 v $B9 s $B9 r    s = ENDWHILE, r = dess rad
;   ENDWHILE          $86 $97 $B9 s $B9 r      s = WHILE
;   REPEAT            $86 $98 $B9 0 $B9 0
;   UNTIL v           $86 $99 v $B9 s $B9 r    s = REPEAT
;   IF v THEN         $86 $9A v $BD $B9 s $B9 r   s = nästa ELIF/ELSE/ENDIF
;   ELIF v THEN       $86 $9B $B9 s $B9 r $86 $9F v $BD   s som IF
;   ELSE              $86 $9C $B9 s $B9 r      s = ENDIF
;   ENDIF             $86 $9D
;   PROC namn(a,b)    $86 $A0 $B9 s $B9 r namn ( a , b )   s = ENDPROC
;   ENDPROC           $86 $A1
;   EXEC namn(x,y)    $86 $A2 $B9 s $B9 r namn ( x , y )   s = PROC
;   CASE v OF         $86 $A3 $B9 s $B9 r v OF    s = första WHEN
;   WHEN a,b          $86 $A4 $B9 s $B9 r a , b   s = nästa WHEN,
;                                                 OTHERWISE eller ENDCASE
;   OTHERWISE         $86 $A5 $B9 s $B9 r      s = ENDCASE
;   ENDCASE           $86 $A6
;   LOCAL a,b         $86 $A7 a , b            första satsen efter PROC
;
; Namnet står som tecken, som SKPELM, LNKELM och DETOK tar ett och ett.
; Parametrarna är variabler med $BC/$BD (adressen, som i READ), och
; argumenten uttryck med $B9/$BA (värdet); strängar går inte. EXEC
; lägger argumenten på stacken och byter dem mot parametrarnas värden
; (grund bindning), kör kroppen med RUNLIN som GOSUB och byter tillbaka
; efter ENDPROC, som är RETURN. LOCAL på raden efter PROC är fler
; parametrar, vars argument är noll. PROC i den vanliga följden hoppar förbi
; ENDPROC.
;
; CASE prövar WHEN-delarna i tur och ordning. Koden för "v a =" (med
; $DA där heltal och flyttal blandas) byggs på stacken och räknas av
; RST $38, så att alla tre typerna jämförs som i ROM:en. Stämmer ingen
; och OTHERWISE saknas blir det fel 73. En WHEN eller OTHERWISE i den
; vanliga följden hoppar till efter ENDCASE.
;
; ELIF är två satser: $9B körs när THEN-delen före är klar och hoppar
; till ENDIF, $9F är villkoret, dit ett falskt villkor före hoppar.
; Platserna fylls vid RUN (PRELX, LNKX). Medan en struktur är öppen
; håller dess plats den förra öppna strukturen och den egna raden, så
; att strukturerna bildar en stack utan eget minne; stackens topp står
; i två bytes först på heapen, på EOFA+1.

BOFA    EQU  $FE1C
EOFA    EQU  $FE1E
CURLIN  EQU  $FE2C
STMSP   EQU  $FE36

ERROR   EQU  $062E
KWSRCH  EQU  $09F5
RUNSTM  EQU  $0D92
NODIR   EQU  $0DBF
PRELNK  EQU  $0E6E
LNKSKE  EQU  $0EA6              ; CALL SKPELM / RET C / LNKELM
LNKEXP  EQU  $1032
SKPEXP  EQU  $10C3
KWTHEN  EQU  $197A
CMPERR  EQU  $1BF7
CMPIF   EQU  $1DFB
CMPEXB  EQU  $2362
CMPVR   EQU  $2397
TRCPRT  EQU  $232F
CMPINX  EQU  $2370
CMPARG  EQU  $264E
ALLOC   EQU  $3374
L0709   EQU  $0709              ; fel 8
DETOK   EQU  $1195
BUF1    EQU  $FE40
T_FOR   EQU  $8B
T_NEXT  EQU  $8C

T_WHL   EQU  $96
T_EWH   EQU  $97
T_REP   EQU  $98
T_UNT   EQU  $99
T_IF    EQU  $9A
T_ELIF  EQU  $9B
T_ELSE  EQU  $9C
T_EIF   EQU  $9D
T_VILL  EQU  $9F
T_PROC  EQU  $A0
T_EPRC  EQU  $A1
T_EXEC  EQU  $A2
T_CASE  EQU  $A3
T_WHEN  EQU  $A4
T_OTHW  EQU  $A5
T_ECAS  EQU  $A6
T_LOC   EQU  $A7

E_OPPEN EQU  $C6                ; fel 70: strukturen slutar aldrig
E_SLUT  EQU  $C7                ; fel 71: slut utan början, eller fel slag
E_PROC  EQU  $C8                ; fel 72: PROC saknas, fel namn eller parametrar
E_CASE  EQU  $C9                ; fel 73: ingen WHEN stämmer
E_TYP   EQU  $91                ; fel 17: sträng och tal blandade
FCMP    EQU  $2A93              ; HL, DE = två flyttals exponenter: Z om lika
INTTOF  EQU  $3F18              ; HL = heltalet, DE = flyttalets exponent
STKCHK  EQU  $3389
RUNLIN  EQU  $0D71
SKPELM  EQU  $0EFB

; --- lapparna i ROM:en ---

        ORG  $0D58
        CALL PRELX              ; RUN: förlänkningen
        ORG  $0DB3
        LD   BC,EXE2            ; EXEST2
        ORG  $0EBF
        CP   $86                ; LNKELM: $8D (fel 8) aldrig i koden
        JP   Z,LNKX
        ORG  $11D5
        LD   BC,KW2             ; DETOK
        ORG  $18E0
        LD   DE,KW2             ; TOKKW
        ORG  $18E6
        LD   BC,TAB2
        ORG  $1A18
        DEFW CMPIFX             ; STTAB1: IF
        ORG  $1BD2
        CALL DIMFIX             ; CMPDMI
        ORG  $0AF7
        CALL LINIT              ; LIST: indragets minne
        NOP
        ORG  $0B5E
        CALL LDETOK             ; LIST: raden med indrag

        ORG  $4000

; --- tabell 2, de gamla satserna och de nya ---
; De nya med END först står före END, så att KWSRCH inte tar END i
; ENDWHILE. $9A (IF-block) och $9F (ELIF:s villkor) står efter $FF:
; DETOK hittar dem med CPIR, men KWSRCH når dem aldrig.
KW2:    DEFB T_WHL,"WHILE"
        DEFB T_EWH,"ENDWHILE"
        DEFB T_REP,"REPEAT"
        DEFB T_UNT,"UNTIL"
        DEFB T_ELIF,"ELIF"
        DEFB T_ELSE,"ELSE"
        DEFB T_EIF,"ENDIF"
        DEFB T_EPRC,"ENDPROC"
        DEFB T_PROC,"PROC"
        DEFB T_EXEC,"EXEC"
        DEFB T_ECAS,"ENDCASE"
        DEFB T_CASE,"CASE"
        DEFB T_WHEN,"WHEN"
        DEFB T_OTHW,"OTHERWISE"
        DEFB T_LOC,"LOCAL"
        DEFB $80,"DIM"
        DEFB $81,"POKE"
        DEFB $82,"OUT"
        DEFB $84,"REM"
        DEFB $85,"OPEN"
        DEFB $86,"PREPARE"
        DEFB $87,"CLOSE"
        DEFB $88,"RANDOMIZE"
        DEFB $89,"STOP"
        DEFB $8A,"END"
        DEFB $8B,"CMD"
        DEFB $8E,"SETDOT"
        DEFB $8F,"CLRDOT"
        DEFB $90,"GET"
        DEFB $91,"CHAIN"
        DEFB $92,"KILL"
        DEFB $93,"NAME"
        DEFB $94,"TRACE"
        DEFB $95,"NOTRACE"
        DEFB $FF
        DEFB T_IF,"IF"
        DEFB T_VILL
        DEFB $FF

TAB2:   DEFW $1BAA,$2104,$20FF,$2370,$226B,$205D,$205D,$2071
        DEFW $226A,$22B5,$22B5,$0147,$0709,$0709,$22B6,$22B6
        DEFW $23A8,$236C,$236C,$0151,$22B5,$22B5
        DEFW CWHILE,CSLOT,CSLOT,CWHILE,L0709,CELIF,CSLOT,CINGET
        DEFW L0709,L0709,CPROC,CINGET,CEXEC,CCASE,CWHEN,CSLOT
        DEFW CINGET,CLOCAL

EXE2:   DEFW $1BFA,$2144,$212A,$0709,$2276,$2074,$2077,$20BF
        DEFW $2258,$2320,$00C9,$0166,$0709,$0709,$22C7,$22CF
        DEFW $1DB1,$0D3F,$0170,$0175,$2325,$232A
        DEFW WHILEX,ENDWX,SKIP6,UNTILX,IFX,ELIFX,ELSEX,ENDIFX
        DEFW L0709,VILLX,PROCX,ENDPX,EXECX,CASEX,WHENX,WHENX
        DEFW ENDIFX,LOCALX

; --- kompileringen ---
; In: DE = koden efter satsens kod, HL = texten. Ut: CY och A = fel.

CWHILE: CALL CMPINX             ; WHILE v, UNTIL v: villkoret
        RET  C
CSLOT:  EX   DE,HL              ; två tomma platser
        LD   B,$02
CSL1:   LD   (HL),$B9
        INC  HL
        LD   (HL),$00
        INC  HL
        LD   (HL),$00
        INC  HL
        DJNZ CSL1
        EX   DE,HL
CINGET: AND  A
        RET

CELIF:  CALL CSLOT              ; ELIF v [THEN]: platsen först
        EX   DE,HL
        LD   (HL),$86
        INC  HL
        LD   (HL),T_VILL
        INC  HL
        EX   DE,HL
        CALL CMPINX
        RET  C
        CALL CTHEN
        JR   CINGET

CTHEN:  PUSH DE                 ; THEN, om det står där
        LD   DE,KWTHEN
        CALL KWSRCH
        POP  DE
        RET  NZ
        LD   (DE),A             ; $BD
        INC  DE
        RET

; PROC namn(a,b): variabler utan index, inte strängar
CPROC:  CALL CSLOT
        CALL CNAME
        RET  C
        CP   '('
        JR   NZ,CPR3
CPR1:   LD   (DE),A             ; "(" eller ","
        INC  DE
        INC  HL
        CALL CVAR
        RET  C
CPR2:   RST  $20
        CP   ','
        JR   Z,CPR1
        CP   ')'
        JP   NZ,CNFEL
        LD   (DE),A
        INC  DE
        INC  HL
CPR3:   AND  A
        RET

; en variabel utan index, inte en sträng
CVAR:   PUSH DE
        CALL CMPVR
        POP  BC
        RET  C
        LD   A,E
        SUB  C
        CP   $06
        JP   NZ,CNFEL           ; index: mer än variabeln och $BC
        DEC  DE
        LD   A,(DE)
        INC  DE
        CP   $BE
        JR   Z,CNFEL            ; sträng
        AND  A
        RET

; LOCAL a,b: som PROC:s parametrar
CLOCAL: CALL CVAR
        RET  C
        RST  $20
        CP   ','
        JR   NZ,CPR3
        LD   (DE),A
        INC  DE
        INC  HL
        JR   CLOCAL

; EXEC namn(x,y): tal, inte strängar
CEXEC:  CALL CSLOT
        CALL CNAME
        RET  C
        CP   '('
        JR   NZ,CPR3
CEX1:   LD   (DE),A
        INC  DE
        INC  HL
        CALL CMPEXB
        RET  C
        DEC  DE
        LD   A,(DE)
        INC  DE
        CP   $BB
        JR   Z,CNFEL            ; sträng
        RST  $20
        CP   ','
        JR   Z,CEX1
        JR   CPR2

; CASE v [OF]: OF står som tecken efter v
CCASE:  CALL CSLOT
        CALL CMPEXB
        RET  C
        RST  $20
        CP   'O'
        JR   NZ,CPR3
        INC  HL
        LD   A,(HL)
        DEC  HL
        CP   'F'
        JR   NZ,CPR3
        LD   A,' '
        LD   (DE),A
        INC  DE
        LD   B,$02
CCA1:   LD   A,(HL)
        LD   (DE),A
        INC  DE
        INC  HL
        DJNZ CCA1
        AND  A
        RET

; WHEN a,b: uttryck av vilken typ som helst
CWHEN:  CALL CSLOT
CWH1:   CALL CMPEXB
        RET  C
        RST  $20
        CP   ','
        JP   NZ,CPR3
        LD   (DE),A
        INC  DE
        INC  HL
        JR   CWH1

; namnet: en bokstav, sedan bokstäver och siffror. Ut: A = tecknet efter
CNAME:  RST  $20
        CP   'A'
        JR   C,CNFEL
        CALL ISNAME
        JR   NC,CNFEL
CN1:    LD   (DE),A
        INC  DE
        INC  HL
        LD   A,(HL)
        CALL ISNAME
        JR   C,CN1
        RST  $20
        AND  A
        RET
CNFEL:  LD   A,E_PROC
        SCF
        RET

; CY om A är en siffra eller bokstav ($41-$7D)
ISNAME: CP   '0'
        CCF
        RET  NC
        CP   '9'+1
        RET  C
        CP   'A'
        CCF
        RET  NC
        CP   $7E
        RET

SKNAM:  LD   A,(HL)
        CALL ISNAME
        RET  NC
        INC  HL
        JR   SKNAM

; IF: står inget efter THEN är det ett IF-block, annars som förut.
CMPIFX: PUSH HL
        PUSH DE
        DEC  DE                 ; $88 -> $86 $9A
        LD   A,$86
        LD   (DE),A
        INC  DE
        LD   A,T_IF
        LD   (DE),A
        INC  DE
        CALL CMPINX
        JR   C,CIF1             ; felet ger CMPIF själv
        PUSH DE
        LD   DE,KWTHEN
        CALL KWSRCH
        POP  DE
        JR   NZ,CIF1
        LD   B,A
        LD   A,(HL)
        CP   $0D
        JR   NZ,CIF1
        LD   A,B
        LD   (DE),A             ; $BD
        INC  DE
        POP  AF
        POP  AF
        JP   CSLOT
CIF1:   POP  DE
        POP  HL
        DEC  DE
        LD   A,$88
        LD   (DE),A
        INC  DE
        JP   CMPIF

; CMPDMI gick till CMPERR med AF kvar på stacken (se
; utanfor/tolkjamforelse/lappa.py); här tas den bort först.
DIMFIX: CALL CMPARG
        RET  NC
        POP  BC                 ; återhoppet till $1BD5
        POP  BC                 ; AF från $1BD1
        SCF
        JP   CMPERR

; --- förlänkningen ---

PRELX:  LD   DE,$0002           ; stackens topp, på EOFA+1
        CALL ALLOC
        CALL PRELNK
        CALL HEAD
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        LD   A,D
        OR   E
        RET  Z
        LD   (CURLIN),DE        ; en adress i raden som inte slutar
        LD   A,E_OPPEN
        JP   ERROR

HEAD:   LD   HL,(EOFA)
        INC  HL
        RET

; LNKELM med HL på $86. På stacken: LNKELM som återhopp.
LNKX:   INC  HL
        LD   A,(HL)
        DEC  HL
        CP   T_WHL
        JR   NC,LX1
        EX   (SP),HL            ; en gammal sats: som förut
        POP  HL
        JP   LNKSKE
LX1:    CALL NODIR
        PUSH HL                 ; S = satsen
        INC  HL
        LD   A,(HL)
        INC  HL                 ; HL = S+2
        CP   T_WHL
        JR   Z,LWHILE
        CP   T_REP
        JR   Z,LOPEN
        CP   T_IF
        JR   Z,LIF
        CP   T_EWH
        JR   Z,LENDW
        CP   T_UNT
        JR   Z,LUNTIL
        CP   T_ELIF
        JP   Z,LELIF
        CP   T_ELSE
        JP   Z,LELSE
        CP   T_PROC
        JR   Z,LOPEN
        CP   T_EXEC
        JP   Z,LEXEC
        CP   T_LOC
        JP   Z,LLOCAL
        CP   T_CASE
        JR   Z,LOPEN
        LD   BC,T_CASE<<8|T_WHEN
        CP   T_WHEN
        JR   Z,LPART
        CP   T_OTHW
        JR   Z,LPART
        LD   BC,T_CASE<<8|T_OTHW
        CP   T_ECAS
        JR   Z,LEND
        LD   BC,T_PROC<<8|T_PROC
        CP   T_EPRC
        JR   Z,LEND
        LD   BC,T_IF<<8|T_ELSE  ; ENDIF
LEND:   CALL LCLOSE
        POP  DE                 ; S
        PUSH DE
        CALL LFILL
        POP  HL
        INC  HL
        INC  HL
        RET

LIF:    CALL LNKEXP
        CALL SKPTHN
        JR   LOPEN
LWHILE: CALL LNKEXP
; öppna: S överst på stacken, HL = platsen
LOPEN:  POP  DE                 ; S
        PUSH HL
        CALL HEAD
        LD   C,(HL)             ; BC = förra toppen
        INC  HL
        LD   B,(HL)
        LD   (HL),D             ; toppen = S
        DEC  HL
        LD   (HL),E
        POP  HL
        LD   D,B
        LD   E,C
        LD   BC,(CURLIN)
        JP   WRSLOT             ; (förra toppen, egen rad), HL efter

LENDW:  LD   BC,T_WHL<<8|T_WHL
        CALL LCLOSE             ; DE = WHILE, BC = dess rad, HL = dess plats
        EX   (SP),HL            ; HL = S
        PUSH HL
        INC  HL
        INC  HL
        CALL WRSLOT             ; ENDWHILE -> WHILE
        POP  DE                 ; S
        EX   (SP),HL            ; WHILE:s plats, efter ENDWHILE på stacken
        CALL LFILL              ; WHILE -> ENDWHILE
        POP  HL
        RET

LUNTIL: CALL LNKEXP
        PUSH HL                 ; den egna platsen
        LD   BC,T_REP<<8|T_REP
        CALL LCLOSE             ; DE = REPEAT, BC = dess rad
        POP  HL
        POP  AF                 ; S
        JP   WRSLOT             ; UNTIL -> REPEAT

LELIF:  POP  DE                 ; S
        LD   BC,LELIF1          ; villkoret efter platsen
        PUSH BC
        PUSH DE
LELSE:  LD   BC,T_IF<<8|T_ELIF  ; HL = platsen
; en ny del (ELIF, ELSE, WHEN, OTHERWISE): stäng den förra, B..C
LPART:  PUSH HL                 ; den egna platsen
        CALL LCLOSE             ; HL = förra delens plats
        POP  BC
        POP  DE                 ; S
        PUSH DE
        PUSH BC
        CALL LFILL              ; förra delen -> den här
        POP  HL
        JR   LOPEN
LELIF1: INC  HL                 ; förbi $86 $9F
        INC  HL
        CALL LNKEXP
        JP   SKPTHN

; Stäng: toppen T måste ha en kod i B..C. Ut: DE = T, BC = T:s rad,
; HL = T:s plats; toppen blir strukturen före T.
LCLOSE: PUSH BC
        CALL HEAD
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        POP  BC
        LD   A,D
        OR   E
        JR   Z,LFEL
        INC  DE
        LD   A,(DE)
        DEC  DE
        CP   B
        JR   C,LFEL
        DEC  A
        CP   C
        JR   NC,LFEL
        CALL SLOTOF
        PUSH HL
        PUSH DE
        CALL RDSLOT             ; DE = förra toppen, BC = T:s rad
        CALL HEAD
        LD   (HL),E
        INC  HL
        LD   (HL),D
        POP  DE
        POP  HL
        RET
LFEL:   LD   A,E_SLUT
        JP   ERROR

; T:s plats ← (S, raden som länkas nu). DE = S, HL = T:s plats.
LFILL:  LD   BC,(CURLIN)
        JP   WRSLOT

; HL = platsen i satsen DE (WHILE, REPEAT, IF, ELIF, ELSE)
SLOTOF: LD   H,D
        LD   L,E
        INC  HL
        LD   A,(HL)
        INC  HL
        CP   T_WHL
        JR   Z,SKPVLL
        CP   T_IF
        RET  NZ
; förbi ett villkor och THEN
SKPVLL: PUSH BC
        PUSH DE
SKV1:   CALL SKPEXP
        JR   NC,SKV1
        POP  DE
        POP  BC
SKPTHN: LD   A,(HL)
        CP   $BD
        RET  NZ
        INC  HL
        RET

; förbi WHEN:s värden
SKPLST: CALL SKPVLL
        LD   A,(HL)
        CP   ','
        RET  NZ
        INC  HL
        JR   SKPLST

; HL på $B9 s $B9 r: DE = s, BC = r, HL efter
RDSLOT: INC  HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        INC  HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        RET

WRSLOT: INC  HL
        LD   (HL),E
        INC  HL
        LD   (HL),D
        INC  HL
        INC  HL
        LD   (HL),C
        INC  HL
        LD   (HL),B
        INC  HL
        RET

; EXEC: platsen får PROC och dess rad; parametrarna prövas mot
; argumenten. Argumenten länkas sedan av LNKELM.
LEXEC:  PUSH HL                 ; platsen
        LD   DE,$0006
        ADD  HL,DE
        PUSH HL                 ; namnet
        CALL FINDPR             ; DE = PROC, BC = dess rad
        POP  HL
        PUSH BC
        PUSH DE
        CALL CHKPAR
        POP  DE
        POP  BC
        POP  HL
        CALL WRSLOT
        POP  AF                 ; S
        RET

; PROC med namnet HL. Ut: DE = PROC, BC = dess rad
FINDPR: EX   DE,HL
        LD   HL,(BOFA)
FP1:    LD   A,(HL)
        DEC  A
        JR   Z,PFEL
        LD   B,H
        LD   C,L
        INC  HL
        INC  HL
        INC  HL
FP2:    LD   A,(HL)
        CP   $86
        JR   NZ,FP3
        INC  HL
        LD   A,(HL)
        DEC  HL
        CP   T_PROC
        JR   NZ,FP3
        PUSH HL
        PUSH DE
        LD   A,$08
        ADD  A,L
        LD   L,A
        JR   NC,FP4
        INC  H
FP4:    CALL NAMEQ
        POP  DE
        POP  HL
        JR   NZ,FP3
        EX   DE,HL
        RET
FP3:    PUSH BC
        PUSH DE
        CALL SKPELM
        POP  DE
        POP  BC
        JR   NC,FP2
        JR   FP1
PFEL:   LD   A,E_PROC
        JP   ERROR

; Z om namnen på HL och DE är lika
NAMEQ:  LD   A,(DE)
        CALL ISNAME
        JR   NC,NQ1
        CP   (HL)
        RET  NZ
        INC  HL
        INC  DE
        JR   NAMEQ
NQ1:    LD   A,(HL)
        CALL ISNAME
        SBC  A,A
        RET

; EXEC:s namn HL, PROC DE: lika många parametrar, och samma typ
CHKPAR: CALL SKNAM
        EX   DE,HL
        LD   BC,$0008
        ADD  HL,BC
        CALL SKNAM
        EX   DE,HL
        LD   A,(DE)
        CP   '('
        LD   A,(HL)
        JR   Z,CHP1
        CP   '('
        RET  NZ                 ; ingen har parametrar
        JR   PFEL
CHP1:   CP   '('
        JR   NZ,PFEL
CHP2:   INC  HL
        INC  DE
        CALL ENDTOK             ; argumentet: $B9 eller $BA
        PUSH AF
        EX   DE,HL
        CALL ENDTOK             ; parametern: $BC eller $BD
        EX   DE,HL
        POP  BC
        SUB  $03
        CP   B
        JR   NZ,PFEL
        LD   A,(DE)
        CP   (HL)
        JR   NZ,PFEL
        CP   ','
        JR   Z,CHP2
        RET

; förbi ett uttryck; A = dess sluttoken
ENDTOK: PUSH BC
        PUSH DE
ET1:    CALL SKPEXP
        JR   NC,ET1
        POP  DE
        POP  BC
        DEC  HL
        LD   A,(HL)
        INC  HL
        RET

; LOCAL: först på raden efter PROC, medan PROC är öppen
LLOCAL: PUSH HL                 ; S+2
        CALL HEAD
        LD   E,(HL)
        INC  HL
        LD   D,(HL)             ; DE = toppen
        LD   A,D
        OR   E
        JR   Z,LLFEL
        INC  DE
        LD   A,(DE)
        DEC  DE
        CP   T_PROC
        JR   NZ,LLFEL
        CALL SLOTOF
        CALL RDSLOT             ; BC = PROC:s rad
        LD   H,B
        LD   L,C
        CALL LOCOF
        POP  DE
        PUSH DE
        DEC  DE
        AND  A
        SBC  HL,DE
        POP  HL
        JR   NZ,LLFEL
        POP  AF                 ; S
        RET
LLFEL:  JP   PFEL

; HL = PROC:s rad. Z och HL på $A7 om nästa rad börjar med LOCAL
LOCOF:  LD   A,(HL)
        ADD  A,L
        LD   L,A
        JR   NC,LO1
        INC  H
LO1:    INC  HL
        INC  HL
        INC  HL
        LD   A,(HL)
        CP   $86
        RET  NZ
        INC  HL
        LD   A,(HL)
        CP   T_LOC
        RET

; --- körningen ---
; In: DE efter satsens kod. På stacken RUNSTM ($0DA1) och RUNLIN ($0D8F).

WHILEX: RST  $38
        LD   A,H
        OR   L
        JR   Z,WFALSK
SKIP6:  EX   DE,HL              ; förbi platsen
        LD   BC,$0006
        ADD  HL,BC
        EX   DE,HL
        RET
WFALSK: EX   DE,HL              ; efter ENDWHILE (UNTIL: efter REPEAT)
        CALL RDSLOT
        LD   HL,$0008
        ADD  HL,DE
        EX   DE,HL
        JR   HOPP

; ENDWHILE räknar WHILE:s villkor själv: sant, till efter WHILE;
; falskt, vidare efter ENDWHILE utan hopp
ENDWX:  EX   DE,HL
        CALL RDSLOT             ; DE = WHILE, BC = dess rad
        PUSH HL
        INC  DE
        INC  DE
        PUSH BC
        RST  $38
        POP  BC
        LD   A,H
        OR   L
        POP  HL
        JR   NZ,SKIP6H
        EX   DE,HL
        RET
SKIP6H: LD   HL,$0006
        ADD  HL,DE
        EX   DE,HL
        JR   HOPP

UNTILX: RST  $38
        LD   A,H
        OR   L
        JR   NZ,SKIP6
        JR   WFALSK

; ELIF:s villkor, platsen står före: $B9 s $B9 r $86 $9F v
VILLX:  PUSH DE
        RST  $38
        LD   A,(DE)
        CP   $BD
        JR   NZ,VX1
        INC  DE
VX1:    LD   A,H
        OR   L
        POP  HL
        RET  NZ
        LD   BC,-8
        ADD  HL,BC
        JR   IFX3

; IF:s villkor
IFX:    RST  $38
        LD   A,(DE)
        CP   $BD
        JR   NZ,IFX1
        INC  DE
IFX1:   LD   A,H
        OR   L
        JR   NZ,SKIP6
        EX   DE,HL
IFX3:   CALL RDSLOT             ; nästa del
        LD   H,D
        LD   L,E
        INC  HL
        LD   A,(HL)
        INC  HL                 ; ENDIF: efter
        CP   T_EIF
        JR   Z,IFX2
        LD   HL,$0008           ; ELIF och ELSE: efter platsen
        ADD  HL,DE
IFX2:   EX   DE,HL
        JR   HOPP

; ELIF och ELSE efter en THEN-del: följ platserna till ENDIF
ELIFX:
ELSEX:  EX   DE,HL
EL1:    CALL RDSLOT
        LD   H,D
        LD   L,E
        INC  HL
        LD   A,(HL)
        INC  HL
        CP   T_EIF
        JR   Z,IFX2
        JR   EL1                ; ELIF och ELSE

ENDIFX: RET

; Fortsätt på DE i raden BC. Samma rad: bara DE. Annars som RUNLIN:
; CURLIN, STMSP och TRACE, och RUNSTM med RUNLIN:s återhopp.
HOPP:   LD   HL,(CURLIN)
        AND  A
        SBC  HL,BC
        RET  Z
        POP  HL
        POP  HL
        LD   (CURLIN),BC
        LD   HL,$0000
        ADD  HL,SP
        LD   (STMSP),HL
        BIT  4,(IY+$0F)
        JR   Z,HOPP1
        PUSH DE
        LD   D,B
        LD   E,C
        CALL TRCPRT
        POP  DE
HOPP1:  LD   HL,$0D8F
        PUSH HL
        JP   RUNSTM

; PROC i den vanliga följden: förbi ENDPROC
PROCX:  EX   DE,HL
        CALL RDSLOT
        INC  DE
        INC  DE
        JR   HOPP

ENDPX:  POP  HL                 ; som RETURN
        POP  HL
        RET

; LOCAL i följden: förbi variablerna
LOCALX: RST  $38                ; adressen, gör inget
        LD   A,(DE)
        CP   ','
        RET  NZ
        INC  DE
        JR   LOCALX

; EXEC. Stacken under körningen av kroppen:
;   [RUNSTM] [PROC:s rad] [PROC] = M, argumenten (nu parametrarnas
;   gamla värden), [fortsättningen] [M] [CURLIN] [STMSP] [återhoppet]
; M står under argumenten i två bytes på EOFA+1, som efter länkningen
; är lediga.
EXECX:  EX   DE,HL
        CALL RDSLOT             ; DE = PROC, BC = dess rad, HL = namnet
        PUSH BC
        PUSH DE
        PUSH HL
        LD   HL,$0002
        ADD  HL,SP              ; M = PROC-ordet
        EX   DE,HL
        CALL HEAD
        LD   (HL),E
        INC  HL
        LD   (HL),D
        POP  HL
        CALL SKNAM
        LD   A,(HL)
        CP   '('
        JR   NZ,EXC3
EXC1:   INC  HL
        EX   DE,HL
        RST  $38                ; flyttal: 5 bytes på stacken; heltal: HL
        DEC  B
        JR   NZ,EXC2
        PUSH HL
EXC2:   EX   DE,HL
        LD   A,(HL)
        CP   ','
        JR   Z,EXC1
        INC  HL                 ; förbi ")"
EXC3:   EX   DE,HL              ; LOCAL: nollor, 5 bytes för ett flyttal
        CALL HEAD
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A                ; M
        INC  HL
        INC  HL
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A                ; PROC:s rad
        CALL LOCOF
        JR   NZ,EXC6
EXC5:   INC  HL
        CALL ENDTOK
        LD   BC,$0000
        PUSH BC
        CP   $BD
        JR   Z,EXC51
        PUSH BC
        PUSH BC
        INC  SP
EXC51:  LD   A,(HL)
        CP   ','
        JR   Z,EXC5
EXC6:   EX   DE,HL
        PUSH HL                 ; fortsättningen
        CALL HEAD
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        PUSH DE                 ; M
        CALL PSWAPM
        POP  HL
        PUSH HL
        INC  HL
        INC  HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)             ; PROC:s rad
        LD   A,(DE)
        ADD  A,E                ; raden efter
        LD   E,A
        JR   NC,EXC4
        INC  D
EXC4:   LD   HL,(CURLIN)
        PUSH HL
        LD   HL,(STMSP)
        PUSH HL
        CALL STKCHK
        CALL RUNLIN             ; kroppen, till ENDPROC
        POP  HL
        LD   (STMSP),HL
        POP  HL
        LD   (CURLIN),HL
        POP  DE                 ; M
        PUSH DE
        CALL PSWAPM             ; de gamla värdena tillbaka
        POP  HL
        POP  DE                 ; fortsättningen
        INC  HL                 ; förbi PROC och raden
        INC  HL
        INC  HL
        INC  HL
        LD   SP,HL
        RET

; Byt parametrarnas värden mot argumenten under M (DE)
; och sedan LOCAL:s variabler mot nollorna
PSWAPM: LD   B,D                ; BC = P, slutet på argumentet
        LD   C,E
        EX   DE,HL
        PUSH HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        LD   HL,$0008
        ADD  HL,DE
        CALL SKNAM
        CP   '('
        CALL Z,PSW1
        POP  HL
        INC  HL
        INC  HL
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A                ; PROC:s rad
        CALL LOCOF
        RET  NZ
PSW1:   INC  HL
        EX   DE,HL
        PUSH BC
        RST  $38                ; HL = variabeln, B = typ
        LD   A,B
        POP  BC
        PUSH DE
        LD   E,$02
        OR   A
        JR   NZ,PSW2
        DEC  HL                 ; flyttal: 5 bytes till exponenten
        DEC  HL
        DEC  HL
        DEC  HL
        LD   E,$05
PSW2:   LD   A,C
        SUB  E
        LD   C,A
        JR   NC,PSW3
        DEC  B
PSW3:   PUSH BC
PSW4:   LD   A,(BC)
        LD   D,(HL)
        LD   (HL),A
        LD   A,D
        LD   (BC),A
        INC  BC
        INC  HL
        DEC  E
        JR   NZ,PSW4
        POP  BC
        POP  HL
        LD   A,(HL)
        CP   ','
        JR   Z,PSW1
        RET

; CASE räknar väljaren P en gång och lägger den i en ram på stacken:
; F+0 P:s slag (i F+1: 0 flyttal, 1 heltal, 2 sträng), F+2 platsen i
; den WHEN som prövas, F+4 dess rad, F+6 SP före CASE, F+8 T (5 bytes
; för ett heltal som görs till flyttal), F+13 P.
CASEX:  PUSH DE                 ; platsen
        LD   HL,$0006
        ADD  HL,DE
        EX   DE,HL
        RST  $38                ; P, B = slaget
        LD   A,B
        DEC  A
        JR   NZ,CX0
        PUSH HL                 ; heltalet på stacken som de andra
CX0:    LD   HL,$0000
        ADD  HL,SP
        LD   A,B
        CALL SIZE               ; HL = platsen på stacken
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        EX   DE,HL              ; HL = platsen, DE = SP före CASE
        PUSH AF                 ; T
        PUSH AF
        DEC  SP
        PUSH DE
        PUSH DE                 ; raden och platsen fylls i av CX2
        PUSH HL
        PUSH BC
CX1:    CALL RDSLOT             ; DE = nästa del, BC = dess rad
        INC  DE
        LD   A,(DE)
        INC  DE                 ; delens plats
        CP   T_WHEN
        JR   Z,CX2
        CP   T_OTHW
        JR   NZ,CXFEL           ; ENDCASE: ingen stämde
        LD   HL,$0006           ; OTHERWISE: efter platsen
        ADD  HL,DE
        EX   DE,HL
        JR   CXUT
CX2:    LD   HL,$0002
        ADD  HL,SP
        LD   (HL),E
        INC  HL
        LD   (HL),D
        INC  HL
        LD   (HL),C
        INC  HL
        LD   (HL),B
        LD   HL,$0006
        ADD  HL,DE
        EX   DE,HL              ; DE = första värdet
CX3:    CALL WHENEQ
        JR   NZ,CX4
        LD   A,(DE)
        INC  DE
        CP   ','
        JR   Z,CX3
        LD   HL,$0002           ; ingen: nästa del, från platsen
        ADD  HL,SP
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        JR   CX1
CX4:    EX   DE,HL              ; lika: förbi resten av värdena
        LD   A,(HL)
        CP   ','
        JR   NZ,CX5
        INC  HL
        CALL SKPLST
CX5:    EX   DE,HL
        LD   HL,$0004
        ADD  HL,SP
        LD   C,(HL)
        INC  HL
        LD   B,(HL)             ; raden
CXUT:   LD   HL,$0006           ; stacken som före CASE
        ADD  HL,SP
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        LD   SP,HL
        JP   HOPP
CXFEL:  LD   A,E_CASE
        JP   ERROR

; A = slaget, HL = värdet på stacken. Ut: HL efter värdet; DE ändras
SIZE:   LD   DE,$0005
        OR   A
        JR   Z,SZ1
        LD   E,$02
        DEC  A
        JR   Z,SZ1
        LD   E,(HL)             ; sträng: n + 6
        INC  HL
        LD   D,(HL)
        DEC  HL
        ADD  HL,DE
        LD   DE,$0006
SZ1:    ADD  HL,DE
        RET

; WHEN och OTHERWISE i den vanliga följden: följ platserna till ENDCASE
WHENX:  EX   DE,HL
WX1:    CALL RDSLOT
        LD   H,D
        LD   L,E
        INC  HL
        LD   A,(HL)
        INC  HL
        CP   T_ECAS
        JR   NZ,WX1
        EX   DE,HL
        JP   HOPP

; Är värdet (DE) lika med P? Värdet räknas direkt ur koden och jämförs
; med P i CASE:s ram, under återhoppet. Ut: DE efter värdet, NZ om lika.
WHENEQ: RST  $38                ; B = värdets slag
        LD   A,B
        DEC  A
        JR   NZ,WE1
        PUSH HL
WE1:    PUSH DE                 ; koden efter värdet
        LD   HL,$0002
        ADD  HL,SP
        PUSH HL
        POP  IX                 ; IX = värdet
        LD   C,B
        LD   A,B
        CALL SIZE               ; HL = återhoppets plats
        PUSH HL
        INC  HL
        INC  HL
        INC  HL
        LD   B,(HL)             ; P:s slag
        LD   DE,$0007
        ADD  HL,DE              ; HL = T
        LD   A,B
        CP   C
        JR   Z,WESAME
        CP   $02                ; sträng mot tal
        JR   Z,WETYP
        LD   A,C
        CP   $02
        JR   Z,WETYP
        PUSH HL                 ; heltal mot flyttal: heltalet till T
        LD   DE,$0005
        ADD  HL,DE              ; P
        DJNZ WE2
        LD   A,(HL)             ; P heltal
        INC  HL
        LD   H,(HL)
        LD   L,A
        POP  DE
        PUSH DE
        CALL ITOT
        POP  HL                 ; T
        PUSH IX
        POP  DE                 ; värdet
        JR   WEF
WE2:    EX   (SP),HL            ; värdet heltal
        EX   DE,HL
        LD   L,(IX+$00)
        LD   H,(IX+$01)
        PUSH DE
        CALL ITOT
        POP  DE                 ; T
        POP  HL                 ; P
WEF:    LD   BC,$0004           ; till exponenterna
        ADD  HL,BC
        EX   DE,HL
        ADD  HL,BC
        CALL FCMP
        JR   WEUT
WESAME: LD   DE,$0005
        ADD  HL,DE              ; P
        PUSH IX
        POP  DE                 ; värdet
        DEC  A
        JR   Z,WEINT
        DEC  A
        JR   NZ,WEF
        PUSH DE                 ; strängar: längden och tecknen
        CALL STRD
        POP  HL
        PUSH DE
        PUSH BC
        CALL STRD
        POP  HL
        AND  A
        SBC  HL,BC
        POP  HL
        JR   NZ,WEUT
WS1:    LD   A,B
        OR   C
        JR   Z,WEUT
        LD   A,(DE)
        CP   (HL)
        JR   NZ,WEUT
        INC  HL
        INC  DE
        DEC  BC
        JR   WS1
WEINT:  LD   A,(DE)
        CP   (HL)
        JR   NZ,WEUT
        INC  HL
        INC  DE
        LD   A,(DE)
        CP   (HL)
WEUT:   LD   A,$00              ; Z = lika
        JR   NZ,WE9
        DEC  A
WE9:    POP  HL                 ; återhoppets plats
        POP  DE                 ; koden
        LD   SP,HL              ; bort med värdet
        OR   A
        RET
WETYP:  LD   A,E_TYP
        JP   ERROR

; HL = heltalet, DE = T: flyttalet i T
ITOT:   INC  DE
        INC  DE
        INC  DE
        INC  DE
        JP   INTTOF

; HL = en sträng på stacken: DE = adressen, BC = längden
STRD:   INC  HL
        INC  HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        RET

; --- LIST med indrag ---
; Två blanka per nivå efter radnumret, i FOR-, WHILE-, REPEAT-, IF-
; och PROC-block. En rad som börjar med en stängare (NEXT, ENDWHILE,
; UNTIL, ENDIF, ENDPROC) eller ELIF/ELSE dras in en nivå mindre.
; LIST:s ram (IX) har räknaren på +0 och filflaggan på +$0F; här
; används +1..+3: nästa rad och djupet där, så att bara den första
; listade raden räknas från programmets början.
LINIT:  LD   (IX+$00),$16
        LD   (IX+$01),$00
        LD   (IX+$02),$00
        RET

; HL = raden, DE = BUF1. Ut som DETOK: HL = nästa rad, DE efter CR
LDETOK: PUSH HL
        PUSH DE
        CALL DEPTH              ; A = nivåer
        POP  DE
        POP  HL
        PUSH AF
        CALL DETOK
        POP  AF
        ADD  A,A
        RET  Z
        PUSH HL                 ; nästa rad
        LD   C,A
        LD   B,$00              ; BC = antal blanka
        LD   HL,$FEFC
        AND  A
        SBC  HL,DE
        JR   C,IN9
        SBC  HL,BC
        JR   C,IN9              ; ryms inte: inget indrag
        LD   HL,BUF1
IN1:    LD   A,(HL)
        INC  HL
        CP   ' '
        JR   NZ,IN1             ; HL = efter blanken efter numret
        PUSH HL                 ; p
        PUSH BC                 ; n
        EX   DE,HL              ; HL = slutet, DE = p
        PUSH HL
        AND  A
        SBC  HL,DE
        LD   B,H
        LD   C,L                ; antal tecken från p
        POP  HL
        POP  DE                 ; n
        PUSH DE
        DEC  HL                 ; källan: sista tecknet
        PUSH HL
        ADD  HL,DE
        EX   DE,HL              ; DE = målet
        POP  HL
        PUSH DE
        LDDR
        POP  HL
        INC  HL                 ; nya slutet
        POP  BC                 ; n
        EX   DE,HL
        POP  HL                 ; p
IN2:    LD   (HL),' '
        INC  HL
        DEC  C
        JR   NZ,IN2
IN9:    POP  HL
        RET

; HL = raden. Ut: A = radens indrag; IX+1..3 = nästa rad och djupet
DEPTH:  LD   A,(IX+$01)
        CP   L
        JR   NZ,DP1
        LD   A,(IX+$02)
        CP   H
        LD   C,(IX+$03)
        JR   Z,DP3
DP1:    EX   DE,HL              ; från början
        LD   HL,(BOFA)
        LD   C,$00
DP2:    AND  A
        SBC  HL,DE
        ADD  HL,DE
        JR   Z,DP3
        CALL NETTO
        JR   DP2
DP3:    CALL NETTO
        LD   (IX+$01),L
        LD   (IX+$02),H
        LD   (IX+$03),C
        LD   A,B
        RET

; HL = raden, C = djupet före. Ut: HL = nästa rad, B = indraget,
; C = djupet efter
NETTO:  PUSH DE
        LD   B,C
        INC  HL
        INC  HL
        INC  HL
        CALL CLASS
        CP   $02
        JR   C,NT1
        LD   A,B
        OR   A
        JR   Z,NT1
        DEC  B
NT1:    CALL CLASS
        DEC  A
        JR   NZ,NT2
        INC  C                  ; öppnar
NT2:    DEC  A
        JR   NZ,NT3
        LD   A,C                ; stänger
        OR   A
        JR   Z,NT3
        DEC  C
NT3:    PUSH BC
        CALL SKPELM
        POP  BC
        JR   NC,NT1
        POP  DE
        RET

; elementet HL: A = 0 annat, 1 öppnar, 2 stänger, 3 ELIF/ELSE
CLASS:  LD   A,(HL)
        CP   T_FOR
        JR   Z,CL1
        CP   T_NEXT
        JR   Z,CL2
        CP   $86
        JR   NZ,CL0
        INC  HL
        LD   A,(HL)
        DEC  HL
        SUB  T_WHL
        JR   C,CL0
        CP   T_LOC-T_WHL+1
        JR   NC,CL0
        PUSH HL
        LD   HL,KLASS
        ADD  A,L
        LD   L,A
        JR   NC,CL3
        INC  H
CL3:    LD   A,(HL)
        POP  HL
        RET
CL0:    XOR  A
        RET
CL1:    LD   A,$01
        RET
CL2:    LD   A,$02
        RET
;              WHL EWH REP UNT IF ELIF ELSE EIF - VILL PROC EPRC EXEC
KLASS:  DEFB   1,  2,  1,  2,  1, 3,   3,   2,  0, 0,  1,   2,   0
;              CASE WHEN OTHW ECAS LOC
        DEFB   1,   3,   3,   2,   0
