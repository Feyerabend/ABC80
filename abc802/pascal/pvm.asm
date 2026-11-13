; pvm.asm -- p-kodtolken för ABC 802 i BASIC-läget (PASCAL.md, avsnitt
; 4 och 5). Den gör samma sak som pvm.py, och proven ska ge samma skärm.
;
; Från BASIC: Z=CALL(32768) frågar efter programmet ("Program: "),
; läser NAMN.PK (.PK om namnet saknar punkt) till KOD och kör det. En tom
; rad går tillbaka till BASIC. CALL:s värde är 0, eller körfelets nummer.
;
; Registren:
;   DE  IP, nästa byte i p-koden     IX  MP, aktiveringsposten
;   SP  p-stacken (Z80-stacken)      HL, BC, A  arbetsregister
;   IY  rörs inte (ROM:ens)
;
; Minnet: tolken på $8000-$8FFF, p-koden från KOD ($9000), de globala
; efter den och p-stacken nedåt från TOPP ($ED00). LADDAD, som DOS:ens
; DRUN hoppar till när PASCAL.ABS är inläst, flyttar BASIC till TOPP.
; Standardprocedurerna (CSP) och laddaren går på BASIC:s stack, under
; den SP som CALL kom med (SAVESP); där finns ett par hundra bytes för
; ROM:en och DOS:en, medan p-stacken bara har MARG bytes över de globala.
;
; Filerna: 0 är tangentbordet och skärmen, 1 och 2 är DOS:ens kanaler 1
; och 2 med sektorbuffertarna $F600 och $F700. På skivan är en fil poster
; som i Kalkyl: data på plats 3-254 och ETX (3) sist, och 0 där filen
; slutar. Byten 0, 3 och 16 skrivs som 16 och byten + 64. I en textfil är
; radslutet CR; CR, CR LF och LF läses alla som radslut (10).
;
; Med CPM EQU 1 blir det PVM.COM för CP/M (PASCAL.md, avsnitt 8; bygg.sh
; cpm). FPP och limmet ligger då i samma fil, och p-koden börjar på KOD
; efter dem. PVM NAMN kör NAMN.PK, och utan namn frågar tolken. P-stacken
; börjar under BDOS, och tolken slutar med en varmstart. Tecknen går
; genom BDOS 2 och 10, och filerna är CP/M-filer genom BDOS, med en sida
; för varje fil: posten på +0, FCB:n på +$80, antalet poster kvar på +$C8
; och antalet bytes i posten på +$CA. En textfil skrivs med CR LF och
; fylls ut med $1A; vid läsningen är $1A sist i den sista posten inte
; med i filen.

CPM     EQU  0              ; 1: PVM.COM för CP/M
 IF CPM
PRTXT   EQU  CSKRIV         ; BDOS 2: skriv BC tecken från HL
INLINE  EQU  CRAD           ; BDOS 10: läs en rad till HL, högst C tecken
 ELSE
PRTXT   EQU  $000B          ; ROM: skriv BC tecken från HL
INLINE  EQU  $0005          ; ROM: läs en rad till HL, högst C tecken
 ENDIF
HEAP    EQU  $FF0A          ; BASIC: FREPTR
BOTRAM  EQU  $FF0C          ; BASIC: första RAM-adressen för BASIC
COMEND  EQU  $FF30          ; BASIC: COMMON-områdets slut
VARBEG  EQU  $FF2E          ; BASIC: variablernas början
DERRX   EQU  $FD33          ; DOS:ens felutgång
DPREP   EQU  $6015          ; DOS: PREPARE, DE = namnet (11 tecken)
DOPEN   EQU  $6018          ; DOS: OPEN
DCLOSE  EQU  $6021          ; DOS: CLOSE
DSETRC  EQU  $602A          ; DOS: posten DE
DRDREC  EQU  $602D          ; DOS: läs nästa post till bufferten
DWRREC  EQU  $6030          ; DOS: skriv bufferten som nästa post
 IF 1-CPM
KOD     EQU  $9000          ; p-koden (i CP/M sist i filen)
 ENDIF
TOPP    EQU  $ED00          ; p-stackens topp och BASIC:s nya BOTRAM
 IF CPM
MARG    EQU  256            ; bytes för uttrycken (en mängd är 32 bytes)
 ELSE
MARG    EQU  64             ; bytes för uttrycken över de globala
 ENDIF

 IF CPM
        ORG  $0100
 ELSE
        ORG  $8000
 ENDIF
        JP   START

; DRUN hoppar hit när PASCAL.ABS är inläst: BASIC börjar på TOPP.
LADDAD: LD   HL,TOPP
        LD   (BOTRAM),HL
        LD   (COMEND),HL
        LD   (HEAP),HL
        LD   (VARBEG),HL
        LD   HL,0
        RET

SAVESP: DEFW 0              ; BASIC:s SP
PSP:    DEFW 0              ; p-stacken under en CSP
TMPIP:  DEFW 0              ; IP under en CSP, MOV och DIV
MPS:    DEFW 0              ; MP under en CSP
GB:     DEFW 0              ; de globalas början
GRANS:  DEFW 0              ; p-stackens gräns: GB + de globala + MARG
GLOB:   DEFW 0              ; de globalas storlek i bytes
 IF CPM
HOG0:   DEFW 0              ; högen: från de globalas slut
HOG:    DEFW 0              ;   och dess topp; GRANS = HOG + MARG
 ENDIF
PTAB:   DEFW 0              ; procedurtabellen (adresser)
DSP:    DEFW 0              ; SP i DOSANR
DERR0:  DEFW 0              ; DOS:ens gamla felutgång
BRED:   DEFW 0              ; fältbredden
TALV:   DEFW 0              ; talet eller längden
SADR:   DEFW 0              ; strängens adress
FVAR:   DEFW 0              ; filvariabelns adress
TPTR:   DEFW 0              ; i TALTXT
LPOS:   DEFW 0              ; laddarens plats
FELPC:  DEFW 0
FELNR:  DEFB 0
TSGN:   DEFB 0              ; tecknet i TALTXT och RDI
SIFF:   DEFB 0              ; RDI: någon siffra
OLAGE:  DEFB 0              ; läget i OPPNA
NSTEG:  DEFB 0              ; 1 eller 2 bytes per tecken i namnet
NGR:    DEFB 0              ; NAMNIN: högst så många tecken
NANT:   DEFB 0              ; NAMNIN: så många hittills
NDOT:   DEFB 0              ; NAMNIN: namnet hade en punkt
STANGER: DEFB 0             ; filerna stängs efter ett fel: inga nya
KP:     DEFB 0              ; tangentbordsbufferten: nästa tecken
KN:     DEFB 0              ;   och antalet
 IF CPM
KSVANS: DEFB 0              ; resten av raden efter programnamnet, antalet
 ENDIF
TBUF:   DEFB 0              ; tecknet i WRC
UBUF:   DEFB 0              ; ett tecken till PRTXT i UTB
FNAMN:  DEFS 11,32          ; filnamnet till DOS:en
; Filerna, 8 bytes var: läget (0 stängd, 1 läsa, 2 skriva, 3 tangentbordet
; och skärmen), BP (platsen i bufferten, 0 = läs nästa post), tittat (0
; nej, 1 tecknet i +3, 2 filens slut), tecknet, CR (det förra var CR),
; slut (filen är slut), kanalen * 16 och buffertens sida.
FILT:   DEFS 24,0

; REAL (preal.asm): opkoderna $A0-$BB går genom RVEK till FPP.BIN, som
; laddas till FPPADR när bit 15 i antalet procedurer är satt; annars är
; de okända instruktioner. TOP är p-stackens topp, FPPADR eller TOPP.
FPPADR  EQU  $DD00
RVEK:   DEFW BADOP
TOP:    DEFW TOPP
RFLAG:  DEFB 0
O_REAL: LD   HL,(RVEK)
        JP   (HL)

; CPS w: a1 a2 -- -1, 0 eller 1, strängarna jämförda byte för byte
O_CPS:  EX   DE,HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        LD   (TMPIP),HL
        POP  DE
        POP  HL
CPS1:   LD   A,B
        OR   C
        JR   Z,CPS3
        LD   A,(DE)
        CP   (HL)
        INC  HL
        INC  DE
        DEC  BC
        JR   Z,CPS1
        LD   HL,1
        JR   C,CPS4
        LD   HL,-1
        JR   CPS4
CPS3:   LD   H,B
        LD   L,C
CPS4:   PUSH HL
        LD   DE,(TMPIP)
        JP   NEXT

; LADFPP: FPP.BIN till FPPADR om programmet använder REAL, och TOP
; (i CP/M är FPP redan med, och TOP är satt)
LADFPP:
 IF CPM
        LD   HL,BADOP
        LD   A,(RFLAG)
        RLA
        JR   NC,LADC1
        LD   HL,RGLUE
LADC1:  LD   (RVEK),HL
        RET
 ELSE
        LD   HL,BADOP
        LD   (RVEK),HL
        LD   HL,TOPP
        LD   A,(RFLAG)
        RLA
        JR   NC,LADF1
        LD   HL,TXTFPP
        LD   DE,FNAMN
        LD   BC,11
        LDIR
        LD   HL,FPPADR
        CALL LADDA
        LD   HL,(FPPADR)
        LD   (RVEK),HL
        LD   HL,FPPADR
LADF1:  LD   (TOP),HL
        RET
TXTFPP: DEFM "FPP     BIN"
 ENDIF

; Hopptabellen, låga och höga byten för varje opkod.
        DEFS (256-($&255))&255,0
TABL:
        DEFB O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255
        DEFB O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255,O_LDCK&255
        DEFB O_LDCN&255,O_LDCB&255,O_LDCW&255,O_LSA&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_LDL0&255,O_LDL1&255,O_LDL2&255,O_LDL3&255,O_LDL4&255,O_LDL5&255,O_LDL6&255,O_LDL7&255
        DEFB O_STL0&255,O_STL1&255,O_STL2&255,O_STL3&255,O_STL4&255,O_STL5&255,O_STL6&255,O_STL7&255
        DEFB O_LDL&255,O_STL&255,O_LLA&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_LDGK&255,O_LDGK&255,O_LDGK&255,O_LDGK&255,O_LDGK&255,O_LDGK&255,O_LDGK&255,O_LDGK&255
        DEFB O_LDG&255,O_STG&255,O_LGA&255,BADOP&255,O_LOD&255,O_STR&255,O_LDA&255,BADOP&255
        DEFB O_IND&255,O_STO&255,O_INDB&255,O_STB&255,O_MOV&255,O_CPS&255,BADOP&255,BADOP&255
        DEFB O_IXA&255,O_CHK&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_ADD&255,O_SUB&255,O_MUL&255,O_DIV&255,O_MOD&255,O_NEG&255,O_ABS&255,O_INC1&255
        DEFB O_DEC1&255,O_SQRI&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_EQU&255,O_NEQ&255,O_LES&255,O_LEQ&255,O_GRT&255,O_GEQ&255,BADOP&255,BADOP&255
        DEFB O_AND&255,O_OR&255,O_NOT&255,O_ODD&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_UJPS&255,O_FJPS&255,O_UJP&255,O_FJP&255,O_XJP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB O_MST&255,O_CUP&255,O_ENT&255,O_ENTW&255,O_RET&255,O_LFA&255,BADOP&255,BADOP&255
        DEFB O_CSP&255,BADOP&255,BADOP&255,BADOP&255,O_DUP&255,O_POP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,O_ERR&255,O_HLT&255
 IF CPM
        DEFB O_LDSE&255,O_SGS&255,O_SRS&255,O_UNI&255,O_INT&255,O_DIF&255,O_INN&255,O_EQS&255
        DEFB O_NES&255,O_LSS&255,O_GSS&255,O_INDS&255,O_STOS&255,BADOP&255,BADOP&255,BADOP&255
 ELSE
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
 ENDIF
        DEFB O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255
        DEFB O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255
        DEFB O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255
        DEFB O_REAL&255,O_REAL&255,O_REAL&255,O_REAL&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
        DEFB BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255,BADOP&255
TABH:
        DEFB O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8
        DEFB O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8,O_LDCK>>8
        DEFB O_LDCN>>8,O_LDCB>>8,O_LDCW>>8,O_LSA>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_LDL0>>8,O_LDL1>>8,O_LDL2>>8,O_LDL3>>8,O_LDL4>>8,O_LDL5>>8,O_LDL6>>8,O_LDL7>>8
        DEFB O_STL0>>8,O_STL1>>8,O_STL2>>8,O_STL3>>8,O_STL4>>8,O_STL5>>8,O_STL6>>8,O_STL7>>8
        DEFB O_LDL>>8,O_STL>>8,O_LLA>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_LDGK>>8,O_LDGK>>8,O_LDGK>>8,O_LDGK>>8,O_LDGK>>8,O_LDGK>>8,O_LDGK>>8,O_LDGK>>8
        DEFB O_LDG>>8,O_STG>>8,O_LGA>>8,BADOP>>8,O_LOD>>8,O_STR>>8,O_LDA>>8,BADOP>>8
        DEFB O_IND>>8,O_STO>>8,O_INDB>>8,O_STB>>8,O_MOV>>8,O_CPS>>8,BADOP>>8,BADOP>>8
        DEFB O_IXA>>8,O_CHK>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_ADD>>8,O_SUB>>8,O_MUL>>8,O_DIV>>8,O_MOD>>8,O_NEG>>8,O_ABS>>8,O_INC1>>8
        DEFB O_DEC1>>8,O_SQRI>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_EQU>>8,O_NEQ>>8,O_LES>>8,O_LEQ>>8,O_GRT>>8,O_GEQ>>8,BADOP>>8,BADOP>>8
        DEFB O_AND>>8,O_OR>>8,O_NOT>>8,O_ODD>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_UJPS>>8,O_FJPS>>8,O_UJP>>8,O_FJP>>8,O_XJP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB O_MST>>8,O_CUP>>8,O_ENT>>8,O_ENTW>>8,O_RET>>8,O_LFA>>8,BADOP>>8,BADOP>>8
        DEFB O_CSP>>8,BADOP>>8,BADOP>>8,BADOP>>8,O_DUP>>8,O_POP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,O_ERR>>8,O_HLT>>8
 IF CPM
        DEFB O_LDSE>>8,O_SGS>>8,O_SRS>>8,O_UNI>>8,O_INT>>8,O_DIF>>8,O_INN>>8,O_EQS>>8
        DEFB O_NES>>8,O_LSS>>8,O_GSS>>8,O_INDS>>8,O_STOS>>8,BADOP>>8,BADOP>>8,BADOP>>8
 ELSE
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
 ENDIF
        DEFB O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8
        DEFB O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8
        DEFB O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8
        DEFB O_REAL>>8,O_REAL>>8,O_REAL>>8,O_REAL>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8
        DEFB BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8,BADOP>>8

; NEXT: nästa instruktion. A = opkoden när rutinen börjar.
NEXT:   LD   A,(DE)
        INC  DE
        LD   L,A
        LD   H,TABL>>8
        LD   C,(HL)
        INC  H
        LD   H,(HL)
        LD   L,C
        JP   (HL)

; ---- konstanterna
O_LDCK: LD   L,A            ; LDC0-15: opkoden är konstanten
        LD   H,0
        PUSH HL
        JP   NEXT
O_LDCN: LD   HL,-1
        PUSH HL
        JP   NEXT
O_LDCB: LD   A,(DE)
        INC  DE
        LD   L,A
        LD   H,0
        PUSH HL
        JP   NEXT
O_LDCW: EX   DE,HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        EX   DE,HL
        PUSH HL
        JP   NEXT
O_LSA:  LD   A,(DE)         ; strängens adress, och förbi strängen
        INC  DE
        PUSH DE
        LD   L,A
        LD   H,0
        ADD  HL,DE
        EX   DE,HL
        JP   NEXT

; ---- de lokala: ord i på MP-2(i+1)
O_LDL0: LD   L,(IX-2)
        LD   H,(IX-1)
        PUSH HL
        JP   NEXT
O_LDL1: LD   L,(IX-4)
        LD   H,(IX-3)
        PUSH HL
        JP   NEXT
O_LDL2: LD   L,(IX-6)
        LD   H,(IX-5)
        PUSH HL
        JP   NEXT
O_LDL3: LD   L,(IX-8)
        LD   H,(IX-7)
        PUSH HL
        JP   NEXT
O_LDL4: LD   L,(IX-10)
        LD   H,(IX-9)
        PUSH HL
        JP   NEXT
O_LDL5: LD   L,(IX-12)
        LD   H,(IX-11)
        PUSH HL
        JP   NEXT
O_LDL6: LD   L,(IX-14)
        LD   H,(IX-13)
        PUSH HL
        JP   NEXT
O_LDL7: LD   L,(IX-16)
        LD   H,(IX-15)
        PUSH HL
        JP   NEXT
O_STL0: POP  HL
        LD   (IX-2),L
        LD   (IX-1),H
        JP   NEXT
O_STL1: POP  HL
        LD   (IX-4),L
        LD   (IX-3),H
        JP   NEXT
O_STL2: POP  HL
        LD   (IX-6),L
        LD   (IX-5),H
        JP   NEXT
O_STL3: POP  HL
        LD   (IX-8),L
        LD   (IX-7),H
        JP   NEXT
O_STL4: POP  HL
        LD   (IX-10),L
        LD   (IX-9),H
        JP   NEXT
O_STL5: POP  HL
        LD   (IX-12),L
        LD   (IX-11),H
        JP   NEXT
O_STL6: POP  HL
        LD   (IX-14),L
        LD   (IX-13),H
        JP   NEXT
O_STL7: POP  HL
        LD   (IX-16),L
        LD   (IX-15),H
        JP   NEXT

; LOKADR: HL = MP - 2(A+1)
LOKADR: LD   L,A
        LD   H,0
        INC  HL
        ADD  HL,HL
        LD   B,H
        LD   C,L
        PUSH IX
        POP  HL
        OR   A
        SBC  HL,BC
        RET

O_LDL:  LD   A,(DE)
        INC  DE
        CALL LOKADR
LADUP:  LD   A,(HL)         ; ordet på HL till stacken
        INC  HL
        LD   H,(HL)
        LD   L,A
        PUSH HL
        JP   NEXT
O_STL:  LD   A,(DE)
        INC  DE
        CALL LOKADR
SPARA:  POP  BC             ; ordet från stacken till HL
        LD   (HL),C
        INC  HL
        LD   (HL),B
        JP   NEXT
O_LLA:  LD   A,(DE)
        INC  DE
        CALL LOKADR
        PUSH HL
        JP   NEXT

; ---- de globala: ord w på GB+2w
O_LDGK: SUB  $30            ; LDG0-7
        ADD  A,A
        LD   L,A
        LD   H,0
        LD   BC,(GB)
        ADD  HL,BC
        JP   LADUP

GLOBADR: EX  DE,HL          ; HL = GB + 2w, w efter opkoden
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        EX   DE,HL
        ADD  HL,HL
        LD   BC,(GB)
        ADD  HL,BC
        RET

O_LDG:  CALL GLOBADR
        JP   LADUP
O_STG:  CALL GLOBADR
        JP   SPARA
O_LGA:  CALL GLOBADR
        PUSH HL
        JP   NEXT

; ---- de yttre: n nivåer ut längs den statiska länken, ord b
UTADR:  LD   A,(DE)
        INC  DE
        PUSH IX
        POP  HL
        OR   A
        JR   Z,UTADR2
UTADR1: LD   BC,4
        ADD  HL,BC
        LD   C,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,C
        DEC  A
        JR   NZ,UTADR1
UTADR2: PUSH HL
        LD   A,(DE)
        INC  DE
        LD   L,A
        LD   H,0
        INC  HL
        ADD  HL,HL
        LD   B,H
        LD   C,L
        POP  HL
        OR   A
        SBC  HL,BC
        RET

O_LOD:  CALL UTADR
        JP   LADUP
O_STR:  CALL UTADR
        JP   SPARA
O_LDA:  CALL UTADR
        PUSH HL
        JP   NEXT

; ---- minnet
O_IND:  POP  HL
        JP   LADUP
O_STO:  POP  BC
        POP  HL
        LD   (HL),C
        INC  HL
        LD   (HL),B
        JP   NEXT
O_INDB: POP  HL
        LD   L,(HL)
        LD   H,0
        PUSH HL
        JP   NEXT
O_STB:  POP  BC
        POP  HL
        LD   (HL),C
        JP   NEXT
O_MOV:  EX   DE,HL          ; till från -- (w bytes)
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        LD   (TMPIP),HL
        POP  HL
        POP  DE
        LD   A,B
        OR   C
        JR   Z,MOV1
        LDIR
MOV1:   LD   DE,(TMPIP)
        JP   NEXT

; IXA w: adr i -- adr+i*w
O_IXA:  EX   DE,HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        EX   DE,HL
        POP  HL
        LD   A,B
        OR   A
        JR   NZ,IXA3
        LD   A,C
        CP   1
        JR   Z,IXA2
        CP   2
        JR   NZ,IXA3
        ADD  HL,HL
        JR   IXA2
IXA3:   CALL MUL16
IXA2:   POP  BC
        ADD  HL,BC
        PUSH HL
        JP   NEXT

; CHK lo hi: värdet ligger kvar; körfel 1 om det är utanför
O_CHK:  POP  HL
        PUSH HL
        EX   DE,HL          ; HL = IP, DE = värdet
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        PUSH HL
        LD   H,D
        LD   L,E
        CALL SCMP           ; värdet < lo?
        POP  HL
        JR   C,CHKF1
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        PUSH HL
        LD   H,B
        LD   L,C
        LD   B,D
        LD   C,E
        CALL SCMP           ; hi < värdet?
        POP  HL
        EX   DE,HL
        JP   NC,NEXT
        JR   CHKF
CHKF1:  INC  HL
        INC  HL
        EX   DE,HL
CHKF:   LD   A,1
        JP   KORFEL

; SCMP: HL och BC med tecken; CY om HL < BC, Z om lika
SCMP:   LD   A,H
        XOR  $80
        LD   H,A
        LD   A,B
        XOR  $80
        LD   B,A
        OR   A
        SBC  HL,BC
        RET

; ---- aritmetiken
O_ADD:  POP  BC
        POP  HL
        ADD  HL,BC
        PUSH HL
        JP   NEXT
O_SUB:  POP  BC
        POP  HL
        OR   A
        SBC  HL,BC
        PUSH HL
        JP   NEXT
O_MUL:  POP  BC
        POP  HL
        CALL MUL16
        PUSH HL
        JP   NEXT
O_DIV:  POP  BC
        POP  HL
        CALL SDIV
        PUSH HL
        JP   NEXT
O_MOD:  POP  BC
        POP  HL
        CALL SDIV
        PUSH BC
        JP   NEXT
O_NEG:  POP  HL
        CALL NEGHL
        PUSH HL
        JP   NEXT
O_ABS:  POP  HL
        BIT  7,H
        CALL NZ,NEGHL
        PUSH HL
        JP   NEXT
O_INC1: POP  HL
        INC  HL
        PUSH HL
        JP   NEXT
O_DEC1: POP  HL
        DEC  HL
        PUSH HL
        JP   NEXT
O_SQRI: POP  HL
        LD   B,H
        LD   C,L
        CALL MUL16
        PUSH HL
        JP   NEXT

NEGHL:  XOR  A
        SUB  L
        LD   L,A
        SBC  A,A
        SUB  H
        LD   H,A
        RET

; MUL16: HL = HL * BC (de låga 16 bitarna); DE behålls
MUL16:  PUSH DE
        EX   DE,HL
        LD   HL,0
        LD   A,16
MULS1:   ADD  HL,HL
        SLA  C
        RL   B
        JR   NC,MUL2
        ADD  HL,DE
MUL2:   DEC  A
        JR   NZ,MULS1
        POP  DE
        RET

; UDIV: HL / BC utan tecken; kvoten i DE och resten i HL
UDIV:   EX   DE,HL
        LD   HL,0
        LD   A,16
UDIV1:  SLA  E
        RL   D
        ADC  HL,HL
        OR   A
        SBC  HL,BC
        JR   NC,UDIV2
        ADD  HL,BC
        JR   UDIV3
UDIV2:  INC  E
UDIV3:  DEC  A
        JR   NZ,UDIV1
        RET

; SDIV: HL / BC med tecken, mot noll; kvoten i HL och resten (med
; täljarens tecken) i BC. Körfel 2 om BC = 0. DE behålls.
SDIV:   LD   A,B
        OR   C
        JR   Z,DIVN0
        LD   (TMPIP),DE
        LD   A,H
        LD   (TSGN),A
        XOR  B
        PUSH AF             ; kvotens tecken i bit 7
        BIT  7,H
        CALL NZ,NEGHL
        BIT  7,B
        JR   Z,SDIV1
        XOR  A
        SUB  C
        LD   C,A
        SBC  A,A
        SUB  B
        LD   B,A
SDIV1:  CALL UDIV
        LD   A,(TSGN)
        BIT  7,A
        CALL NZ,NEGHL
        LD   B,H
        LD   C,L
        EX   DE,HL
        POP  AF
        BIT  7,A
        CALL NZ,NEGHL
        LD   DE,(TMPIP)
        RET
DIVN0:   LD   A,2
        JP   KORFEL

; ---- jämförelserna med tecken: a b -- boolesk
O_EQU:  POP  BC
        POP  HL
        OR   A
        SBC  HL,BC
        JR   Z,SANT
FALSKT: LD   HL,0
        PUSH HL
        JP   NEXT
SANT:   LD   HL,1
        PUSH HL
        JP   NEXT
O_NEQ:  POP  BC
        POP  HL
        OR   A
        SBC  HL,BC
        JR   NZ,SANT
        JR   FALSKT
O_LES:  POP  BC
        POP  HL
        CALL SCMP
        JR   C,SANT
        JR   FALSKT
O_GEQ:  POP  BC
        POP  HL
        CALL SCMP
        JR   NC,SANT
        JR   FALSKT
O_GRT:  POP  HL
        POP  BC
        CALL SCMP           ; b < a
        JR   C,SANT
        JR   FALSKT
O_LEQ:  POP  HL
        POP  BC
        CALL SCMP
        JR   NC,SANT
        JR   FALSKT

O_AND:  POP  BC
        POP  HL
        LD   A,L
        AND  C
BOOL:   AND  1
        LD   L,A
        LD   H,0
        PUSH HL
        JP   NEXT
O_OR:   POP  BC
        POP  HL
        LD   A,L
        OR   C
        JR   BOOL
O_NOT:  POP  HL
        LD   A,L
        CPL
        JR   BOOL
O_ODD:  POP  HL
        LD   A,L
        JR   BOOL

; ---- hoppen, relativa till adressen efter instruktionen
O_FJPS: POP  HL
        LD   A,H
        OR   L
        JR   Z,O_UJPS
        INC  DE
        JP   NEXT
O_UJPS: LD   A,(DE)
        INC  DE
        LD   L,A
        RLA
        SBC  A,A
        LD   H,A
        ADD  HL,DE
        EX   DE,HL
        JP   NEXT
O_FJP:  POP  HL
        LD   A,H
        OR   L
        JR   Z,O_UJP
        INC  DE
        INC  DE
        JP   NEXT
O_UJP:  EX   DE,HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        ADD  HL,BC
        EX   DE,HL
        JP   NEXT

; XJP lo hi tabell: i fallet v-lo till slutet + tabell[v-lo], annars
; vidare efter tabellen
O_XJP:  EX   DE,HL
        LD   C,(HL)         ; BC = lo
        INC  HL
        LD   B,(HL)
        INC  HL
        LD   E,(HL)         ; DE = hi
        INC  HL
        LD   D,(HL)
        INC  HL
        PUSH HL             ; tabellen
        EX   DE,HL
        OR   A
        SBC  HL,BC
        INC  HL
        EX   DE,HL          ; DE = n = hi-lo+1
        POP  HL
        EX   (SP),HL        ; HL = v, tabellen på stacken
        OR   A
        SBC  HL,BC          ; HL = k = v-lo
        PUSH HL
        OR   A
        SBC  HL,DE          ; CY om k < n (utan tecken)
        POP  BC
        POP  HL             ; tabellen
        JR   NC,XJP2
        PUSH HL
        ADD  HL,DE
        ADD  HL,DE          ; slutet
        EX   (SP),HL
        ADD  HL,BC
        ADD  HL,BC
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        POP  HL
        ADD  HL,BC
        EX   DE,HL
        JP   NEXT
XJP2:   ADD  HL,DE
        ADD  HL,DE
        EX   DE,HL
        JP   NEXT

; ---- anropen
; MST n: statisk länk (n nivåer ut från MP), dynamisk länk, återhopp 0
O_MST:  LD   A,(DE)
        INC  DE
        PUSH IX
        POP  HL
        OR   A
        JR   Z,MST2
MST1:   LD   BC,4
        ADD  HL,BC
        LD   C,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,C
        DEC  A
        JR   NZ,MST1
MST2:   PUSH HL
        PUSH IX
        LD   HL,0
        PUSH HL
        JP   NEXT
; CUP k p: MP = SP + k, återhoppet i ord[MP], till procedur p
O_CUP:  LD   A,(DE)
        INC  DE
        LD   L,A
        LD   H,0
        ADD  HL,SP
        PUSH HL
        POP  IX
        LD   A,(DE)
        INC  DE
        LD   (IX+0),E
        LD   (IX+1),D
        LD   L,A
        LD   H,0
        ADD  HL,HL
        LD   BC,(PTAB)
        ADD  HL,BC
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        JP   NEXT
; ENT n, ENTW n: n lokala ord, nollställda; körfel 3 om SP-2n < GRANS
O_ENT:  LD   A,(DE)
        INC  DE
        LD   C,A
        LD   B,0
        JR   ENT0
O_ENTW: EX   DE,HL
        LD   C,(HL)
        INC  HL
        LD   B,(HL)
        INC  HL
        EX   DE,HL
ENT0:   LD   HL,0
        ADD  HL,SP
        OR   A
        SBC  HL,BC
        JR   C,STFULL
        SBC  HL,BC
        JR   C,STFULL
        PUSH BC
        LD   BC,(GRANS)
        OR   A
        SBC  HL,BC
        POP  BC
        JR   C,STFULL
        LD   A,B
        OR   C
        JP   Z,NEXT
        LD   HL,0
ENT1:   PUSH HL
        DEC  BC
        LD   A,B
        OR   C
        JR   NZ,ENT1
        JP   NEXT
STFULL: LD   A,3
        JP   KORFEL
; RET: SP = MP+6 (funktionsvärdet ligger kvar), MP och IP tillbaka;
; återhoppet 0 är slutet
O_RET:  LD   E,(IX+0)
        LD   D,(IX+1)
        LD   L,(IX+2)
        LD   H,(IX+3)
        LD   BC,6
        ADD  IX,BC
        LD   SP,IX
        PUSH HL
        POP  IX
        LD   A,D
        OR   E
        JP   NZ,NEXT
        JP   SLUT
O_LFA:  PUSH IX
        POP  HL
        LD   BC,6
        ADD  HL,BC
        PUSH HL
        JP   NEXT

O_DUP:  POP  HL
        PUSH HL
        PUSH HL
        JP   NEXT
O_POP:  POP  HL
        JP   NEXT
O_ERR:  LD   A,(DE)
        INC  DE
        JP   KORFEL
O_HLT:  JP   SLUT
BADOP:  LD   A,6
        JP   KORFEL

; ---- standardprocedurerna
; CSP b går till BASIC:s stack; argumenten tas med POPA från p-stacken
; (PSP), och rutinen slutar med CSPX, eller CSPXH som lägger HL på den.
O_CSP:  LD   A,(DE)
        INC  DE
        LD   (TMPIP),DE
        LD   (MPS),IX
        LD   (PSP),SP
        LD   SP,(SAVESP)
        CP   15
        JR   NC,BADCSP
        ADD  A,A
        LD   L,A
        LD   H,0
        LD   BC,CSPTAB
        ADD  HL,BC
        LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A
        JP   (HL)
BADCSP:
 IF CPM
        CP   18             ; NEW, MARK och RELEASE
        JP   C,CSPST
 ENDIF
        LD   A,7
CFEL:   LD   DE,(TMPIP)
        JP   KORFEL
F5:     LD   A,5
        JR   CFEL

CSPTAB: DEFW C_WRC,C_WRI,C_WRS,C_WRB,C_WLN,C_RDC,C_RDI,C_RLN
        DEFW C_EOL,C_EOF,C_RST,C_RWR,C_CLO,C_RSV,C_RWV

POPA:   PUSH DE             ; HL = nästa argument
        LD   HL,(PSP)
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        INC  HL
        LD   (PSP),HL
        EX   DE,HL
        POP  DE
        RET

CSPX:   LD   SP,(PSP)
        LD   DE,(TMPIP)
        LD   IX,(MPS)
        JP   NEXT
CSPXH:  LD   SP,(PSP)
        PUSH HL
        LD   DE,(TMPIP)
        LD   IX,(MPS)
        JP   NEXT
SANTX:  LD   HL,1
        JR   CSPXH
FALSKX: LD   HL,0
        JR   CSPXH

; FILIX: HL = filen; IX = filposten och A = läget, körfel 5 om filen
; inte finns eller inte är öppen
FILIX:  LD   A,H
        OR   A
        JR   NZ,F5
        LD   A,L
        CP   3
        JR   NC,F5
        ADD  A,A
        ADD  A,A
        ADD  A,A
        LD   C,A
        LD   B,0
        LD   IX,FILT
        ADD  IX,BC
        LD   A,(IX+0)
        OR   A
        JR   Z,F5
        RET
SKRFIL: CALL FILIX          ; en fil att skriva till
        CP   1
        JR   Z,F5
        RET
LASFIL: CALL FILIX          ; en fil att läsa från
        CP   2
        JP   Z,F5
        RET

C_WRC:  CALL POPA           ; f tecken b --
        LD   (BRED),HL
        CALL POPA
        LD   A,L
        LD   (TBUF),A
        CALL POPA
        CALL SKRFIL
        LD   HL,TBUF
        LD   BC,1
SKRX:   CALL SKRIVB
        JP   CSPX
C_WRI:  CALL POPA           ; f tal b --
        LD   (BRED),HL
        CALL POPA
        LD   (TALV),HL
        CALL POPA
        CALL SKRFIL
        LD   HL,(TALV)
        CALL TALTXT
        JR   SKRX
C_WRS:  CALL POPA           ; f adr längd b --
        LD   (BRED),HL
        CALL POPA
        LD   (TALV),HL
        CALL POPA
        LD   (SADR),HL
        CALL POPA
        CALL SKRFIL
        LD   HL,(SADR)
        LD   BC,(TALV)
        JR   SKRX
C_WRB:  CALL POPA           ; f boolesk b --
        LD   (BRED),HL
        CALL POPA
        LD   (TALV),HL
        CALL POPA
        CALL SKRFIL
        LD   HL,(TALV)
        LD   A,H
        OR   L
        LD   HL,TXTFLS
        LD   BC,5
        JR   Z,SKRX
        LD   HL,TXTTRU
        LD   BC,4
        JR   SKRX
C_WLN:  CALL POPA           ; f --
        CALL SKRFIL
        CP   3
        JR   Z,WLN1
        LD   A,13
        CALL UTB
 IF CPM
        LD   A,10
        CALL UTB
 ENDIF
        JP   CSPX
WLN1:   LD   HL,CRLF
        LD   BC,2
        CALL SKRIV
        JP   CSPX

C_RDC:  CALL POPA           ; f -- tecken (32 vid radslut och filslut)
        CALL LASFIL
        CALL TA
        JR   C,RDC1
        CP   10
        JR   NZ,RDC2
RDC1:   LD   A,32
RDC2:   LD   L,A
        LD   H,0
        JP   CSPXH

C_RDI:  CALL POPA           ; f -- tal
        CALL LASFIL
RDI1:   CALL TITTA          ; blanka, radslut och tab först
        JR   C,RDI3
        CP   32
        JR   Z,RDI2
        CP   10
        JR   Z,RDI2
        CP   9
        JR   NZ,RDI3
RDI2:   CALL TA
        JR   RDI1
RDI3:   XOR  A
        LD   (TSGN),A
        LD   (SIFF),A
        LD   HL,0
        LD   (TALV),HL
        CALL TITTA
        JR   C,RDI5
        CP   '+'
        JR   Z,RDI4
        CP   '-'
        JR   NZ,RDI5
        LD   A,1
        LD   (TSGN),A
RDI4:   CALL TA
RDI5:   CALL TITTA
        JR   C,RDI6
        SUB  '0'
        JR   C,RDI6
        CP   10
        JR   NC,RDI6
        LD   HL,(TALV)
        LD   D,H
        LD   E,L
        ADD  HL,HL
        ADD  HL,HL
        ADD  HL,DE
        ADD  HL,HL
        LD   E,A
        LD   D,0
        ADD  HL,DE
        LD   (TALV),HL
        LD   A,1
        LD   (SIFF),A
        CALL TA
        JR   RDI5
RDI6:   LD   A,(SIFF)
        OR   A
        JP   Z,F5
        LD   HL,(TALV)
        LD   A,(TSGN)
        OR   A
        CALL NZ,NEGHL
        JP   CSPXH

C_RLN:  CALL POPA           ; f --: till och med radslutet
        CALL LASFIL
RLN1:   CALL TA
        JP   C,CSPX
        CP   10
        JR   NZ,RLN1
        JP   CSPX
C_EOL:  CALL POPA           ; f -- boolesk
        CALL LASFIL
        CALL TITTA
        JP   C,SANTX
        CP   10
        JP   Z,SANTX
        JP   FALSKX
C_EOF:  CALL POPA           ; f -- boolesk
        CALL LASFIL
        CALL TITTA
        JP   C,SANTX
        JP   FALSKX

; RESET och REWRITE: filvariabelns adr, namnets adr, längd --
C_RST:  LD   A,1
        LD   B,1
        JR   OPPNA
C_RWR:  LD   A,2
        LD   B,1
        JR   OPPNA
C_RSV:  LD   A,1
        LD   B,2
        JR   OPPNA
C_RWV:  LD   A,2
        LD   B,2
OPPNA:  LD   (OLAGE),A
        LD   A,B
        LD   (NSTEG),A
        CALL POPA
        LD   (TALV),HL
        CALL POPA
        LD   (SADR),HL
        CALL POPA
        LD   (FVAR),HL
        CALL NAMNIN
        LD   IX,FILT+8      ; en ledig kanal, 1 eller 2
        LD   A,(IX+0)
        OR   A
        LD   L,1
        JR   Z,OPPNA1
        LD   IX,FILT+16
        LD   A,(IX+0)
        OR   A
        LD   L,2
        JP   NZ,F5
OPPNA1: LD   H,0
        PUSH HL
        CALL INITF
 IF CPM
        CALL FCBIN
        LD   A,(OLAGE)
        CP   1
        JR   NZ,OPPNC2
        CALL FOPEN
        JP   C,F5
        JR   OPPNA3
OPPNC2: LD   C,19           ; REWRITE: bort med den gamla, och en ny
        CALL FBDOS
        LD   C,22
        CALL FBDOS
        INC  A
        JP   Z,F5
 ELSE
        LD   A,(OLAGE)
        CP   1
        JR   NZ,OPPNA2
        LD   HL,DOPEN
        LD   DE,FNAMN
        CALL DOSANR
        JR   NC,OPPNA3
        LD   HL,DCLOSE
        LD   DE,0
        CALL DOSANR
        JP   F5
OPPNA2: LD   HL,DPREP
        LD   DE,FNAMN
        CALL DOSK
        LD   HL,DSETRC
        LD   DE,0
        CALL DOSK
        LD   (IX+1),3
 ENDIF
OPPNA3: LD   A,(OLAGE)
        LD   (IX+0),A
        POP  DE
        LD   HL,(FVAR)
        LD   (HL),E
        INC  HL
        LD   (HL),D
        JP   CSPX

C_CLO:  CALL POPA           ; f --
        LD   A,H
        OR   L
        JP   Z,F5
        CALL FILIX
        CALL STANG
        JP   CSPX

; INITF: filposten IX för kanalen L, stängd
INITF:  XOR  A
        LD   (IX+0),A
        LD   (IX+1),A
        LD   (IX+2),A
        LD   (IX+3),A
        LD   (IX+4),A
        LD   (IX+5),A
        LD   A,L
        ADD  A,A
        ADD  A,A
        ADD  A,A
        ADD  A,A
        LD   (IX+6),A
        LD   A,L
 IF CPM
        ADD  A,-1+(FBUF>>8)
 ELSE
        ADD  A,$F5
 ENDIF
        LD   (IX+7),A
        RET

; STANG: stäng filen IX; en fil som skrivs får 0 och ETX sist (i CP/M
; fylls den sista posten ut med $1A)
STANG:
 IF CPM
        LD   A,(IX+0)
        LD   (IX+0),0
        CP   2
        RET  NZ             ; en fil som läses behöver inte stängas
STANC1: LD   A,(IX+1)
        OR   A
        JR   Z,STANC2
        LD   A,$1A
        CALL PUTB
        JR   STANC1
STANC2: LD   C,16
        CALL FBDOS
        INC  A
        RET  NZ
        JP   CPMFEL
 ELSE
        LD   A,(IX+0)
        LD   (IX+0),0
        CP   2
        JR   NZ,STANG1
        XOR  A
        CALL PUTB
        LD   L,(IX+1)
        LD   H,(IX+7)
        LD   (HL),3
        LD   HL,DWRREC
        LD   DE,0
        CALL DOSK
STANG1: LD   HL,DCLOSE
        LD   DE,0
        JP   DOSK
 ENDIF

; STALLA: stäng alla öppna filer, utan körfel
STALLA: LD   A,1
        LD   (STANGER),A
        LD   IX,FILT+8
        LD   A,(IX+0)
        OR   A
        CALL NZ,STANG
        LD   IX,FILT+16
        LD   A,(IX+0)
        OR   A
        CALL NZ,STANG
        XOR  A
        LD   (STANGER),A
        RET

; NAMNIN: FNAMN (8 + 3 tecken) från namnet på SADR, TALV tecken med
; NSTEG bytes per tecken. Blanka först hoppas över, och ett blanksteg
; slutar namnet; a-z blir A-Z, och för många tecken tappas.
NAMNIN: LD   HL,FNAMN
        LD   B,11
NI0:    LD   (HL),32
        INC  HL
        DJNZ NI0
        XOR  A
        LD   (NDOT),A
        LD   (NANT),A
 IF CPM
        LD   (FDRIVE),A
 ENDIF
        LD   A,8
        LD   (NGR),A
        LD   HL,(SADR)
        LD   BC,(TALV)
        LD   DE,FNAMN
NI1:    LD   A,B
        OR   C
        RET  Z
        LD   A,(HL)
        CP   33
        JR   NC,NI2
        CALL NINC
        JR   NI1
NI2:    LD   A,B
        OR   C
        RET  Z
        LD   A,(HL)
        CP   33
        RET  C
 IF CPM
        CP   ':'            ; B:NAMN, enheten
        JR   Z,NIDRV
 ENDIF
        CP   '.'
        JR   NZ,NI3
        LD   DE,FNAMN+8
        LD   A,3
        LD   (NGR),A
        LD   A,1
        LD   (NDOT),A
        XOR  A
        LD   (NANT),A
        JR   NI5
NI3:    CP   'a'
        JR   C,NI4
        CP   'z'+1
        JR   NC,NI4
        SUB  32
NI4:    PUSH HL
        LD   HL,NANT
        PUSH AF
        LD   A,(NGR)
        CP   (HL)
        JR   Z,NI6
        INC  (HL)
        POP  AF
        LD   (DE),A
        INC  DE
        POP  HL
        JR   NI5
NI6:    POP  AF
        POP  HL
NI5:    CALL NINC
        JR   NI2
NINC:   DEC  BC
        LD   A,(NSTEG)
        INC  HL
        DEC  A
        RET  Z
        INC  HL
        RET
 IF CPM
NIDRV:  LD   A,(FNAMN)      ; det som stod före : är enheten
        SUB  '@'
        LD   (FDRIVE),A
        PUSH HL
        LD   HL,FNAMN
        LD   A,8
NIDR1:  LD   (HL),32
        INC  HL
        DEC  A
        JR   NZ,NIDR1
        POP  HL
        LD   DE,FNAMN
        XOR  A
        LD   (NANT),A
        JR   NI5
 ENDIF

; ---- skrivningen
; SKRIVB: BC bytes från HL till filen IX, högerställda i BRED tecken
SKRIVB: PUSH HL
        PUSH BC
        LD   HL,(BRED)
        OR   A
        SBC  HL,BC
        JR   Z,SKRB2
        BIT  7,H
        JR   NZ,SKRB2
SKRB1:  PUSH HL
        LD   A,32
        CALL UTB
        POP  HL
        DEC  HL
        LD   A,H
        OR   L
        JR   NZ,SKRB1
SKRB2:  POP  BC
        POP  HL
        LD   A,B
        OR   C
        RET  Z
        LD   A,(IX+0)
        CP   3
        JP   Z,SKRIV
SKRB3:  LD   A,(HL)
        INC  HL
        PUSH HL
        PUSH BC
        CALL UTB
        POP  BC
        POP  HL
        DEC  BC
        LD   A,B
        OR   C
        JR   NZ,SKRB3
        RET

; SKRIV: PRTXT, med IX och DE kvar
SKRIV:  PUSH IX
        PUSH DE
        CALL PRTXT
        POP  DE
        POP  IX
        RET

; UTB: byten A till filen IX
UTB:    LD   C,A
        LD   A,(IX+0)
        CP   3
        LD   A,C
        JR   NZ,UTB1
        LD   (UBUF),A
        LD   HL,UBUF
        LD   BC,1
        JR   SKRIV
UTB1:
 IF 1-CPM
        OR   A
        JR   Z,UTB2
        CP   3
        JR   Z,UTB2
        CP   16
        JR   NZ,PUTB
UTB2:   PUSH AF
        LD   A,16
        CALL PUTB
        POP  AF
        ADD  A,64
 ENDIF
; PUTB: byten A till bufferten; ett fullt block (3-254) får ETX och skrivs
; (i CP/M: en full post om 128 bytes skrivs)
PUTB:
 IF CPM
        LD   L,(IX+1)
        LD   H,(IX+7)
        LD   (HL),A
        INC  L
        LD   (IX+1),L
        BIT  7,L
        RET  Z
        LD   (IX+1),0
        CALL FDMA
        LD   C,21
        CALL FBDOS
        OR   A
        RET  Z
        JP   CPMFEL
 ELSE
        LD   L,(IX+1)
        LD   H,(IX+7)
        LD   (HL),A
        INC  L
        LD   (IX+1),L
        LD   A,L
        CP   255
        RET  NZ
        LD   (HL),3
        LD   (IX+1),3
        LD   HL,DWRREC
        LD   DE,0
        JP   DOSK
 ENDIF

; TALTXT: talet HL med tecken som text; HL = texten och BC = längden
TALTXT: LD   A,H
        LD   (TSGN),A
        BIT  7,H
        CALL NZ,NEGHL
        LD   BC,NUMEND
        LD   (TPTR),BC
TT1:    LD   BC,10
        CALL UDIV
        LD   A,L
        ADD  A,'0'
        LD   HL,(TPTR)
        DEC  HL
        LD   (HL),A
        LD   (TPTR),HL
        EX   DE,HL
        LD   A,H
        OR   L
        JR   NZ,TT1
        LD   A,(TSGN)
        BIT  7,A
        JR   Z,TT2
        LD   HL,(TPTR)
        DEC  HL
        LD   (HL),'-'
        LD   (TPTR),HL
TT2:    LD   HL,NUMEND
        LD   DE,(TPTR)
        OR   A
        SBC  HL,DE
        LD   B,H
        LD   C,L
        EX   DE,HL
        RET

; ---- läsningen
; TITTA: tecknet som står på tur i filen IX, i A; CY vid filens slut.
; TA: detsamma, och tecknet är taget.
TITTA:  LD   A,(IX+2)
        CP   1
        JR   Z,TITTA2
        CP   2
        JR   Z,TITTA3
        CALL HAMTA
        JR   C,TITTA1
        LD   (IX+3),A
        LD   (IX+2),1
        RET
TITTA1: LD   (IX+2),2
        RET
TITTA2: LD   A,(IX+3)
        OR   A
        RET
TITTA3: SCF
        RET
TA:     CALL TITTA
        LD   (IX+2),0
        RET

; HAMTA: nästa tecken från tangentbordet eller textfilen, CR, CR LF och
; LF som 10; CY vid filens slut
HAMTA:  LD   A,(IX+0)
        CP   3
        JR   Z,KHAMTA
HAMTA1: CALL RAWB
        RET  C
        CP   13
        JR   NZ,HAMTA2
        LD   (IX+4),1
        LD   A,10
        OR   A
        RET
HAMTA2: LD   C,(IX+4)
        LD   (IX+4),0
        CP   10
        JR   NZ,HAMTA3
        DEC  C
        JR   Z,HAMTA1       ; LF efter CR
HAMTA3: OR   A
        RET

; KHAMTA: tangentbordet, en rad i taget med INLINE; radslutet blir 10
KHAMTA: LD   A,(KN)
        LD   C,A
        LD   A,(KP)
        CP   C
        JR   C,KHAMT1
        PUSH IX
        LD   HL,KBUF
        LD   C,80
        CALL INLINE
        LD   (HL),10
        PUSH HL
        LD   HL,CRLF
        LD   BC,2
        CALL PRTXT
        POP  HL
        POP  IX
        LD   DE,KBUF-1
        OR   A
        SBC  HL,DE
        LD   A,L
        LD   (KN),A
        XOR  A
KHAMT1: LD   E,A
        INC  A
        LD   (KP),A
        LD   D,0
        LD   HL,KBUF
        ADD  HL,DE
        LD   A,(HL)
        OR   A
        RET

; RAWB: nästa byte i filen IX, i A; CY vid filens slut. ETX går till
; nästa post, 0 är filens slut och 16 följs av byten + 64. (I CP/M är
; BP platsen i posten, och sidan+$CA antalet bytes i den.)
RAWB:
 IF CPM
        LD   A,(IX+5)
        OR   A
        SCF
        RET  NZ
        LD   H,(IX+7)
        LD   L,$CA
        LD   A,(IX+1)
        CP   (HL)
        JR   C,RAWC2
        CALL LASPOST
        JR   C,RAWSL
        XOR  A
RAWC2:  LD   L,A
        LD   H,(IX+7)
        LD   A,(HL)
        INC  L
        LD   (IX+1),L
        OR   A
        RET
 ELSE
        LD   A,(IX+5)
        OR   A
        SCF
        RET  NZ
        LD   A,(IX+1)
        OR   A
        JR   NZ,RAWB2
RAWB1:  LD   HL,DRDREC
        LD   DE,0
        CALL DOSANR
        JR   C,RAWSL
        LD   A,3
RAWB2:  LD   L,A
        LD   H,(IX+7)
        LD   A,(HL)
        INC  L
        LD   (IX+1),L
        CP   3
        JR   Z,RAWB1
        OR   A
        JR   Z,RAWSL
        CP   16
        JR   Z,RAWB3
        OR   A
        RET
RAWB3:  CALL RAWB
        RET  C
        SUB  64
        OR   A
        RET
 ENDIF
RAWSL:  LD   (IX+5),1
        SCF
        RET

 IF CPM
; ---- CP/M
; CBDOS: BDOS C med DE, IX behålls. FBDOS: med FCB:n för filen IX.
CBDOS:  PUSH IX
        CALL 5
        POP  IX
        RET
FBDOS:  LD   D,(IX+7)
        LD   E,$80
        JR   CBDOS
FDMA:   LD   D,(IX+7)       ; posten på filens sida
        LD   E,0
        LD   C,26
        JR   CBDOS
; CPMFEL: ett fel i en fil är körfel 5 (inte när filerna stängs efter ett
; fel)
CPMFEL: LD   A,(STANGER)
        OR   A
        RET  NZ
        JP   F5

; FCBIN: FCB:n för filen IX ur FDRIVE och FNAMN, resten 0; ingen post
; är läst
FCBIN:  LD   H,(IX+7)
        LD   L,$80
        LD   A,(FDRIVE)
        LD   (HL),A
        INC  L
        EX   DE,HL
        LD   HL,FNAMN
        LD   BC,11
        LDIR
        EX   DE,HL
        LD   B,24
FCB1:   LD   (HL),0
        INC  L
        DJNZ FCB1
        LD   L,$C8
        LD   (HL),B
        INC  L
        LD   (HL),B
        INC  L
        LD   (HL),B
        RET

; FOPEN: öppna filen IX för läsning, med antalet poster (BDOS 35) på
; sidan+$C8; CY om den inte finns
FOPEN:  LD   C,15
        CALL FBDOS
        INC  A
        SCF
        RET  Z
        LD   C,35
        CALL FBDOS
        LD   H,(IX+7)
        LD   L,$A1          ; FCB:ns r0 och r1
        LD   E,(HL)
        INC  L
        LD   D,(HL)
        LD   L,$C8
        LD   (HL),E
        INC  L
        LD   (HL),D
        OR   A
        RET

; LASPOST: nästa post i filen IX till sidan. Antalet bytes i den blir
; 128, eller i den sista posten 128 utan $1A sist. CY vid filens slut.
LASPOST: LD  H,(IX+7)
        LD   L,$C8
        LD   E,(HL)
        INC  L
        LD   D,(HL)
        LD   A,D
        OR   E
        SCF
        RET  Z
        DEC  DE
        LD   (HL),D
        DEC  L
        LD   (HL),E
        CALL FDMA
        LD   C,20
        CALL FBDOS
        OR   A
        SCF
        RET  NZ
        LD   H,(IX+7)
        LD   L,$C8
        LD   A,(HL)
        INC  L
        OR   (HL)
        LD   B,128
        JR   NZ,LASP2
        LD   L,127
LASP1:  LD   A,(HL)
        CP   $1A
        JR   NZ,LASP2
        DEC  L
        DJNZ LASP1
LASP2:  LD   L,$CA
        LD   (HL),B
        LD   A,B
        OR   A
        SCF
        RET  Z
        CCF
        RET

; CSKRIV: BC tecken från HL till skärmen
CSKRIV: LD   A,B
        OR   C
        RET  Z
        PUSH BC
        PUSH HL
        LD   E,(HL)
        LD   C,2
        CALL CBDOS
        POP  HL
        POP  BC
        INC  HL
        DEC  BC
        JR   CSKRIV

; CRAD: en rad från tangentbordet till HL, högst C tecken; HL = slutet
CRAD:   PUSH HL
        LD   A,C
        LD   (CBUF),A
        LD   DE,CBUF
        LD   C,10
        CALL CBDOS
        POP  DE
        LD   HL,CBUF+1
        LD   C,(HL)
        LD   B,0
        INC  HL
        LD   A,C
        OR   A
        JR   Z,CRAD1
        LDIR
CRAD1:  EX   DE,HL
        RET
 ELSE
; ---- DOS:en
; DOSANR: DOS-rutinen HL med DE och kanalen i filposten IX (B = kanal
; * 16, C = 255: vilken enhet som helst). CY vid fel; ett fel som går
; till DOS:ens felutgång kommer också hit (DERRH). IX behålls.
DOSANR: LD   (DJMP+1),HL
        LD   B,(IX+6)
        PUSH IX
        LD   (DSP),SP
        LD   HL,(DERRX)
        LD   (DERR0),HL
        LD   HL,DERRH
        LD   (DERRX),HL
        LD   C,255
        CALL DJMP
DOSUT:  LD   HL,(DERR0)
        LD   (DERRX),HL
        POP  IX
        RET
DERRH:  LD   SP,(DSP)
        SCF
        JR   DOSUT
DJMP:   JP   0

; DOSK: som DOSANR, men ett fel är körfel 5 (inte när filerna stängs
; efter ett fel)
DOSK:   CALL DOSANR
        RET  NC
        LD   A,(STANGER)
        OR   A
        RET  NZ
        JP   F5
 ENDIF

; SVANS: det som står efter programnamnet (HL, BC tecken kvar efter
; NAMNIN) flyttas först i KBUF med 10 sist, så att det blir den första
; raden som programmet läser från tangentbordet; KSVANS är antalet.
 IF CPM
SVANS:  LD   A,B
        OR   C
        JR   Z,SVANS1
        LD   A,(HL)
        CP   33
        JR   NC,SVANS1
        INC  HL
        DEC  BC
        JR   SVANS
SVANS1: LD   A,C            ; högst 40 tecken, så B är 0
        LD   (KSVANS),A
        OR   A
        RET  Z
        LD   DE,KBUF
        LDIR
        EX   DE,HL
        LD   (HL),10
        LD   HL,KSVANS
        INC  (HL)
        RET
 ENDIF

; ---- början och slutet
START:
 IF CPM
        LD   SP,CSTK        ; återhoppet är 0, en varmstart
        LD   HL,0
        PUSH HL
        LD   HL,(6)         ; p-stacken under BDOS
        LD   L,0
        LD   (TOP),HL
 ENDIF
        LD   (SAVESP),SP
        XOR  A
        LD   (STANGER),A
        LD   HL,FILT
        LD   B,24
START1: LD   (HL),A
        INC  HL
        DJNZ START1
 IF CPM
        LD   A,($80)        ; PVM NAMN: namnet efter kommandot
        OR   A
        JR   Z,STARC1
        CP   41
        JR   C,STARC0
        LD   A,40
STARC0: LD   C,A
        LD   B,0
        LD   HL,$81
        LD   DE,KBUF
        LDIR
        EX   DE,HL
        JR   STARC2
STARC1:
 ENDIF
        LD   HL,TXTPRG
        LD   BC,9
        CALL SKRIV
        LD   HL,KBUF
        LD   C,40
        CALL INLINE
        PUSH HL
        LD   HL,CRLF
        LD   BC,2
        CALL PRTXT
        POP  HL
 IF CPM
STARC2:
 ENDIF
        LD   DE,KBUF
        OR   A
        SBC  HL,DE
        LD   (TALV),HL
        LD   (SADR),DE
        LD   A,1
        LD   (NSTEG),A
        CALL NAMNIN
 IF CPM
        CALL SVANS
 ENDIF
        LD   A,(FNAMN)
        CP   32
        JP   Z,TILLB
        LD   A,(NDOT)
        OR   A
        JR   NZ,START2
        LD   HL,TXTPK
        LD   DE,FNAMN+8
        LD   BC,3
        LDIR
START2: LD   HL,KOD
        CALL LADDA
        LD   HL,(LPOS)      ; minst "PK", version, segment och 4 bytes
        LD   DE,KOD+8
        OR   A
        SBC  HL,DE
        JP   C,INTEPK
        LD   HL,(KOD)
        LD   DE,'P'+256*'K'
        OR   A
        SBC  HL,DE
        JP   NZ,INTEPK
        LD   A,(KOD+2)
 IF CPM
        DEC  A              ; version 1 eller 2 (mängderna och högen)
        CP   2
        JP   NC,INTEPK
 ELSE
        CP   1
        JP   NZ,INTEPK
 ENDIF
        LD   HL,(LPOS)      ; sist globalernas bytes och antalet procedurer
        DEC  HL
        LD   B,(HL)
        LD   A,B            ; bit 15: REAL
        LD   (RFLAG),A
        RES  7,B
        DEC  HL
        LD   C,(HL)
        DEC  HL
        LD   D,(HL)
        DEC  HL
        LD   E,(HL)
        LD   (GLOB),DE
        OR   A
        SBC  HL,BC
        SBC  HL,BC
        LD   (PTAB),HL
LAS3:   LD   A,B            ; tabellen till adresser: KOD + 4 + offset
        OR   C
        JR   Z,LAS4
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        DEC  HL
        PUSH HL
        LD   HL,KOD+4
        ADD  HL,DE
        EX   DE,HL
        POP  HL
        LD   (HL),E
        INC  HL
        LD   (HL),D
        INC  HL
        DEC  BC
        JR   LAS3
LAS4:   LD   HL,(LPOS)      ; GB = slutet, jämnt
        INC  HL
        RES  0,L
        LD   (GB),HL
        LD   DE,(GLOB)
        ADD  HL,DE
        JR   C,RYMS
 IF CPM
        LD   (HOG0),HL
        LD   (HOG),HL
 ENDIF
        LD   DE,MARG
        ADD  HL,DE
        JR   C,RYMS
        LD   (GRANS),HL
        PUSH HL
        CALL LADFPP
        POP  HL
        LD   DE,(TOP)
        OR   A
        SBC  HL,DE
        JR   NC,RYMS
        LD   HL,(GB)        ; de globala nollställs
        LD   BC,(GLOB)
LAS5:   LD   A,B
        OR   C
        JR   Z,LAS6
        LD   (HL),0
        INC  HL
        DEC  BC
        JR   LAS5
LAS6:   LD   A,3            ; fil 0: tangentbordet och skärmen
        LD   (FILT),A
        XOR  A
        LD   (KP),A
 IF CPM
        LD   A,(KSVANS)     ; PVM PC HEJ.PAS: HEJ.PAS är den första raden
 ENDIF
        LD   (KN),A
        LD   SP,(TOP)       ; märket för huvudprogrammet
        LD   HL,0
        PUSH HL
        PUSH HL
        PUSH HL
        LD   IX,0
        ADD  IX,SP
        LD   HL,(PTAB)
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        JP   NEXT

LSTANG:
 IF 1-CPM
        LD   HL,DCLOSE
        LD   DE,0
        CALL DOSANR
 ENDIF
        LD   (IX+0),0
        RET

INTEPK: LD   HL,TXTIPK
        LD   BC,16
        JR   LMED
RYMS:   LD   HL,TXTRYM
        LD   BC,20
LMED:   CALL SKRIV
        LD   HL,CRLF
        LD   BC,2
        CALL SKRIV
TILLB:  LD   SP,(SAVESP)
        LD   HL,0
        RET

; LADDA: filen FNAMN till HL och framåt, högst till TOPP; LPOS = slutet
LADDA:  LD   (LPOS),HL
        LD   IX,FILT+8
        LD   L,1
        CALL INITF
 IF CPM
        CALL FCBIN
        CALL FOPEN
 ELSE
        LD   HL,DOPEN
        LD   DE,FNAMN
        CALL DOSANR
 ENDIF
        JR   NC,LADDA1
        CALL LSTANG
        LD   HL,TXTFF
        LD   BC,16
        JP   LMED
LADDA1: CALL RAWB
        JR   C,LSTANG
        LD   HL,(LPOS)
        LD   (HL),A
        INC  HL
        LD   (LPOS),HL
 IF CPM
        LD   DE,(TOP)
 ELSE
        LD   DE,TOPP
 ENDIF
        OR   A
        SBC  HL,DE
        JR   C,LADDA1
        CALL LSTANG
        JP   RYMS
; SLUT: programmet är slut; filerna stängs och CALL ger 0
SLUT:   LD   SP,(SAVESP)
        CALL STALLA
        LD   HL,0
        RET

; KORFEL: körfel A, med IP i DE. "KÖRFEL n: text, PC hhhh" på en egen
; rad; filerna stängs och CALL ger n.
KORFEL: LD   (FELNR),A
        LD   (FELPC),DE
        LD   SP,(SAVESP)
        LD   HL,TXTKF
        LD   BC,9
        CALL SKRIV
        LD   A,(FELNR)
        LD   L,A
        LD   H,0
        CALL TALTXT
        CALL SKRIV
        LD   HL,TXTKOL
        LD   BC,2
        CALL SKRIV
        LD   A,(FELNR)
        DEC  A
 IF CPM
        CP   9
 ELSE
        CP   8
 ENDIF
        LD   HL,TXTQ
        JR   NC,KORF1
        ADD  A,A
        LD   L,A
        LD   H,0
        LD   DE,FELTAB
        ADD  HL,DE
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        EX   DE,HL
KORF1:  LD   C,(HL)
        LD   B,0
        INC  HL
        CALL SKRIV
        LD   HL,TXTPC
        LD   BC,5
        CALL SKRIV
        LD   DE,NUMBUF
        LD   A,(FELPC+1)
        CALL HEX2
        LD   A,(FELPC)
        CALL HEX2
        LD   HL,NUMBUF
        LD   BC,4
        CALL SKRIV
        LD   HL,CRLF
        LD   BC,2
        CALL SKRIV
        CALL STALLA
        LD   A,(FELNR)
        LD   L,A
        LD   H,0
        RET

HEX2:   PUSH AF
        RRCA
        RRCA
        RRCA
        RRCA
        CALL HEX1
        POP  AF
HEX1:   AND  15
        ADD  A,'0'
        CP   '9'+1
        JR   C,HEX3
        ADD  A,7
HEX3:   LD   (DE),A
        INC  DE
        RET

 IF CPM
; ---- mängderna (PASCAL.md, avsnitt 9): 32 bytes på stacken, byte 0
; lägst (vid SP), element i är bit i&7 i byte i>>3. Opkoderna som rör
; DE har IP i TMPIP under tiden.
O_LDSE: LD   HL,0           ; -- m
        LD   B,16
LDSE1:  PUSH HL
        DJNZ LDSE1
        JP   NEXT
O_SGS:  POP  HL             ; i -- m: [i..i]
        PUSH HL
        PUSH HL
O_SRS:  POP  HL             ; lo hi -- m; tom om hi < lo
        POP  BC
        PUSH HL
        CALL SCMP           ; hi < lo?
        POP  HL
        JR   C,O_LDSE
        LD   A,B            ; lo och hi i 0..255, annars körfel 1
        XOR  $80            ; (SCMP vände B:s tecken)
        LD   B,A
        OR   H
        JP   NZ,CHKF
        LD   (TMPIP),DE
        LD   D,L            ; D = hi, E = lo
        LD   E,C
        LD   HL,0
        LD   B,16
SRS1:   PUSH HL
        DJNZ SRS1
SRS2:   LD   HL,0
        ADD  HL,SP
        LD   A,E
        CALL BITM
        OR   (HL)
        LD   (HL),A
        LD   A,E
        CP   D
        JR   Z,MNEXT
        INC  E
        JR   SRS2
MNEXT:  LD   DE,(TMPIP)
        JP   NEXT
; BITM: HL = mängden, A = elementet; HL = byten och A = biten
BITM:   PUSH BC
        LD   C,A
        RRCA
        RRCA
        RRCA
        AND  31
        ADD  A,L
        LD   L,A
        JR   NC,BITM1
        INC  H
BITM1:  LD   A,C
        AND  7
        LD   B,A
        LD   A,1
        JR   Z,BITM3
BITM2:  RLCA
        DJNZ BITM2
BITM3:  POP  BC
        RET
; MPAR: a b på stacken (b överst); DE = a, HL = b, B = 32
MPAR:   LD   (TMPIP),DE
        LD   HL,34          ; över återhoppet och b
        ADD  HL,SP
        EX   DE,HL
        LD   HL,2
        ADD  HL,SP
        LD   B,32
        RET
O_UNI:  CALL MPAR           ; a b -- a+b
UNI1:   LD   A,(DE)
        OR   (HL)
        LD   (DE),A
        INC  HL
        INC  DE
        DJNZ UNI1
        JR   MEND
O_INT:  CALL MPAR           ; a b -- a*b
INT1:   LD   A,(DE)
        AND  (HL)
        LD   (DE),A
        INC  HL
        INC  DE
        DJNZ INT1
        JR   MEND
O_DIF:  CALL MPAR           ; a b -- a-b
DIF1:   LD   A,(HL)
        CPL
        LD   C,A
        LD   A,(DE)
        AND  C
        LD   (DE),A
        INC  HL
        INC  DE
        DJNZ DIF1
MEND:   LD   SP,HL          ; HL = a
        JP   MNEXT
O_EQS:  LD   C,0            ; a b -- boolesk
        JR   EQS0
O_NES:  LD   C,1
EQS0:   CALL MPAR
EQS1:   LD   A,(DE)
        CP   (HL)
        JR   NZ,EQS2
        INC  HL
        INC  DE
        DJNZ EQS1
        LD   A,1
        JR   EQS3
EQS2:   XOR  A
EQS3:   XOR  C
        JR   MBOOL
O_LSS:  CALL MPAR           ; a <= b: a ingår i b
LSS1:   LD   A,(HL)
        CPL
        LD   C,A
        LD   A,(DE)
        AND  C
        JR   NZ,MFALSK
        INC  HL
        INC  DE
        DJNZ LSS1
        JR   MSANT
O_GSS:  CALL MPAR           ; a >= b: b ingår i a
GSS1:   LD   A,(DE)
        CPL
        LD   C,A
        LD   A,(HL)
        AND  C
        JR   NZ,MFALSK
        INC  HL
        INC  DE
        DJNZ GSS1
MSANT:  LD   A,1
        JR   MBOOL
MFALSK: XOR  A
MBOOL:  LD   HL,64          ; de två mängderna bort, A som boolesk
        ADD  HL,SP
        LD   SP,HL
        LD   L,A
        LD   H,0
        PUSH HL
        JP   MNEXT
O_INN:  LD   HL,32          ; i m -- boolesk
        ADD  HL,SP
        LD   C,(HL)
        INC  HL
        LD   A,(HL)
        OR   A
        LD   A,0
        JR   NZ,INN1        ; i utanför 0..255
        LD   HL,0
        ADD  HL,SP
        LD   A,C
        CALL BITM
        AND  (HL)
        JR   Z,INN1
        LD   A,1
INN1:   LD   HL,34
        ADD  HL,SP
        LD   SP,HL
        LD   L,A
        LD   H,0
        PUSH HL
        JP   NEXT
O_INDS: LD   (TMPIP),DE     ; adr -- m
        POP  HL
        LD   BC,30
        ADD  HL,BC
        LD   B,16
INDS1:  LD   E,(HL)
        INC  HL
        LD   D,(HL)
        DEC  HL
        DEC  HL
        DEC  HL
        PUSH DE
        DJNZ INDS1
        JP   MNEXT
O_STOS: LD   (TMPIP),DE     ; adr m --
        LD   HL,32
        ADD  HL,SP
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        LD   HL,0
        ADD  HL,SP
        LD   BC,32
        LDIR
        INC  HL
        INC  HL
        LD   SP,HL
        JP   MNEXT
 ENDIF

 IF CPM
; ---- högen: från de globalas slut uppåt mot stacken. NEW flyttar
; HOG och GRANS uppåt, RELEASE tillbaka; körfel 9 om den inte räcker.
CSPST:  SUB  15             ; CSP 15-17 (BADCSP hoppar hit)
        JP   Z,C_NEW
        DEC  A
        JP   Z,C_MRK
        JP   C_REL
C_NEW:  CALL POPA           ; pekarens adr bytes --
        LD   B,H
        LD   C,L
        CALL POPA
        PUSH HL
        LD   HL,(HOG)
        ADD  HL,BC
        JR   C,HFULL
        PUSH HL
        LD   DE,MARG
        ADD  HL,DE
        JR   C,HFULL
        LD   DE,(PSP)       ; HOG + bytes + MARG över stacken?
        OR   A
        SBC  HL,DE
        JR   NC,HFULL
        POP  DE             ; den nya toppen
        LD   HL,(HOG)
        LD   (HOG),DE
        EX   DE,HL          ; DE = den nya posten
        POP  HL
        LD   (HL),E         ; till pekaren
        INC  HL
        LD   (HL),D
        LD   A,B            ; nollställd
        OR   C
        JR   Z,HGRANS
        EX   DE,HL
        LD   (HL),0
        DEC  BC
        LD   A,B
        OR   C
        JR   Z,HGRANS
        LD   D,H
        LD   E,L
        INC  DE
        LDIR
HGRANS: LD   HL,(HOG)
        LD   DE,MARG
        ADD  HL,DE
        LD   (GRANS),HL
        JP   CSPX
HFULL:  LD   A,9
        JP   CFEL
C_MRK:  CALL POPA           ; pekarens adr --
        LD   DE,(HOG)
        LD   (HL),E
        INC  HL
        LD   (HL),D
        JP   CSPX
C_REL:  CALL POPA           ; pekare --; HOG0 <= pekaren <= HOG
        EX   DE,HL
        LD   HL,(HOG)
        OR   A
        SBC  HL,DE
        JR   C,HFULL
        LD   HL,(HOG0)
        EX   DE,HL
        OR   A
        SBC  HL,DE
        JR   C,HFULL
        ADD  HL,DE
        LD   (HOG),HL
        JR   HGRANS
 ENDIF

; ---- texterna (ABC: | = ö, { = ä, } = å, \ = Ö)
TXTPRG: DEFM "Program: "
TXTPK:  DEFM "PK "
TXTFF:  DEFM "filen finns inte"
TXTIPK: DEFM "inte en p-kodfil"
TXTRYM: DEFM "programmet ryms inte"
TXTTRU: DEFM "TRUE"
TXTFLS: DEFM "FALSE"
CRLF:   DEFB 13,10
TXTKF:  DEFB 13,10
        DEFM "K\RFEL "
TXTKOL: DEFM ": "
TXTPC:  DEFM ", PC "
TXTQ:   DEFB 1
        DEFM "?"
FELTAB: DEFW FEL1,FEL2,FEL3,FEL4,FEL5,FEL6,FEL7,FEL8
 IF CPM
        DEFW FEL9
FEL9:   DEFB 15
        DEFM "minnet {r fullt"
 ENDIF
FEL1:   DEFB 23
        DEFM "index utanf|r gr{nserna"
FEL2:   DEFB 17
        DEFM "division med noll"
FEL3:   DEFB 15
        DEFM "stacken {r full"
FEL4:   DEFB 14
        DEFM "CASE utan fall"
FEL5:   DEFB 5
        DEFM "filen"
FEL6:   DEFB 17
        DEFM "ok{nd instruktion"
FEL7:   DEFB 22
        DEFM "ok{nd standardprocedur"
FEL8:   DEFB 11
        DEFM "flyttalsfel"

NUMBUF: DEFS 7,0
NUMEND:
KBUF:   DEFS 82,0
 IF CPM
FDRIVE: DEFB 0              ; enheten i FCB:n, 0 = den aktuella
CBUF:   DEFB 0,0            ; BDOS 10: högst, antalet och tecknen
        DEFS 82,0
        DEFS 256,0          ; stacken för CSP, FPP och BDOS
CSTK:
        DEFS (256-($&255))&255,0
FBUF:   DEFS 512,0          ; sidorna för fil 1 och 2
 ENDIF
TOLKSLUT:
; bygg.sh kontrollerar att TOLKSLUT <= KOD
