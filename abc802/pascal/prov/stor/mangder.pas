PROGRAM MANGDER;
(* SET OF: mängder av tecken, tal och en uppräkning *)
TYPE
  DAG = (MAN, TIS, ONS, TOR, FRE, LOR, SON);
  DAGAR = SET OF DAG;
VAR
  VOKAL, BOKST: SET OF CHAR;
  T: SET OF 0..255;
  ARBETE, HELG, ALLA: DAGAR;
  D: DAG;
  C: CHAR;
  I, N: INTEGER;
  RAD: PACKED ARRAY [1..16] OF CHAR;

PROCEDURE VISA(M: DAGAR);
VAR D: DAG;
BEGIN
  FOR D := MAN TO SON DO
    IF D IN M THEN WRITE(ORD(D): 2);
  WRITELN
END;

FUNCTION UDDA(N: INTEGER): DAGAR;
VAR D: DAG;
    M: DAGAR;
BEGIN
  M := [];
  FOR D := MAN TO SON DO
    IF ODD(ORD(D)) THEN M := M + [D];
  UDDA := M
END;

BEGIN
  VOKAL := ['A', 'E', 'I', 'O', 'U', 'Y'];
  BOKST := ['A'..'Z'];
  RAD := 'PASCAL PA ABC802';
  N := 0;
  FOR I := 1 TO 16 DO
    IF RAD[I] IN VOKAL THEN N := N + 1;
  WRITELN(N);
  FOR I := 1 TO 16 DO
    IF RAD[I] IN BOKST - VOKAL THEN WRITE(RAD[I]);
  WRITELN;
  ARBETE := [MAN..FRE];
  HELG := [LOR, SON];
  ALLA := ARBETE + HELG;
  VISA(ALLA);
  VISA(ALLA - [TIS, TOR]);
  VISA(ARBETE * [FRE..SON]);
  VISA(UDDA(0));
  WRITELN(ARBETE <= ALLA, ALLA <= ARBETE: 6, ALLA >= HELG: 5);
  WRITELN(ARBETE = HELG, ARBETE + HELG <> ALLA: 6, [] = ARBETE * HELG: 5);
  (* sållet *)
  T := [2..255];
  FOR I := 2 TO 15 DO
    IF I IN T THEN
    BEGIN
      N := I * I;
      WHILE N <= 255 DO
      BEGIN
        T := T - [N];
        N := N + I
      END
    END;
  N := 0;
  FOR I := 0 TO 255 DO
    IF I IN T THEN N := N + 1;
  WRITELN(N, 251 IN T: 6, 300 IN T: 6, -1 IN T: 6);
  I := 256;
  T := [I]
END.
