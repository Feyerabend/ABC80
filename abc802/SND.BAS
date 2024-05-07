

1000 INTEGER
1010 DEF FNSND(F,L) LOCAL T$=15
1020   T$=CHR$(66,22,0,219,5,197,16,254,193,27,122,179,32,245,201)
1030   RETURN CALL(VARPTR(T$),F*256+L)
1040 FNEND
1050 !
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



2000 DATA 'b',200,242
2010 DATA 'H',205,228
2020 DATA 'C',210,214
2030 DATA 'C#',215,204
2040 DATA 'D',220,190
2050 DATA 'D#',225,180
2060 DATA 'E',230,170
2070 DATA 'F',235,160
2080 DATA 'F#',240,150
2090 DATA 'G',245,143
2100 DATA 'G#',250,136
2110 DATA 'A',255,128
2120 DATA 'B',255,120
2130 DATA 'H',255,114
2140 DATA 'C',255,107
2150 DATA 'C#',255,102
2160 DATA 'D',255,95
2170 DATA 'D#',255,88
2180 DATA 'E',255,85
2190 DATA 'F#',255,80
2200 DATA 'G',255,73


! (D^)  B - B     B       B - B     B      B-^D  G    A     B
! ^C   ^C    ^C ^C ^C  B   B
! B  B   B       A        A-B   A-^D
! B - B      B       B - B     B       B-^D G    A     B
! ^C   ^C    ^C ^C ^C  B    B
! B  B ^D     ^D    ^C-A     G

! D  -  B             A          G       D      D D D        B        A-G      E
! E         ^C      B       A   F#  ^D-^D  ^C   A     B
! D         B    A - G      D     D  -  B     A - G     E
! E            E  ^C  B A   ^D   ^D   ^D ^D  ^E-^D     ^C      A  -  G
