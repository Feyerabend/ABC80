10 REM KALKYL: laddar KALKYL.ABS (kalkyl.fs, se gor.sh) och startar det.
20 REM En liten rutin på $D7B0 (Forths TIB med gränsen på $D800) laddar
30 REM filen med DRUN och går till Forth: LADDAD flyttar BASIC till $D800.
40 PRINT CHR$(12);"KALKYL. Orden: SATT namn formel, TABORT namn, VISA, NY,"
45 PRINT "SPARA namn och LADDA namn (filen NAMN.KAL), FORMAT namn mall"
46 PRINT "(som i PRINT USING, utan namn för alla) och SKRIV till PR:."
47 PRINT "OMGANG och JAMFOR för omgångar, TRAD text och TABELL för en tabell."
50 PRINT "BYE går till BASIC, och Z=CALL(32768,1) tillbaka.":PRINT
60 POKE 55216,17,190,215,14,255,205,30,96,17,1,0,195,0,128
70 POKE 55230,75,65,76,75,89,76,32,32,65,66,83
80 ;CALL(55216)
