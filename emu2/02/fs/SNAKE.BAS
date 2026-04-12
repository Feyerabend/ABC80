1 REM UNTESTED .. JUST UPLOADED .. ERASE THIS AFTER TEST AND ADJUSTMENTS
10 REM -- CONFIG --
20 A%=36% : REM width
30 B%=20% : REM height
40 REM -- INIT STATE --
50 DIM C%(1000%),D%(1000%) : REM snake x/y arrays
60 E%=5% : REM start length of snake
70 FOR L%=1% TO E%
80 C%(L%)=INT(A%/2%)-L%
90 D%(L%)=INT(B%/2%)
100 NEXT L%
110 F%=1% : G%=0% : REM moving right, direction (dx, dy)
120 GOSUB 1000 : REM place food
130 GOSUB 2000 : REM draw borders
140 REM -- MAIN --
150 GOSUB 3000 : REM get input
160 GOSUB 4000 : REM update
170 GOSUB 5000 : REM draw
180 GOTO 150
1000 REM -- PLACE FOOD --
1010 J%=INT(RND*(A%-2%))+2% : REM food pos x
1020 K%=INT(RND*(B%-2%))+2% : REM food pos y
1030 FOR L%=1% TO E% : REM recalc if pos collide w/ snake
1040 IF C%(L%)=J% AND D%(L%)=K% THEN GOTO 1010
1050 NEXT L%
1060 RETURN
2000 REM -- DRAW BORDERS --
2010 FOR L%=1% TO A%
2020 ; CUR(L%,1%)"#";
2030 ; CUR(L%,B%)"#";
2040 NEXT L%
2050 FOR L%=1% TO B%
2060 ; CUR(1%,L%)"#";
2070 ; CUR(A%,L%)"#";
2080 NEXT L%
2090 RETURN
3000 REM -- INPUT --
3010 M%=INP(56%) : REM input key (this right?)
3020 IF M%=119% AND G%<>1% THEN F%=0% : G%=-1%
3030 IF M%=115% AND G%<>-1% THEN F%=0% : G%=1%
3040 IF M%=97% AND F%<>1% THEN F%=-1% : G%=0%
3050 IF M%=100% AND F%<>-1% THEN F%=1% : G%=0%
3060 RETURN
4000 REM -- UPDATE --
4010 H%=C%(1%)+F%
4020 I%=D%(1%)+G%
4030 IF H%=1% OR H%=A% OR I%=1% OR I%=B% THEN GOTO 9000
4040 FOR L%=1% TO E%
4050 IF C%(L%)=H% AND D%(L%)=I% THEN GOTO 9000
4060 NEXT L%
4070 FOR L%=E% TO 2% STEP -1%
4080 C%(L%)=C%(L%-1%)
4090 D%(L%)=D%(L%-1%)
4100 NEXT L%
4110 C%(1%)=H% : REM next head pos x
4120 D%(1%)=I% : REM next head pos y
4130 IF H%=J% AND I%=K% THEN E%=E%+1% : GOSUB 1000
4140 RETURN
5000 REM -- DRAW --
5010 ; CUR(C%(E%),D%(E%))" ";
5020 FOR L%=1% TO E%
5030 U%=C%(L%) : V%=D%(L%)
5040 IF L%=1% THEN ; CUR(U%,V%)"O"; ELSE ; CUR(U%,V%)"o";
5050 NEXT L%
5060 ; CUR(J%,K%)"*";
5070 RETURN
9000 REM -- GAME OVER --
9010 ; CUR(INT(A%/2%)-5%,INT(B%/2%))"GAME OVER";
9030 END
