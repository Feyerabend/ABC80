\ Exempel 7: den yttre tolken. Ingång 3 = FORTH (ny start, bara
\ bildens ord), 4 = QUIT (fortsätt med de ord som redan definierats).
\ BYE går tillbaka till BASIC.
\
\ Ett huvud är en länk till föregående huvud, en längdbyte (bit 7 =
\ IMMEDIATE) och namnet; koden följer direkt efter. Nya ord läggs från
\ HERE och uppåt mot TIB, radbufferten, som står närmast under BOFA.

VARIABLE STATE   VARIABLE DP   VARIABLE LAST   VARIABLE NEWEST
VARIABLE >IN     VARIABLE #TIB   VARIABLE ERR

: SPACE  32 EMIT ;
: ?DUP  DUP IF DUP THEN ;
: HERE  DP @ ;
: ,  HERE !  HERE 2 + DP ! ;
: C,  HERE C!  HERE 1+ DP ! ;
: ALLOT  DP @ + DP ! ;
: TIB  BOFA @ 80 - ;

\ Tolka raden: nästa ord, avgränsat av blanktecken.
: END?  ( a -- f )  TIB #TIB @ + U< 0= ;
: PARSE-NAME  ( -- a n )
  TIB >IN @ +
  BEGIN DUP END? IF 0 ELSE DUP C@ 32 = THEN WHILE 1+ REPEAT
  DUP
  BEGIN DUP END? IF 0 ELSE DUP C@ 32 = 0= THEN WHILE 1+ REPEAT
  DUP TIB - >IN !  OVER - ;
: PARSE  ( c -- a n )  \ text fram till tecknet c
  >R TIB >IN @ + DUP
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
    TIB 79 ACCEPT #TIB !  0 >IN !  0 ERR !  SPACE INTERPRET
    ?STACK IF ." STACK ?" ERROR THEN
    ERR @ IF RESET ELSE STATE @ 0= IF ." OK" THEN THEN CR
  AGAIN ;
\ Efter ett stackfel i DOCOL eller BRANCH; stackarna är redan tomma.
: ABORT  ." STACK ?" ERROR CR QUIT ;
: FORTH  ['] SLUT DP !  ['] LATEST LAST !  0 STATE !  QUIT ;

\ Kompilatorn.
: S,  ( a n -- )  BEGIN DUP WHILE OVER C@ C, 1- SWAP 1+ SWAP REPEAT DROP DROP ;
: HEADER  HERE NEWEST !  LAST @ ,  PARSE-NAME DUP C, S, ;
: REVEAL  NEWEST @ LAST ! ;
: IMMEDIATE  LAST @ 2 + DUP C@ 128 OR SWAP C! ;
: :  HEADER  205 C, ['] DOCOL ,  -1 STATE ! ;
: ;  ['] EXIT ,  REVEAL  0 STATE ! ; IMMEDIATE
: VARIABLE  HEADER  205 C, ['] DOVAR ,  0 ,  REVEAL ;
: CONSTANT  HEADER  205 C, ['] DOCON ,  ,  REVEAL ;
: '  PARSE-NAME LOOKUP DUP IF >XT THEN ;
: LITERAL  ['] LIT , , ; IMMEDIATE
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
: ."  >IN @ 1+ >IN !  ['] (S") ,  [CHAR] " PARSE DUP C, S,  ['] TYPE , ;
  IMMEDIATE
: WORDS
  LAST @ BEGIN DUP WHILE DUP 3 + OVER 2 + C@ 127 AND TYPE SPACE @ REPEAT
  DROP CR ;

ENTRY FORTH
ENTRY QUIT
