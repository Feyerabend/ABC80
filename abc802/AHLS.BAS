1 ' Ahl's simple benchmark test
2 ' In Lines 30 and 40, some computers
3 ' may require RND(1) for correct results
10 PRINT "Accuracy  Random"
20 FOR N=1 TO 100:A=N
30 FOR I=1 TO 10:A=SQR(A):R=R+RND(0):NEXT I
40 FOR I=1 TO 10:A=A^2:R=R+RND(0):NEXT I
50 S=S+A:NEXT N
60 PRINT ABS(1010-S/5);ABS(1000-R)