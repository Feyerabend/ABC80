\ Bank 2: anropar den fasta delen och bank 1, och har en slinga efter
\ anropen. IMM2 körs när ett ord kompileras.
: B2  ( n -- n+5 )  ." B2" BANK. VIA C1 ." B2" BANK.  3 0 DO I + LOOP ;
: TOM2 ;
: IMM2  ." IMM2" BANK. ; IMMEDIATE
