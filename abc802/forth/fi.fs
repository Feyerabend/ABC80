\ Den yttre tolken, från bokens exempel 7. Ingång 0 = FORTH (ny start,
\ bara bildens ord), 1 = QUIT (fortsätt med de ord som redan
\ definierats), 2 = (REM) (tolka BASIC-programmets REM FORTH-rader),
\ CALL(32768,VAROOT(A$)) = (STR) (tolka strängen). BYE går tillbaka
\ till BASIC.
\
\ Ett huvud är en länk till föregående huvud, en längdbyte (bit 7 =
\ IMMEDIATE) och namnet; koden följer direkt efter. Nya ord läggs från
\ HERE och uppåt mot TIB, radbufferten, de 80 bytena under UPP ($B800, BASIC:s BOTRAM).

VARIABLE STATE   VARIABLE DP   VARIABLE LAST   VARIABLE NEWEST
VARIABLE >IN     VARIABLE #TIB   VARIABLE ERR
VARIABLE SRC     \ raden som tolkas: TIB, en sträng eller en REM-rad

: SPACE  32 EMIT ;
: ?DUP  DUP IF DUP THEN ;
: HERE  DP @ ;
: ,  HERE !  HERE 2 + DP ! ;
: C,  HERE C!  HERE 1+ DP ! ;
: ALLOT  DP @ + DP ! ;
$B800 CONSTANT UPP
: TIB  UPP 80 - ;

\ Tolka raden: nästa ord, avgränsat av blanktecken.
: END?  ( a -- f )  SRC @ #TIB @ + U< 0= ;
: PARSE-NAME  ( -- a n )
  SRC @ >IN @ +
  BEGIN DUP END? IF 0 ELSE DUP C@ 32 = THEN WHILE 1+ REPEAT
  DUP
  BEGIN DUP END? IF 0 ELSE DUP C@ 32 = 0= THEN WHILE 1+ REPEAT
  DUP SRC @ - >IN !  OVER - ;
: PARSE  ( c -- a n )  \ text fram till tecknet c
  >R SRC @ >IN @ + DUP
  BEGIN DUP END? IF 0 ELSE DUP C@ R@ = 0= THEN WHILE 1+ REPEAT
  R> DROP  OVER -  DUP >IN @ + 1+ >IN ! ;

\ Ordlistan; sökningen är i maskinkod, (LOOKUP).
: LOOKUP  ( a n -- h | 0 )  LAST @ (LOOKUP) ;
: >XT  ( h -- xt )  2 + DUP C@ 127 AND + 1+ ;
: IMM?  ( h -- f )  2 + C@ 128 AND ;

\ Tal: decimalt, ev. med minustecken.
: 10*  DUP 2* 2* + 2* ;
: NUMBER?  ( a n -- x -1 | 0 )
  OVER C@ [CHAR] - = DUP >R IF 1- SWAP 1+ SWAP THEN
  DUP 0= IF DROP DROP R> DROP 0 EXIT THEN
  0 SWAP 0 DO
    OVER I + C@ [CHAR] 0 -  DUP 10 U< 0= IF
      DROP DROP DROP UNLOOP R> DROP 0 EXIT THEN
    SWAP 10* +
  LOOP
  SWAP DROP  R> IF NEGATE THEN  -1 ;

\ Ett fel avbryter raden; en halvfärdig definition tas bort, och QUIT
\ tömmer stackarna.
: ERROR  #TIB @ >IN !  STATE @ IF NEWEST @ DP ! THEN  0 STATE !  -1 ERR ! ;
: NOTFOUND  ( a n -- )  TYPE ."  ?" ERROR ;
: INTERPRET
  BEGIN PARSE-NAME DUP WHILE
    OVER OVER LOOKUP ?DUP IF
      ROT ROT DROP DROP
      DUP IMM? STATE @ 0= OR IF >XT EXECUTE ELSE >XT , THEN
    ELSE
      OVER OVER NUMBER? IF
        ROT ROT DROP DROP  STATE @ IF ['] LIT , , THEN
      ELSE NOTFOUND THEN
    THEN
  REPEAT DROP DROP ;
: QUIT
  BEGIN
    TIB 79 ACCEPT #TIB !  TIB SRC !  0 >IN !  0 ERR !  SPACE INTERPRET
    ?STACK IF ." STACK ?" ERROR THEN
    ERR @ IF RESET ELSE STATE @ 0= IF ."  OK" THEN THEN CR
  AGAIN ;
\ Efter ett stackfel i DOCOL eller BRANCH; stackarna är redan tomma.
: ABORT  ." STACK ?" ERROR CR QUIT ;
: NYTT  ['] SLUT DP !  ['] LATEST LAST !  0 STATE ! ;
: FORTH  NYTT QUIT ;

\ Kompilatorn.
: S,  ( a n -- )  BEGIN DUP WHILE OVER C@ C, 1- SWAP 1+ SWAP REPEAT DROP DROP ;
: HEADER  HERE NEWEST !  LAST @ ,  PARSE-NAME DUP C, S, ;
: REVEAL  NEWEST @ LAST ! ;
: IMMEDIATE  LAST @ 2 + DUP C@ 128 OR SWAP C! ;
: :  HEADER  205 C, ['] DOCOL ,  -1 STATE ! ;
: ;  ['] EXIT ,  REVEAL  0 STATE ! ; IMMEDIATE
: CREATE  HEADER  205 C, ['] DOVAR ,  REVEAL ;
: VARIABLE  HEADER  205 C, ['] DOVAR ,  0 ,  REVEAL ;
: CONSTANT  HEADER  205 C, ['] DOCON ,  ,  REVEAL ;
: '  PARSE-NAME LOOKUP DUP IF >XT THEN ;
: LITERAL  ['] LIT , , ; IMMEDIATE
\ Ordet som definieras, som anropar sig självt (namnet syns först efter ;).
: RECURSE  NEWEST @ >XT , ; IMMEDIATE
: IF  ['] 0BRANCH , HERE 0 , ; IMMEDIATE
: THEN  HERE SWAP ! ; IMMEDIATE
: ELSE  ['] BRANCH , HERE 0 , SWAP HERE SWAP ! ; IMMEDIATE
: BEGIN  HERE ; IMMEDIATE
: UNTIL  ['] 0BRANCH , , ; IMMEDIATE
: AGAIN  ['] BRANCH , , ; IMMEDIATE
: WHILE  ['] 0BRANCH , HERE 0 , SWAP ; IMMEDIATE
: REPEAT  ['] BRANCH , , HERE SWAP ! ; IMMEDIATE
: DO  ['] (DO) , HERE ; IMMEDIATE
: LOOP  ['] (LOOP) , , ; IMMEDIATE
: +LOOP  ['] (+LOOP) , , ; IMMEDIATE
: ."  >IN @ 1+ >IN !  ['] (S") ,  [CHAR] " PARSE DUP C, S,  ['] TYPE , ;
  IMMEDIATE
\ Kommentar: ( till ). Inget \, som är Ö på ABC 802.
: (  [CHAR] ) PARSE DROP DROP ; IMMEDIATE

\ FORGET namn: ordet och allt efter det tas bort (bara egna ord).
: FORGET  PARSE-NAME LOOKUP DUP ['] SLUT U< IF DROP ."  ?" ERROR
  ELSE DUP DP ! @ LAST ! THEN ;

: WORDS
  LAST @ BEGIN DUP WHILE DUP 3 + OVER 2 + C@ 127 AND TYPE SPACE @ REPEAT
  DROP CR ;

\ Från BASIC. TOLKA tolkar n tecken på a, och ett fel avbryter.
: TOLKA  ( a n -- )  #TIB ! SRC !  0 >IN !  0 ERR !  INTERPRET
  ?STACK IF ."  STACK ?" ERROR THEN ;

\ Z=CALL(32768,2): ordboken töms, och varje rad som börjar med
\ REM FORTH eller ! FORTH tolkas, i ordning. En definition får gå över
\ flera rader. En rad är: längd (till nästa rad), radnummer, satserna
\ och 135; REM är 143 och ! 144, följda av textens längd + 1 och texten.
\ En text kortare än FORTH har 135 inom de fem första bytena.
\ Svaret är 0, radnumret vid ett fel, eller -1 om ; saknas sist.
: REM?  ( a -- f )  3 + C@ DUP 143 = SWAP 144 = OR ;
: FORTH-RAD?  ( a -- f )  DUP REM? IF 5 + S" FORTH" SAME ELSE DROP 0 THEN ;
\ Radens adress står i RAD, inte på stacken: en definition över flera
\ rader har IF och DO:s adresser på stacken mellan raderna.
VARIABLE RAD
: (REM)  ( -- n )  NYTT
  65286 @ 21 + RAD !
  BEGIN RAD @ C@ IF RAD @ 65288 @ U< ELSE 0 THEN WHILE
    RAD @ FORTH-RAD? IF
      RAD @ 10 +  RAD @ 4 + C@ 6 -  TOLKA  ERR @ IF RAD @ 1+ @ EXIT THEN
    THEN
    RAD @ DUP C@ + RAD !
  REPEAT
  STATE @ IF NEWEST @ DP !  0 STATE !  ."  ; ?" CR -1 ELSE 0 THEN ;

\ Z=CALL(32768,VAROOT(A$)): strängen tolkas med de ord som finns.
\ VAROOT ger strängens post: +2 textens adress, +4 längden. Svaret är
\ talet överst på stacken, 0 om stacken är tom och -32768 vid ett fel.
: (STR)  ( post -- x )  DP @ 0= IF NYTT THEN
  DUP 2 + @ SWAP 4 + @ TOLKA
  ERR @ IF -32768 ELSE DEPTH 0= IF 0 THEN THEN ;

ENTRY FORTH
ENTRY QUIT
ENTRY (REM)
ENTRY (STR)
