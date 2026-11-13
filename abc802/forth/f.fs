\ Forth för ABC 802: orden före tolken (fi.fs). Kärnan fk.inc har
\ orden i maskinkod.

: CR  13 EMIT 10 EMIT ;
: .  DUP 0< IF [CHAR] - EMIT NEGATE THEN U. ;
: ABS  DUP 0< IF NEGATE THEN ;
: >  SWAP < ;
: 0>  0 SWAP < ;
: SPACES  ( n -- )  BEGIN DUP 0> WHILE 32 EMIT 1- REPEAT DROP ;

\ /MOD ( n1 n2 -- rest kvot ) finns i kärnan: kvoten avrundas mot noll
\ och resten får täljarens tecken (som BASIC:s heltal).
: /  /MOD SWAP DROP ;
: MOD  /MOD DROP ;

\ Skärmen: tecknen 12 (töm) och 7 (pip) till ROM:ens skrivrutin, och
\ markören i CON:s variabler CURCOL och CURROW.
: PAGE  12 EMIT ;
: BEEP  7 EMIT ;
: AT  ( kolumn rad -- )  $FF53 C!  $FF52 C! ;

\ Grafiken: GR tömmer skärmen och sätter grafikkoden 151 (vit mosaik)
\ först på var och en av de 24 raderna; sedan PLOT, UNPLOT och POINT?
\ ( x y ), x 0-77 (0-157 med 80 tecken) och y 0-71 uppifrån.
$7800 CONSTANT SKARM
: GR  PAGE  24 0 DO 151 I 80 * SKARM + C! LOOP ;

: +!  ( n a -- )  DUP @ ROT + SWAP ! ;
: MAX  OVER OVER < IF SWAP THEN DROP ;
: MIN  OVER OVER < 0= IF SWAP THEN DROP ;
: SGN  ( n -- -1|0|1 )  DUP 0< IF DROP -1 ELSE 0= 0= NEGATE THEN ;

\ LINE ( x1 y1 x2 y2 -- ) en rät linje med PLOT, Bresenhams metod:
\ FEL håller reda på hur långt punkten ligger från den rätta linjen.
VARIABLE PX   VARIABLE PY   VARIABLE DX   VARIABLE DY
VARIABLE SX   VARIABLE SY   VARIABLE FEL
: LINE
  >R >R  PY ! PX !  R> R>
  PY @ - DUP SGN SY !  ABS NEGATE DY !
  PX @ - DUP SGN SX !  ABS DX !
  DX @ DY @ + FEL !
  DX @ DY @ NEGATE MAX 1+ 0 DO
    PX @ PY @ PLOT
    FEL @ 2*
    DUP DY @ < 0= IF DY @ FEL +!  SX @ PX +! THEN
    DX @ SWAP < 0= IF DX @ FEL +!  SY @ PY +! THEN
  LOOP ;

\ Slumptal: RND ( n -- 0..n-1 ) med en linjär kongruensgenerator; de
\ höga bitarna används, eftersom de låga har kort period. RANDOMIZE
\ ( n -- ) sätter fröet, med 0 från klockans avbrottsräknare.
VARIABLE SEED
: RANDOMIZE  DUP 0= IF DROP 65525 C@ 65524 C@ 256 * + THEN SEED ! ;
: RND  -1 SWAP U/MOD SWAP DROP 1+
  SEED @ 31421 * 6927 + DUP SEED !  SWAP U/MOD SWAP DROP ;

\ INKEY ( -- c | 0 ) tangenten utan att vänta: KEYINT lägger koden i
\ LASTKY ($FFE3) och sätter bit 7 i KEYFLG ($FFE2), som KEY nollställer.
: INKEY  $FFE2 C@ 128 AND IF
  $FFE2 C@ 127 AND $FFE2 C!  $FFE3 C@ ELSE 0 THEN ;

\ Flyttalen (fk.inc): F. skriver talet som PRINT i BASIC II med DOUBLE.
\ (F.) ger talet i decimal form, med upp till 22 siffror; de avrundas
\ till 16 (FUPP lägger till 1 på siffra i, med minnessiffran åt
\ vänster), och nollorna sist stryks. Med högst 16 siffror före eller
\ efter punkten skrivs talet utan exponent, annars som 1.25E-15.
VARIABLE FR   VARIABLE FN   VARIABLE FE
: FDIG  ( i -- a )  FR @ 2 + + ;
: FUPP  ( i -- )
  BEGIN
    DUP 0< IF DROP [CHAR] 1 0 FDIG C!  1 FE +!  EXIT THEN
    DUP FDIG C@ [CHAR] 9 = WHILE
    [CHAR] 0 OVER FDIG C!  1-
  REPEAT
  FDIG DUP C@ 1+ SWAP C! ;
: FAVR  ( n -- )  \ högst n siffror, avrundade
  DUP FN @ < IF
    DUP FDIG C@ [CHAR] 5 < 0= IF DUP 1- FUPP THEN  DUP FN !
  THEN DROP
  BEGIN FN @ 1 > IF FN @ 1- FDIG C@ [CHAR] 0 = ELSE 0 THEN WHILE
    -1 FN +! REPEAT ;
: FFIX
  FE @ 0> IF FE @ 0 DO I FN @ < IF I FDIG C@ ELSE [CHAR] 0 THEN EMIT LOOP THEN
  FN @ FE @ > IF
    [CHAR] . EMIT
    FE @ 0< IF FE @ NEGATE 0 DO [CHAR] 0 EMIT LOOP THEN
    FN @ FE @ 0 MAX DO I FDIG C@ EMIT LOOP
  THEN ;
: FEXP
  0 FDIG C@ EMIT  [CHAR] . EMIT
  FN @ 1 > IF FN @ 1 DO I FDIG C@ EMIT LOOP THEN
  [CHAR] E EMIT  FE @ 1-  DUP 0< IF [CHAR] - ELSE [CHAR] + THEN EMIT
  ABS 10 /MOD [CHAR] 0 + EMIT [CHAR] 0 + EMIT ;
: (F)  ( F: r -- )  \ talet utan blanktecken efter, tecknet först
  (F.) FR !
  FR @ C@ DUP 127 AND FN !  128 AND IF [CHAR] - ELSE 32 THEN EMIT
  FR @ 1+ C@ DUP 127 > IF 256 - THEN FE !
  16 FAVR
  0 FDIG C@ [CHAR] 0 = IF [CHAR] 0 EMIT EXIT THEN
  FE @ 16 >  FN @ FE @ - 16 > OR IF FEXP ELSE FFIX THEN ;
: F.  (F) 32 EMIT ;
