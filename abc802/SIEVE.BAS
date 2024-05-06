120 S%=4095%
130 DIM F%(S%+1%)
140 PRINT "Only 1 iteration"
150 C%=0%
160 FOR I%=0% TO S%
170   F%(I%)=1%
180 NEXT I%
190 FOR I%=0% TO S%
200   IF F%(I%)=0% THEN 280
210   P%=I%+I%+3%
220   K%=I%+P%
230   IF K%>S% THEN 270
240   F%(K%)=0%
250   K%=K%+P%
260   GOTO 230
270   C%=C%+1%
280 NEXT I%
290 PRINT C%,” Primes”
300 END