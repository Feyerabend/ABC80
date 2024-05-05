1000 EXTEND : RANDOMIZE : INTEGER
1010 ! init of game for ABC802
1020 F=0 ! flag for dot
1030 GOTO 1510 ! init screen
1040 Cu1=1 ! keep last keypress, at start choose 1-4
1050 Fx=0 : Fy=0 ! food coords.
1060 Mx=79*2 : My=71 ! where the food can be (change if width 40)
1070 Smin=5 ! snake minimal length
1080 Tail=Smin ! "last" bit of snake
1090 Smax=500 ! the maximum for the snake length
1100 DIM R(Smax),C(Smax) ! coords. for snake, each part thus indexed
1110 R(0)=5 : R(1)=4 : R(2)=3 : R(3)=2 : R(4)=1 ! snake at start
1120 T=Mx/2
1130 C(0)=T : C(1)=T : C(2)=T : C(3)=T : C(5)=T
1140 R1=R(Tail-1) : C1=C(Tail-1) ! last dot of snake (to clear when moved)
1150 Score=0 ! keep track of
1160 GOSUB 1660 ! prepare food
1170 !
1180 ! game loop
1190 !
1200 GOTO 1340 ! get key and prepare move
1210 IF Fx=X AND Fy=Y THEN GOSUB 1800 ELSE GOSUB 1950 ! food at the head?
1220 IF F THEN 1270 ! dot was not food, then crash
1230 GOTO 1900 ! move
1240 GOTO 1700 ! shift
1250 GOTO 2000 ! draw
1260 GOTO 1200 ! loop again
1270 ; CUR(10,30);CHR$(135) 'G A M E   O V E R' CHR$(151);
1280 ; CUR(12,34);CHR$(135) 'Score: ';Score;CHR$(151);
1290 END
1300 !
1310 ! (sub)routines
1320 !
1330 ! keys
1340 !
1350 K=ABS(SYS(8)*INP(34)) ! key polling, ascii from key
1360 IF 97=K AND Cu1<>3 THEN Cu=1
1370 IF 115=K AND Cu1<>1 THEN Cu=3
1380 IF 122=K AND Cu1<>4 THEN Cu=2
1390 IF 119=K AND Cu1<>2 THEN Cu=4
1400 X=R(0) : Y=C(0)
1410 IF Cu=0 Cu=Cu1 ! take last "valid" keypress
1420 ON Cu GOTO 1430,1440,1450,1460
1430 Y=Y-1 : GOTO 1470
1440 X=X+1 : GOTO 1470
1450 Y=Y+1 : GOTO 1470
1460 X=X-1 : GOTO 1470
1470 Cu1=Cu
1480 GOTO 1210
1490 !
1500 ! graphics init
1510 !
1520 ; CHR$(12);
1530 FOR I=0 TO 23
1540   ; CUR(I,0);GWHT;
1550 NEXT I
1560 FOR I=2 TO Mx
1570   SET DOT 0,I : SET DOT I,Mx
1580 NEXT I
1590 FOR I=0 TO My
1600   SET DOT I,2 : SET DOT I,My
1610 NEXT I
1620 GOTO 1040
1630 !
1640 ! create food
1650 !
1660 Fx=INT(RND*(Mx/2-20))+10
1670 Fy=INT(RND*(My-10))+5
1680 RETURN
1690 !
1700 ! shift snake
1710 !
1720 T1=Tail-1
1730 Tr=R(T1) : Tc=C(T1)
1740 FOR I=Tail TO 1 STEP -1
1750   R(I)=R(I-1) : C(I)=C(I-1)
1760 NEXT I
1770 R(0)=Tr : C(0)=Tc
1780 GOTO 1250
1790 !
1800 ! eat food
1810 !
1820 R(Tail)=X : C(Tail)=Y
1830 IF Tail<Smax THEN Tail=Tail+1 ELSE Tail=Smax
1840 Score=Score+1
1850 GOSUB 1660
1860 F=0
1870 RETURN
1880 !
1890 ! move
1900 !
1910 R(Tail-1)=X : C(Tail-1)=Y
1920 GOTO 1240
1930 !
1940 ! test DOT
1950 !
1960 IF DOT(X,Y) F=-1 ELSE F=0
1970 RETURN
1980 !
1990 ! draw
2000 !
2010 T=Tail-1
2020 CLR DOT R1,C1
2030 FOR I=0 TO T
2040   SET DOT R(1),C(I)
2050 NEXT I
2060 R1=R(T) : C1=C(T)
2070 SET DOT Fx,Fy
2080 GOTO 1260
2090 !
2100 ! S. Lonnert, public domain 2024
