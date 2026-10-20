\ Forthaid: kommandon i stil med Smartaid, i Forth på tolken från
\ exempel 7 (e7.fs och e7i.fs, som läses före den här filen). Utanför
\ boken, se LASMIG.md och MANUAL.md. Startas från BASIC med
\ Z=CALL(16384), och Z=CALL(16384,1) fortsätter med de egna orden; BYE
\ går tillbaka.
\ Z=CALL(16384,2) kopplar in kretsen, så att kommandona skrivs direkt i
\ BASIC (PA och KBD sist i filen).
\
\ RAM: radbufferten och nya ord ligger i kassettens blockbuffertar,
\ $FB00-$FCFF, som ROM:en bara använder medan en fil läses eller skrivs,
\ och där ligger också stackarna när kretsen är inkopplad (de översta
\ 128 byten). Variablerna ligger som förut i bildminnets osynliga byte.

: TIB  $FB00 ;
$FC80 CONSTANT TAK               \ där de nya orden måste sluta (KBD:s stackar)

\ BASIC:s pekare (band 1, systemvariablerna).
$FE1E CONSTANT EOFA
$FE27 CONSTANT STKTOP
\ Pekarna också som ord, så att de går att skriva vid OK (en konstant
\ har inget huvud, och i kretsens egna ord blir de fortfarande tal).
: BOFA  $FE1C ;
: EOFA  $FE1E ;
: HEAP  $FE20 ;

\ Ett kommando som ändrar programmet sätter CHANGED. Då går BYE tillbaka
\ som när ett program är slut (ENDPRG: CLEAR och ABC80): raden med CALL
\ kan vara överskriven och dess variabel borta. Annars som förut, med
\ talet överst på stacken som CALL:s värde.
VARIABLE CHANGED
: BYE  CHANGED @ IF 0 CHANGED ! ENDPRG THEN (BYE) ;
: BAD  ." ?" CR ;

\ Tal: decimalt eller med $ hexadecimalt, ev. med minustecken. Ersätter
\ bokens NUMBER?, så att också tolken tar $-tal.
VARIABLE BASE
: DIGIT?  ( c -- d -1 | 0 )
  [CHAR] 0 -  9 OVER < IF 7 -  DUP 10 < IF DROP 0 EXIT THEN THEN
  DUP BASE @ U< IF -1 ELSE DROP 0 THEN ;
: NUMBER?  ( a n -- x -1 | 0 )
  OVER C@ [CHAR] - = DUP >R IF 1- SWAP 1+ SWAP THEN
  10 BASE !  OVER C@ [CHAR] $ = IF 16 BASE ! 1- SWAP 1+ SWAP THEN
  DUP 0= IF DROP DROP R> DROP 0 EXIT THEN
  0 SWAP 0 DO
    OVER I + C@ DIGIT? 0= IF DROP DROP UNLOOP R> DROP 0 EXIT THEN
    SWAP BASE @ * +
  LOOP
  SWAP DROP  R> IF NEGATE THEN  -1 ;

: +!  ( n a -- )  DUP @ ROT + SWAP ! ;

\ Hexadecimalt: en siffra, en byte, ett ord och ett blanktecken.
: DIG  ( n -- c )  DUP 10 < IF 48 ELSE 55 THEN + ;
: H2  ( b -- )  16 U/MOD DIG EMIT DIG EMIT ;
: H.  ( u -- )  256 U/MOD H2 H2 SPACE ;

\ Resten av raden: blanktecken, nästa tecken (0 vid slutet) och ett
\ decimalt tal, med f = 0 om det inte fanns något.
: PEEKC  ( -- c )  TIB >IN @ + DUP END? IF DROP 0 ELSE C@ THEN ;
: SKIPBL  BEGIN PEEKC 32 = WHILE 1 >IN +! REPEAT ;
: GETNUM  ( -- n f )
  SKIPBL 0 0
  BEGIN PEEKC [CHAR] 0 - DUP 10 U< WHILE
    ROT 10* + SWAP DROP -1  1 >IN +!
  REPEAT DROP ;
\ Ett radintervall som i LIST: n, n-m, n-, -m eller inget (allt);
\ komma går lika bra som bindestreck.
: RANGE  ( -- lo hi )
  GETNUM SKIPBL PEEKC DUP [CHAR] - = SWAP [CHAR] , = OR IF
    DROP 1 >IN +!  GETNUM 0= IF DROP 65535 THEN
  ELSE IF DUP ELSE DROP 0 65535 THEN THEN ;

\ SYS: programmets gränser och det lediga minnet, för BASIC och för
\ Forth.
: SYS
  CR ." BOFA " BOFA @ H.  ." EOFA " EOFA @ H.  ." HEAP " HEAP @ H. CR
  ." PROGRAM " EOFA @ 1+ BOFA @ - U.
  ." VAR " HEAP @ EOFA @ 1+ - U.
  ." FRITT " STKTOP @ HEAP @ - U. CR
  ." ORDEN " HERE H. ." FRITT " TAK HERE - U. CR ;

\ Programmets rader: längden, radnumret (två byte) och koden; längden
\ 1 är slutet (band 1, kommandoloopen).
: LINE#  ( a -- n )  1+ @ ;
: END-PRG?  ( a -- f )  C@ 1 = ;
: NEXTLINE  ( a -- a' )  DUP C@ + ;
: FINDLN  ( n -- a )  \ första raden med numret n eller högre
  BOFA @ BEGIN DUP END-PRG? 0= IF OVER OVER LINE# SWAP U< ELSE 0 THEN
  WHILE NEXTLINE REPEAT SWAP DROP ;

\ Sidorna: ROWS räknar ned de skärmrader som är kvar. När sidan är full
\ väntar MORE? på en tangent: mellanslag ger en rad till, RETURN en ny
\ sida, och en annan tangent avbryter (f = sant). BREAK avbryter också.
VARIABLE ROWS
: PAGE  22 ROWS ! ;
: BRK?  ( -- f )  $FE07 C@  0 $FE07 C! ;
: MORE?  ( n -- f )
  ROWS @ SWAP - DUP ROWS !  1 < IF
    KEY DUP 32 = IF DROP 1 ROWS ! 0 ELSE
      13 = IF PAGE 0 ELSE -1 THEN THEN
  ELSE 0 THEN  BRK? OR ;

\ LIST: raden görs om till text med ROM:ens DETOK, på heapen efter
\ variablerna; n = skärmraderna den tog.
: LISTLINE  ( a -- n )
  HEAP @ DETOK  HEAP @ - 1-  DUP HEAP @ SWAP TYPE CR
  40 U/MOD SWAP DROP 1+ ;
: (LIST)  ( lo hi -- )
  PAGE  SWAP FINDLN
  BEGIN DUP END-PRG? 0= IF OVER OVER LINE# U< 0= ELSE 0 THEN WHILE
    DUP LISTLINE MORE? IF DROP DROP EXIT THEN
    NEXTLINE
  REPEAT DROP DROP ;
: LIST  RANGE CR (LIST) ;

\ PEEK a: åtta byte per rad, hexadecimalt och som tecken. Utan adress
\ fortsätter den där den slutade.
VARIABLE PADR
: PCHAR  ( c -- )  127 AND DUP 32 < IF DROP [CHAR] . THEN EMIT ;
: PEEKROW  ( a -- a' )
  DUP H.  8 0 DO DUP I + C@ H2 SPACE LOOP
  8 0 DO DUP I + C@ PCHAR LOOP CR  8 + ;
: PEEK
  PARSE-NAME NUMBER? 0= IF PADR @ THEN
  CR PAGE BEGIN PEEKROW 1 MORE? UNTIL PADR ! ;

\ FIND text: de rader där texten står, så som raden listas; nyckelord,
\ variabler och radnummer söks alltså också som text. Texten är resten
\ av raden, och den står kvar i radbufferten medan sökningen pågår.
: REST  ( -- a n )  \ resten av raden, och raden är slut
  SKIPBL TIB >IN @ +  #TIB @ >IN @ -  #TIB @ >IN ! ;
: HIT?  ( a -- f )  \ står PAT PLEN i raden, så som den listas?
  HEAP @ DETOK  HEAP @ -  1-  HEAP @ SWAP PAT @ PLEN @ SEARCH ;
: FIND
  REST PLEN ! PAT !  CR PAGE  PLEN @ 0= IF EXIT THEN
  BOFA @ BEGIN DUP END-PRG? 0= WHILE
    DUP HIT? IF DUP LISTLINE MORE? IF DROP EXIT THEN THEN
    NEXTLINE
  REPEAT DROP ;

\ VAR: alla variabler i programmet, i bokstavsordning. VAR namn: de rader
\ där variabeln står; A( betyder A med index. En variabel i koden är
\ token, namnbyten, bokstaven och adressen. Namnbyten har siffran i bit
\ 4-7 ($F utan siffra), antalet index i bit 2-3 och typen i bit 0-1
\ (0 flyttal, 1 heltal, 2 sträng). MATCHVAR och SETVARS hittar dem i
\ raden.
VARIABLE NB   VARIABLE LET
: ADV1  ( a n -- a+1 n-1 )  1- SWAP 1+ SWAP ;
: VARNAME  ( a n -- )  \ namnet till NB och LET
  OVER C@ $DF AND LET !  ADV1  $F0 NB !
  DUP IF OVER C@ [CHAR] 0 - DUP 10 U< IF
    16 * $0F OR NB @ AND NB !  ADV1 ELSE DROP THEN THEN
  DUP IF OVER C@ DUP [CHAR] % = IF DROP NB @ 1 OR NB !  ADV1 ELSE
    [CHAR] $ = IF NB @ 2 OR NB !  ADV1 THEN THEN THEN
  DUP IF OVER C@ 40 = IF NB @ 4 OR NB ! THEN THEN
  DROP DROP ;
: VKEY  ( -- k )  LET @ 256 * NB @ + ;      \ till MATCHVAR
: USES?  ( rad -- f )  3 + 0 VKEY MATCHVAR SWAP DROP ;

\ Utan namn: en bit per tänkbar variabel i en tabell på heapen, i
\ ordningen bokstav, siffra (ingen först), typ och index, 26*11*3*2 =
\ 1716 bitar i 215 byte. SETVARS sätter bitarna för en rad, och
\ sedan skrivs tabellen ut i ordning; tomma byte hoppas över.
: .VAR  ( n -- )  \ bitens nummer tillbaka till namnet
  2 U/MOD 3 U/MOD 11 U/MOD [CHAR] A + EMIT
  ?DUP IF [CHAR] / + EMIT THEN               \ siffran + 1, 0 utan
  DUP 1 = IF [CHAR] % EMIT THEN 2 = IF [CHAR] $ EMIT THEN
  IF 40 EMIT THEN SPACE ;
: ALLVARS
  STKTOP @ HEAP @ - 256 U< IF BAD EXIT THEN
  0 HEAP @ C!  HEAP @ DUP 1+ 214 CMOVE
  BOFA @ BEGIN DUP END-PRG? 0= WHILE DUP SETVARS NEXTLINE REPEAT DROP
  215 0 DO HEAP @ I + C@ ?DUP IF 8 0 DO
    2 U/MOD SWAP IF J 8 * I + .VAR THEN
  LOOP DROP THEN LOOP CR ;
: VAR
  PARSE-NAME CR  DUP 0= IF DROP DROP ALLVARS EXIT THEN
  VARNAME PAGE  BOFA @ BEGIN DUP END-PRG? 0= WHILE
    DUP USES? IF DUP LISTLINE MORE? IF DROP EXIT THEN THEN
    NEXTLINE
  REPEAT DROP ;

\ DEL a-b: tar bort raderna; resten av programmet och slutmarkeringen
\ flyttas ned. Utan intervall händer ingenting.
: DEL
  CR SKIPBL PEEKC 0= IF EXIT THEN
  RANGE SWAP FINDLN SWAP  DUP 65535 = IF DROP EOFA @ ELSE 1+ FINDLN THEN
  OVER OVER U< 0= IF DROP DROP EXIT THEN
  OVER OVER SWAP - >R  DUP EOFA @ SWAP - 1+  ROT SWAP CMOVE
  R> NEGATE EOFA +!  -1 CHANGED ! ;

\ OLD: tillbaka efter NEW, som bara skrev slutmarkeringen 1 över den
\ första radens längd. Längden letas upp: den första L där raden slutar
\ med CR och raderna efter bildar en kedja med stigande nummer fram till
\ en slutmarkering. Sedan CLEAR, så att heapen hamnar efter
\ programmet. Kretsen måste startas med PRINT CALL(16384):
\ Z=CALL(16384) lägger Z på heapen, som efter NEW börjar mitt i den
\ första raden (variabellistan $FE29 är då inte tom).
VARIABLE PREV
\ CLEAR ligger på $0A81 i 9913 och $0A80 i 11273; ENDPRG på $00C9
\ anropar den i båda.
: CLEAR  $00CA @ ROMCALL ;
: CHAIN  ( a -- slut | 0 )
  BEGIN DUP C@ 1 = 0= WHILE
    DUP STKTOP @ U< 0=  OVER C@ 4 < OR  OVER NEXTLINE 1- C@ 13 = 0= OR
    OVER LINE# PREV @ SWAP U< 0= OR  IF DROP 0 EXIT THEN
    DUP LINE# PREV !  NEXTLINE
  REPEAT ;
: OLD
  CR BOFA @ DUP C@ 1 = 0= IF DROP BAD EXIT THEN
  $FE29 @ IF DROP ." PRINT CALL" 40 EMIT ." 16384" 41 EMIT CR EXIT THEN
  256 4 DO
    DUP I + 1- C@ 13 = IF
      DUP LINE# PREV !  DUP I + CHAIN ?DUP IF
        EOFA !  I SWAP C!  CLEAR  -1 CHANGED !  UNLOOP EXIT
      THEN
    THEN
  LOOP DROP BAD ;

\ CHANGE v1,v2: byter variabeln v1 mot v2 överallt. Typen och om den
\ har index måste vara desamma. Namnbyten får v2:s siffra; antalet
\ index står kvar.
VARIABLE NB2   VARIABLE LET2
: NAME?  ( a n -- f )  DUP IF VARNAME -1 ELSE DROP DROP 0 THEN ;
: RENAME  ( v -- )
  DUP 1+ DUP C@ $0C AND NB2 @ $F3 AND OR SWAP C!  LET2 @ SWAP 2 + C! ;
: CHANGE
  CR SKIPBL [CHAR] , PARSE NAME?  NB @ LET @  PARSE-NAME NAME?
  NB @ NB2 !  LET @ LET2 !  >R LET ! NB ! R> AND
  NB @ 7 AND NB2 @ 7 AND = AND 0= IF BAD EXIT THEN
  BOFA @ BEGIN DUP END-PRG? 0= WHILE
    DUP 3 + 0 BEGIN VKEY MATCHVAR WHILE DUP RENAME 5 + 1 REPEAT
    DROP NEXTLINE
  REPEAT DROP  -1 CHANGED ! ;

\ REN a,s,från-till: numrerar om raderna från-till med a, a+s, ...;
\ utan intervall hela programmet, utan a och s 10 och 10. Referenserna
\ följer med som i BASIC:s REN: ROM:ens LNKREF ger varje referens
\ målradens adress, raderna får sina nya nummer, och REN:s tredje varv
\ hämtar numren till referenserna. De ligger två byte längre fram i
\ 11273 än i 9913 ($0CFF och $0CCD); anropet av TOKLIN på $0101 visar
\ vilken ROM det är ($1890 i 11273). Numren måste
\ stiga också efteråt, annars ändras ingenting. En referens till en rad
\ som inte finns ger fel 6 i LNKREF, före ändringen, som i BASIC.
VARIABLE RA   VARIABLE RS   VARIABLE RHI
: +V  ( a -- a' )  $0102 C@ $90 = IF 2 + THEN ;
: GETN  ( d -- n )  GETNUM IF SWAP DROP ELSE DROP THEN ;
: COMMA?  ( -- f )  SKIPBL PEEKC [CHAR] , = DUP IF 1 >IN +! THEN ;
: IN?  ( a -- f )  DUP END-PRG? IF DROP 0 ELSE LINE# RHI @ SWAP U< 0= THEN ;
: PREVLN  ( a -- p )
  BOFA @ BEGIN OVER OVER NEXTLINE = 0= WHILE NEXTLINE REPEAT SWAP DROP ;
: RENOK?  ( a -- f )  \ a = den första raden i intervallet
  DUP IN? 0= IF DROP 0 EXIT THEN
  DUP BOFA @ = 0= IF DUP PREVLN LINE# RA @ U< 0= IF DROP 0 EXIT THEN THEN
  RA @ SWAP BEGIN NEXTLINE DUP IN? WHILE
    SWAP DUP RS @ + DUP ROT U< IF DROP DROP 0 EXIT THEN  SWAP
  REPEAT
  DUP END-PRG? IF DROP DROP -1 ELSE LINE# U< THEN ;
: RENUM  ( a -- )
  RA @ SWAP BEGIN DUP IN? WHILE
    OVER OVER 1+ !  SWAP RS @ + SWAP  NEXTLINE
  REPEAT DROP DROP ;
: REN
  CR 10 GETN 10 COMMA? IF GETN THEN  RS ! RA !
  COMMA? IF RANGE ELSE 0 65535 THEN RHI !  FINDLN
  RA @ 0= RS @ 0= OR IF DROP BAD EXIT THEN
  DUP RENOK? 0= IF DROP BAD EXIT THEN
  $0CFF +V ROMCALL  RENUM  $0CCD +V ROMCALL  -1 CHANGED ! ;

\ AUTO a,s: radnumret skrivs ut, och resten av raden läses med ROM:ens
\ INLINE (ACCEPT) i BASIC:s radbuffert och läggs in med (INS). En tom
\ rad slutar.
: AUTO
  10 GETN 10 COMMA? IF GETN THEN  CR
  BEGIN OVER U.  $FE40 119 ACCEPT DROP CR  OVER (INS) WHILE
    -1 CHANGED !  SWAP OVER + SWAP
  REPEAT DROP DROP ;

\ ED n: raden n listas, och markören ställs på dess början. Raden läses
\ sedan med kretsens editor, med skärmen som mall: högerpilen tar
\ tecknet under markören, andra tecken skriver över, och CTRL-D och
\ CTRL-P tar bort och sätter in. RETURN ger ROM:en raden så långt den
\ har lästs, som om den hade skrivits. Som ROM:ens ED, där TAB tar
\ nästa tecken ur en mall i minnet. Markörens rad och kolumn står på
\ $FDF3 och $FDF4, och ! sätter kolumnen till 0.
: LN?  ( n -- a -1 | 0 )  \ raden med numret n
  DUP FINDLN DUP END-PRG? IF DROP DROP 0 EXIT THEN
  DUP LINE# ROT = IF -1 ELSE DROP 0 THEN ;
: ED
  CR GETNUM DROP LN? 0= IF BAD EXIT THEN
  LISTLINE $FDF3 C@ SWAP - $FDF3 !  $FE40 120 EDIT DROP TOROM ;

\ START n: programmet körs från rad n (eller nästa), eller från början,
\ utan att variablerna nollställs. RUNLIN ligger två byte längre fram i
\ 11273, som REN:s rutiner.
: START  CR GETNUM DROP FINDLN $0D6F +V (START) ;

\ HELP: kommandona, och de egna orden, som ligger i RAM.
: HELP
  CR ." SYS LIST PEEK FIND VAR DEL AUTO OLD ED" CR
  ." START CHANGE REN WORDS BYE EX FORTH" CR
  LAST @ BEGIN DUP $FB50 U< 0= WHILE
    DUP 3 + OVER 2 + C@ 127 AND TYPE SPACE @
  REPEAT DROP CR ;

\ Direkt i BASIC: Z=CALL(16384,2) kopplar in kretsens tangentbordsrutin
\ (fai.asm), och EX kopplar ur den. Står BASIC:en och väntar på en rad
\ läser KBD (ingång 3) raden i stället. Börjar den med ett av kommandona
\ körs det, och BYE går tillbaka till BASIC; annars får ROM:en raden.
\ KBD:s stackar ligger i kassettbuffertarnas översta 128 byte, under
\ $FD00 (KBD2 i fai.asm), så att BASIC behåller allt sitt minne; KBD
\ använder dem bara medan en rad läses och ett kommando körs, och då
\ läses eller skrivs ingen fil. PA ger 0.
: PA  ['] SLUT DP !  ['] LATEST LAST !  0 STATE !   \ som FORTH
  0 $7F7E !  $5F I! 0 ;                         \ STEPF i fai.asm
: EX  CR  0 I! ;
: KBD
  0 CHANGED !  $FE40 120 EDIT  0 $FE07 C!     \ BRKFLG, som TSTBRK
  DUP 80 U< IF
    DUP #TIB !  $FE40 TIB ROT CMOVE  0 >IN !  PARSE-NAME LOOKUP CMD?
  ELSE DROP 0 THEN
  ?DUP IF DUP ['] FORTH = IF CR THEN  EXECUTE BYE THEN  TOROM ;

: FA  0 CHANGED !  FORTH ;
ENTRY FA
ENTRY QUIT
ENTRY PA
ENTRY KBD
