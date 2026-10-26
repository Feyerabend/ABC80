\ Den fasta delen, sist: ingångarna 5 (PROV) och 6 (TID).
: PROV  BANK. CR  0 A1 . CR  20 ['] C1 EXECUTE . BANK. CR ;
\ Klockan på $FDF0 räknar ned 50 gånger i sekunden.
: MAT  ( xt -- n )  $FDF0 @ SWAP EXECUTE $FDF0 @ - ;
: TL  10000 0 DO LOOP ;
: T0  10000 0 DO TOM LOOP ;
: T3  10000 0 DO TOM1 LOOP ;
: TID  ['] TL MAT . ['] T0 MAT . ['] T1 MAT . ['] T2 MAT . ['] T3 MAT .
  BANK. CR ;
ENTRY PROV
ENTRY TID
