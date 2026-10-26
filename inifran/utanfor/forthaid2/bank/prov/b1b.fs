\ Bank 1 igen: anropar bank 2 i en slinga, som räknar vidare efteråt.
: A1  ( n -- n+10 )  ." A1" BANK. CR
  2 0 DO B2 ." A1" BANK. I . CR LOOP ;
\ Tiden: 10 000 anrop av ett tomt ord, i banken och i bank 2.
: T1  10000 0 DO TOM1 LOOP ;
: T2  10000 0 DO TOM2 LOOP ;
