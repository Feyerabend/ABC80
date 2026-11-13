( KALKYL: kalkylprogrammet. gor.sh skriver in filen )
( i Forth med gränsen mot BASIC på D800, och sparar KALKYL.ABS )
( Tre delar: den fasta delen, överläggens område och en fast del )
( till. Två överlägg delar området: 1 kompilatorn och SATT, 2 )
( VISA, filerna och utskriften. De ligger på RAM-skivan och hämtas med )
( RMOVE när de behövs; se Överläggen nedan. )
( Exekveringstoken som formlerna kompileras med )
' CR 1+ @ CONSTANT DOCOL  ' LIT CONSTANT XLIT
' EXIT CONSTANT XEXIT  ' BRANCH CONSTANT XBR
' 0BRANCH CONSTANT X0BR  ' 0= CONSTANT X0=
' F@ CONSTANT XF@  ' F! CONSTANT XF!  ' FDROP CONSTANT XFDROP
' F+ CONSTANT X+  ' F- CONSTANT X-  ' F* CONSTANT X*
' F/ CONSTANT X/  ' FNEGATE CONSTANT XNEG
' F< CONSTANT XF<  ' F= CONSTANT XF=  ' FSWAP CONSTANT XFSWAP
( Ett tal i koden: FLIT och de 8 bytena )
: (FLIT)  R> DUP 8 + >R F@ ;  ' (FLIT) CONSTANT XFLIT
: FMAX  FOVER FOVER F< IF FSWAP THEN FDROP ;
: FMIN  FOVER FOVER F< 0= IF FSWAP THEN FDROP ;
' FMAX CONSTANT XMAX  ' FMIN CONSTANT XMIN
( Cellerna, 28 byte: +0 namnets längd, +1 namnet, +9 typen )
( 0 tal, 1 formel, 2 rubrik, 3 räknefel, 4 trasig formel; )
( +10 koden, +12 värdet, +20 cellerna formeln läser, en bit )
( var. Plats CMAX är en extra, när celler flyttas. )
28 CONSTANT CL  40 CONSTANT CMAX  VARIABLE NC  VARIABLE KBAS
CREATE CELLER CMAX 1+ CL * ALLOT
( Mallarna till FORMAT, som i PRINT USING: 12 byte per cell, )
( längden först [0 = ingen]. Plats CMAX 1+ är den som nya )
( celler får. )
CREATE MALLAR CMAX 2 + 12 * ALLOT  MALLAR CMAX 2 + 12 * 0 FILL
: CMSK ( i -- a )  12 * MALLAR + ;
( CELL: 28 i = 4 7 i, med 2* som går fortare än * )
: CELL ( i -- a )  DUP 2* 2* 2* SWAP - 2* 2* CELLER + ;
: CTYP  CELL 9 + ;  : CXT  CELL 10 + ;
: CVAL  CELL 12 + ;  : CDEP  CELL 20 + ;
: CSOK ( a n -- i | -1 )  -1 NC @ 0> IF NC @ 0 DO
  OVER I CELL C@ = IF 2 PICK I CELL 1+ 3 PICK SAME
  IF DROP I LEAVE THEN THEN LOOP THEN >R 2DROP R> ;
( Formeln: KA KL texten, KP platsen, KFEL felet, KFP där )
( felet hittades, KD talen på flyttalsstacken )
VARIABLE KA  VARIABLE KL  VARIABLE KP  VARIABLE KD
VARIABLE KFEL  VARIABLE KFP  CREATE KDEP 8 ALLOT
CREATE KFN 34 ALLOT  : KB  KFN 1+ ;
: FEL ( n -- )  KFEL @ IF DROP ELSE KFEL ! KP @ KFP ! THEN ;
: UPC  DUP 96 > OVER 126 < AND IF 32 - THEN ;
( Nästa tecken med versaler, 0 sist eller efter ett fel )
: CH ( -- c )  KFEL @ IF 0 EXIT THEN
  KP @ KL @ < IF KA @ KP @ + C@ UPC ELSE 0 THEN ;
: >>  1 KP +! ;
: BL?  BEGIN CH 32 = WHILE >> REPEAT ;
: TECKEN? ( c -- f )  BL? CH = DUP IF >> THEN ;
: BOKST? ( c -- f )  DUP 64 > SWAP 94 < AND ;
: SIFF? ( c -- f )  48 - 10 U< ;
: +D  1 KD +!  KD @ 16 > IF 15 FEL THEN ;
: -D  -1 KD +! ;
( Ordet eller talet till KB, n tecken )
: TKN ( n c -- n+1 )  OVER 32 < IF OVER KB + C! ELSE DROP THEN
  1+ >> ;
: NAMN ( -- n )  0 BEGIN CH DUP BOKST? OVER SIFF? OR
  WHILE TKN REPEAT DROP ;
CREATE BITAR 1 C, 2 C, 4 C, 8 C, 16 C, 32 C, 64 C, 128 C,
: BIT ( n -- b )  BITAR + C@ ;
( Bitmängder: bit i i a, 8 byte )
( BYTN: i/8 som tabell, eftersom / tar tid )
CREATE BYTN 48 ALLOT
: FYLL  48 0 DO I 8 / BYTN I + C! LOOP ;  FYLL
: SBIT ( i a -- )  OVER BYTN + C@ + SWAP 7 AND BIT OVER C@ OR
  SWAP C! ;
: IBIT ( i a -- b )  OVER BYTN + C@ + C@ SWAP 7 AND BIT AND ;
: CNAMN ( n -- i | -1 )  DUP 8 > IF DROP 7 FEL -1 EXIT THEN
  KB SWAP CSOK DUP 0< IF 1 FEL THEN ;
( KSTART: formeln a n, från början )
: KSTART ( a n -- )  KL ! KA ! 0 KP ! 0 KFEL ! 0 KD !
  KDEP 8 0 FILL ;
( Felens texter i FTT, räknade och i ordning från 1. TXT" lägger )
( en räknad text i ordlistan; VERK använder det också. )
: TXT"  32 PARSE 2DROP 34 PARSE  HERE OVER 1+ ALLOT
  2DUP C! 1+ SWAP CMOVE ;
CREATE FTT  TXT" okänt namn"  TXT" okänd funktion"  TXT" ) saknas"
  TXT" ; saknas"  TXT" tal eller namn saknas"
  TXT" en rubrik har inget värde"  TXT" för långt namn eller tal"
  TXT" ^ finns inte än"  TXT" < > = <> <= eller >= saknas"
  TXT" tecken efter formeln"  TXT" inga värden"  TXT" fel område"
  TXT" minnet räcker inte"  TXT" fel i talet"
  TXT" formeln är för djup"  TXT" för många celler"  TXT" fel namn"
: FT ( n -- )  FTT SWAP BEGIN 1- ?DUP WHILE SWAP DUP C@ + 1+ SWAP
  REPEAT DUP 1+ SWAP C@ TYPE ;
: FELV  ." FEL: " KFEL @ FT ."  vid tecken " KFP @ 1+ . ;
( Texterna på RAM-skivan: cell i på 27392 + 80 i, längden )
( först. THAMT lägger texten i TIB, så den används bara )
( av ord som har läst hela raden. )
: TXT ( i -- ra )  80 * 27392 + ;
: TSPAR ( a n i -- )  TXT 2DUP RC! 1+ SWAP RMOVE ;
: THAMT ( i -- a n )  TXT DUP RC@ >R 1+ TIB R@ RMOVE TIB R> ;
: RTYPE ( i -- )  TXT DUP RC@ ?DUP IF 0 DO
  DUP I + 1+ RC@ EMIT LOOP THEN DROP ;
: FLYTTA ( från till -- )  OVER CMSK OVER CMSK 12 CMOVE
  OVER CELL OVER CELL CL CMOVE
  SWAP TXT SWAP TXT 80 RMOVE ;
: CN. ( i -- )  CELL DUP 1+ SWAP C@ TYPE ;
: ANVAND? ( i -- f )  0 NC @ 0> IF NC @ 0 DO
  OVER I CDEP IBIT OR LOOP THEN SWAP DROP ;
( CNY: cellen till SATT och LADDA )
VARIABLE CI  VARIABLE NY?  VARIABLE TA  VARIABLE TN
VARIABLE GTYP  VARIABLE NYPOS  -1 NYPOS !
: CNY ( n -- )  ( cellen med namnet i KB, eller en ny )
  KB OVER CSOK DUP CI !  0< NY? !  NY? @ IF
  NC @ CMAX = IF DROP 16 FEL EXIT THEN  NC @ CI !
  CI @ CELL CL 0 FILL  DUP CI @ CELL C!
  KB CI @ CELL 1+ ROT CMOVE
  CMAX 1+ CMSK CI @ CMSK 12 CMOVE
  0 S>F CI @ CVAL F!  ELSE DROP THEN ;
: REST ( -- a n )  KA @ KP @ +  KL @ KP @ - ;
CREATE ORDN CMAX ALLOT  CREATE KLAR 8 ALLOT  VARIABLE NO
: NY  0 NC !  0 NO !  KBAS @ HERE - ALLOT  CMAX 1+ CMSK 12 0 FILL ;
' DUP 2 + @ CONSTANT XNEXT
( Talen med mall: ROM:ens USEFLD [4713] läser fältet i mallen )
( och FMUSE [4575] skriver talet, som PRINT USING. Ryms talet )
( inte blir det % och fri form [PRFREE 4504], som i BASIC. )
( XUSE: UDEC talet i decimal form [ur [F.]], UMASK UN mallen, )
( UOUT dit texten skrivs och sedan där den slutar; f sant om )
( mallen inte börjar med ett talfält. )
VARIABLE UDEC  VARIABLE UMASK  VARIABLE UN  VARIABLE UOUT
CREATE UPOST 4 ALLOT
( PUSH BC PUSH DE PUSH IX LD HL,[UMASK] LD A,[UN] LD B,A )
( CALL USEFLD JR C,fel LD A,D ADD A,A ADD A,A JR C,fel )
( LD IX,UPOST LD A,D OR 128 LD [IX+0],A LD [IX+1],16 )
( LD [IX+2],C LD [IX+3],E LD HL,[UDEC] LD DE,[UOUT] PUSH HL )
( CALL FMUSE POP HL JR NC,ok LD A,37 LD [DE],A INC DE )
( LD [IX+1],16 CALL PRFREE ok: LD [UOUT],DE AND A )
( fel: SBC A,A POP IX POP DE LD C,A LD B,A JP NEXT )
CREATE XUSE  197 C,  213 C,  221 C, 229 C,  42 C, UMASK ,
  58 C, UN ,  71 C,  205 C, 18195 ,  56 C, 55 C,
  122 C,  135 C,  135 C,  56 C, 50 C,
  221 C, 33 C, UPOST ,  122 C,  246 C, 128 C,  221 C, 119 C, 0 C,
  221 C, 54 C, 1 C, 16 C,  221 C, 113 C, 2 C,  221 C, 115 C, 3 C,
  42 C, UDEC ,  237 C, 91 C, UOUT ,  229 C,  205 C, 17781 ,
  225 C,  48 C, 11 C,  62 C, 37 C,  18 C,  19 C,
  221 C, 54 C, 1 C, 16 C,  205 C, 17668 ,
  237 C, 83 C, UOUT ,  167 C,  159 C,  221 C, 225 C,  209 C,
  79 C,  71 C,  195 C, XNEXT ,
( Talet till NBUF, NL tecken: med mallen eller i fri form som F. )
CREATE NBUF 48 ALLOT  VARIABLE NL
( Som F. i f.fs: QR talet i decimal form, QN siffrorna, QE )
( tiopotensen; QAVR avrundar till högst n siffror )
VARIABLE QR  VARIABLE QN  VARIABLE QE
: QDIG ( i -- a )  QR @ 2 + + ;
: QUPP ( i -- )  BEGIN DUP 0< IF DROP 49 0 QDIG C!  1 QE +! EXIT
  THEN DUP QDIG C@ 57 = WHILE 48 OVER QDIG C! 1- REPEAT
  QDIG DUP C@ 1+ SWAP C! ;
: QAVR ( n -- )  DUP QN @ < IF DUP QDIG C@ 53 < 0= IF DUP 1- QUPP
  THEN DUP QN ! THEN DROP  BEGIN QN @ 1 > IF QN @ 1- QDIG C@ 48 =
  ELSE 0 THEN WHILE -1 QN +! REPEAT ;
: N+ ( c -- )  NL @ 40 < IF NBUF NL @ + C! 1 NL +! ELSE DROP THEN ;
: NFIX  QE @ 0> IF QE @ 0 DO I QN @ < IF I QDIG C@ ELSE 48 THEN
  N+ LOOP THEN  QN @ QE @ > IF 46 N+  QE @ 0< IF QE @ NEGATE 0
  DO 48 N+ LOOP THEN  QN @ QE @ 0 MAX DO I QDIG C@ N+ LOOP THEN ;
: NEXP  0 QDIG C@ N+ 46 N+  QN @ 1 > IF QN @ 1 DO I QDIG C@ N+
  LOOP THEN  69 N+  QE @ 1- DUP 0< IF 45 ELSE 43 THEN N+
  ABS 10 /MOD 48 + N+ 48 + N+ ;
: NFRI  QR @ C@ DUP 127 AND QN !  128 AND IF 45 N+ THEN
  QR @ 1+ C@ DUP 127 > IF 256 - THEN QE !  16 QAVR
  0 QDIG C@ 48 = IF 48 N+ EXIT THEN
  QE @ 16 >  QN @ QE @ - 16 > OR IF NEXP ELSE NFIX THEN ;
: MALL ( a n -- f )  UN ! UMASK ! QR @ UDEC ! NBUF UOUT !
  XUSE EXECUTE  DUP 0= IF UOUT @ NBUF - NL ! THEN ;
: NTAL ( m -- ) ( F: r -- )  0 NL !  (F.) QR !
  DUP C@ IF DUP 1+ SWAP C@ MALL IF NFRI THEN ELSE DROP NFRI THEN
  BEGIN NL @ IF NL @ 1- NBUF + C@ 32 = ELSE 0 THEN WHILE -1 NL +!
  REPEAT ;
: DFELV ( d -- )  ." FEL: skivan, fel " 255 AND . ;
( RAKNA räknar i ordningen. Spill eller division med 0 )
( ger typen 3, och formler som läser en sådan cell också: )
( FELM har de celler som är fel, typ 3 eller 4. )
CREATE FELM 8 ALLOT
: SNITT? ( a b -- f )  0 4 0 DO 2 PICK I 2* + @
  2 PICK I 2* + @ AND OR LOOP >R 2DROP R> ;
: RAKN1 ( j -- j )  DUP CDEP FELM SNITT?
  IF 3 ELSE 0 FERR !  DUP CXT @ EXECUTE
  FERR @ IF FCLEAR 3 ELSE 1 THEN THEN OVER CTYP C! ;
( Bara det som ändrats räknas: ANDM har de ändrade cellerna, )
( och en formel som läser en av dem räknas och kommer med. )
CREATE ANDM 8 ALLOT
: FEL? ( j -- )  DUP CTYP C@ 2 > IF FELM SBIT ELSE DROP THEN ;
( RAKNAM börjar på plats p i ordningen; före den bara FELM )
: RAKNAM ( p -- )  FELM 8 0 FILL
  DUP 0> IF DUP 0 DO I ORDN + C@ FEL? LOOP THEN
  NO @ OVER > IF NO @ SWAP DO I ORDN + C@
  DUP CTYP C@ DUP 1 = SWAP 3 = OR IF DUP ANDM IBIT 0= 0=
  OVER CDEP ANDM SNITT? OR IF DUP ANDM SBIT RAKN1 THEN THEN
  FEL? LOOP ELSE DROP THEN ;
( OPOS: platsen i ordningen för cell i )
: OPOS ( i -- p )  0 BEGIN DUP NO @ < IF 2DUP ORDN + C@ = 0=
  ELSE 0 THEN WHILE 1+ REPEAT SWAP DROP ;
: RAKNA  ANDM 8 255 FILL 0 RAKNAM ;
: RAKNA1 ( i -- )  ANDM 8 0 FILL DUP ANDM SBIT OPOS RAKNAM ;
( Överläggen: OVT har för var och ett platsen på RAM-skivan, )
( längden och summan av bytena. OV n hämtar överlägg n till )
( OVB med RMOVE, om det inte redan är där [OVNU], och ser efter )
( att summan stämmer. Orden i ett överlägg har en egen kedja och )
( nås via variablerna 'SATT osv., som får sina xt när överlägget )
( kompileras. Ett ord i ett överlägg får inte anropa ett ord som )
( hämtar ett annat överlägg. )
' LOOKUP 5 + @ CONSTANT 'LAST
VARIABLE OVB  VARIABLE OVNU  VARIABLE OVINIT  VARIABLE OVIMG
VARIABLE RLAST  VARIABLE OVLST  VARIABLE OV2LST
CREATE OVT 24 ALLOT
: OVE ( i -- a )  1- DUP 2* + 2* OVT + ;
: OVN ( i -- n )  OVE 2 + @ ;
( SUM: POP HL PUSH DE LD DE,0 l: LD A,B OR C JR Z,s LD A,[HL] )
( ADD A,E LD E,A JR NC,+1 INC D INC HL DEC BC JR l )
( s: LD B,D LD C,E POP DE JP NEXT )
CREATE XSUM  225 C,  213 C,  17 C, 0 ,  120 C,  177 C,  40 C, 10 C,
  126 C,  131 C,  95 C,  48 C, 1 C,  20 C,  35 C,  11 C,  24 C, 242 C,
  66 C,  75 C,  209 C,  195 C, XNEXT ,
: SUM ( a n -- s )  XSUM EXECUTE ;
: OVFEL  CR ." FEL: överlägget på RAM-skivan är överskrivet,"
  ."  ladda programmet igen" CR RESET QUIT ;
: OV ( n -- )  DUP OVNU @ = IF DROP EXIT THEN  0 OVNU !
  DUP OVE >R  R@ @ OVB @ R@ 2 + @ RMOVE
  OVB @ R@ 2 + @ SUM R> 4 + @ = IF OVNU ! ELSE DROP OVFEL THEN ;
: OVX ( n a -- )  SWAP OV @ EXECUTE ;
( OVSPAR: överlägget i som just kompilerats på OVB får platsen ra )
: OVSPAR ( ra i -- )  OVE >R  R@ !  HERE OVB @ - R@ 2 + !
  OVB @ R@ 2 + @ SUM R@ 4 + !  OVB @ R@ @ R> 2 + @ RMOVE ;
VARIABLE 'SATT  VARIABLE 'TABORT  VARIABLE 'LADDA2
VARIABLE 'SPARA  VARIABLE 'LADDA  VARIABLE 'SKRIV  VARIABLE 'FORMAT
: SATT  1 'SATT OVX ;
: TABORT  1 'TABORT OVX ;
: SPARA  2 'SPARA OVX ;
: SKRIV  2 'SKRIV OVX ;
: FORMAT  2 'FORMAT OVX ;
VARIABLE 'VISA  : VISA  2 'VISA OVX ;
( Överläggens område börjar här. Först överlägg 2, som sedan )
( ligger på RAM-skivan och i filen KALKYL.OVL, och sist )
( överlägg 1, som ligger kvar i området. )
'LAST @ RLAST !  HERE OVB !
( ---- Överlägg 2: VISA, FORMAT, filerna och utskriften ---- )
( Raderna i VISA och SKRIV byggs i LB och går till skärmen eller )
( till PR: [xt till VISAR]. Namnet, värdet till höger i 16 tecken och )
( formeln [UFORM]; rubriken som text, i SKRIV utan namnet. )
CREATE LB 100 ALLOT  VARIABLE LL  VARIABLE UFORM
16 CONSTANT VW
: U+ ( c -- )  LL @ 100 < IF LB LL @ + C! 1 LL +! ELSE DROP THEN ;
: US ( a n -- )  ?DUP IF 0 DO DUP I + C@ U+ LOOP THEN DROP ;
: UB ( n -- )  0 MAX ?DUP IF 0 DO 32 U+ LOOP THEN ;
: UTEXT ( i -- )  TXT DUP RC@ ?DUP IF 0 DO DUP I + 1+ RC@ U+
  LOOP THEN DROP ;
: URUB ( i -- )  0 LL !  TXT DUP RC@ ?DUP IF 0 DO DUP I + 1+ RC@
  DUP 34 = I 0= AND IF DROP ELSE U+ THEN LOOP THEN DROP ;
CREATE TFEL 102 C, 101 C, 108 C,
: VRAD ( i -- )  0 LL !  DUP CELL DUP 1+ SWAP C@ DUP >R US
  9 R> - UB  DUP CTYP C@ DUP 2 = IF DROP UFORM @ IF UTEXT
  ELSE URUB THEN EXIT THEN
  DUP 2 > IF DROP VW 3 - UB TFEL 3 US -1 ELSE >R DUP CVAL F@
  DUP CMSK NTAL  VW NL @ - UB NBUF NL @ US R> THEN
  UFORM @ AND IF 2 UB UTEXT ELSE DROP THEN ;
( VISAR: raderna, och xt gör något med var och en )
: VISAR ( xt -- )  NC @ 0> IF NC @ 0 DO I VRAD DUP EXECUTE LOOP
  THEN DROP ;
: SKARM  CR LB LL @ TYPE ;  ' SKARM CONSTANT XSKARM
( VISA listar namn, värde och formel; rubriken som text )
: (VISA)  -1 UFORM !  XSKARM VISAR CR ;
( ---- Det delade: härifrån till "Hit det delade" tar gor.sh också )
( till VERK:s överlägg 2 [till "i överlägg 2"] och 4 [till "i )
( båda" och från "i överlägg 2"] ---- )
: -BL ( a n -- a n' )  BEGIN DUP IF 2DUP + 1- C@ 32 = ELSE 0 THEN
  WHILE 1- REPEAT ;
( SPARA namn och LADDA namn: arket som text på skivan, en )
( cell per rad: namnet och texten, som i SATT. Filen är NAMN.KAL )
( om inget annat sägs [FTYP]. DOS:en anropas direkt: DOS kör rutinen )
( adr med B = kanal * 16, C = 255 [vilken enhet som helst] och )
( DE = de, och ger A och CY. Ett diskfel går via felutgången )
( DERRX [FD33] till DERRH, som tar tillbaka stacken; BASIC:s )
( felutgång läggs tillbaka efteråt. )
VARIABLE DADR  VARIABLE DSP  VARIABLE DERR0
( DOJP: LD HL,[DADR] JP [HL] )
CREATE DOJP  42 C, DADR ,  233 C,
( DSLUT: LD DE,[DERR0] LD [DERRX],DE POP IX POP DE )
( LD C,L LD B,0 PUSH BC LD C,H LD B,H JP NEXT )
CREATE DSLUT  237 C, 91 C, DERR0 ,  237 C, 83 C, 64819 ,
  221 C, 225 C,  209 C,  77 C,  6 C, 0 C,  197 C,  76 C,  68 C,
  195 C, XNEXT ,
( DERRH: LD SP,[DSP] LD L,A LD H,255 JP DSLUT )
CREATE DERRH  237 C, 123 C, DSP ,  111 C,  38 C, 255 C,
  195 C, DSLUT ,
( XDOS: LD H,B LD L,C LD [DADR],HL POP HL LD A,L POP HL )
( PUSH DE PUSH IX LD [DSP],SP LD DE,[DERRX] LD [DERR0],DE )
( LD DE,DERRH LD [DERRX],DE EX DE,HL LD B,A LD C,255 )
( CALL DOJP LD L,A SBC A,A LD H,A JP DSLUT )
CREATE XDOS  96 C, 105 C,  34 C, DADR ,  225 C,  125 C,
  225 C,  213 C,  221 C, 229 C,  237 C, 115 C, DSP ,
  237 C, 91 C, 64819 ,  237 C, 83 C, DERR0 ,  17 C, DERRH ,
  237 C, 83 C, 64819 ,  235 C,  71 C,  14 C, 255 C,  205 C, DOJP ,
  111 C,  159 C,  103 C,  195 C, DSLUT ,
: DOS ( de b adr -- a f )  XDOS EXECUTE ;
( Kanal 1, med bufferten på F600. DFEL: 256 + A efter det )
( första felet, och sedan görs inget mer. BP: platsen i )
( blocket, 3-254 data, 255 ETX. )
62976 CONSTANT BUF  VARIABLE DFEL  VARIABLE BP
: DOSK ( de adr -- )  DFEL @ IF 2DROP EXIT THEN
  16 SWAP DOS IF 256 + DFEL ! ELSE DROP THEN ;
: STANG  0 16 24609 DOS 2DROP ;
( Filnamnet: högst 8 tecken, en punkt och högst 3 tecken )
CREATE FNAMN 11 ALLOT  VARIABLE FP  VARIABLE FG  VARIABLE FOK
CREATE FTYP  75 C, 65 C, 76 C,
: FNAMN! ( -- f )  FNAMN 8 32 FILL  FTYP FNAMN 8 + 3 CMOVE
  BL?  0 FP !  8 FG !  -1 FOK !
  BEGIN CH DUP 32 > WHILE  DUP 46 = IF DROP 8 FP !  11 FG !
  FNAMN 8 + 3 32 FILL ELSE FP @ FG @ < IF FNAMN FP @ + C!
  1 FP +! ELSE DROP 0 FOK ! THEN THEN >> REPEAT DROP
  FOK @ FNAMN C@ 32 > AND ;
( ---- Hit det delade i båda ---- )
( Skriva: PUTB lägger ett tecken i blocket; fullt block skrivs )
: PUTB ( c -- )  BUF BP @ + C!  1 BP +!  BP @ 255 = IF
  3 BUF 255 + C!  0 24624 DOSK  3 BP ! THEN ;
: PUTS ( a n -- )  DUP 0> IF 0 DO DUP I + C@ PUTB LOOP ELSE DROP
  THEN DROP ;
: PUTT ( i -- )  TXT DUP RC@ DUP 0> IF 0 DO DUP I + 1+ RC@ PUTB
  LOOP ELSE DROP THEN DROP ;
( SOPPNA: filen a n med PREPARE och posten 0; f falskt vid fel )
: SOPPNA ( a n -- f )  KSTART FNAMN! 0= IF ." FEL: filnamnet" 0 EXIT
  THEN  0 DFEL !  3 BP !  FNAMN 24597 DOSK  0 24618 DOSK  -1 ;
( PCELLER: cellerna, en per rad, och mallarna med % )
: PCELLER  NC @ 0> IF NC @ 0 DO I CELL DUP 1+ SWAP C@ PUTS  32 PUTB
  I PUTT  13 PUTB LOOP THEN
  NC @ 0> IF NC @ 0 DO I CMSK C@ IF 37 PUTB I CELL DUP 1+ SWAP
  C@ PUTS 32 PUTB I CMSK DUP 1+ SWAP C@ PUTS 13 PUTB THEN LOOP THEN ;
( PSLUT: 0 och ETX sist )
: PSLUT  0 PUTB  3 BUF BP @ + C!  0 24624 DOSK  STANG
  DFEL @ ?DUP IF DFELV THEN ;
( Utskriften till PR: med ROM:ens drivrutin PRDRV [7091], som )
( OPEN "PR:" i BASIC: skrivarkortet eller DART A, med de )
( parametrar som gäller för PR:. PR anropar den med IX = PFCB )
( och funktionen fn [0 OPEN, 2 CLOSE, 4 PRINT], DE = PDE, )
( HL = PHL och BC = PBC; f sant vid fel, med felet i PFN. )
VARIABLE PDE  VARIABLE PHL  VARIABLE PBC  VARIABLE PFN
CREATE PFCB 32 ALLOT  CREATE PPAR 12 ALLOT  VARIABLE PFEL
( PUSH BC PUSH DE PUSH IX LD IX,PFCB LD DE,[PDE] LD HL,[PHL] )
( LD BC,[PBC] LD A,[PFN] CALL PRDRV LD [PBC],BC LD [PFN],A )
( POP IX POP DE SBC A,A LD C,A LD B,A JP NEXT )
CREATE XPR  197 C,  213 C,  221 C, 229 C,  221 C, 33 C, PFCB ,
  237 C, 91 C, PDE ,  42 C, PHL ,  237 C, 75 C, PBC ,  58 C, PFN ,
  205 C, 28817 ,  237 C, 67 C, PBC ,  50 C, PFN ,  221 C, 225 C,
  209 C,  159 C,  79 C,  71 C,  195 C, XNEXT ,
: PR ( fn -- f )  PFN ! XPR EXECUTE ;
( POPPNA: OPEN med blanka parametrar [standard]; sedan radlängden )
( C i FCB+8 och kolumnen 0, som BASIC gör )
: POPPNA ( -- f )  PFCB 32 0 FILL  PPAR 11 32 FILL  PPAR PDE !
  0 PR DUP 0= IF PBC @ 255 AND PFCB 8 + C!  0 PFCB 6 + C! THEN ;
: PRUT ( a n -- )  PFEL @ IF 2DROP EXIT THEN  PBC ! PHL !
  4 PR IF PFN @ 255 AND 256 + PFEL ! THEN ;
CREATE CRLF 13 C, 10 C,
( PRALLT: OPEN, xt skriver raderna, CLOSE; med fel eller BREAK )
( [fel 0] slutar det )
: PRALLT ( xt -- )  0 PFEL !  POPPNA IF DROP PFN @ 255 AND 256 +
  PFEL ! ELSE EXECUTE 2 PR DROP THEN
  PFEL @ IF ." FEL: skrivaren, fel " PFEL @ 255 AND . THEN ;
( ---- Hit det delade i överlägg 2 ---- )
( FORMAT namn mall, FORMAT namn [ingen mall], FORMAT mall [alla )
( celler och de nya]. Mallen prövas med talet 0. )
: FMT1 ( i a n -- f )  -BL DUP 11 > IF 2DROP DROP
  ." FEL: för lång mall" 0 EXIT THEN
  DUP IF 2DUP 0 S>F (F.) QR ! MALL IF 2DROP DROP
  ." FEL: mallen har inget talfält" 0 EXIT THEN THEN
  ROT CMSK 2DUP C! 1+ SWAP CMOVE -1 ;
: FMT ( a n -- )  KSTART BL? CH BOKST? IF NAMN CNAMN
  KFEL @ IF DROP FELV EXIT THEN  BL? REST FMT1 DROP EXIT THEN
  CMAX 1+ REST FMT1 IF NC @ 0> IF NC @ 0 DO CMAX 1+ CMSK
  I CMSK 12 CMOVE LOOP THEN THEN ;
: (FORMAT)  0 PARSE FMT ;
( Läsa: GETB ger nästa tecken, 0 vid filens slut eller fel. )
( ETX går till nästa block. )
: GETB ( -- c )  DFEL @ IF 0 EXIT THEN  BP @ 255 > IF
  0 24621 DOSK 3 BP !  DFEL @ IF 0 EXIT THEN THEN
  BUF BP @ + C@  1 BP +!  DUP 3 = IF DROP 256 BP ! RECURSE THEN ;
CREATE LBUF 80 ALLOT  VARIABLE LN
: LTKN ( c -- )  LN @ 80 < IF LBUF LN @ + C! 1 LN +! ELSE DROP
  THEN ;
( LRAD: en rad till LBUF; 9 n är n blanka. c = 13, 0 sist )
: LRAD ( -- c )  0 LN !  BEGIN GETB DUP 13 = OVER 0= OR 0= WHILE
  DUP 9 = IF DROP GETB DUP 0> IF 0 DO 32 LTKN LOOP ELSE DROP
  THEN ELSE DUP 31 > IF LTKN ELSE DROP THEN THEN REPEAT ;
( LCELL: raden till en cell, utan att kompilera )
: LCELL ( a n -- )  KSTART BL? CH BOKST? 0= IF 17 FEL THEN NAMN
  DUP 8 > IF 7 FEL THEN  KFEL @ IF DROP ELSE CNY THEN
  KFEL @ IF CR LBUF LN @ TYPE CR FELV EXIT THEN
  BL? REST TN ! TA !  NY? @ IF 1 NC +! THEN
  TA @ TN @ CI @ TSPAR  CI @ CDEP 8 0 FILL
  TA @ C@ 34 = TN @ 0> AND IF 2 ELSE TA @ TN @ >FLOAT
  IF CI @ CVAL F! 0 ELSE 1 THEN THEN CI @ CTYP C! ;
( LOPPNA: filen a n; d är felet och f sant om den öppnades )
: LOPPNA ( a n -- d f )  KSTART FNAMN! 0= IF
  ." FEL: filnamnet" 0 0 EXIT THEN
  0 DFEL !  FNAMN 24600 DOSK  DFEL @ IF STANG DFEL @ 255 AND 35 <
  IF ." FEL: filen finns inte" 0 ELSE DFEL @ THEN 0 EXIT THEN
  256 BP !  0 -1 ;
( LCELLER: raderna till filens slut till celler och mallar )
: LCELLER  BEGIN LRAD LN @ IF LBUF C@ 37 = IF LBUF 1+ LN @
  1- FMT ELSE LBUF LN @ LCELL THEN THEN 0= UNTIL ;
( ---- Hit det delade ---- )
: (SPARA)  0 PARSE SOPPNA IF PCELLER PSLUT THEN ;
( [LADDA] läser filen till cellerna, utan att kompilera: d är )
( felet från skivan och f sant om cellerna ska kompileras och )
( räknas; det gör LADDA, med överlägg 1. )
: (LADDA) ( -- d f )  0 PARSE LOPPNA IF DROP NY LCELLER STANG
  DFEL @ -1 ELSE 0 THEN ;
( SKRIV: arket till PR:, utan formler )
: PRRAD  LB LL @ PRUT CRLF 2 PRUT ;  ' PRRAD CONSTANT XPRRAD
: KSKRIV  XPRRAD VISAR ;  ' KSKRIV CONSTANT XKSKRIV
: (SKRIV)  0 UFORM !  XKSKRIV PRALLT ;
( ---- Tabellen: bara i KALKYL, gor.sh tar bort den för VERK ---- )
( Tabellen är rader med text och cellnamn, som TABELL och SKRIV )
( ställer upp med värdena. Ett ord som är namnet på en cell blir )
( cellens värde med dess mall, högerställt så att det slutar där )
( namnet slutar; " först gör ett ord till text. Raderna ligger på )
( RAM-skivan från 8192, 80 byte var med längden först, och antalet )
( på 8191. I .KAL-filen står de efter cellerna med %= först. )
40 CONSTANT TMAX
: NTAB@ ( -- n )  8191 RC@ ;  : NTAB! ( n -- )  8191 RC! ;
: TADR ( k -- ra )  80 * 8192 + ;
: TRSP ( a n k -- )  TADR 2DUP RC! 1+ SWAP RMOVE ;
( THAMTA: rad k till LBUF, LN tecken )
: THAMTA ( k -- )  TADR DUP RC@ DUP LN ! SWAP 1+ LBUF ROT RMOVE ;
( Raden byggs i LB som i VISA; TJ är platsen i LBUF )
VARIABLE TJ
: TTILL ( j -- )  LL @ - UB ;
( TVAL: värdet i cell i, så att det slutar på plats e )
: TVAL ( e i -- )  DUP CTYP C@ DUP 2 = IF 2DROP DROP EXIT THEN
  2 > IF DROP TFEL 3 ELSE DUP CVAL F@ CMSK NTAL NBUF NL @ THEN
  ROT OVER - TTILL US ;
: TCH ( j -- c )  LBUF + C@ ;
: TORD ( -- n )  0 BEGIN TJ @ OVER + DUP LN @ < IF TCH 32 = 0=
  ELSE DROP 0 THEN WHILE 1+ REPEAT ;
: TRADEN ( k -- )  THAMTA 0 LL ! 0 TJ !
  BEGIN TJ @ LN @ < WHILE  TJ @ TCH 32 = IF 1 TJ +! ELSE
  TORD LBUF TJ @ + OVER  OVER C@ 34 = IF TJ @ TTILL 1- SWAP 1+
  SWAP US ELSE 2DUP CSOK DUP 0< IF DROP TJ @ TTILL US ELSE
  >R 2DROP R> OVER TJ @ + SWAP TVAL THEN THEN TJ +! THEN REPEAT ;
( TVISAR: raderna, och xt gör något med var och en )
: TVISAR ( xt -- )  NTAB@ 0> IF NTAB@ 0 DO I TRADEN DUP EXECUTE
  LOOP THEN DROP ;
: (TABELL)  NTAB@ IF XSKARM TVISAR CR ELSE ." FEL: ingen tabell"
  THEN ;
: TSKRIV  XPRRAD TVISAR ;  ' TSKRIV CONSTANT XTSKRIV
( SKRIV skriver tabellen om det finns en, annars cellerna )
: (SKRIV)  NTAB@ IF XTSKRIV PRALLT ELSE (SKRIV) THEN ;
( TRAD text, n TBYT text, n TTA och TLISTA )
: TREST ( -- a n )  0 PARSE DUP IF 1- SWAP 1+ SWAP THEN 79 MIN ;
: (TRAD)  TREST NTAB@ TMAX = IF 2DROP ." FEL: tabellen är full"
  EXIT THEN  NTAB@ TRSP NTAB@ 1+ NTAB! ;
: TNR? ( n -- k f )  1- DUP 0< OVER NTAB@ < 0= OR
  IF ." FEL: ingen sådan rad" 0 ELSE -1 THEN ;
: (TBYT) ( n -- )  TREST ROT TNR? IF TRSP ELSE DROP 2DROP THEN ;
: (TTA) ( n -- )  TNR? IF BEGIN DUP 1+ NTAB@ < WHILE DUP 1+ THAMTA
  LBUF LN @ ROT DUP >R TRSP R> 1+ REPEAT DROP NTAB@ 1- NTAB!
  ELSE DROP THEN ;
: (TLISTA)  NTAB@ 0> IF NTAB@ 0 DO CR I 1+ . I THAMTA LBUF LN @
  TYPE LOOP THEN CR ;
( SPARA och LADDA med tabellens rader efter cellerna och mallarna )
: TPUT  NTAB@ 0> IF NTAB@ 0 DO 37 PUTB 61 PUTB I THAMTA
  LBUF LN @ PUTS 13 PUTB LOOP THEN ;
: (SPARA)  0 PARSE SOPPNA IF PCELLER TPUT PSLUT THEN ;
: TLAS  NTAB@ TMAX < IF LBUF 2 + LN @ 2 - 0 MAX NTAB@ TRSP
  NTAB@ 1+ NTAB! THEN ;
: TLCELLER  BEGIN LRAD LN @ IF LBUF C@ 37 = IF LBUF 1+ C@ 61 = IF
  TLAS ELSE LBUF 1+ LN @ 1- FMT THEN ELSE LBUF LN @ LCELL THEN
  THEN 0= UNTIL ;
: (LADDA) ( -- d f )  0 PARSE LOPPNA IF DROP NY 0 NTAB! TLCELLER
  STANG DFEL @ -1 ELSE 0 THEN ;
( Sist en RET: KALKYL.OVL startar på den )
201 C,
( ---- Tabellen slut ---- )
' (SPARA) 'SPARA !  ' (LADDA) 'LADDA !  ' (SKRIV) 'SKRIV !
' (FORMAT) 'FORMAT !  ' (VISA) 'VISA !
'LAST @ OV2LST !  27392 HERE OVB @ - - 2 OVSPAR
RLAST @ 'LAST !  OVB @ HERE - ALLOT
( ---- Överlägg 1: formelkompilatorn, SATT, TABORT och ordningen ---- )
: SIFFROR ( n -- n' )  BEGIN CH DUP SIFF? OVER 46 = OR
  WHILE TKN REPEAT DROP ;
: TALTEXT ( -- n )  0 SIFFROR  CH 69 = IF 69 TKN
  CH DUP 43 = SWAP 45 = OR IF CH TKN THEN SIFFROR THEN ;
( Koden: ett tal, en cell )
: KTAL ( F: r -- )  XFLIT , HERE F! 8 ALLOT +D ;
: KREF ( i -- )  DUP CTYP C@ 2 = IF DROP 6 FEL EXIT THEN
  DUP KDEP SBIT  XLIT , CVAL , XF@ , +D ;
: KNUM  TALTEXT DUP 30 > IF DROP 7 FEL EXIT THEN
  KB SWAP >FLOAT IF KTAL ELSE 14 FEL THEN ;
( Funktionerna är ord med = först: =SUMMA osv., i ordlistan )
( eller i överläggets kedja OVLST )
: FUNK ( n -- )  61 KFN C!  KFN SWAP 1+ 2DUP LOOKUP ?DUP IF
  >R 2DROP R> ELSE OVLST @ (LOOKUP) THEN
  ?DUP IF >XT EXECUTE ELSE 2 FEL THEN ;
: SEMI  59 TECKEN? 0= IF 4 FEL THEN ;
: SLUT)  41 TECKEN? 0= IF 3 FEL THEN ;
( Uttrycken, med rekursiv nedstigning )
VARIABLE 'UTTR  : UTTR  'UTTR @ EXECUTE ;
: PRIM  BL? CH  DUP SIFF? OVER 46 = OR IF DROP KNUM EXIT THEN
  DUP 40 = IF DROP >> UTTR SLUT) EXIT THEN
  BOKST? IF NAMN  40 TECKEN? IF FUNK EXIT THEN
  CNAMN DUP 0< IF DROP ELSE KREF THEN EXIT THEN  5 FEL ;
( ^ med F^, när det ordet finns )
: XPOT  70 KFN C! 94 KB C!  KFN 2 LOOKUP
  ?DUP IF >XT , -D ELSE 8 FEL THEN ;
: POT  PRIM BEGIN 94 TECKEN? WHILE PRIM XPOT REPEAT ;
: UNAR  45 TECKEN? IF RECURSE XNEG , EXIT THEN
  43 TECKEN? DROP POT ;
: TERM  UNAR BEGIN BL? CH DUP 42 = OVER 47 = OR WHILE
  >> UNAR 42 = IF X* ELSE X/ THEN , -D REPEAT DROP ;
: UTTRYCK  TERM BEGIN BL? CH DUP 43 = OVER 45 = OR WHILE
  >> TERM 43 = IF X+ ELSE X- THEN , -D REPEAT DROP ;
' UTTRYCK 'UTTR !
( Jämförelsen i OM: 1 < 2 > 3 <= 4 >= 5 <> 6 = )
: KJFR ( k -- )  DUP 2 = OVER 3 = OR IF XFSWAP , THEN
  DUP 5 < IF XF< ELSE XF= THEN ,
  DUP 3 = OVER 4 = OR SWAP 5 = OR IF X0= , THEN ;
: JFR  UTTRYCK BL? CH
  DUP 60 = IF DROP >> CH 61 = IF >> 3 ELSE
    CH 62 = IF >> 5 ELSE 1 THEN THEN ELSE
  DUP 62 = IF DROP >> CH 61 = IF >> 4 ELSE 2 THEN ELSE
  61 = IF >> 6 ELSE 9 FEL 0 THEN THEN THEN
  UTTRYCK -D -D KJFR ;
( Argumenten: semikolon mellan dem; A..B är cellerna )
( från A till B. xt slår ihop två värden; n är antalet. )
: VARDE ( xt n -- xt n+1 )  DUP IF OVER , -D THEN 1+ ;
: OMRADE ( xt n len -- xt n' )  CNAMN
  >> 46 TECKEN? 0= IF 12 FEL THEN  BL? NAMN CNAMN
  OVER OVER > OVER 0< OR 2 PICK 0< OR IF 2DROP 12 FEL EXIT THEN
  1+ SWAP DO I CTYP C@ 2 = 0= IF I KREF VARDE THEN LOOP ;
: POST ( xt n -- xt n' )  BL? CH BOKST? IF KP @ >R NAMN
  BL? CH 46 = IF R> DROP OMRADE EXIT THEN DROP R> KP ! THEN
  UTTRYCK VARDE ;
: LISTA ( xt -- n )  0 BEGIN POST 59 TECKEN? 0= UNTIL
  SLUT) SWAP DROP ;
: =SUMMA  X+ LISTA 0= IF 0 S>F KTAL THEN ;
: =MAX  XMAX LISTA 0= IF 11 FEL THEN ;
: =MIN  XMIN LISTA 0= IF 11 FEL THEN ;
: =MEDEL  X+ LISTA ?DUP IF S>F KTAL X/ , -D ELSE 11 FEL THEN ;
: =ANTAL  X+ LISTA ?DUP IF XFDROP , -D S>F ELSE 0 S>F THEN
  KTAL ;
: =OM  JFR SEMI X0BR , HERE 0 ,  UTTRYCK SEMI -D
  XBR , HERE 0 , SWAP HERE SWAP !  UTTRYCK SLUT) HERE SWAP ! ;
( KOMP: formeln a n till ett ord som räknar cell i och )
( sparar värdet; f falskt vid fel, och koden tas bort )
: (KOMP) ( a n i -- f )  >R KSTART
  HERE TIB 400 - U< 0= IF 13 FEL THEN
  HERE 205 C, DOCOL ,  UTTRYCK BL? CH IF 10 FEL THEN
  XLIT , R@ CVAL , XF! , XEXIT ,
  KFEL @ IF HERE - ALLOT R> DROP 0 EXIT THEN
  R@ CXT !  1 R@ CTYP C!  KDEP R> CDEP 8 CMOVE -1 ;
( Cirkeln: PAR k = j när j läser k, CMAX för den nya )
( formeln. Sökningen går bredden först med kön KO, så att )
( varje cell bara tas en gång; når den cellen sig själv blir )
( det en cirkel. Cell NC är med, för en ny cell. )
CREATE PAR CMAX 1+ ALLOT  CREATE KO CMAX 1+ ALLOT
VARIABLE KOH  VARIABLE KOT
: PAR! ( k j -- )  OVER PAR + C@ 255 = IF OVER PAR + C!
  KO KOT @ + C!  1 KOT +! ELSE 2DROP THEN ;
: SPRID ( a j -- )  NC @ 1+ 0 DO
  I 2 PICK IBIT IF I OVER PAR! THEN LOOP 2DROP ;
: CIRKEL? ( i -- f )  PAR CMAX 1+ 255 FILL  0 KOH !  0 KOT !
  KDEP CMAX SPRID
  BEGIN KOH @ KOT @ < OVER PAR + C@ 255 = AND WHILE
  KO KOH @ + C@  1 KOH +!  DUP CDEP SWAP SPRID REPEAT
  PAR + C@ 255 = 0= ;
: CIRKV ( i -- )  ." FEL: cirkel: " DUP BEGIN DUP CN. SPACE
  PAR + C@ DUP CMAX = UNTIL DROP CN. SPACE ;
( Ordningen: ORDN har cellerna så att de en formel läser )
( kommer före; NO är antalet. KLAR är de som är med. )
: KLAR? ( j -- f )  CDEP -1 4 0 DO OVER I 2* + @
  KLAR I 2* + @ DUP >R OR R> = AND LOOP SWAP DROP ;
( Ett varv tar de celler vars formler bara läser klara celler; )
( det slutar när ett varv inte ger någon ny. Står cellerna i )
( ordning räcker ett varv. NO mindre än NC: en cirkel. )
VARIABLE OKAD
: ORDNA  KLAR 8 0 FILL  0 NO !  NC @ 0> IF BEGIN 0 OKAD !
  NC @ 0 DO I KLAR IBIT 0= IF I KLAR? IF I ORDN NO @ + C!
  1 NO +!  I KLAR SBIT  -1 OKAD ! THEN THEN LOOP
  OKAD @ 0=  NO @ NC @ < 0= OR UNTIL THEN ;
( KOMPALLA kompilerar alla formler från texterna igen: när )
( cellerna har flyttats, och när minnet tar slut. )
: KOMPALLA  KBAS @ HERE - ALLOT  NC @ 0> IF NC @ 0 DO
  I CTYP C@ DUP 1 = SWAP 3 = OR IF I THAMT I (KOMP) 0= IF
  4 I CTYP C!  I CDEP 8 0 FILL THEN THEN LOOP THEN ORDNA ;
( SATT namn formel: cellen får formeln, ett tal eller "en )
( rubrik". En ny cell läggs sist, eller före cell NYPOS om )
( den är 0 eller mer. Vid fel ändras inget. )
( Det gamla, om den nya formeln ger en cirkel )
VARIABLE OXT  CREATE ODEP 8 ALLOT  VARIABLE KH
: ANVFEL  ." FEL: cellen används i en formel " ;
( KPLATS: när minnet börjar ta slut kompileras alla formler )
( om, så att de gamla formlernas kod försvinner. Texten sparas )
( på plats CMAX, eftersom KOMPALLA använder TIB. )
: KPLATS  HERE TIB 700 - U< 0= IF TA @ TN @ CMAX TSPAR
  KOMPALLA CMAX THAMT TN ! TA ! THEN ;
( CIRK?: en ny cell kan bara läsa sig själv. Läser formeln )
( samma celler som förut är ordningen kvar; annars görs den om, )
( och kommer inte alla celler med blir det en cirkel. )
: LIKA? ( -- f )  CI @ CDEP ODEP 8 SAME ;
: CIRK? ( -- f )  NY? @ IF CI @ KDEP IBIT 0= 0= EXIT THEN
  LIKA? IF 0 EXIT THEN  ORDNA NO @ NC @ < ;
: GOR ( -- f )  CI @ CTYP C@ GTYP !  CI @ CDEP ODEP 8 CMOVE
  TA @ C@ 34 = TN @ 0> AND IF CI @ ANVAND? IF ANVFEL 0 EXIT
  THEN 2 CI @ CTYP C!  CI @ CDEP 8 0 FILL -1 EXIT THEN
  TA @ TN @ >FLOAT IF CI @ CVAL F!  0 CI @ CTYP C!
  CI @ CDEP 8 0 FILL -1 EXIT THEN
  KPLATS  CI @ CXT @ OXT !  HERE KH !
  TA @ TN @ CI @ (KOMP) 0= IF FELV 0 EXIT THEN
  CIRK? IF CI @ CIRKEL? IF CI @ CIRKV ELSE ." FEL: cirkel" THEN
  GTYP @ CI @ CTYP C!  OXT @ CI @ CXT !  ODEP CI @ CDEP 8 CMOVE
  KH @ HERE - ALLOT  ORDNA 0 EXIT THEN -1 ;
: ROTERA ( p -- )  NC @ 1- DUP CMAX FLYTTA
  BEGIN 2DUP < WHILE 1- DUP DUP 1+ FLYTTA REPEAT
  DROP CMAX SWAP FLYTTA ;
: (SATT)  0 PARSE  ( resten av raden )
  KSTART BL? CH BOKST? 0= IF 17 FEL THEN NAMN
  DUP 8 > IF 7 FEL THEN  KFEL @ IF DROP ELSE CNY THEN
  KFEL @ IF FELV EXIT THEN  BL? REST TN ! TA !
  GOR 0= IF EXIT THEN  TA @ TN @ CI @ TSPAR
  NY? @ IF 1 NC +!  NYPOS @ DUP 0< 0= OVER NC @ 1- < AND
  IF ROTERA KOMPALLA RAKNA EXIT THEN DROP
  CI @ ORDN NO @ + C!  1 NO +! THEN  CI @ RAKNA1 ;
( TABORT namn tar bort cellen, om ingen formel läser den. )
: (TABORT)  0 PARSE KSTART BL? NAMN CNAMN
  KFEL @ IF DROP FELV EXIT THEN
  DUP ANVAND? IF DROP ANVFEL EXIT THEN
  DUP 1+ NC @ < IF NC @ OVER 1+ DO I I 1- FLYTTA LOOP THEN
  DROP -1 NC +!  KOMPALLA RAKNA ;
( [LADDA2]: efter [LADDA] i överlägg 2 kompileras och räknas )
( cellerna. En formel som inte kom med i ordningen blir trasig, )
( typ 4. )
: (LADDA2) ( d -- )  KOMPALLA  NC @ 0> IF NC @ 0 DO I KLAR
  IBIT 0= IF CR ." FEL: cirkel: " I CN.  4 I CTYP C! THEN LOOP THEN
  RAKNA  ?DUP IF DFELV THEN  NC @ . ." celler" ;
' (SATT) 'SATT !  ' (TABORT) 'TABORT !  ' (LADDA2) 'LADDA2 !
'LAST @ OVLST !  2 OVE @ HERE OVB @ - - 1 OVSPAR
RLAST @ 'LAST !  OVB @ 1 OVN 2 OVN MAX + HERE - ALLOT
( ---- Överläggens område slutar här ---- )
( LADDA namn: [LADDA] med överlägg 2, sedan [LADDA2] med 1 )
: LADDA  2 'LADDA OVX IF 1 'LADDA2 OVX ELSE ?DUP IF DFELV THEN THEN ;
( BYE: BASIC:s NEW och READY0, eftersom laddaren KALKYL kan vara )
( överskriven. Z=CALL 32768,1 i BASIC går tillbaka till KALKYL. )
( LD SP,SAVESP 8003, CALL NEW 0E23, JP READY0 010E )
HERE 237 C, 123 C, 32771 , 205 C, 3619 , 195 C, 270 ,
CONSTANT XBAS  : BYE  XBAS EXECUTE ;
( ---- Hit är KALKYL och VERK lika: gor.sh tar raderna hit och )
( sedan verk.fs till VERK.ABS ---- )
( Omgångarna, som i SPRIT: OMGANG sparar alla cellers värden som )
( nästa omgång, högst OMAX, och JAMFOR visar omgångarna sida vid )
( sida, en rad per cell, med # efter ett värde som skiljer sig )
( från omgången före. OMGANG NY tömmer dem, och NY och LADDA )
( också. En omgång är ONR poster om 18 byte på RAM-skivan från )
( 13568: namnet [längden och 8 tecken], typen och värdet. Posten )
( söks på namnet, så celler kan läggas till och tas bort emellan. )
4 CONSTANT OMAX  16 CONSTANT OW  VARIABLE NOMG
CREATE ONR OMAX 2* ALLOT  CREATE OP 18 ALLOT
CREATE OFV 8 ALLOT  VARIABLE OFF
: OADR ( k r -- ra )  720 * SWAP 18 * + 13568 + ;
: OANT ( r -- a )  2* ONR + ;
: OSPAR ( r -- )  NC @ OVER OANT !  NC @ 0> IF NC @ 0 DO
  I CELL OP 10 CMOVE  I CVAL OP 10 + 8 CMOVE
  OP I 2 PICK OADR 18 RMOVE LOOP THEN DROP ;
: OPOST ( k r -- )  OADR OP 18 RMOVE ;
: OSAMMA? ( i -- f )  CELL OP 9 SAME ;
( OHAMT: posten för cell i i omgång r till OP, f falskt om den )
( saknas. Först platsen i, där den brukar vara. )
: OHAMT ( i r -- f )  2DUP OANT @ < IF 2DUP OPOST OVER OSAMMA? IF
  2DROP -1 EXIT THEN THEN  DUP OANT @ ?DUP IF 0 DO I OVER OPOST
  OVER OSAMMA? IF 2DROP -1 UNLOOP EXIT THEN LOOP THEN 2DROP 0 ;
( OKOL: cell i i omgång r, OW tecken och # eller blankt )
: OKOL ( i r -- i )  OVER SWAP OHAMT 0= IF OW 1+ SPACES 0 OFF !
  EXIT THEN  OP 9 + C@ DUP 2 = IF DROP OW 1+ SPACES 0 OFF ! EXIT
  THEN 2 > IF OW 3 - SPACES ." fel " 0 OFF ! EXIT THEN
  OP 10 + F@ DUP CMSK NTAL  OW NL @ - SPACES NBUF NL @ TYPE
  OFF @ IF OP 10 + F@ OFV F@ F= IF SPACE ELSE 35 EMIT THEN
  ELSE SPACE THEN  OP 10 + OFV 8 CMOVE -1 OFF ! ;
: ORUB ( i -- )  TXT DUP RC@ ?DUP IF 0 DO DUP I + 1+ RC@
  DUP 34 = I 0= AND IF DROP ELSE EMIT THEN LOOP THEN DROP ;
: ORAD ( i -- )  CR DUP CN.  9 OVER CELL C@ - SPACES
  DUP CTYP C@ 2 = IF ORUB EXIT THEN
  0 OFF !  NOMG @ 0 DO I OKOL LOOP DROP ;
: JAMFOR  NOMG @ 0= IF ." FEL: inga omgångar, OMGANG sparar en"
  EXIT THEN  CR 9 SPACES NOMG @ 0 DO OW 1- SPACES I 49 + EMIT
  SPACE LOOP  NC @ 0> IF NC @ 0 DO I ORAD LOOP THEN CR ;
: OMGANG  0 PARSE KSTART BL? NAMN  DUP 2 = KB C@ 78 = AND KB 1+ C@ 89 = AND
  IF DROP 0 NOMG ! ." omgångarna tömda" EXIT THEN
  IF ." FEL: OMGANG eller OMGANG NY" EXIT THEN
  NOMG @ OMAX = IF ." FEL: fyra omgångar finns" EXIT THEN
  NOMG @ OSPAR  1 NOMG +!  ." omgång " NOMG @ . ;
( Tabellens ord ligger i överlägg 2 och nås med sina xt, som slås )
( upp i överläggets kedja medan det är hämtat. )
: OV2'  PARSE-NAME OV2LST @ (LOOKUP) >XT ;
2 OV
OV2' (TABELL) CONSTANT XTABELL  OV2' (TRAD) CONSTANT XTRAD
OV2' (TBYT) CONSTANT XTBYT  OV2' (TTA) CONSTANT XTTA
OV2' (TLISTA) CONSTANT XTLISTA
1 OV
( TABELL visar tabellen, TRAD text lägger till en rad, n TBYT text )
( byter rad n, n TTA tar bort den, TLISTA visar raderna och TNY )
( tömmer tabellen. )
: TABELL  2 OV XTABELL EXECUTE ;  : TRAD  2 OV XTRAD EXECUTE ;
: TBYT  2 OV XTBYT EXECUTE ;  : TTA  2 OV XTTA EXECUTE ;
: TLISTA  2 OV XTLISTA EXECUTE ;  : TNY  0 8191 RC! ;
: NY  NY 0 NOMG ! TNY ;
: LADDA  LADDA 0 NOMG ! ;
( KOVL läser KALKYL.OVL, överlägg 2, till OVB med DRUN [601E], som )
( laddar filen och kör startadressen, RET sist i överlägget. DE är )
( namnet [11 tecken] i DNAMN och C = 255. Ett fel går via DERRX )
( [FD33] till KOH, som tar tillbaka stacken; saknas filen kommer )
( DRUN tillbaka med CY. f sant om filen lästes och summan stämmer. )
( PUSH BC PUSH DE PUSH IX LD [DSP2],SP LD HL,[DERRX] LD [DERR2],HL )
( LD HL,KOH LD [DERRX],HL LD DE,[DNAMN] LD C,255 CALL DRUN SBC A,A )
( KOH: LD SP,[DSP2] LD HL,[DERR2] LD [DERRX],HL POP IX POP DE )
( LD C,A LD B,0 JP NEXT )
VARIABLE DSP2  VARIABLE DERR2  VARIABLE DNAMN
CREATE KOVLN  75 C, 65 C, 76 C, 75 C, 89 C, 76 C, 32 C, 32 C,
  79 C, 86 C, 76 C,
CREATE XOVL  197 C,  213 C,  221 C, 229 C,  237 C, 115 C, DSP2 ,
  42 C, 64819 ,  34 C, DERR2 ,  33 C, HERE 15 + ,  34 C, 64819 ,
  237 C, 91 C, DNAMN ,  14 C, 255 C,  205 C, 24606 ,  159 C,
  237 C, 123 C, DSP2 ,  42 C, DERR2 ,  34 C, 64819 ,
  221 C, 225 C,  209 C,  79 C,  6 C, 0 C,  195 C, XNEXT ,
: KOVL ( -- f )  KOVLN DNAMN ! XOVL EXECUTE 0= DUP IF DROP
  OVB @ 2 OVN SUM 2 OVE 4 + @ = THEN ;
( OVSTART: ingång 1 [kabs.py], när KALKYL.ABS är inläst och från )
( BASIC med Z=CALL[32768,1]. Första gången läggs överläggen på )
( RAM-skivan: 1 från området och 2 från KALKYL.OVL, tabellen töms )
( och HERE går tillbaka till KBAS. )
: OVSTART  OVINIT @ 0= IF  OVB @ 1 OVE @ 1 OVN RMOVE  0 OVNU !
  KOVL IF OVB @ 2 OVE @ 2 OVN RMOVE  2 OVNU ! ELSE
  ." FEL: KALKYL.OVL kunde inte läsas" CR THEN
  TNY  KBAS @ HERE - ALLOT  -1 OVINIT ! THEN  QUIT ;
HERE KBAS !
