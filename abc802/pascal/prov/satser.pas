PROGRAM SATSER(OUTPUT);
(* CASE, REPEAT, DOWNTO, CHAR och BOOLEAN. *)
VAR C : CHAR; I : INTEGER; B : BOOLEAN;
    T : ARRAY ['A'..'E'] OF INTEGER;
BEGIN
  FOR I := 0 TO 5 DO
    CASE I OF
      0: WRITE('noll ');
      1, 3, 5: WRITE('udda ');
      2, 4: WRITE('jamn ');
    END;
  WRITELN;
  FOR C := 'E' DOWNTO 'A' DO
  BEGIN
    T[C] := ORD(C) - ORD('A');
    WRITE(C, SUCC(C), T[C]:2, ' ')
  END;
  WRITELN;
  I := 1;
  REPEAT I := I * 3 UNTIL I > 100;
  B := (I = 243) AND NOT (I < 0) OR FALSE;
  WRITELN(I:1, ' ', B, ' ', CHR(ORD('a') + 2), ' ', -7 DIV 2:1, ' ', -7 MOD 2:1, ' ', ABS(-5):1, ' ', PRED(10):1);
  WRITELN('It''s', MAXINT:6)
END.
