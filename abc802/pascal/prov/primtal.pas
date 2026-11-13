PROGRAM PRIMTAL(OUTPUT);
(* Eratosthenes såll till 8191. *)
CONST MAX = 8191;
VAR SALL : ARRAY (.2..MAX.) OF BOOLEAN;
    I, J, N : INTEGER;
BEGIN
  FOR I := 2 TO MAX DO SALL(.I.) := TRUE;
  N := 0;
  FOR I := 2 TO MAX DO
    IF SALL(.I.) THEN
    BEGIN
      N := N + 1;
      IF I < 100 THEN WRITE(I:3);
      J := I + I;
      WHILE J <= MAX DO
      BEGIN
        SALL(.J.) := FALSE;
        J := J + I
      END
    END;
  WRITELN;
  WRITELN(N:1, ' primtal under 8192')
END.
