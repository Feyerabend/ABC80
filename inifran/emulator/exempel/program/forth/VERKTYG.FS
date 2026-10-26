\ VERKTYG.FTH, egna ord till Forthaid II på exempeldisketten.
\
\   n RAM     raden n som den ligger i minnet, byte för byte i hex,
\             efter raden själv (Superbasics RAM)
\   n CROSS   de rader som hoppar till rad n: GOTO, GOSUB, THEN, ELSE
\             och listorna i ON ... GOTO (ABC80L:s CROSS, Schoolaids XREF)
\   FIX       en referens till en rad som inte finns får numret på nästa
\             rad som finns; de ändrade raderna listas (Superbasics FIX)
\   PAC       tar bort raderna som börjar med REM och gör sedan FIX
\             (Superbasics PAC)
\
\ Orden går genom raderna i den inre koden (band 1, kapitlet
\ Arkitekturen) ett element i taget med ROM:ens SKPELM ($0EFB i både
\ 9913 och 11273), så att ett tal, ett namn eller en adress aldrig läses
\ som en referens. STUB är ett litet stycke Z80-kod som ROMCALL anropar
\ med HL = STUB: det hämtar elementets adress ur ordet efter koden
\ (STUB 18 +), anropar SKPELM och lägger tillbaka adressen efter
\ elementet. Koden använder bara sin egen adress, så att den fungerar var
\ LOADW än lägger den. En referens är $BF följt av radnumret och målradens
\ adress, och en lista $BE följt av antalet k och k gånger radnummer och
\ adress. Adresserna fylls i på nytt vid varje RUN, så FIX och PAC
\ behöver bara ändra radnumren; PAC flyttar resten av programmet ned som
\ Forthaids DEL och gör CLEAR ($00CA).
\
\ gor.sh skriver in raderna i Forthaid II:s tolk. Rader som börjar med \
\ hoppas över, och en tom rad avslutar en omgång: orden sparas med SAVEW
\ och laddas med LOADW, så att de flyttas under STKTOP, eftersom alla
\ orden inte ryms i de 272 byten på $FB70 på en gång. Ingen rad får vara
\ längre än 79 tecken.

: HSIFF DUP 10 < 0= IF 7 + THEN 48 + EMIT ;
: HBYTE 16 U/MOD HSIFF HSIFF ;
: RAM CR FINDLN DUP END-PRG? IF DROP EXIT THEN DUP LISTLINE DROP
DUP C@ 0 DO I 10 U/MOD DROP 0= IF CR THEN DUP I + C@ HBYTE SPACE LOOP
DROP CR ;

\ STUB: LD DE,18  ADD HL,DE  PUSH HL  LD A,(HL)  INC HL  LD H,(HL)
\ LD L,A  CALL SKPELM  EX DE,HL  POP HL  LD (HL),E  INC HL  LD (HL),D
\ RET, och sedan ordet med adressen.
VARIABLE STUB -2 ALLOT $11 C, 18 C, 0 C, $19 C, $E5 C, $7E C, $23 C,
$66 C, $6F C, $CD C, $FB C, $0E C, $EB C, $E1 C, $73 C, $23 C, $72 C,
$C9 C, 0 ,
: STEG STUB 18 + ! STUB ROMCALL STUB 18 + @ ;

VARIABLE MAL
: LISTA? 2 + DUP 1- C@ 0 SWAP 0 DO OVER I 4 * + @ MAL @ = OR LOOP
OVER 1- C@ 4 * ROT + SWAP ;
: ELEM DUP C@ $BF = IF DUP 1+ @ MAL @ = SWAP 5 + SWAP EXIT THEN
DUP C@ $BE = IF LISTA? EXIT THEN STEG 0 ;

: REF? 3 + 0 BEGIN OVER C@ 13 = 0= WHILE >R ELEM R> OR REPEAT SWAP DROP ;
: CROSS MAL ! BOFA @ BEGIN DUP END-PRG? 0= WHILE DUP REF? IF CR DUP
LISTLINE DROP THEN NEXTLINE REPEAT DROP CR ;

VARIABLE NY
: REF DUP @ FINDLN DUP END-PRG? IF DROP DROP EXIT THEN LINE# OVER @
OVER = IF DROP DROP EXIT THEN SWAP ! -1 NY ! ;
: REFS DUP C@ $BF = IF 1+ REF EXIT THEN DUP C@ $BE = IF DUP 1+ C@ 0
DO DUP I 4 * + 2 + REF LOOP THEN DROP ;

: FIXL 0 NY ! DUP 3 + BEGIN DUP C@ 13 = 0= WHILE DUP REFS STEG REPEAT
DROP NY @ IF CR DUP LISTLINE DROP THEN DROP ;
: FIX BOFA @ BEGIN DUP END-PRG? 0= WHILE DUP FIXL NEXTLINE REPEAT DROP
CR ;

: REM? 3 + @ $8486 = ;
: PAC BOFA @ BEGIN DUP END-PRG? 0= WHILE DUP REM? IF DUP C@ >R DUP DUP
NEXTLINE DUP EOFA @ SWAP - 1+ ROT SWAP CMOVE R> NEGATE EOFA +! ELSE
NEXTLINE THEN REPEAT DROP $00CA @ ROMCALL FIX ;
