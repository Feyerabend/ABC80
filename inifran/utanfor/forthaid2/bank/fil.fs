\ Forthaid II: SAVEW och LOADW, de egna orden till en fil och tillbaka
\ (steg 7 i PLAN.md, utanför boken). Läses efter fa.fs; orden ligger i
\ bank 4 (banker.txt) och FCALL i bank/fil.inc.
\
\ SAVEW namn skriver alla egna ord som går att nå från LAST: de som
\ ligger på $FB70 och uppåt och de som LOADW har laddat. Filen har ett
\ huvud på 8 byte ($46 $57 "FW", LATEST, längden n, den sista ordets
\ huvud räknat från början), orden (n byte, adresserna i dem räknade
\ från början) och en bitkarta (en bit per byte i orden: här börjar en
\ adress som ska flyttas). LATEST säger vilken Forthaid II filen hör
\ till; en annan version vägrar ladda den.
\
\ LOADW namn tar bort de egna orden (som FORTH) och läser filens ord
\ till RAM under STKTOP, som sänks. TOP0 är STKTOP före den första
\ LOADW (giltig när TOP0 + TOPK = $4641), och orden slutar där, så att
\ nästa LOADW tar samma minne. LE är slutet på de laddade orden.
\ BASIC:s stack är tom när ett kommando körs direkt i BASIC (KBD, också
\ i Forth efter kommandot FORTH), men inte i Forth efter Z=CALL(16384,n):
\ då ligger BASIC:s SP (SAVESP) under STKTOP och Forths stackar under
\ den, och LOADW tar bara minnet över STKTOP, där de förra laddade orden
\ låg.
\
\ Bara adresser i kolonord flyttas: ordens anrop och hoppen, inte talen
\ efter LIT och inte texten efter (S"). En VARIABLE eller CONSTANT
\ sparas som den är, så en konstant med en adress till ett eget ord
\ pekar fel efter LOADW.

VARIABLE TOP0   VARIABLE TOPK   VARIABLE LE

\ FB: FCB:n och FCALL:s fält, 160 byte över HEAP, med FCALL:s stack
\ under; sedan R1 (de laddade ordens början), NN (orden i filen, n
\ byte), och från SCR orden och bitkartan när de sparas.
: FB  ( -- a )  HEAP @ 160 + ;
: BP  FB 28 + ;   : BN  FB 30 + ;   : R1  FB 32 + ;   : NN  FB 34 + ;
: SCR  FB 36 + ;
: NBK  ( -- n )  NN @ 7 + 8 U/MOD SWAP DROP ;    \ bitkartans längd
: BM  ( -- a )  SCR NN @ + ;
\ BTOP: toppen på det lediga minnet över HEAP. Efter Z=CALL ligger
\ SAVESP under STKTOP, och Forths stackar under den (128 byte räcker);
\ i KBD ligger SAVESP i KBD:s stackar, och allt under STKTOP är ledigt.
: CALL?  ( -- f )  $FB6C @ STKTOP @ U< ;
: BTOP  ( -- a )  CALL? IF $FB6C @ 128 - ELSE STKTOP @ THEN ;

\ ROM:ens filrutiner med FCB:n i FB (band 1, in- och utmatningen).
: ROM  ( hl bc a rutin -- hl )
  FB 22 + !  FB 20 + !  FB 18 + !  FB 16 + !  FB FCALL  FB 16 + @ ;
: FCLOSE  0 0 0 $087F ROM DROP ;
: FGET  ( -- buf )  0 0 0 $0899 ROM ;
: FPUT  ( -- buf )  0 0 0 $089C ROM DROP  FB 26 + @ ;
: BLK  ( -- n )  FB 13 + C@ ;                  \ byte per block, 252 i DOS:en
\ OPENF: filen med namnet på raden öppnas (0) eller skapas (1). f = 0
\ om namnet saknas eller enheten inte har en buffert utanför
\ kassettbuffertarna ($FB00-$FCFF, där de egna orden ligger).
: OPENF  ( typ -- f )
  >R PARSE-NAME DUP 0= IF R> DROP DROP DROP 0 EXIT THEN
  R> $07E2 ROM DROP
  FB 9 + C@ DUP 0= SWAP $FB U< 0= OR IF FCLOSE 0 EXIT THEN
  FB 8 + @ BP !  -1 ;

\ Byte för byte genom blocken.
: GETB  ( -- c )
  BN @ 0= IF FGET BP !  BLK BN ! THEN
  BP @ C@  1 BP +!  -1 BN +! ;
: PUTB  ( c -- )
  BN @ 0= IF FPUT BP !  BLK BN ! THEN
  BP @ C!  1 BP +!  -1 BN +! ;
: GETW  ( -- n )  GETB GETB 256 * + ;
: PUTW  ( n -- )  256 U/MOD SWAP PUTB PUTB ;
: FGETS  ( a n -- )  ?DUP IF 0 DO GETB OVER I + C! LOOP THEN DROP ;
: FPUTS  ( a n -- )  ?DUP IF 0 DO DUP I + C@ PUTB LOOP THEN DROP ;

\ Var orden ligger: de laddade på R1 till LE, om LAST når dem, sedan
\ de på $FB70 (SLUT) till HERE. REL ger en adress räknad från filens
\ början, IN-ORD? om den pekar in bland orden.
: REGION  LE @ R1 !  LAST @ BEGIN DUP ['] LATEST = 0= WHILE
    DUP ['] SLUT U< IF DUP R1 ! THEN @ REPEAT DROP ;
: N1  ( -- n )  LE @ R1 @ - ;
: REL  ( a -- o )  DUP ['] SLUT U< IF R1 @ - ELSE ['] SLUT - N1 + THEN ;
: MELLAN  ( a lo hi -- f )  >R OVER SWAP U< 0= SWAP R> U< AND ;
: IN-ORD?  ( a -- f )  DUP R1 @ LE @ MELLAN  SWAP ['] SLUT HERE MELLAN OR ;

\ Bitkartan och kopian i SCR: adressen i cellen p görs relativ.
: BIT  ( n -- b )  1 SWAP ?DUP IF 0 DO 2* LOOP THEN ;
: SETBIT  ( o -- )  8 U/MOD BM + SWAP BIT OVER C@ OR SWAP C! ;
: FIXA  ( p -- )
  DUP @ DUP IN-ORD? IF REL OVER REL SCR + !  REL SETBIT
  ELSE DROP DROP THEN ;
\ Ett kolonords celler från a till slutet: LIT följs av ett tal, (S")
\ av en text med längden först.
: CELLER  ( slut a -- )
  BEGIN OVER OVER SWAP U< WHILE
    DUP @ DUP ['] LIT = IF DROP 4 + ELSE
    ['] (S") = IF 2 + DUP C@ + 1+ ELSE DUP FIXA 2 + THEN THEN
  REPEAT DROP DROP ;
\ Ett ord med huvudet h: länken, och cellerna om det är ett kolonord.
: ORDET  ( h slut -- )
  OVER FIXA
  SWAP DUP 2 + C@ 127 AND 3 + +
  DUP 1+ @ ['] DOCOL = IF 3 + CELLER ELSE DROP DROP THEN ;
\ Alla ord från LAST; ett ord slutar där nästa börjar, det sista på
\ HERE eller LE.
: ALLA
  HERE LAST @ BEGIN DUP ['] LATEST = 0= WHILE
    DUP ['] SLUT U< IF OVER ['] SLUT U< 0= IF SWAP DROP LE @ SWAP THEN THEN
    OVER OVER SWAP ORDET  SWAP DROP DUP @
  REPEAT DROP DROP ;

: SAVEW
  REGION  N1 HERE + ['] SLUT - DUP NN !  0=  LAST @ ['] LATEST = OR IF BAD EXIT THEN
  SCR NN @ + NBK + 64 +  BTOP U< 0= IF BAD EXIT THEN
  R1 @ SCR N1 CMOVE  ['] SLUT SCR N1 + HERE ['] SLUT - CMOVE
  0 BM C!  BM DUP 1+ NBK 1- CMOVE
  ALLA
  1 OPENF 0= IF BAD EXIT THEN  BLK BN !
  $5746 PUTW  ['] LATEST PUTW  NN @ PUTW  LAST @ REL PUTW
  SCR NN @ NBK + FPUTS
  BN @ BLK = 0= IF FPUT DROP THEN  FCLOSE ;

\ PLATS: n byte som slutar på TOP0. RYMS?: där får de ligga, över
\ bitkartan och, efter Z=CALL, över STKTOP.
: PLATS  ( n -- x )
  TOP0 @ DUP TOPK @ + $4641 = OVER STKTOP @ U< 0= AND 0= IF
    DROP STKTOP @ DUP TOP0 !  $4641 OVER - TOPK ! THEN
  SWAP - ;
: RYMS?  ( x -- f )
  >R  SCR NBK + 64 + R@ U<  R> STKTOP @ U< CALL? AND 0= AND ;
: RELOC  ( x -- x )
  NN @ 0 DO
    I 8 U/MOD SCR + C@ SWAP BIT AND IF DUP DUP I + +! THEN
  LOOP ;

: LOADW
  0 OPENF 0= IF BAD EXIT THEN  0 BN !
  GETW $5746 = GETW ['] LATEST = AND 0= IF FCLOSE BAD EXIT THEN
  GETW DUP NN !  GETW >R  DUP PLATS            ( n x )
  DUP RYMS? 0= IF FCLOSE R> DROP DROP DROP BAD EXIT THEN
  ['] SLUT DP !  ['] LATEST LAST !  0 STATE !   \ som FORTH
  DUP STKTOP !  OVER OVER + LE !
  DUP ROT FGETS  SCR NBK FGETS  FCLOSE
  RELOC  R> + LAST ! ;
