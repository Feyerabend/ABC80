100 REM INTERFACE AGE’s benchmark program to
110 REM ’discover’ the first 1000 Prime numbers
115 SINGLE
120 PRINT "Starting:"
140 FOR N = 1 TO 1000
150   FOR K = 2 TO 500
160     LET M = N/K
170     LET L = INT(M)
180     IF L = 0 THEN 230
190     IF L = 1 THEN 220
200     IF M > L THEN 220
210     IF M = L THEN 240
220   NEXT K
230   PRINT ;N;" ";
240 NEXT N
260 PRINT CHR$(7)
270 PRINT "Finished."
300 END