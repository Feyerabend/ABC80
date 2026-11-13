PROGRAM HALSA(OUTPUT);
(* En hälsning och kvadraterna. *)
CONST N = 10;
VAR I, S : INTEGER;
BEGIN
  WRITELN('Hej fran Pascal!');
  S := 0;
  FOR I := 1 TO N DO
  BEGIN
    WRITE(SQR(I):4);
    S := S + I
  END;
  WRITELN;
  WRITELN('Summan 1..', N:1, ' = ', S:1)
END.
