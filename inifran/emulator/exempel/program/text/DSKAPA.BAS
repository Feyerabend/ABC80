10 REM DSKAPA, GÖR BLANKETTENS SIDOR PÅ DATASKIVAN (UTANFÖR BOKEN)
20 REM VARJE SIDA ÄR SKÄRMRADERNA 0-22 I FYRA BLOCK OM SEX RADER I
30 REM DR1:DEKL.DAT, SIDA S I BLOCK 4*S-4 TILL 4*S-1, OCH BLOCK 16 ÄR SIDAN
40 REM SOM VISAS. RADERNA STÅR SOM DATA FRÅN RAD 1000: #N ÄR EN RUTA ATT
50 REM FYLLA I, =N EN RUTA SOM RÄKNAS, !TEXT|L ETT TEXTFÄLT, / SLUTAR SIDAN
60 DIM Q0$=253,B$=253,A$=40,L$=40,D$=40,T$=40,N$=4
70 D$=SPACE$(40) : PRINT CHR$(12);"Gör DR1:DEKL.DAT"
80 PREPARE "DR1:DEKL.DAT" ASFILE 1
90 FOR I%=0% TO 16% : PRINT #1,STRING$(252%,46%) : NEXT I% : CLOSE 1
100 OPEN "DR1:DEKL.DAT" ASFILE 1
110 FOR S%=1% TO 4% : PRINT "Sida";S%
120 B$="" : R%=0% : K%=4%*S%-4%
130 L$=LEFT$("SJÄLVDEKLARATION 1982"+D$,29%)+"SIDA "+CHR$(48%+S%)+" AV 4" : GOSUB 500
140 L$=STRING$(40%,45%) : GOSUB 500
150 READ A$ : IF A$="/" THEN 200
160 C$=LEFT$(A$+" ",1%) : L$=A$ : IF C$="#" OR C$="=" THEN GOSUB 300
170 IF C$="!" THEN GOSUB 400
180 GOSUB 500 : GOTO 150
200 IF R%<23% THEN L$="" : GOSUB 500 : GOTO 200
210 L$="" : GOSUB 500 : NEXT S%
220 Z=CALL(28666,1) : Q0$="1"+SPACE$(252) : Z=CALL(28670,16)
230 CLOSE 1 : PRINT "Klart." : GOTO 9999
300 REM RUTA: NUMRET I KOLUMN 0-1, LEDTEXTEN, = ELLER BLANKT OCH FÄLTET
310 P%=INSTR(1%,A$," ") : N$=MID$(A$,2%,P%-2%) : T$=RIGHT$(A$,P%+1%)
320 IF C$="#" THEN C$=" "
330 L$=RIGHT$(" "+N$,LEN(N$))+" "+LEFT$(T$+D$,25%)+C$+"<"+SPACE$(9%)+">"
340 RETURN
400 REM TEXTFÄLT: LEDTEXTEN I TIO TECKEN OCH L TECKEN MELLAN < OCH >
410 P%=INSTR(1%,A$,"|") : T$=MID$(A$,2%,P%-2%)
420 L$=LEFT$(T$+D$,10%)+"<"+LEFT$(D$,VAL(RIGHT$(A$,P%+1%)))+">"
430 RETURN
500 REM EN RAD TILL BLOCKET, SEX RADER I VARJE
510 B$=B$+LEFT$(L$+D$,40%) : R%=R%+1%
520 IF R%=6%*(R%/6%) THEN Z=CALL(28666,1) : Q0$=B$+SPACE$(13%) : Z=CALL(28670,K%) : K%=K%+1% : B$=""
530 RETURN
1000 DATA "PERSONUPPGIFTER",""
1010 DATA "!Namn|27","!Personnr|11","!Adress|27","!Postort|27"
1020 DATA "!Kommun|18","","INKOMST AV TJÄNST"
1030 DATA "#1 Lön, arvoden, förmåner","#2 Pension, livränta"
1040 DATA "#3 Sjukpenning m.m.","AVDRAG I TJÄNST","#4 Resor till arbetet"
1050 DATA "#5 Övriga avdrag","=6 Inkomst av tjänst","   (1 + 2 + 3 - 4 - 5)"
1060 DATA "","","Efterbildad blankett, inte RSV:s.","/"
1100 DATA "KAPITAL OCH ALLMÄNNA AVDRAG","","INKOMST AV KAPITAL"
1110 DATA "#7 Ränta på bankmedel m.m.","#8 Utdelning på aktier"
1120 DATA "#9 Övriga kapitalinkomster","#10 Räntor på skulder"
1130 DATA "=11 Inkomst av kapital","   (7 + 8 + 9 - 10, kan bli minus)",""
1140 DATA "ALLMÄNNA AVDRAG","#12 Periodiskt understöd","#13 Pensionsförsäkring",""
1150 DATA "=14 Sammanräknad inkomst","   (6 + 11)","=15 Taxerad inkomst"
1160 DATA "   (14 - 12 - 13)","/"
1200 DATA "FÖRMÖGENHET DEN 31 DECEMBER 1981","","TILLGÅNGAR"
1210 DATA "#16 Bankmedel","#17 Aktier, obligationer"
1220 DATA "#18 Fastighet, taxeringsvärde","#19 Övriga tillgångar",""
1230 DATA "SKULDER","#20 Skulder","","=21 Nettoförmögenhet"
1240 DATA "   (16 + 17 + 18 + 19 - 20)","/"
1300 DATA "SKATTEN, FÖRENKLAD","","=22 Taxerad inkomst (15)"
1310 DATA "=23 Grundavdrag","=24 Taxerbar inkomst",""
1320 DATA "=25 Kommunalskatt 30 %","=26 Statlig skatt","   (25 % över 40 000)"
1330 DATA "=27 Förmögenhetsskatt","   (1 % över 200 000)","=28 Summa skatt",""
1340 DATA "#29 Inbetald preliminärskatt","=30 Kvarskatt, minus = åter"
1350 DATA "","Skattereglerna är påhittade och","förenklade, inte 1982 års.","/"
9999 END
