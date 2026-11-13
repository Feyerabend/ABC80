PROGRAM SORTERA(OUTPUT);
(* Quicksort på ett fält som VAR-parameter, och nästlade procedurer. *)
CONST N = 20;
TYPE TAL = ARRAY (.1..N.) OF INTEGER;
VAR A, B : TAL;
    I, FRO : INTEGER;

PROCEDURE SKRIV(VAR X : TAL);
VAR I : INTEGER;
BEGIN
  FOR I := 1 TO N DO WRITE(X(.I.):4);
  WRITELN
END;

PROCEDURE SORT(VAR X : TAL);
VAR BYTEN : INTEGER;

  PROCEDURE BYT(VAR P, Q : INTEGER);
  VAR T : INTEGER;
  BEGIN
    T := P; P := Q; Q := T;
    BYTEN := BYTEN + 1
  END;

  PROCEDURE QS(L, R : INTEGER);
  VAR I, J, M : INTEGER;
  BEGIN
    I := L; J := R; M := X(.(L + R) DIV 2.);
    REPEAT
      WHILE X(.I.) < M DO I := I + 1;
      WHILE M < X(.J.) DO J := J - 1;
      IF I <= J THEN
      BEGIN
        BYT(X(.I.), X(.J.));
        I := I + 1; J := J - 1
      END
    UNTIL I > J;
    IF L < J THEN QS(L, J);
    IF I < R THEN QS(I, R)
  END;

BEGIN
  BYTEN := 0;
  QS(1, N);
  WRITELN(BYTEN:1, ' byten')
END;

BEGIN
  FRO := 4711;
  FOR I := 1 TO N DO
  BEGIN
    FRO := (FRO * 13 + 7) MOD 1000;
    IF FRO < 0 THEN FRO := -FRO;
    A(.I.) := FRO
  END;
  B := A;
  SORT(A);
  SKRIV(A);
  SKRIV(B)
END.
