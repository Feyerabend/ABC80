( VERK: text och kalkyl i ett, se VERK.md. Före den här filen )
( kommer kalkyl.fs till raden "Hit är KALKYL och VERK lika": )
( cellerna, omräkningen och överläggen. Sedan ORD:s editor, fälten )
( och menyn. gor.sh skriver in alltihop i Forth med gränsen mot )
( BASIC på ED00 och sparar VERK.ABS; laddaren VERK [verk.bas] läser )
( in den. Ingång 1 är VERK [kabs.py], som startar menyn. )

( Fälten: ett fält är lika många byte i texten som det är brett: )
( 128 + cellens nummer och sedan 255. På skärmen visas cellens )
( värde där, inverterat [bit 7 i bildminnet]. )
( HSCAN: första byten med bit 7 bland n från a, som SCAN. POP HL )
( l: LD A,B OR C JR Z,s BIT 7,[HL] JR NZ,s INC HL DEC BC JR l )
( s: PUSH HL JP NEXT )
CREATE XHSCAN  225 C,  120 C,  177 C,  40 C, 8 C,  203 C, 126 C,
  32 C, 4 C,  35 C,  11 C,  24 C, 244 C,  229 C,  195 C, XNEXT ,
: HSCAN ( a n -- a2 n2 )  XHSCAN EXECUTE ;
: FALT? ( c -- f )  127 > ;
( Bredden: rubrikens längd, mallens eller 10 )
: FBREDD ( i -- w )  DUP CTYP C@ 2 = IF TXT RC@ 1- 1 MAX EXIT THEN
  CMSK C@ ?DUP 0= IF 10 THEN 40 MIN ;
( Värdet som text: a n, och f sant för ett tal [högerställt] )
CREATE FB 48 ALLOT  CREATE FQ 63 C,  CREATE FTF 102 C, 101 C, 108 C,
: FVAL ( i -- a n f )  DUP NC @ < 0= IF DROP FQ 1 -1 EXIT THEN
  DUP CTYP C@ DUP 2 = IF DROP TXT DUP RC@ 1- 0 MAX 40 MIN >R
  2 + FB R@ RMOVE FB R> 0 EXIT THEN
  2 > IF DROP FTF 3 -1 EXIT THEN
  DUP CVAL F@ CMSK NTAL NBUF NL @ -1 ;
( Fältet i bildraden: w tecken från a, med värdet av cell i; ett )
( tal som inte ryms blir *, en rubrik kortas )
VARIABLE FS  VARIABLE FN  VARIABLE FH
: FVISA ( a w i -- )  FVAL FH ! FN ! FS !  2DUP 160 FILL
  FN @ OVER > IF FH @ IF 170 FILL EXIT THEN DUP FN ! THEN
  FH @ IF FN @ - + ELSE DROP THEN
  FN @ ?DUP IF 0 DO FS @ I + C@ 128 OR OVER I + C! LOOP THEN DROP ;
( FW: fältets bredd från a, inom n )
: FW ( a n -- w )  1 BEGIN 2DUP > IF 2 PICK OVER + C@ 255 =
  ELSE 0 THEN WHILE 1+ REPEAT >R 2DROP R> ;
( Fälten i en bildrad, a n; en rest av ett fält blir blank )
: FALTEN ( a n -- )  BEGIN HSCAN DUP WHILE
  OVER C@ 255 = IF OVER 160 SWAP C! 1 ELSE 2DUP FW >R OVER R@
  3 PICK C@ 127 AND FVISA R> THEN  ROT OVER + ROT ROT - REPEAT
  2DROP ;

( Texten på RAM-skivan från 0 till överläggen [OVE], med en lucka )
( vid markören: före markören 0 till GS, efter markören GE till TS. )
( Under överlägg 1 har överlägg 3 2080 bytes [OV3R] och under det )
( överlägg 4 OV4R bytes. )
2080 CONSTANT OV3R  2560 CONSTANT OV4R  VARIABLE GS  VARIABLE GE
1 OVE @ OV3R - OV4R - CONSTANT TS
CREATE MB 256 ALLOT  ( mellanbuffert i vanligt RAM )
: TNY  0 GS !  TS GE ! ;
: LUCKA ( -- n )  GE @ GS @ - ;
: LANGD ( -- n )  TS LUCKA - ;
: POS ( -- n )  GS @ ;
( Skriv in vid markören: ett tecken, n tecken från adr )
: INS ( c -- ) LUCKA IF GS @ RC! 1 GS +! ELSE DROP BEEP THEN ;
: INSN ( adr n -- )  LUCKA OVER < IF 2DROP BEEP EXIT THEN
  >R GS @ R@ RMOVE  R> GS +! ;
( Ta bort n tecken före och efter markören )
: RADERA ( n -- )  POS MIN NEGATE GS +! ;
: STRYK ( n -- )  TS GE @ - MIN GE +! ;
( Flytta markören: framåt flyttas texten efter luckan ned )
: HOGER ( n -- )  TS GE @ - MIN  >R GE @ GS @ R@ RMOVE
  R@ GS +!  R> GE +! ;
( Bakåt flyttas texten upp, från slutet och 256 åt gången )
( via MB, eftersom RMOVE bara kopierar framåt )
: VANSTER ( n -- )  POS MIN  BEGIN DUP WHILE  DUP 256 MIN >R
  R@ NEGATE DUP GS +! GE +!  GS @ MB R@ RMOVE  MB GE @ R@ RMOVE
  R> - REPEAT DROP ;
: GA ( p -- )  POS - DUP 0< IF NEGATE VANSTER ELSE HOGER THEN ;
( Läs texten från p till adr, högst n tecken, utom luckan )
: >FYS ( p -- a )  DUP POS < 0= IF LUCKA + THEN ;
VARIABLE HP  VARIABLE HA  VARIABLE HN
: HAMTA ( p adr n -- n2 )  ROT DUP HP !  LANGD SWAP - 0 MAX MIN
  DUP >R HN ! HA !  POS HP @ - 0 MAX HN @ MIN
  HP @ HA @ ROT DUP >R RMOVE R@ HP +! R@ HA +! R> NEGATE HN +!
  HP @ >FYS HA @ HN @ RMOVE  R> ;
( Prov: T" text" skriver in vid markören )
: T"  32 PARSE 2DROP  34 PARSE INSN ;
( Skärmen: 25 rader, R6 = 25. Bildminnet är 2 KB och läses runt: )
( rad r ligger på $7800 + SO + 80r mod 2048, där SO är början )
( i R12-R13. Rad 0-23 är texten, rad 24 läget. Att rulla en )
( rad är att flytta SO 80 byte och rita bara den nya raden )
79 CONSTANT W  24 CONSTANT NR
CREATE LB 82 ALLOT  CREATE RS 48 ALLOT  CREATE RL 48 ALLOT
VARIABLE TOP  VARIABLE NP  VARIABLE SO
( Tangentkön: ROM:en har plats för ett tecken, så tangenterna )
( hämtas till KQ under ritandet, med POLL )
CREATE KQ 32 ALLOT  VARIABLE KN  VARIABLE GAMMAL
: POLL  INKEY ?DUP IF KN @ 32 < IF KQ KN @ + C! 1 KN +! EXIT THEN
  DROP BEEP THEN ;
: RS@ 2* RS + @ ;  : RS! ( p r -- ) 2* RS + ! ;
: RL@ 2* RL + @ ;  : RL! ( n r -- ) 2* RL + ! ;
( En rad från a: längden som visas och tecknen till nästa )
( rad; vid radslut, blanksteg eller efter W tecken )
: RADLEN ( a n -- len skip )  2DUP W 1+ MIN 13 SCAN
  IF SWAP DROP SWAP - DUP 1+ EXIT THEN
  DROP DUP W > 0= IF SWAP DROP DUP EXIT THEN
  DROP W 1+ 32 -SCAN ?DUP IF DUP 1- SWAP ELSE W DUP THEN ;
( Raden från p till LB; p2 är nästa rad )
: NASTA ( p -- p2 len )  DUP LB W 1+ HAMTA LB SWAP RADLEN
  ROT + SWAP ;
( Bildminnet: 80 tecken från a till rad r, i en eller två delar )
CREATE RB 80 ALLOT  CREATE SB 80 ALLOT
: VPUT ( a r -- )  80 * SO @ + 2047 AND  2048 OVER - 80 MIN >R
  30720 + OVER SWAP R@ CMOVE  80 R@ - ?DUP
  IF SWAP R@ + 30720 ROT CMOVE ELSE DROP THEN R> DROP ;
: RAD! ( a len r -- )  >R >R RB R@ CMOVE
  RB R@ FALTEN  RB R@ + 80 R> - 32 FILL  RB R> VPUT ;
: CRTC ( v r -- )  56 P! 57 P! ;
: START!  SO @ 30720 + DUP 256 / 12 CRTC 255 AND 13 CRTC ;
: VRUM ( n -- )  SO @ + 2047 AND SO ! START! ;
( Sidan hämtas på en gång från PBP till PB i vanligt RAM )
CREATE PB 1920 ALLOT  VARIABLE PN  VARIABLE PBP
: PRAD ( p -- a n )  PBP @ - DUP PB + SWAP PN @ SWAP - 0 MAX ;
: PB! ( p -- )  DUP PBP ! PB 1920 HAMTA PN ! ;
( Rad r från p: början i RS, längden i RL, och ritad )
: RADEN ( p r -- p2 )  POLL >R DUP R@ RS!  DUP PRAD RADLEN ROT +
  SWAP DUP R@ RL!  R@ RS@ PRAD DROP SWAP R> RAD! ;
: LAYOUT  TOP @ DUP PB!  NR 0 DO POLL I RADEN LOOP NP ! ;
( Ändrat sedan skärmen ritades: ANDR till AMAX; VL längden )
VARIABLE ANDR  VARIABLE AMAX  VARIABLE VL  VARIABLE D
: RITAD  32767 ANDR !  0 AMAX !  LANGD VL ! ;
: HELA  LAYOUT RITAD ;
: ANDRAD ( p0 d -- )  OVER POS MIN  DUP ANDR @ MIN ANDR !
  AMAX @ > 0= IF DUP AMAX +! THEN DROP
  POS MAX AMAX @ MAX AMAX ! ;
( Markörens rad, -1 om den inte syns )
: MRAD ( -- r )  -1 NR 0 DO  POLL POS I RS@ -
  DUP 0< 0= SWAP I RL@ > 0= AND IF DROP I LEAVE THEN LOOP ;
( Styckets början: efter sista radslutet före p )
: HSTART ( p -- h )  BEGIN DUP WHILE  DUP 256 MIN >R  R@ -
  DUP MB R@ HAMTA DROP  MB R@ 13 -SCAN ?DUP
  IF R> DROP + EXIT THEN  R> DROP REPEAT ;
( Början på raden som p ligger på )
: RSTART ( p -- p2 )  DUP HSTART BEGIN POLL DUP NASTA >R OVER R> +
  3 PICK < WHILE SWAP DROP REPEAT DROP SWAP DROP ;
( TOP n rader upp )
: UPPN ( n -- )  0 DO TOP @ IF TOP @ 1- RSTART TOP ! THEN LOOP ;
( Rulla en rad: SO flyttas 80 byte och bara nya raden ritas )
: RULLNED  RS 2 + RS 46 CMOVE  RL 2 + RL 46 CMOVE  0 RS@ TOP !
  NP @ DUP NR 1- RS! NASTA DUP NR 1- RL!  SWAP NP !
  80 VRUM  LB SWAP NR 1- RAD!  SB NR VPUT ;
: RULLUPP  NR 1- RS@ NP !  RS RS 2 + 46 CMOVE>  RL RL 2 + 46 CMOVE>
  TOP @ 1- RSTART DUP TOP ! DUP 0 RS! NASTA SWAP DROP DUP 0 RL!
  -80 VRUM  LB SWAP 0 RAD!  SB NR VPUT ;
( Markören efter sista raden )
: UNDER? ( -- f )  POS NR 1- DUP RS@ SWAP RL@ + > ;
( Rulla tills markören syns; långt bort ritas hela skärmen om )
: LANGT  POS NP @ < 0= POS RSTART TOP ! IF NR 1- UPPN THEN HELA ;
: FJARRAN ( -- f )  POS TOP @ 1920 - <  POS NP @ 1920 + > OR ;
: SYNLIG  FJARRAN IF LANGT EXIT THEN
  0 BEGIN DUP NR < POS TOP @ < AND WHILE POLL RULLUPP 1+ REPEAT
  BEGIN DUP NR < UNDER? AND WHILE POLL RULLNED 1+ REPEAT
  NR < 0= IF LANGT THEN ;
( Efter ändringar: rita om från raden KR som kan ha ändrats, )
( tills en rad börjar på samma tecken som förut, D längre fram )
VARIABLE RR  VARIABLE KR
( Rad r kan ha ändrats om ANDR är före radens slut, eller om )
( raden inte slutar med radslut och ANDR är inom 80 tecken )
: PAV? ( r -- f )  DUP RS@ SWAP RL@ OVER +
  DUP ANDR @ < 0= IF 2DROP -1 EXIT THEN
  >FYS RC@ 13 = IF DROP 0 EXIT THEN  80 + ANDR @ < 0= ;
: KRAD  NR  NR 0 DO POLL I PAV? IF DROP I LEAVE THEN LOOP ;
: SYNK? ( p -- f )  DUP D @ - RR @ RS@ =  SWAP AMAX @ < 0= AND ;
: SKIFTA  NR RR @ DO I RS@ D @ + I RS! LOOP  D @ NP +! ;
: OMRITA  ANDR @ TOP @ < IF ANDR @ LANGD MIN RSTART TOP !
  HELA EXIT THEN
  KRAD DUP KR ! DUP RR !  NR < 0= IF EXIT THEN
  LANGD VL @ - D !  RR @ RS@ DUP PB!
  BEGIN POLL RR @ NR < WHILE  RR @ KR @ > IF DUP SYNK? IF
  DROP SKIFTA EXIT THEN THEN  RR @ RADEN  1 RR +!  REPEAT NP ! ;
: NYTT  ANDR @ 32767 < IF OMRITA THEN  RITAD SYNLIG  0 GAMMAL ! ;
: AKTUELL  GAMMAL @ IF NYTT THEN ;
( Tecknet efter och före markören, 0 vid slutet och början )
: EFTER ( -- c )  POS LANGD < IF GE @ RC@ ELSE 0 THEN ;
: FORE ( -- c )  POS IF GS @ 1- RC@ ELSE 0 THEN ;
( Läget på rad 24: texten byggs i SB. Vid ett fält cellens namn, )
( formeln och mallen )
VARIABLE LP  : C>L ( c -- )  LP @ SB 80 + < IF LP @ C! 1 LP +!
  ELSE DROP THEN ;
: L>L ( n -- n2 )  SB 80 + LP @ - MIN 0 MAX ;
: S>L ( a n -- )  L>L LP @ SWAP DUP LP +! CMOVE ;
: U>L ( u -- )  10 U/MOD ?DUP IF RECURSE THEN 48 + C>L ;
: T>L ( a -- )  DUP 1+ SWAP C@ S>L ;
CREATE T1 TXT" VERK markören "  CREATE T2 TXT"  av "
CREATE T3 TXT"  fritt "
( Cellen vid markören, -1 om markören inte står vid ett fält )
: FCELL ( -- i )  EFTER DUP FALT? IF 127 AND DUP NC @ < IF EXIT
  THEN THEN DROP -1 ;
: CN ( i -- a n )  CELL DUP 1+ SWAP C@ ;
: LAGE  POLL SB 80 32 FILL  SB LP !  T1 T>L POS U>L  T2 T>L
  POLL LANGD U>L  T3 T>L LUCKA U>L  FCELL DUP 0< IF DROP ELSE
  32 C>L 32 C>L DUP CN S>L 32 C>L  DUP TXT DUP RC@ L>L SWAP 1+
  LP @ ROT DUP LP +! RMOVE  32 C>L 32 C>L CMSK T>L THEN
  SB NR VPUT ;
: KOL ( r -- c )  POS SWAP RS@ - ;
( Markören: R14-R15 är adressen, 14 bitar som 6845:ans räknare. )
( R10 = 73 blinkar, 41 släcker den medan ORD arbetar )
: CUR ( k r -- )  80 * + SO @ + 14336 + 16383 AND
  DUP 256 / 14 CRTC  255 AND 15 CRTC  73 10 CRTC ;
: MARK  MRAD DUP KOL SWAP CUR ;
: RADER ( n start -- )  DUP 256 / 12 CRTC 255 AND 13 CRTC 6 CRTC ;
( Tangenten: först från kön. CTRL-Q blir ESC [27], som ABC 802:s )
( tangentbord inte har, DEL [en PC:s] blir <- [8] och CTRL-D -> [9] )
: TKEY ( -- c )  KN @ IF KQ C@  KQ 1+ KQ KN @ 1- CMOVE  -1 KN +!
  ELSE BEGIN INKEY ?DUP UNTIL THEN  DUP 17 = IF DROP 27 THEN
  DUP 127 = IF DROP 8 THEN  DUP 4 = IF DROP 9 THEN
  41 10 CRTC ;
( Ett fält är ett tecken för markören: den hamnar aldrig i ett )
( fält [UTUR], och <- och CTRL-G tar bort hela fältet )
: UTUR  BEGIN EFTER 255 = WHILE 1 VANSTER REPEAT ;
: FLEN ( -- n )  1 BEGIN DUP GE @ + DUP TS < IF RC@ 255 =
  ELSE DROP 0 THEN WHILE 1+ REPEAT ;
: STEG ( -- n )  EFTER FALT? IF FLEN ELSE 1 THEN ;
( FINS, FIXA, radeditorn RADED, INTRO, MENY och tangenterna )
( TANGENT med UPP, NED osv. är i överlägg 3 )
VARIABLE 'TANGENT  : TANGENT  3 'TANGENT OVX ;
VARIABLE 'FINS  VARIABLE 'FIXA  VARIABLE 'RADED  VARIABLE 'INTRO
VARIABLE 'MENY  VARIABLE FK  VARIABLE FD  VARIABLE FP  VARIABLE F0
: FINS  3 'FINS OVX ;  : FIXA  3 'FIXA OVX ;  : RADED  3 'RADED OVX ;
: INTRO  3 'INTRO OVX ;  : MENY  3 'MENY OVX ;
( FORMAT och läsningen är i överlägg 4 )
: FORMAT  4 'FORMAT OVX ;
CREATE EB 80 ALLOT  VARIABLE EN  VARIABLE EP
( Raden som tolkas: variablerna har inga namn i kärnan, men TOLKA )
( lägger tal i dem [LIT adress !] )
' TOLKA 5 + @ CONSTANT #TIB  ' TOLKA 11 + @ CONSTANT SRC
' TOLKA 21 + @ CONSTANT >IN
( SATT och TABORT flyttar celler: fälten följer med )
: SATT  NC @ SATT NC @ <  NYPOS @ DUP 0< 0= SWAP NC @ 1- < AND
  AND IF NYPOS @ 1 FIXA THEN ;
: TABORT  SRC @ >IN @ + #TIB @ >IN @ - KSTART BL? NAMN CNAMN
  NC @ >R TABORT R> NC @ > IF -1 FIXA ELSE DROP THEN ;
( CTRL-K: formeln i statusfältet, vid ett fält dess cell. RETURN )
( gör SATT, eller FORMAT för %namn mall; utan fält vid markören )
( blir det ett nytt fält. Felet visas på rad 24, och raden kan )
( rättas. ESC [CTRL-Q] ångrar. )
( KORA: raden som SATT eller FORMAT. Det som skrivs ut är ett )
( fel: EMIT, TYPE och U. anropar PRTXT [000B], och UT byter det )
( mot BTXT, som lägger tecknen från 32 i SB, högst 80. HL är )
( texten och BC längden. f sant om inget skrevs, alltså inget fel. )
( LD DE,[SBP] L1: LD A,B OR C JR Z,L2 DEC BC PUSH HL LD HL,SB+80 )
( AND A SBC HL,DE POP HL JR Z,L2 LD A,[HL] INC HL CP 32 JR C,L1 )
( LD [DE],A INC DE JR L1 L2: LD [SBP],DE RET )
VARIABLE SBP
CREATE BTXT  237 C, 91 C, SBP ,  120 C, 177 C, 40 C, 21 C,  11 C,
  229 C,  33 C, SB 80 + ,  167 C,  237 C, 82 C,  225 C,  40 C, 10 C,
  126 C, 35 C,  254 C, 32 C, 56 C, 235 C,  18 C, 19 C,  24 C, 231 C,
  237 C, 83 C, SBP ,  201 C,
( UTA: anropens adresser i EMIT, TYPE och U. )
CREATE UTA  ' EMIT 10 + ,  ' TYPE 3 + ,  ' U. 31 + ,
: UT ( a -- )  DUP UTA @ !  DUP UTA 2 + @ !  UTA 4 + @ ! ;
: KORA ( -- f )  SRC @ #TIB @ >IN @  EB SRC ! EN @ #TIB ! 0 >IN !
  SB 80 32 FILL  SB SBP !  BTXT UT  EB C@ 37 = IF 1 >IN ! FORMAT
  ELSE SATT THEN  11 UT  >IN ! #TIB ! SRC !  SBP @ SB = ;
: KALK  FCELL DUP 0< IF 0 EN ! ELSE DUP CN DUP EN ! EB SWAP CMOVE
  32 EB EN @ + C! 1 EN +!  DUP TXT DUP RC@ 79 EN @ - MIN SWAP 1+
  EB EN @ + ROT DUP EN +! RMOVE THEN  EN @ EP !
  BEGIN RADED 0= IF DROP EXIT THEN  KORA 0= WHILE
  HELA SB NR VPUT TKEY DROP REPEAT
  0< EB C@ 37 = 0= AND IF EB EN @ KSTART BL? NAMN CNAMN FINS THEN
  99 0 FIXA HELA ;
( Tangenten, och var texten ändrades. CTRL-K här, eftersom KALK )
( hämtar överlägg 1 och TANGENT är i överlägg 3 )
: TANG ( c -- f )  DUP 11 = IF DROP AKTUELL KALK 0 EXIT THEN
  POS LANGD >R >R  TANGENT UTUR  R> R> LANGD SWAP -
  ?DUP IF ANDRAD ELSE DROP THEN ;
( Skärmen ritas bara när kön är tom )
: ORD  PAGE 128 SO ! 25 30848 RADER  SB 80 32 FILL  0 KN !
  99 0 FIXA HELA
  BEGIN KN @ 0= IF NYTT LAGE MARK THEN  TKEY TANG  -1 GAMMAL !
  UNTIL 24 30720 RADER PAGE ;
: TNY  TNY 0 TOP ! ;
( FALT namn: ett fält för cellen vid markören )
: FALT  0 PARSE KSTART BL? NAMN CNAMN  KFEL @ IF DROP FELV EXIT THEN
  FINS ;
( Menyn och introduktionen, som i ord.bas )
: TECKEN ( -- c )  BEGIN INKEY ?DUP UNTIL ;
( XDRUN läser en ABS-fil med DRUN [601E], som laddar den och kör )
( startadressen, här en RET efter datat: VERK.OVL till OVB och )
( VERK.HLP till PB. DE är namnet [11 tecken] i DNAMN och C = 255. )
( Ett fel går via DERRX [FD33] till VOH, som tar tillbaka stacken; )
( saknas filen kommer DRUN tillbaka med CY, och SBC A,A ger -1. )
( Ger 0 eller felet. )
( PUSH BC PUSH DE PUSH IX LD [DSP2],SP LD HL,[DERRX] LD [DERR2],HL )
( LD HL,VOH LD [DERRX],HL LD DE,[DNAMN] LD C,255 CALL DRUN SBC A,A )
( VOH: LD SP,[DSP2] LD HL,[DERR2] LD [DERRX],HL POP IX POP DE )
( LD C,A LD B,0 JP NEXT )
VARIABLE DSP2  VARIABLE DERR2  VARIABLE DNAMN
CREATE OVLN  86 C, 69 C, 82 C, 75 C, 32 C, 32 C, 32 C, 32 C,
  79 C, 86 C, 76 C,
CREATE HLPN  86 C, 69 C, 82 C, 75 C, 32 C, 32 C, 32 C, 32 C,
  72 C, 76 C, 80 C,
CREATE XOVL  197 C,  213 C,  221 C, 229 C,  237 C, 115 C, DSP2 ,
  42 C, 64819 ,  34 C, DERR2 ,  33 C, HERE 15 + ,  34 C, 64819 ,
  237 C, 91 C, DNAMN ,  14 C, 255 C,  205 C, 24606 ,  159 C,
  237 C, 123 C, DSP2 ,  42 C, DERR2 ,  34 C, 64819 ,
  221 C, 225 C,  209 C,  79 C,  6 C, 0 C,  195 C, XNEXT ,
: XDRUN ( namn -- f )  DNAMN ! XOVL EXECUTE ;
( Menyns 6 spara, 7 läsa och 8 skriva ut: FILOP. FNAMNET frågar )
( efter namnet, som FNB har kvar med längden först. )
VARIABLE 'FNAMNET  CREATE FNB 21 ALLOT  0 FNB C!
( ---- Överlägg 3: FINS, FIXA, radeditorn, INTRO, MENY och TANGENT. Det )
( kompileras på OVB, där överlägg 1 läggs tillbaka efteråt, och )
( ligger på RAM-skivan under överlägg 1 och i VERK.ABS efter )
( överlägg 2 i bitarna [OVBIT] ---- )
VARIABLE VH  VARIABLE OV3LST  VARIABLE OV4LST
HERE VH !  'LAST @ RLAST !  OVB @ HERE - ALLOT
: K? ( c k -- c f )  OVER = ;
( Upp och ned: samma kolumn, eller radens slut )
: TILL ( p len c -- )  MIN + GA ;
: UPP  MRAD DUP KOL SWAP ?DUP IF 1- DUP RS@ SWAP RL@ ROT TILL
  ELSE TOP @ IF TOP @ 1- RSTART DUP NASTA SWAP DROP ROT TILL
  ELSE DROP THEN THEN ;
: NED  MRAD DUP KOL SWAP  DUP RS@ OVER RL@ + LANGD < 0= IF
  2DROP EXIT THEN  DUP NR 1- < IF 1+ DUP RS@ SWAP RL@ ROT TILL
  ELSE DROP NP @ DUP NASTA SWAP DROP ROT TILL THEN ;
: HEM  MRAD RS@ GA ;
: RSLUT  MRAD DUP RS@ SWAP RL@ + GA ;
: SUPP  MRAD KOL  TOP @ 0= IF DROP 0 GA EXIT THEN
  NR 1- UPPN  HELA  0 RS@ 0 RL@ ROT TILL ;
: SNED  MRAD KOL  NP @ LANGD < IF NP @ TOP ! HELA
  0 RS@ 0 RL@ ROT TILL ELSE DROP LANGD GA THEN ;
( Tangenterna; ESC [CTRL-Q] slutar. <- suddar som i BASIC, )
( CTRL-S flyttar åt vänster )
: (TANGENT) ( c -- f )  27 K? IF DROP -1 EXIT THEN
  DUP 32 < 0= OVER 127 < AND IF INS 0 EXIT THEN
  13 K? IF INS 0 EXIT THEN
  19 K? IF 1 VANSTER THEN  9 K? IF STEG HOGER THEN
  8 K? IF FORE FALT? IF 1 VANSTER UTUR FLEN STRYK ELSE 1 RADERA
  THEN THEN  7 K? IF STEG STRYK THEN
  5 K? IF AKTUELL UPP THEN  24 K? IF AKTUELL NED THEN
  1 K? IF AKTUELL HEM THEN  6 K? IF AKTUELL RSLUT THEN
  18 K? IF AKTUELL SUPP THEN  22 K? IF AKTUELL SNED THEN
  DROP 0 ;
( FINS: ett fält för cell i vid markören, i dess bredd )
: (FINS) ( i -- )  DUP FBREDD DUP LUCKA > IF 2DROP BEEP EXIT THEN
  1- SWAP 128 + INS  ?DUP IF 0 DO 255 INS LOOP THEN ;
( FIXA: alla fält i texten får cellens bredd, och numren följer )
( cellerna: från cell k ändras numret med d, och d = -1 tar bort )
( fälten för cell k. Markören och TOP följer med. )
: FFL ( n -- )  F0 @ FP @ < IF DUP FP +! THEN
  F0 @ TOP @ < IF DUP TOP +! THEN DROP ;
: FIX1  GE @ RC@ 127 AND  FLEN  OVER FK @ = FD @ 0< AND
  IF DUP STRYK NEGATE FFL DROP EXIT THEN
  SWAP DUP FK @ < 0= IF FD @ + THEN  DUP 128 + GE @ RC!
  DUP NC @ < IF FBREDD ELSE DROP DUP THEN  1 HOGER
  2DUP SWAP - DUP FFL  DUP 0< IF NEGATE STRYK ELSE ?DUP IF 0 DO
  255 INS LOOP THEN THEN  MIN 1- HOGER ;
: (FIXA) ( k d -- )  FD ! FK !  POS FP !  0 GA
  BEGIN TS GE @ - DUP WHILE  256 MIN >R  GE @ MB R@ RMOVE
  MB R@ HSCAN IF MB - HOGER  POS F0 !  GE @ RC@ 255 = IF 1 STRYK -1 FFL
  ELSE FIX1 THEN ELSE DROP R@ HOGER THEN R> DROP REPEAT DROP
  FP @ GA ;
: EINS ( c -- )  EN @ 79 < IF EB EP @ + DUP 1+ EN @ EP @ - CMOVE>
  EB EP @ + C! 1 EP +! 1 EN +! ELSE DROP BEEP THEN ;
: EDEL  EP @ IF EB EP @ + DUP 1- EN @ EP @ - CMOVE -1 EP +!
  -1 EN +! THEN ;
: (RADED) ( -- f )  BEGIN SB 80 32 FILL EB SB EN @ CMOVE SB NR VPUT
  EP @ NR CUR TKEY  27 K? IF DROP 0 EXIT THEN
  13 K? IF DROP -1 EXIT THEN  8 K? IF EDEL THEN
  19 K? IF EP @ IF -1 EP +! THEN THEN
  9 K? OVER 7 = OR IF EP @ EN @ < IF 1 EP +! 7 K? IF EDEL THEN THEN
  THEN  1 K? IF 0 EP ! THEN  6 K? IF EN @ EP ! THEN
  24 K? IF 0 EP ! 0 EN ! THEN
  DUP 32 < 0= OVER 127 < AND IF DUP EINS THEN DROP AGAIN ;
( Introduktionen är i VERK.HLP, som läses till PB [det ritas om )
( efteråt]: kod 1 är textens längd, 13 en ny rad och 0 slutet. )
: (INTRO)  PAGE  HLPN XDRUN IF ." VERK.HLP kunde inte läsas." CR CR
  ." Tryck på en tangent." ELSE PB BEGIN DUP C@ ?DUP WHILE
  DUP 1 = IF DROP TS . ELSE DUP 13 = IF DROP CR ELSE EMIT THEN THEN
  1+ REPEAT DROP THEN  TECKEN DROP ;
: (MENY) ( -- k )  PAGE ." VERK" CR CR
  ." 1 fortsätt   2 ny text   3 introduktionen   4 slut"
  ."    5 Forth" CR CR ." 6 spara      7 läsa       8 skriva ut"
  BEGIN TECKEN 48 - DUP 1 < OVER 8 > OR WHILE DROP REPEAT ;
: (FNAMNET) ( -- a n )  ." Filens namn"  FNB C@ IF ."  ["
  FNB DUP 1+ SWAP C@ TYPE ." ]" THEN ." : "  MB 40 ACCEPT
  ?DUP IF 20 MIN DUP FNB C!  MB FNB 1+ ROT CMOVE THEN
  FNB DUP 1+ SWAP C@ ;
' (TANGENT) 'TANGENT !  ' (FINS) 'FINS !  ' (FIXA) 'FIXA !
' (RADED) 'RADED !
' (INTRO) 'INTRO !  ' (MENY) 'MENY !  ' (FNAMNET) 'FNAMNET !
'LAST @ OV3LST !  1 OVE @ HERE OVB @ - - 3 OVSPAR
RLAST @ 'LAST !  1 OVE @ OVB @ 1 OVN RMOVE  VH @ HERE - ALLOT
3 OVN OV3R > 3 OVN 1 OVN 2 OVN MAX > OR
( Byggprovet: längden och -1 om det får plats )
CR 3333 . 3 OVN . 0= .
( ---- Överlägg 2 i VERK: spara och skriva ut. Det kompileras om på )
( OVB som överlägg 3, med det delade ur kalkyl.fs till "i överlägg )
( 2", och får KALKYL:s plats på RAM-skivan och i bitarna. ---- )
VARIABLE V2H  VARIABLE OV2G  2 OVN OV2G !
HERE V2H !  'LAST @ RLAST !  OVB @ HERE - ALLOT
( ---- gor.sh lägger det delade här ---- )
86 FTYP C!  82 FTYP 1+ C!  75 FTYP 2 + C!
( Spara: texten med fälten som <NAMN>, sedan en rad %% och cellerna )
( som i en .KAL-fil. Filen är NAMN.VRK. )
: PFALT ( c -- )  127 AND DUP NC @ < IF 60 PUTB CN PUTS 62 PUTB
  ELSE DROP THEN ;
: PTECK ( c -- )  DUP 255 = IF DROP EXIT THEN
  DUP FALT? IF PFALT EXIT THEN  PUTB ;
: PTEXT  0 BEGIN DUP LANGD < WHILE DUP MB 256 HAMTA
  DUP 0 DO MB I + C@ PTECK LOOP + REPEAT DROP ;
: (SPARA) ( a n -- )  SOPPNA IF PTEXT  13 PUTB 37 PUTB 37 PUTB
  13 PUTB PCELLER PSLUT DFEL @ 0= IF ." Sparad: " FNAMN 8 -BL TYPE
  46 EMIT FNAMN 8 + 3 -BL TYPE THEN THEN ;
( SKRIV: texten till PR: med raderna som på skärmen och värdena i )
( fälten, utan bit 7 )
: PRAD1 ( p -- p2 )  NASTA ?DUP IF >R LB RB R@ CMOVE  RB R@ FALTEN
  R@ 0 DO RB I + DUP C@ 127 AND SWAP C! LOOP  RB R> PRUT THEN
  CRLF 2 PRUT ;
: VSKR  0 BEGIN DUP LANGD < PFEL @ 0= AND WHILE PRAD1 REPEAT DROP ;
' VSKR CONSTANT XVSKR
: (SKRIV)  ." Utskrift till PR:" CR  XVSKR PRALLT
  PFEL @ 0= IF ." Klart." THEN ;
: (VISA)  ." VISA finns inte i VERK" ;
' (SPARA) 'SPARA !  ' (SKRIV) 'SKRIV !  ' (VISA) 'VISA !
'LAST @ OV2LST !  2 OVE @ 2 OVN + HERE OVB @ - - 2 OVSPAR
RLAST @ 'LAST !  1 OVE @ OVB @ 1 OVN RMOVE  V2H @ HERE - ALLOT
( Byggprovet: längden, den i KALKYL och -1 om det får plats )
CR 2222 . 2 OVN . OV2G @ . 2 OVN OV2G @ > 0= .
( ---- Överlägg 4 i VERK: läsningen och FORMAT, med det delade ur )
( kalkyl.fs till "i båda" och från "i överlägg 2". Det kompileras )
( på OVB, ligger på RAM-skivan på TS och följer inte med i )
( VERK.ABS: kabs.py tar det till filen VERK.OVL, som VSTART läser. )
VARIABLE V4H
HERE V4H !  'LAST @ RLAST !  OVB @ HERE - ALLOT
( ---- gor.sh lägger läsningen här ---- )
86 FTYP C!  82 FTYP 1+ C!  75 FTYP 2 + C!
( Läsa: först cellerna efter raden %%, sedan filen igen och texten, )
( LANT rader. <NAMN> med en cell blir ett fält, annat är text. )
( FULL: texten fick inte plats. PEND: ett tecken att läsa igen. )
VARIABLE LANT  VARIABLE PEND  VARIABLE FULL  CREATE NB 8 ALLOT
VARIABLE NBN
: %%? ( -- f )  LN @ 2 = LBUF C@ 37 = AND LBUF 1+ C@ 37 = AND ;
: RAKNAR  0 LANT !  BEGIN LRAD %%? IF DROP LCELLER EXIT THEN
  LN @ OVER OR IF 1 LANT +! THEN 0= UNTIL ;
: LI ( c -- )  LUCKA IF INS ELSE DROP -1 FULL ! THEN ;
: GETC ( -- c )  PEND @ DUP 0< IF DROP GETB ELSE -1 PEND ! THEN ;
: LFINS ( i -- )  DUP FBREDD DUP LUCKA > IF 2DROP -1 FULL ! EXIT THEN
  1- SWAP 128 + INS  ?DUP IF 0 DO 255 INS LOOP THEN ;
: LFALT  0 NBN !  BEGIN GETB DUP BOKST? OVER SIFF? OR NBN @ 8 < AND
  WHILE NB NBN @ + C! 1 NBN +! REPEAT
  DUP 62 = NBN @ AND IF NB NBN @ CSOK DUP 0< 0= IF SWAP DROP
  LFINS EXIT THEN DROP THEN
  PEND !  60 LI  NBN @ ?DUP IF 0 DO NB I + C@ LI LOOP THEN ;
: LTECK ( c -- )  DUP 60 = IF DROP LFALT EXIT THEN
  DUP 9 = IF DROP GETB ?DUP IF 0 DO 32 LI LOOP THEN EXIT THEN
  DUP 31 > OVER 127 < AND IF LI ELSE DROP THEN ;
: LTEXT  TNY  -1 PEND !  LANT @ BEGIN DUP WHILE GETC
  DUP 13 = IF DROP 1- DUP IF 13 LI THEN ELSE
  ?DUP IF LTECK ELSE DROP 0 THEN THEN REPEAT DROP  0 GA ;
( VLA: d f som [LADDA] i KALKYL )
: VLA ( a n -- d f )  0 FULL !  2DUP LOPPNA 0= IF >R 2DROP R> 0 EXIT
  THEN DROP  NY RAKNAR STANG  DFEL @ ?DUP IF >R 2DROP TNY R> -1 EXIT
  THEN  LOPPNA IF DROP LTEXT STANG DFEL @ ELSE TNY THEN -1
  FULL @ IF ." FEL: texten fick inte plats" CR THEN ;
' (FORMAT) 'FORMAT !  ' VLA 'LADDA !
'LAST @ OV4LST !  TS 4 OVSPAR
RLAST @ 'LAST !  1 OVE @ OVB @ 1 OVN RMOVE  V4H @ HERE - ALLOT
( Byggprovet: längden och -1 om det får plats, med en byte till )
( för filens startadress [RET] )
CR 4444 . 4 OVN . 4 OVN OV4R > 4 OVN 1+ OV2G @ > OR 0= .
( LADDA, SPARA och SKRIV i Forth. Efter läsningen kompileras och )
( räknas cellerna med överlägg 1 [VLADDA]: f sant om det gick. )
: VLADDA ( a n -- f )  4 'LADDA OVX IF 1 'LADDA2 OVX -1 ELSE
  ?DUP IF DFELV THEN 0 THEN ;
: LADDA  0 PARSE VLADDA DROP ;  : SPARA  0 PARSE 2 'SPARA OVX ;
( FILOP: menyns 6, 7 och 8; f sant om en text lästes )
: FILOP ( k -- f )  PAGE DUP 8 = IF DROP SKRIV 0 ELSE
  3 'FNAMNET OVX CR ROT 6 = IF 2 'SPARA OVX 0 ELSE VLADDA THEN THEN
  CR ." Tryck på en tangent." TECKEN DROP ;
( VERK: ingång 1. Första gången introduktionen och en ny text; från )
( BASIC med Z=CALL[32768,1] menyn, med texten kvar. 5 går till Forth: )
( VERK startar menyn igen, BYE går till BASIC. 2 tar bort cellerna )
( också. )
VARIABLE 'VSTART  VARIABLE VLAST
( Returstacken har 64 bytes under BASIC:s SP [SAVESP 8003]; CTRL-K )
( som kompilerar om alla formler går djupare. VERK flyttar därför )
( datastackens botten S0 [8005] till 160 bytes under SP och tömmer )
( stackarna med RESET. VERK går aldrig tillbaka, så det går. )
: VERK  32771 @ 160 - 32773 ! RESET
  OVINIT @ 0= IF 'VSTART @ EXECUTE INTRO TNY NY ORD THEN
  BEGIN MENY  DUP 4 = IF DROP PAGE BYE THEN
  DUP 5 = IF DROP PAGE ." Forth. VERK går till menyn, BYE till"
  ."  BASIC." CR QUIT THEN
  DUP 5 > IF FILOP ELSE DUP 3 = IF INTRO THEN 2 = IF TNY NY THEN
  -1 THEN  IF ORD THEN AGAIN ;
( ---- Efter KBAS det som bara används vid starten: där hamnar )
( sedan formlernas kod. VSTART tar bort orden ur ordlistan ---- )
HERE KBAS !  'LAST @ VLAST !
( Överlägg 2 och 3 ligger i VERK.ABS efter varandra i OVSN bitar )
( i OVBIT, som inte behöver något innehåll vid starten: PB, )
( CELLER, MALLAR, MB, EB, RB, SB och sist det som är fritt upp )
( till $ECB0 [den biten får sin plats på slutet]. OVDEL flyttar )
( dem mellan bitarna och RAM-skivan: f sant till bitarna. OVI och )
( OVO är platsen i bitarna, och OVK det som inte fick plats. )
CREATE OVBIT  PB , 1920 ,  CELLER , CMAX 1+ CL * ,
  MALLAR , CMAX 2 + 12 * ,  MB , 256 ,  EB , 80 ,  RB , 80 ,
  SB , 80 ,  0 , 0 ,
8 CONSTANT OVSN  OVSN 1- 4 * OVBIT + CONSTANT OVSIS
VARIABLE OVR  VARIABLE OVK  VARIABLE OVI  VARIABLE OVO  VARIABLE OVF
VARIABLE OVOK
: OVBA ( -- a k )  OVBIT OVI @ + DUP @ OVO @ +
  SWAP 2 + @ OVO @ -  OVK @ MIN ;
: OV1 ( a k -- )  >R  OVF @ IF OVR @ SWAP ELSE OVR @ THEN R@ RMOVE
  R@ OVR +!  R@ NEGATE OVK +!  R> OVO +! ;
: OVNB  OVBIT OVI @ + 2 + @ OVO @ = IF 0 OVO ! 4 OVI +! THEN ;
: OVSTR ( ra n -- )  OVK ! OVR !
  BEGIN OVK @ OVI @ OVSN 4 * < AND WHILE OVBA OV1 OVNB REPEAT ;
: OVDEL ( f -- )  OVF !  0 OVI !  0 OVO !
  2 OVE @ 2 OVN OVSTR  OVK @  3 OVE @ 3 OVN OVSTR  OVK +! ;
( Det som används av den sista biten )
: OVSIST ( -- n )  OVBIT OVI @ + OVSIS = IF OVO @ ELSE
  OVBIT OVI @ + OVSIS > IF OVSIS 2 + @ ELSE 0 THEN THEN ;
( VOVL: f sant om överlägg 4 lästes och summan stämmer )
: VOVL ( -- f )  OVLN XDRUN 0= DUP IF DROP
  OVB @ 4 OVN SUM 4 OVE 4 + @ = THEN ;
( VSTART: första gången läggs överläggen på RAM-skivan, som )
( OVSTART i kalkyl.fs: 1 från området, 2 och 3 från bitarna och 4 )
( från VERK.OVL, som sedan är kvar i området )
: VSTART  OVB @ 1 OVE @ 1 OVN RMOVE  0 OVDEL
  VOVL IF OVB @ 4 OVE @ 4 OVN RMOVE  4 OVNU ! ELSE 0 OVNU !
  PAGE ." FEL: VERK.OVL kunde inte läsas, läsa och FORMAT går inte"
  CR TECKEN DROP THEN
  VLAST @ 'LAST !  KBAS @ HERE - ALLOT
  MALLAR CMAX 2 + 12 * 0 FILL  -1 OVINIT ! ;
' VSTART 'VSTART !
( Sist bitarna med överläggen i ett ord, så att inget tolkas mellan )
: OVFYLL  HERE OVSIS !  60592 HERE - OVSIS 2 + !
  -1 OVDEL  OVK @ 0= OVOK !  OVSIST ALLOT ;
OVFYLL
