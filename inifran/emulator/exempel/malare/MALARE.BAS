10 REM ***COPYRIGHT 1982.04.02    Mikael Bonnier***
20 ; CHR$(12)
30 ; "   M ] L A R E"
40 ; "UPPERCASE skall lysa!!!"
50 ; : ; "Tidsf|rdr|jning";
60 INPUT X%
70 OUT 6%,137% : ; CHR$(12%) : FOR I%=0% TO 23% : ; CUR(I%,0%)CHR$(151%); : NEXT I%
80 K%=38% : R%=34%
90 LET A%=INP(56%)
100 IF R%<0% R%=0%
110 IF R%>71% R%=71%
120 IF K%<2% K%=2%
130 IF K%>79% K%=79
140 SETDOT R%,K%
150 IF A%=210% OR A%=196% OR A%=199% OR A%=214% OR A%=197% OR A%=212% OR A%=194% OR A%=195% CLRDOT R%,K%
160 FOR T%=0% TO X% : NEXT T% : OUT 6%,0%
170 IF A%=211% OR A%=243% 70 : REM S
180 IF A%=210% OR A%=242% R%=R%-1% : OUT 6%,137% : REM R
190 IF A%=196% OR A%=228% K%=K%-1% : OUT 6%,7% : REM D
200 IF A%=199% OR A%=231% K%=K%+1% : OUT 6%,77% : REM G
210 IF A%=214% OR A%=246% R%=R%+1% : OUT 6%,83% : REM V
220 IF A%=197% OR A%=229% R%=R%-1% : K%=K%-1% : OUT 6%,9% : REM E
230 IF A%=212% OR A%=244% R%=R%-1% : K%=K%+1% : OUT 6%,139% : REM T
240 IF A%=194% OR A%=226% R%=R%+1% : K%=K%+1% : OUT 6%,13% : REM B
250 IF A%=195% OR A%=227% R%=R%+1% : K%=K%-1% : OUT 6%,117% : REM C
260 GOTO 90
