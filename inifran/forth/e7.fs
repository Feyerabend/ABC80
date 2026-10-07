\ Exempel 7: Forth-källan. Ingång 0 = MAIN, 1 = TIO, 2 = FIND.

: CR  13 EMIT 10 EMIT ;
: .  DUP 0< IF [CHAR] - EMIT NEGATE THEN U. ;

: MAIN  1 2 + . CR
  5 0 DO I . LOOP CR
  -42 . ." HEJ" CR ;
: TIO  7 3 + ;

\ FIND, som i Smartaid: skriv numren på de rader där en text finns.
\ Texten läses med ACCEPT (ROM:ens radinmatning) och läggs i det
\ lediga minnet efter variablerna (HEAP). Raderna går från BOFA till
\ längdbyten $01; en rad är längden, radnumret och koden, och texter
\ står i koden som de är.
$FE1C CONSTANT BOFA
$FE20 CONSTANT HEAP
VARIABLE PAT   VARIABLE PLEN

: MATCH ( a -- f )  PAT @ PLEN @ SAME ;   \ står texten på adress a?
: LINE? ( l -- f )    \ finns texten någonstans i raden l?
  DUP C@ 2 - PLEN @ -           \ antal platser där den kan börja
  DUP 1 < IF DROP DROP 0 EXIT THEN
  >R 3 + 0 SWAP DUP R> + SWAP   ( 0 slut början )
  DO I MATCH OR LOOP ;
: FIND ( -- n )       \ n = antal rader
  HEAP @ PAT !  PAT @ 40 ACCEPT PLEN ! CR
  0  PLEN @ 0= IF EXIT THEN
  BOFA @ BEGIN DUP C@ 1 = 0= WHILE
    DUP LINE? IF DUP 1+ @ U. SWAP 1+ SWAP THEN
    DUP C@ +
  REPEAT DROP CR ;

ENTRY MAIN
ENTRY TIO
ENTRY FIND
