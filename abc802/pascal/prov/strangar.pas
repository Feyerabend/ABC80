PROGRAM STRANGAR;
(* PACKED ARRAY OF CHAR: tilldelning och jämförelse av hela strängar,
   strängkonstanter, tecken för tecken, och som parameter *)
TYPE NAMN = PACKED ARRAY [1..6] OF CHAR;
VAR A, B, C: NAMN;
    P, Q, R: NAMN;
    J: INTEGER;
    F: TEXT;
    FN: PACKED ARRAY [1..8] OF CHAR;

PROCEDURE SKRIV(VAR S: NAMN);
BEGIN
  WRITE('[', S, ']')
END;

PROCEDURE VAND(VAR S: NAMN);
VAR I: INTEGER;
    C: CHAR;
BEGIN
  FOR I := 1 TO 3 DO
  BEGIN
    C := S[I]; S[I] := S[7 - I]; S[7 - I] := C
  END
END;

PROCEDURE ORDNA(VAR X, Y: NAMN);
VAR Z: NAMN;
BEGIN
  IF Y < X THEN
  BEGIN
    Z := X; X := Y; Y := Z
  END
END;

BEGIN
  A := 'PASCAL';
  B := A;
  C := 'BASIC ';
  WRITELN(A, ' ', B, ' ', C);
  WRITELN(A = B, ' ', A <> B, ' ', A < C, ' ', C < A, ' ', A = 'PASCAL');
  SKRIV(C); VAND(C); SKRIV(C); WRITELN;
  A[1] := 'R';
  WRITELN(A, ' ', A[2], ' ', A > B);
  P := 'ZILOG '; Q := 'INTEL '; R := 'MOTORO';
  ORDNA(P, Q); ORDNA(Q, R); ORDNA(P, Q);
  WRITELN(P, ' ', Q, ' ', R);
  WRITELN('ABC802' :10, '|');
  FN := 'STR.TXT ';
  REWRITE(F, FN);
  WRITELN(F, 'STRANG');
  CLOSE(F);
  RESET(F, FN);
  FOR J := 1 TO 6 DO READ(F, A[J]);
  CLOSE(F);
  WRITELN(A)
END.
