10 ! PROGRAM FROM "BIT FÖR BIT MED ABC800", Luxor datorer, Motala, 1984 s.52-3
20 ! 

1000 INTEGER
1010 DEF FNSND(F,L) LOCAL T$=15
1020   T$=CHR$(66,22,0,219,5,197,16,254,193,27,122,179,32,245,201)
1030   RETURN CALL(VARPTR(T$),F*256+L)
1040 FNEND
1050 !-------------------------
1060 ! T     EQU     *
1070 !       LD      B,D
1080 !       LD      D,00H
1090 ! LOOP  EQU     *
1100 !       IN      A,05H
1110 !       PUSH    BC
1120 ! WLOOP EQU     *
1130 !       DJNZ    WLOOP
1140 !       POP     BC
1150 !       DEC     DE
1160 !       LD      A,D
1170 !       OR      E
1180 !       JR      NZ,LOOP
1190 !       RET
1200 !-------------------------
1210 FOR T=0 TO 20
1220   READ A$(T),K(T),G(T)
1230 NEXT T
1240 FOR T=0 TO 9
1250   READ B$
1260 NEXT T
1270 !
1280 
1290
1300
1310
1320

2000 DATA 'b',200,242
2010 DATA 'Hi',205,228
2020 DATA 'Ci',210,214
2030 DATA 'Ci#',215,204
2040 DATA 'Di',220,190
2050 DATA 'Di#',225,180
2060 DATA 'Ei',230,170
2070 DATA 'Fi',235,160
2080 DATA 'Fi#',240,150
2090 DATA 'Gi',245,143
2100 DATA 'G#',250,136
2110 DATA 'A',255,128
2120 DATA 'B',255,120
2130 DATA 'Hj',255,114
2140 DATA 'Cj',255,107
2150 DATA 'Cj#',255,102
2160 DATA 'Dj',255,95
2170 DATA 'Dj#',255,88
2180 DATA 'Ej',255,85
2190 DATA 'Fj#',255,80
2200 DATA 'Gj',255,73
2210 !
3000 DATA "Di#,B,-,B,B,B,-,B,B,B,-,Di#,Gi,A,B" ! (D^)  B - B     B       B - B     B      B-^D  G    A     B
2010 DATA "Ci#,Ci#,Ci#,Ci#,Ci#,B,B" ! ^C   ^C    ^C ^C ^C  B   B
2020 DATA ! B  B   B       A        A-B   A-^D
2030 DATA ! B - B      B       B - B     B       B-^D G    A     B
2040 DATA ! ^C   ^C    ^C ^C ^C  B    B
2050 DATA ! B  B ^D     ^D    ^C-A     G
2060 DATA ! D  -  B             A          G       D      D D D        B        A-G      E
2070 DATA ! E         ^C      B       A   F#  ^D-^D  ^C   A     B
2080 DATA ! D         B    A - G      D     D  -  B     A - G     E
2090 DATA ! E            E  ^C  B A   ^D   ^D   ^D ^D  ^E-^D     ^C      A  -  G
