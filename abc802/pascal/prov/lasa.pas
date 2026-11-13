PROGRAM LASA(INPUT, OUTPUT);
(* Läsa tal från tangentbordet och skriva och läsa en fil. *)
VAR F : TEXT; N, S, K : INTEGER; C : CHAR;
BEGIN
  READLN(N);
  S := 0;
  FOR K := 1 TO N DO
  BEGIN
    READ(C);
    S := S + ORD(C) - ORD('0')
  END;
  READLN;
  READ(K);
  WRITELN('Siffrorna ', S:1, ', talet ', K:1);
  REWRITE(F, 'UT.TXT');
  FOR K := 1 TO 3 DO WRITELN(F, 'rad ', K:1);
  CLOSE(F);
  RESET(F, 'UT.TXT');
  N := 0;
  WHILE NOT EOF(F) DO
  BEGIN
    WHILE NOT EOLN(F) DO BEGIN READ(F, C); WRITE(C) END;
    READLN(F);
    N := N + 1;
    WRITE('/')
  END;
  CLOSE(F);
  WRITELN(N:2)
END.
