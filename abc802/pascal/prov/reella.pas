PROGRAM REELLA;
(* REAL: konstanter, aritmetik, blandat med INTEGER, funktioner,
  jämförelser, WRITE med :w:d och READ *)
CONST PI = 3.14159265;
      MINPI = -PI;
TYPE VEKTOR = ARRAY [1..5] OF REAL;
VAR X, Y, S: REAL;
    I, N: INTEGER;
    V: VEKTOR;

FUNCTION MEDEL(VAR A: VEKTOR; N: INTEGER): REAL;
VAR I: INTEGER;
    S: REAL;
BEGIN
  S := 0;
  FOR I := 1 TO N DO S := S + A[I];
  MEDEL := S / N
END;

PROCEDURE BYT(VAR A, B: REAL);
VAR T: REAL;
BEGIN
  T := A; A := B; B := T
END;

FUNCTION KVADRATROT(X: REAL): REAL;
(* Newton, för att pröva REAL som värdeparameter *)
VAR R: REAL;
BEGIN
  R := X;
  WHILE ABS(R * R - X) > 1E-6 * X DO R := (R + X / R) / 2;
  KVADRATROT := R
END;

BEGIN
  X := 1.5;
  Y := 2;
  WRITELN(X + Y :8:3, X - Y :8:3, X * Y :8:3, X / Y :8:3);
  WRITELN(7 / 2 :6:2, 1 + X :6:2, X + 1 :6:2, -X :6:2);
  WRITELN(PI :10:6, MINPI :10:6);
  WRITELN(SQRT(2) :10:6, SQR(X) :8:3, ABS(-2.5) :6:2, SQR(3) :4, ABS(-4) :4);
  WRITELN(SIN(PI / 6) :8:4, COS(0) :8:4, ARCTAN(1) * 4 :10:6);
  WRITELN(LN(EXP(1)) :8:4, EXP(2) :10:5);
  WRITELN(TRUNC(3.7) :4, TRUNC(-3.7) :4, ROUND(3.5) :4, ROUND(-3.5) :4, ROUND(2.4) :4);
  WRITELN(X < Y :6, X = 1.5 :6, Y >= 2 :6, 3 > X :6, X <> 1.5 :6);
  FOR I := 1 TO 5 DO V[I] := I * 1.5;
  WRITELN(MEDEL(V, 5) :8:3);
  BYT(X, Y);
  WRITELN(X :6:2, Y :6:2);
  WRITELN(KVADRATROT(10) :10:5);
  WRITELN(1E3 :10:1, 2.5E-2 :8:4, 0.0 :6:2);
  WRITELN(X);
  WRITELN(1 / 3);
  S := 0;
  N := 0;
  READ(X);
  WHILE X <> 0 DO
  BEGIN
    S := S + X;
    N := N + 1;
    READ(X)
  END;
  WRITELN(N, ' tal, summan ', S :8:2)
END.
