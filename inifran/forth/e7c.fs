\ Exempel 7 med CMOS-minne: tolken i kretsen på $4000 (som e7ip), men
\ nya ord läggs i CMOS-minnet på $5000-$57FF, det batteriförsedda RAM
\ som Super Smartaid hade. Orden finns kvar när datorn slås av och på.
\ Inte med i boken: en vidareutveckling som emulatorn kan köra
\ (abc80 -l e7c.bin@4000 -M cmos.bin, eller "Forth med CMOS" i
\ webbläsaren). Läses efter e7.fs och e7i.fs; ett ord som definieras
\ igen här ersätter det tidigare, också där det redan används.
\
\ CMOS-minnet:
\     $5000  märke: LATEST, det sista huvudet i kretsen (andra
\            versioner av kretsen har andra adresser i sina ord)
\     $5002  DP     var nästa ord börjar
\     $5004  LAST   det senaste huvudet
\     $5006  summa  av de tre, plus $A5A5
\     $5008  ordlistan, uppåt
\     $57B0  TIB, radbufferten (80 bytes), så att tolken inte behöver
\            något RAM från BASIC: Z=CALL(16384,3) räcker.
\ Efter varje rad utan fel, och utan halvfärdig definition, skrivs
\ huvudet; en rad som inte får plats tas bort. EMPTY tömmer ordlistan.

: CMOS  20480 ;
: TIB  22448 ;
: SUMMA  CMOS @  CMOS 2 + @ +  CMOS 4 + @ +  -23131 + ;
: SPARA  ['] LATEST CMOS !  DP @ CMOS 2 + !  LAST @ CMOS 4 + !
  SUMMA CMOS 6 + ! ;
: LADDA  CMOS 2 + @ DP !  CMOS 4 + @ LAST ! ;
: GILTIG?  CMOS @ ['] LATEST =  SUMMA CMOS 6 + @ =  AND ;
: EMPTY  ['] SLUT DP !  ['] LATEST LAST !  SPARA ;
: SPARA-RADEN  HERE TIB U< IF SPARA ." OK" ELSE ." CMOS FULLT" LADDA THEN ;
: QUIT
  BEGIN
    TIB 79 ACCEPT #TIB !  0 >IN !  0 ERR !  SPACE INTERPRET
    ?STACK IF ." STACK ?" ERROR THEN
    ERR @ IF RESET ELSE STATE @ 0= IF SPARA-RADEN THEN THEN CR
  AGAIN ;
: FORTH
  GILTIG? IF LADDA ELSE ." NY ORDLISTA" CR EMPTY THEN  0 STATE !  QUIT ;
