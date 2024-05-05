100 REM A BASIC, ASCII MANDELBROT
120 REM This implementation copyright (c) 2019, Gordon Henderson
140 REM Permission to use/abuse anywhere for any purpose granted, but
150 REM it comes with no warranty whatsoever. Good luck!
160 REM ABC800 variant by litwr, 2021
165 EXTEND
170 C$ = ".,'~=+:;[/<&?oxOX# " : REM 'Pallet' Lightest to darkest...
180 SO = 1 : REM Set to 0 if your MID$() indexes from 0.
190 MI = LEN(C$) : MX = 4
210 LS = -2.0 : TP = 1.25 : XS = 2.5 : YS = -2.5
250 W = 38 : H = 21
270 SX = XS / W : SY = YS / H
290 T1$=RIGHT$(TIME$,12)
300 FOR Y = 0 TO H
310   CY = Y * SY + TP
320   FOR X = 0 TO W
330     CX = X * SX + LS
340     ZX = 0
350     ZY = 0
360     CC = SO
370     X2 = ZX * ZX
380     Y2 = ZY * ZY
390     IF CC > MI THEN GOTO 460
400     IF (X2 + Y2) > MX THEN GOTO 460
410     T = X2 - Y2 + CX
420     ZY = 2 * ZX * ZY + CY
430     ZX = T
440     CC = CC + 1
450     GOTO 370
460     PRINT MID$(C$, CC - SO, 1);
470   NEXT X
480   PRINT
490 NEXT Y
500 T2$=RIGHT(TIME$,12)
510 T=(VAL(LEFT$(T2$,2))-VAL(LEFT$(T1$,2)))*3600+VAL(RIGHT$(T2$,7))-VAL(RIGHT$(T1$,7))
514 PRINT T+(VAL(MID$(T2$,4,2))-VAL(MID$(T1$,4,2)))*60;
520 END
