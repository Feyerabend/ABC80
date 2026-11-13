PROGRAM PC(INPUT, OUTPUT);
(* pc.pas -- kompilatorn för delmängden, skriven i delmängden själv
   (PASCAL.md, avsnitt 2 och 3). Den läser källans namn från
   tangentbordet, till exempel HEJ.PAS, och skriver p-koden till HEJ.PK.

   Den gör som pc.py och ger samma bytes: ett pass, rekursiv nedstigning
   och koden direkt utan träd. Varje procedur skrivs till filen när den
   är klar, och tabellen över procedurerna kommer sist.

   Symbolerna är tecken: ett tecken som + är sig självt, och orden och
   de andra symbolerna har koderna nedan. Namnen packas två tecken per
   ord i POOL och hittas genom en hashtabell. *)

CONST
  KODMAX = 1535;    (* bytes i en procedurs kod *)
  KODORD = 767;     (* två bytes i varje ord *)
  TABMAX = 370;     (* poster i symboltabellen *)
  POOLMAX = 1300;   (* ord för namnen *)
  PROCMAX = 90;     (* procedurer *)
  PARMAX = 90;      (* parametrar i alla procedurer *)
  TYPMAX = 30;      (* fälttyper *)
  FALLMAX = 64;     (* fall i CASE i varandra *)
  TEMPMAX = 8;      (* FOR i varandra *)
  LISTMAX = 16;     (* namn i en VAR-grupp *)

  (* symbolerna *)
  SAND = 'a'; SARRAY = 'b'; SBEGIN = 'c'; SCASE = 'd'; SCONST = 'e';
  SDIV = 'f'; SDO = 'g'; SDOWNTO = 'h'; SELSE = 'i'; SEND = 'j';
  SFOR = 'k'; SFORWARD = 'l'; SFUNCTION = 'm'; SIF = 'n'; SMOD = 'o';
  SNOT = 'p'; SOF = 'q'; SOR = 'r'; SPROCEDURE = 's'; SPROGRAM = 't';
  SREPEAT = 'u'; STHEN = 'v'; STO = 'w'; STYPE = 'x'; SUNTIL = 'y';
  SVAR = 'z'; SWHILE = '{'; SPACKED = '|';
  SNAMN = '1'; STAL = '2'; STECKEN = '3'; SSTRANG = '4'; SSLUT = '5';
  STILL = '6'; SLE = '7'; SGE = '8'; SNE = '9'; SPP = '!'; SREELL = '#';

  (* namnens slag; en död parameter (FORWARD) har slaget + 10 *)
  KKONST = 1; KTYP = 2; KVAR = 3; KVARP = 4; KPROC = 5; KFUNK = 6;
  KSTD = 7; KSTDFIL = 8; KORD = 9;

  (* typerna; fälten är FTYP och uppåt, och en strängkonstant med
     längden n har typen -n *)
  HEL = 1; TKN = 2; BOO = 3; TXT = 4; REA = 5; FTYP = 6;

  (* opkoderna (pkod.py) *)
  OLDCN = 16; OLDCB = 17; OLDCW = 18; OLSA = 19;
  OLDL0 = 24; OSTL0 = 32; OLDL = 40; OSTL = 41; OLLA = 42;
  OLDG0 = 48; OLDG = 56; OSTG = 57; OLGA = 58;
  OLOD = 60; OSTR = 61; OLDA = 62;
  OIND = 64; OSTO = 65; OINDB = 66; OSTB = 67; OMOV = 68; OCPS = 69;
  OIXA = 72; OCHK = 73;
  OADD = 80; OSUB = 81; OMUL = 82; ODIV = 83; OMOD = 84; ONEG = 85;
  OABS = 86; OINC1 = 87; ODEC1 = 88; OSQRI = 89;
  OEQU = 96; ONEQ = 97; OLES = 98; OLEQ = 99; OGRT = 100; OGEQ = 101;
  OAND = 104; OOR = 105; ONOT = 106; OODD = 107;
  OUJP = 114; OFJP = 115; OXJP = 116;
  OMST = 120; OCUP = 121; OENT = 122; ORET = 124; OLFA = 125;
  OCSP = 128; OERR = 142; OHLT = 143;
  OLDCR = 160; OINDR = 161; OSTOR = 162; OFLT = 163; OFLO = 164;
  OTRC = 165; OADR = 167; OSBR = 168; OMPR = 169; ODVR = 170; ONGR = 171;
  OABR = 172; OSQRR = 173; OEQR = 174; OSQT = 180; OWRR = 186; ORDR = 187;

VAR
  F, G : TEXT;
  KNAMN, UNAMN : PACKED ARRAY [1..24] OF CHAR;

  (* läsaren *)
  CH, CH2 : CHAR;                       (* tecknet och nästa *)
  RAD, KOL, SYMRAD, SYMKOL, FORRAD, FORKOL : INTEGER;
  SYM : CHAR;
  VAL : INTEGER;                        (* talet eller tecknet *)
  SBUF : PACKED ARRAY [1..80] OF CHAR;       (* strängen *)
  SLANGD : INTEGER;
  STORT, LANG, REELL : BOOLEAN;         (* REELL: koden har REAL *)
  ID : PACKED ARRAY [1..33] OF CHAR;         (* namnet *)
  NBUF : ARRAY [0..17] OF INTEGER;      (* namnet packat *)

  (* namnen *)
  POOL : ARRAY [1..POOLMAX] OF INTEGER;
  POOLTOP : INTEGER;
  HUVUD : ARRAY [0..60] OF INTEGER;
  TNAMN, TINFO, TVAL, TLANK : ARRAY [0..TABMAX] OF INTEGER;
  (* TINFO är slaget + 32 * typen + 2048 * nivån *)
  TOP, SCOPE : INTEGER;
  NLISTA : ARRAY [1..LISTMAX] OF INTEGER;

  (* fälten och procedurerna *)
  FLO, FHI, FELEM : ARRAY [FTYP..TYPMAX] OF INTEGER;
  FPACK : ARRAY [FTYP..TYPMAX] OF BOOLEAN;
  TYPER : INTEGER;
  PFORST, PANT, PFPAR, POFFS : ARRAY [0..PROCMAX] OF INTEGER;
  PFORW : ARRAY [0..PROCMAX] OF BOOLEAN;
  PTYP : ARRAY [1..PARMAX] OF INTEGER;
  PVAR : ARRAY [1..PARMAX] OF BOOLEAN;
  PARTOP, PROCS, NIVA, LOKALA, GLOBALA : INTEGER;

  (* koden *)
  KOD : ARRAY [0..KODORD] OF INTEGER;  (* byte - 128 högst *)
  KLEN, UTLEN : INTEGER;
  TEMP : ARRAY [0..TEMPMAX] OF INTEGER; (* FOR:s gränser *)
  TEMPANT, TEMPDJUP : INTEGER;
  FVAL, FPOS, UTPOS : ARRAY [1..FALLMAX] OF INTEGER;
  FALLTOP, UTTOP : INTEGER;
  FILREF : INTEGER;                     (* filen i WRITE och READ *)
  I, P : INTEGER;
  C : CHAR;

(* -- posterna i symboltabellen --------------------------------------- *)

FUNCTION SLAGET(E : INTEGER) : INTEGER;
BEGIN SLAGET := TINFO[E] MOD 32 END;

FUNCTION TYPEN(E : INTEGER) : INTEGER;
BEGIN TYPEN := TINFO[E] DIV 32 MOD 64 END;

FUNCTION NIVAN(E : INTEGER) : INTEGER;
BEGIN NIVAN := TINFO[E] DIV 2048 END;

(* -- felen ----------------------------------------------------------- *)

PROCEDURE FELPOS(N, R, K : INTEGER);
BEGIN
  WRITELN('rad ', R, ', kolumn ', K + 1, ': fel ', N);
  CLOSE(G);
  HALT
END;

PROCEDURE FEL(N : INTEGER);       (* vid symbolen *)
BEGIN FELPOS(N, SYMRAD, SYMKOL) END;

PROCEDURE FELLAS(N : INTEGER);    (* vid tecknet som läses *)
BEGIN FELPOS(N, RAD, KOL) END;

PROCEDURE FELFOR(N : INTEGER);    (* vid symbolen före *)
BEGIN FELPOS(N, FORRAD, FORKOL) END;

(* -- läsaren --------------------------------------------------------- *)

PROCEDURE LAS;
BEGIN
  IF EOF(F) THEN CH2 := CHR(0)
  ELSE IF EOLN(F) THEN BEGIN READLN(F); CH2 := CHR(10) END
  ELSE READ(F, CH2)
END;

PROCEDURE FRAM;
BEGIN
  IF CH = CHR(10) THEN BEGIN RAD := RAD + 1; KOL := 0 END
  ELSE KOL := KOL + 1;
  CH := CH2;
  LAS
END;

FUNCTION BOKSTAV(C : CHAR) : BOOLEAN;
BEGIN
  BOKSTAV := (C >= 'A') AND (C <= 'Z') OR (C >= 'a') AND (C <= 'z')
END;

FUNCTION SIFFRA(C : CHAR) : BOOLEAN;
BEGIN SIFFRA := (C >= '0') AND (C <= '9') END;

PROCEDURE PACKA(N : INTEGER);     (* ID[1..N] till NBUF *)
VAR J : INTEGER;
BEGIN
  NBUF[0] := N;
  ID[N + 1] := CHR(0);
  FOR J := 1 TO (N + 1) DIV 2 DO
    NBUF[J] := ORD(ID[2 * J - 1]) * 128 + ORD(ID[2 * J])
END;

FUNCTION LAGG : INTEGER;          (* NBUF överst i POOL, inte tagen *)
VAR J, N : INTEGER;
BEGIN
  N := (NBUF[0] + 1) DIV 2;
  IF POOLTOP + N > POOLMAX THEN FEL(26);
  FOR J := 0 TO N DO POOL[POOLTOP + J] := NBUF[J];
  LAGG := POOLTOP
END;

FUNCTION HASH(P : INTEGER) : INTEGER;
VAR H, J : INTEGER;
BEGIN
  H := POOL[P];
  FOR J := 1 TO (POOL[P] + 1) DIV 2 DO H := (H + POOL[P + J]) MOD 61;
  HASH := H
END;

FUNCTION LIKA(P, Q : INTEGER) : BOOLEAN;
VAR J, N : INTEGER; L : BOOLEAN;
BEGIN
  L := POOL[P] = POOL[Q];
  IF L THEN
  BEGIN
    N := (POOL[P] + 1) DIV 2;
    J := 1;
    WHILE L AND (J <= N) DO
    BEGIN
      L := POOL[P + J] = POOL[Q + J];
      J := J + 1
    END
  END;
  LIKA := L
END;

PROCEDURE SPARA;                  (* CH till ID, högst 32 tecken *)
BEGIN
  IF SLANGD = 32 THEN LANG := TRUE
  ELSE BEGIN SLANGD := SLANGD + 1; ID[SLANGD] := CH END;
  FRAM
END;

PROCEDURE SIFFROR;                (* och talet i VAL *)
VAR D : INTEGER;
BEGIN
  WHILE SIFFRA(CH) DO
  BEGIN
    D := ORD(CH) - ORD('0');
    IF (VAL > 3276) OR (VAL = 3276) AND (D > 7) THEN STORT := TRUE
    ELSE VAL := VAL * 10 + D;
    SPARA
  END
END;

FUNCTION SOKA(P : INTEGER) : INTEGER;   (* posten, eller 0 *)
VAR E : INTEGER; FUNNEN : BOOLEAN;
BEGIN
  E := HUVUD[HASH(P)];
  FUNNEN := FALSE;
  WHILE (E <> 0) AND NOT FUNNEN DO
    IF LIKA(TNAMN[E], P) THEN FUNNEN := TRUE ELSE E := TLANK[E];
  SOKA := E
END;

PROCEDURE NASTA;
VAR KLAR : BOOLEAN; N, E : INTEGER;
BEGIN
  KLAR := FALSE;
  REPEAT
    IF (CH = ' ') OR (CH = CHR(9)) OR (CH = CHR(10)) OR (CH = CHR(12))
       OR (CH = CHR(13)) THEN FRAM
    ELSE IF (CH = '(') AND (CH2 = '*') THEN
    BEGIN
      FRAM; FRAM;
      WHILE NOT ((CH = '*') AND (CH2 = ')')) DO
      BEGIN
        IF CH = CHR(0) THEN FELLAS(3);
        FRAM
      END;
      FRAM; FRAM
    END
    ELSE KLAR := TRUE
  UNTIL KLAR;
  FORRAD := SYMRAD; FORKOL := SYMKOL;
  SYMRAD := RAD; SYMKOL := KOL;
  IF CH = CHR(0) THEN SYM := SSLUT
  ELSE IF BOKSTAV(CH) THEN
  BEGIN
    N := 0;
    WHILE BOKSTAV(CH) OR SIFFRA(CH) DO
    BEGIN
      IF N = 32 THEN FEL(25);
      N := N + 1;
      IF CH >= 'a' THEN ID[N] := CHR(ORD(CH) - 32) ELSE ID[N] := CH;
      FRAM
    END;
    PACKA(N);
    E := SOKA(LAGG);
    IF SLAGET(E) = KORD THEN SYM := CHR(TVAL[E]) ELSE SYM := SNAMN
  END
  ELSE IF SIFFRA(CH) THEN
  BEGIN
    (* ett REAL har en punkt med en siffra efter, eller E; texten
       packas i NBUF *)
    SLANGD := 0; VAL := 0; STORT := FALSE; LANG := FALSE;
    SIFFROR;
    SYM := STAL;
    IF (CH = '.') AND SIFFRA(CH2) THEN BEGIN SPARA; SIFFROR; SYM := SREELL END;
    IF (CH = 'E') OR (CH = 'e') THEN
    BEGIN
      CH := 'E'; SPARA;
      IF (CH = '+') OR (CH = '-') THEN SPARA;
      IF NOT SIFFRA(CH) THEN FELLAS(5);
      SIFFROR; SYM := SREELL
    END;
    IF SYM = SREELL THEN BEGIN IF LANG THEN FEL(27); PACKA(SLANGD) END
    ELSE IF STORT THEN FEL(4)
  END
  ELSE IF CH = '''' THEN
  BEGIN
    N := 0;
    REPEAT
      FRAM;
      IF (CH = CHR(0)) OR (CH = CHR(10)) THEN FELLAS(2);
      IF CH = '''' THEN
      BEGIN
        FRAM;
        IF CH <> '''' THEN KLAR := FALSE ELSE KLAR := TRUE
      END
      ELSE KLAR := TRUE;
      IF KLAR THEN
      BEGIN
        IF N = 80 THEN FEL(24);
        N := N + 1; SBUF[N] := CH
      END
    UNTIL NOT KLAR;
    IF N = 1 THEN BEGIN SYM := STECKEN; VAL := ORD(SBUF[1]) END
    ELSE BEGIN SYM := SSTRANG; SLANGD := N END
  END
  ELSE
  BEGIN
    SYM := CHR(0);
    IF CH2 = '=' THEN
    BEGIN
      IF CH = ':' THEN SYM := STILL;
      IF CH = '<' THEN SYM := SLE;
      IF CH = '>' THEN SYM := SGE
    END;
    IF (CH = '<') AND (CH2 = '>') THEN SYM := SNE;
    IF (CH = '.') AND (CH2 = '.') THEN SYM := SPP;
    IF (CH = '(') AND (CH2 = '.') THEN SYM := '[';
    IF (CH = '.') AND (CH2 = ')') THEN SYM := ']';
    IF SYM <> CHR(0) THEN BEGIN FRAM; FRAM END
    ELSE IF (CH = '+') OR (CH = '-') OR (CH = '*') OR (CH = '/') OR (CH = '=')
         OR (CH = '<') OR (CH = '>') OR (CH = '(') OR (CH = ')')
         OR (CH = '[') OR (CH = ']') OR (CH = ',') OR (CH = ';')
         OR (CH = ':') OR (CH = '.') THEN
    BEGIN SYM := CH; FRAM END
    ELSE FELLAS(1)
  END
END;

PROCEDURE VANT(S : CHAR);
BEGIN
  IF SYM <> S THEN FEL(5);
  NASTA
END;

(* -- symboltabellen -------------------------------------------------- *)

FUNCTION TAG : INTEGER;           (* NBUF tas i POOL *)
VAR P : INTEGER;
BEGIN
  P := LAGG;
  POOLTOP := P + (NBUF[0] + 1) DIV 2 + 1;
  TAG := P
END;

FUNCTION NAMNET : INTEGER;        (* namnet tas i POOL *)
BEGIN
  IF SYM <> SNAMN THEN FEL(5);
  NAMNET := TAG;
  NASTA
END;

FUNCTION SOK(P : INTEGER) : INTEGER;   (* och lämnar namnet *)
VAR E : INTEGER;
BEGIN
  E := SOKA(P);
  IF E = 0 THEN FELFOR(7);
  POOLTOP := P;
  SOK := E
END;

PROCEDURE LANKA(E : INTEGER);
VAR H : INTEGER;
BEGIN
  H := HASH(TNAMN[E]);
  TLANK[E] := HUVUD[H];
  HUVUD[H] := E
END;

FUNCTION NY(P, SLAG, TYP, VARDE : INTEGER) : INTEGER;
VAR E : INTEGER;
BEGIN
  E := HUVUD[HASH(P)];
  WHILE E > SCOPE DO
  BEGIN
    IF LIKA(TNAMN[E], P) THEN FELFOR(6);
    E := TLANK[E]
  END;
  IF TOP = TABMAX THEN FEL(26);
  TOP := TOP + 1;
  TNAMN[TOP] := P; TINFO[TOP] := SLAG + 32 * TYP + 2048 * NIVA;
  TVAL[TOP] := VARDE;
  LANKA(TOP);
  NY := TOP
END;

PROCEDURE TOM(S : INTEGER);       (* tar bort namnen över S *)
BEGIN
  WHILE TOP > S DO
  BEGIN
    IF SLAGET(TOP) < 10 THEN HUVUD[HASH(TNAMN[TOP])] := TLANK[TOP];
    TOP := TOP - 1
  END
END;

(* -- koden ----------------------------------------------------------- *)

(* Två bytes i varje ord i KOD: den låga och den höga minus 128, så att
   ordet ryms med tecken. *)
FUNCTION BYTEN(P : INTEGER) : INTEGER;
VAR W, L : INTEGER;
BEGIN
  W := KOD[P DIV 2];
  L := W MOD 256;
  IF L < 0 THEN L := L + 256;
  IF ODD(P) THEN BYTEN := (W - L) DIV 256 + 128 ELSE BYTEN := L
END;

PROCEDURE SATT(P, X : INTEGER);
VAR W, L, H : INTEGER;
BEGIN
  W := KOD[P DIV 2];
  L := W MOD 256;
  IF L < 0 THEN L := L + 256;
  H := (W - L) DIV 256;
  IF ODD(P) THEN H := X - 128 ELSE L := X;
  KOD[P DIV 2] := H * 256 + L
END;

PROCEDURE KB(X : INTEGER);        (* en byte *)
BEGIN
  IF KLEN > KODMAX THEN FEL(26);
  SATT(KLEN, X);
  KLEN := KLEN + 1
END;

PROCEDURE KW(X : INTEGER);        (* ett ord, lägsta byten först *)
BEGIN
  IF X < 0 THEN
  BEGIN
    X := X + 32767 + 1;
    KB(X MOD 256); KB(X DIV 256 + 128)
  END
  ELSE BEGIN KB(X MOD 256); KB(X DIV 256) END
END;

PROCEDURE KR(X : INTEGER);        (* en opkod för REAL *)
BEGIN
  REELL := TRUE;
  KB(X)
END;

(* Talet med texten i POOL[P], och med - före när P är negativt. *)
PROCEDURE LDCR(P : INTEGER);
VAR J, N : INTEGER;
BEGIN
  KR(OLDCR);
  N := POOL[ABS(P)];
  IF P < 0 THEN BEGIN KB(N + 1); KB(ORD('-')); P := -P END ELSE KB(N);
  FOR J := 1 TO N DO
    IF ODD(J) THEN KB(POOL[P + (J + 1) DIV 2] DIV 128)
    ELSE KB(POOL[P + J DIV 2] MOD 128)
END;

PROCEDURE LDC(V : INTEGER);
BEGIN
  IF (V >= 0) AND (V <= 15) THEN KB(V)
  ELSE IF V = -1 THEN KB(OLDCN)
  ELSE IF (V >= 0) AND (V <= 255) THEN BEGIN KB(OLDCB); KB(V) END
  ELSE BEGIN KB(OLDCW); KW(V) END
END;

FUNCTION FRAMAT(O : INTEGER) : INTEGER;   (* hopp med tomt mål *)
BEGIN
  KB(O); KW(0);
  FRAMAT := KLEN - 3
END;

PROCEDURE FYLL(P : INTEGER);
VAR R : INTEGER;
BEGIN
  R := KLEN - (P + 3);
  SATT(P + 1, R MOD 256);
  SATT(P + 2, R DIV 256)
END;

PROCEDURE BAKAT(O, MAL : INTEGER);   (* O är den långa formen *)
VAR R : INTEGER;
BEGIN
  R := MAL - (KLEN + 2);
  IF R >= -128 THEN BEGIN KB(O - 2); KB(R + 256) END
  ELSE BEGIN KB(O); KW(MAL - (KLEN + 2)) END
END;

(* -- typerna och variablerna ----------------------------------------- *)

FUNCTION UTTRYCK : INTEGER; FORWARD;

FUNCTION SLEN(T : INTEGER) : INTEGER;   (* strängens längd, eller 0 *)
BEGIN
  SLEN := 0;
  IF T < 0 THEN SLEN := -T
  ELSE IF T >= FTYP THEN IF FPACK[T] THEN SLEN := FHI[T] - FLO[T] + 1
END;

FUNCTION SAMMASTR(T, U : INTEGER) : BOOLEAN;
BEGIN SAMMASTR := (SLEN(T) > 0) AND (SLEN(T) = SLEN(U)) END;

(* Typen T ska vara ONSKAD; ett INTEGER blir REAL med FLT. *)
PROCEDURE TA(T, ONSKAD : INTEGER);
BEGIN
  IF (T = HEL) AND (ONSKAD = REA) THEN KR(OFLT)
  ELSE IF T <> ONSKAD THEN IF NOT SAMMASTR(T, ONSKAD) THEN FELFOR(8)
END;

FUNCTION SKALAR(T : INTEGER) : BOOLEAN;
BEGIN SKALAR := (T >= HEL) AND (T <= BOO) END;

FUNCTION ARVAR(E : INTEGER) : BOOLEAN;
BEGIN ARVAR := (SLAGET(E) = KVAR) OR (SLAGET(E) = KVARP) END;

FUNCTION ORDI(T : INTEGER) : INTEGER;
BEGIN
  IF T = REA THEN ORDI := 3
  ELSE IF T < FTYP THEN ORDI := 1
  ELSE IF FPACK[T] THEN ORDI := (FHI[T] - FLO[T] + 2) DIV 2
  ELSE ORDI := (FHI[T] - FLO[T] + 1) * ORDI(FELEM[T])
END;

FUNCTION PARORD(NR : INTEGER) : INTEGER;   (* parametrarnas ord *)
VAR J, N : INTEGER;
BEGIN
  N := 0;
  FOR J := PFORST[NR] TO PFORST[NR] + PANT[NR] - 1 DO
    IF PVAR[J] THEN N := N + 1 ELSE N := N + ORDI(PTYP[J]);
  PARORD := N
END;

FUNCTION NYTTORD(N : INTEGER) : INTEGER;
BEGIN
  IF NIVA = 0 THEN BEGIN NYTTORD := GLOBALA; GLOBALA := GLOBALA + N END
  ELSE BEGIN NYTTORD := LOKALA; LOKALA := LOKALA + N END
END;

PROCEDURE LADORD(NV, ADR : INTEGER);
BEGIN
  IF NV = 0 THEN
  BEGIN
    IF ADR < 8 THEN KB(OLDG0 + ADR) ELSE BEGIN KB(OLDG); KW(ADR) END
  END
  ELSE IF NV = NIVA THEN
  BEGIN
    IF ADR < 8 THEN KB(OLDL0 + ADR) ELSE BEGIN KB(OLDL); KB(ADR) END
  END
  ELSE BEGIN KB(OLOD); KB(NIVA - NV); KB(ADR) END
END;

PROCEDURE LAGRAORD(NV, ADR : INTEGER);
BEGIN
  IF NV = 0 THEN BEGIN KB(OSTG); KW(ADR) END
  ELSE IF NV = NIVA THEN
  BEGIN
    IF ADR < 8 THEN KB(OSTL0 + ADR) ELSE BEGIN KB(OSTL); KB(ADR) END
  END
  ELSE BEGIN KB(OSTR); KB(NIVA - NV); KB(ADR) END
END;

PROCEDURE ADRESSEN(E : INTEGER);
VAR B : INTEGER;
BEGIN
  IF SLAGET(E) = KVARP THEN LADORD(NIVAN(E), TVAL[E])
  ELSE IF NIVAN(E) = 0 THEN BEGIN KB(OLGA); KW(TVAL[E]) END
  ELSE
  BEGIN
    B := TVAL[E] + ORDI(TYPEN(E)) - 1;
    IF NIVAN(E) = NIVA THEN BEGIN KB(OLLA); KB(B) END
    ELSE BEGIN KB(OLDA); KB(NIVA - NIVAN(E)); KB(B) END
  END
END;

(* Variabeln E är läst. 0 för en hel variabel (ingen kod än); när
   adressen till ett element ligger på stacken 1 för ett ord, 2 för ett
   tecken i en sträng och 3 för ett REAL. *)
FUNCTION VARIABEL(E : INTEGER; VAR T : INTEGER) : INTEGER;
VAR FT : INTEGER;
BEGIN
  FT := TYPEN(E);
  IF SYM <> '[' THEN BEGIN T := FT; VARIABEL := 0 END
  ELSE
  BEGIN
    IF FT < FTYP THEN FEL(8);
    NASTA;
    ADRESSEN(E);
    IF NOT SKALAR(UTTRYCK) THEN FEL(8);
    KB(OCHK); KW(FLO[FT]); KW(FHI[FT]);
    IF FLO[FT] = 1 THEN KB(ODEC1)
    ELSE IF FLO[FT] <> 0 THEN BEGIN LDC(FLO[FT]); KB(OSUB) END;
    T := FELEM[FT];
    KB(OIXA);
    IF FPACK[FT] THEN BEGIN KW(1); VARIABEL := 2 END
    ELSE
    BEGIN
      KW(2 * ORDI(T));
      IF T = REA THEN VARIABEL := 3 ELSE VARIABEL := 1
    END;
    VANT(']')
  END
END;

(* Ett REAL laddas och lagras genom sin adress (INDR och STOR), och ett
   tecken i en sträng med INDB och STB. *)
PROCEDURE LAD(A, E : INTEGER);
BEGIN
  IF A = 0 THEN IF TYPEN(E) = REA THEN BEGIN ADRESSEN(E); A := 3 END;
  IF A = 3 THEN KR(OINDR)
  ELSE IF A = 2 THEN KB(OINDB)
  ELSE IF A = 1 THEN KB(OIND)
  ELSE IF SLAGET(E) = KVARP THEN BEGIN LADORD(NIVAN(E), TVAL[E]); KB(OIND) END
  ELSE LADORD(NIVAN(E), TVAL[E])
END;

PROCEDURE FORELAGRA(A, E : INTEGER);
BEGIN
  IF A = 0 THEN
    IF (SLAGET(E) = KVARP) OR (TYPEN(E) = REA) THEN ADRESSEN(E)
END;

PROCEDURE LAGRA(A, E : INTEGER);
BEGIN
  IF A = 0 THEN IF TYPEN(E) = REA THEN A := 3;
  IF A = 3 THEN KR(OSTOR)
  ELSE IF A = 2 THEN KB(OSTB)
  ELSE IF (A = 1) OR (SLAGET(E) = KVARP) THEN KB(OSTO)
  ELSE LAGRAORD(NIVAN(E), TVAL[E])
END;

PROCEDURE REFADRESS(A, E : INTEGER);
BEGIN
  IF A = 0 THEN ADRESSEN(E)
END;

FUNCTION KONSTANT(VAR V : INTEGER) : INTEGER;   (* ger typen *)
VAR S, E : INTEGER;
BEGIN
  IF (SYM = '+') OR (SYM = '-') THEN
  BEGIN
    IF SYM = '-' THEN S := -1 ELSE S := 1;
    NASTA;
    KONSTANT := HEL;
    IF SYM = STAL THEN BEGIN V := S * VAL; NASTA END
    ELSE IF SYM = SREELL THEN BEGIN V := S * TAG; NASTA; KONSTANT := REA END
    ELSE
    BEGIN
      E := SOK(NAMNET);
      IF SLAGET(E) <> KKONST THEN FEL(9);
      IF (TYPEN(E) <> HEL) AND (TYPEN(E) <> REA) THEN FEL(9);
      V := S * TVAL[E];
      KONSTANT := TYPEN(E)
    END
  END
  ELSE IF SYM = STAL THEN BEGIN V := VAL; NASTA; KONSTANT := HEL END
  ELSE IF SYM = SREELL THEN BEGIN V := TAG; NASTA; KONSTANT := REA END
  ELSE IF SYM = STECKEN THEN BEGIN V := VAL; NASTA; KONSTANT := TKN END
  ELSE IF SYM = SNAMN THEN
  BEGIN
    E := SOK(NAMNET);
    IF SLAGET(E) <> KKONST THEN FEL(9);
    V := TVAL[E];
    KONSTANT := TYPEN(E)
  END
  ELSE FEL(9)
END;

(* -- anropen och standardprocedurerna -------------------------------- *)

PROCEDURE ANROP(E : INTEGER);
VAR NR, N, A, V, T, AD : INTEGER; KLAR : BOOLEAN;
BEGIN
  NR := TVAL[E];
  IF SLAGET(E) = KFUNK THEN FOR N := 1 TO ORDI(TYPEN(E)) DO KB(0);
  KB(OMST); KB(NIVA - (NIVAN(E) - 1));
  N := 0;
  IF SYM = '(' THEN
  BEGIN
    NASTA;
    KLAR := FALSE;
    REPEAT
      IF N >= PANT[NR] THEN FEL(13);
      A := PFORST[NR] + N;
      IF PVAR[A] THEN
      BEGIN
        IF SYM <> SNAMN THEN FEL(12);
        V := SOK(NAMNET);
        IF NOT ARVAR(V) THEN FEL(12);
        AD := VARIABEL(V, T);
        IF T <> PTYP[A] THEN FEL(8);
        IF AD = 2 THEN FEL(12);       (* ett tecken i en sträng *)
        REFADRESS(AD, V)
      END
      ELSE TA(UTTRYCK, PTYP[A]);
      N := N + 1;
      IF SYM = ',' THEN NASTA ELSE KLAR := TRUE
    UNTIL KLAR;
    VANT(')')
  END;
  IF N <> PANT[NR] THEN FEL(13);
  KB(OCUP); KB(2 * PARORD(NR)); KB(NR)
END;

FUNCTION FILEN : BOOLEAN;   (* är nästa argument en fil: ladda den *)
VAR E : INTEGER; R : BOOLEAN;
BEGIN
  R := FALSE;
  IF SYM = SNAMN THEN
  BEGIN
    E := SOKA(LAGG);
    IF (TYPEN(E) = TXT) AND (ARVAR(E) OR (SLAGET(E) = KSTDFIL)) THEN
    BEGIN
      NASTA;
      IF SLAGET(E) = KSTDFIL THEN KB(0) ELSE LAD(0, E);
      R := TRUE
    END
  END;
  FILEN := R
END;

PROCEDURE FILIGEN;
BEGIN
  IF FILREF = 0 THEN KB(0) ELSE LAD(0, FILREF)
END;

FUNCTION STRANGEN : INTEGER;   (* LSA med strängen eller tecknet *)
VAR J : INTEGER;
BEGIN
  KB(OLSA);
  IF SYM = STECKEN THEN BEGIN KB(1); KB(VAL); STRANGEN := 1 END
  ELSE
  BEGIN
    KB(SLANGD);
    FOR J := 1 TO SLANGD DO KB(ORD(SBUF[J]));
    STRANGEN := SLANGD
  END
END;

(* Ett argument till WRITE; ett REAL har :w:d, och utan d skrivs det
   med exponent. *)
PROCEDURE SKRIVARG;
VAR T, C : INTEGER;
BEGIN
  FILIGEN;
  T := UTTRYCK;
  C := SLEN(T);
  IF C > 0 THEN BEGIN LDC(C); C := 2 END
  ELSE IF T = HEL THEN C := 1 ELSE IF T = TKN THEN C := 0
  ELSE IF T = BOO THEN C := 3 ELSE IF T <> REA THEN FEL(8);
  IF SYM = ':' THEN BEGIN NASTA; TA(UTTRYCK, HEL) END ELSE KB(0);
  IF T = REA THEN
  BEGIN
    IF SYM = ':' THEN BEGIN NASTA; TA(UTTRYCK, HEL) END ELSE KB(OLDCN);
    KR(OWRR)
  END
  ELSE BEGIN KB(OCSP); KB(C) END
END;

PROCEDURE LASARG;
VAR E, T, A : INTEGER;
BEGIN
  IF SYM <> SNAMN THEN FEL(12);
  E := SOK(NAMNET);
  IF NOT ARVAR(E) THEN FEL(12);
  A := VARIABEL(E, T);
  IF (T <> HEL) AND (T <> TKN) AND (T <> REA) THEN FEL(8);
  FORELAGRA(A, E);
  FILIGEN;
  IF T = REA THEN KR(ORDR)
  ELSE BEGIN KB(OCSP); IF T = HEL THEN KB(6) ELSE KB(5) END;
  LAGRA(A, E)
END;

(* N: 1 WRITE, 2 WRITELN, 3 READ, 4 READLN, 5 RESET, 6 REWRITE, 7 CLOSE,
   8 ABS, 9 SQR, 10 ODD, 11 ORD, 12 CHR, 13 SUCC, 14 PRED, 15 EOF,
   16 EOLN, 17 HALT, 18 SQRT, 19 SIN, 20 COS, 21 ARCTAN, 22 LN, 23 EXP,
   24 TRUNC, 25 ROUND. Ger funktionens typ. *)
FUNCTION STANDARD(N : INTEGER; SATSEN : BOOLEAN) : INTEGER;
VAR E, T, Q : INTEGER; FORSTA, KLAR : BOOLEAN;
BEGIN
  STANDARD := 0;
  IF N <= 4 THEN
  BEGIN
    IF NOT SATSEN THEN FEL(8);
    FILREF := 0;
    FORSTA := TRUE;
    IF SYM = '(' THEN
    BEGIN
      NASTA;
      KLAR := FALSE;
      REPEAT
        IF FORSTA AND (SYM = SNAMN) THEN
        BEGIN
          E := SOK(LAGG);
          IF (TYPEN(E) = TXT) AND (ARVAR(E) OR (SLAGET(E) = KSTDFIL)) THEN
          BEGIN
            NASTA;
            IF SLAGET(E) = KSTDFIL THEN FILREF := 0 ELSE FILREF := E;
            IF SYM <> ',' THEN KLAR := TRUE ELSE NASTA
          END
        END;
        FORSTA := FALSE;
        IF NOT KLAR THEN
        BEGIN
          IF N >= 3 THEN LASARG ELSE SKRIVARG;
          IF SYM = ',' THEN NASTA ELSE KLAR := TRUE
        END
      UNTIL KLAR;
      VANT(')')
    END;
    IF N = 2 THEN BEGIN FILIGEN; KB(OCSP); KB(4) END
    ELSE IF N = 4 THEN BEGIN FILIGEN; KB(OCSP); KB(7) END
  END
  ELSE IF N <= 6 THEN
  BEGIN
    IF NOT SATSEN THEN FEL(8);
    VANT('(');
    E := SOK(NAMNET);
    IF NOT ARVAR(E) OR (TYPEN(E) <> TXT) THEN FEL(12);
    ADRESSEN(E);
    VANT(',');
    IF SYM = SNAMN THEN
    BEGIN
      Q := SOK(NAMNET);
      T := TYPEN(Q);
      IF NOT ARVAR(Q) OR (T < FTYP) THEN FEL(5)
      ELSE IF FELEM[T] <> TKN THEN FEL(5);
      ADRESSEN(Q);
      LDC(FHI[T] - FLO[T] + 1);
      KB(OCSP);
      IF FPACK[T] THEN KB(N + 5) ELSE KB(N + 8)
    END
    ELSE
    BEGIN
      IF (SYM <> SSTRANG) AND (SYM <> STECKEN) THEN FEL(5);
      LDC(STRANGEN);
      NASTA;
      KB(OCSP); KB(N + 5)
    END;
    VANT(')')
  END
  ELSE IF N = 7 THEN
  BEGIN
    IF NOT SATSEN THEN FEL(8);
    VANT('(');
    IF NOT FILEN THEN FEL(12);
    KB(OCSP); KB(12);
    VANT(')')
  END
  ELSE IF N = 17 THEN
  BEGIN
    IF NOT SATSEN THEN FEL(8);
    KB(OHLT)
  END
  ELSE
  BEGIN
    IF SATSEN THEN FEL(14);
    IF (N = 15) OR (N = 16) THEN
    BEGIN
      IF SYM = '(' THEN
      BEGIN
        NASTA;
        IF NOT FILEN THEN FEL(12);
        VANT(')')
      END
      ELSE KB(0);
      KB(OCSP);
      IF N = 15 THEN KB(9) ELSE KB(8);
      STANDARD := BOO
    END
    ELSE
    BEGIN
      VANT('(');
      T := UTTRYCK;
      VANT(')');
      IF N = 11 THEN
      BEGIN
        IF NOT SKALAR(T) THEN FEL(8);
        STANDARD := HEL
      END
      ELSE IF N = 12 THEN BEGIN TA(T, HEL); STANDARD := TKN END
      ELSE IF (N = 13) OR (N = 14) THEN
      BEGIN
        IF NOT SKALAR(T) THEN FEL(8);
        IF N = 13 THEN KB(OINC1) ELSE KB(ODEC1);
        STANDARD := T
      END
      ELSE IF N = 10 THEN BEGIN TA(T, HEL); KB(OODD); STANDARD := BOO END
      ELSE IF N >= 24 THEN BEGIN TA(T, REA); KR(OTRC + N - 24); STANDARD := HEL END
      ELSE IF (N <= 9) AND (T = HEL) THEN
      BEGIN
        IF N = 8 THEN KB(OABS) ELSE KB(OSQRI);
        STANDARD := HEL
      END
      ELSE
      BEGIN
        TA(T, REA);
        IF N = 8 THEN KR(OABR) ELSE IF N = 9 THEN KR(OSQRR)
        ELSE KR(OSQT + N - 18);
        STANDARD := REA
      END
    END
  END
END;

(* -- uttrycken ------------------------------------------------------- *)

FUNCTION FAKTOR : INTEGER;
VAR E, T, A : INTEGER;
BEGIN
  IF SYM = STAL THEN BEGIN LDC(VAL); NASTA; FAKTOR := HEL END
  ELSE IF SYM = STECKEN THEN BEGIN LDC(VAL); NASTA; FAKTOR := TKN END
  ELSE IF SYM = SREELL THEN BEGIN LDCR(LAGG); NASTA; FAKTOR := REA END
  ELSE IF SYM = SSTRANG THEN BEGIN FAKTOR := -STRANGEN; NASTA END
  ELSE IF SYM = '(' THEN
  BEGIN
    NASTA;
    FAKTOR := UTTRYCK;
    VANT(')')
  END
  ELSE IF SYM = SNOT THEN
  BEGIN
    NASTA;
    TA(FAKTOR, BOO);
    KB(ONOT);
    FAKTOR := BOO
  END
  ELSE IF SYM = SNAMN THEN
  BEGIN
    E := SOK(NAMNET);
    IF SLAGET(E) = KKONST THEN
    BEGIN
      IF TYPEN(E) = REA THEN LDCR(TVAL[E]) ELSE LDC(TVAL[E]);
      FAKTOR := TYPEN(E)
    END
    ELSE IF ARVAR(E) THEN
    BEGIN
      A := VARIABEL(E, T);
      IF T = TXT THEN FEL(8);
      IF T >= FTYP THEN REFADRESS(A, E) ELSE LAD(A, E);  (* ett fält: adressen *)
      FAKTOR := T
    END
    ELSE IF SLAGET(E) = KFUNK THEN BEGIN ANROP(E); FAKTOR := TYPEN(E) END
    ELSE IF SLAGET(E) = KSTD THEN FAKTOR := STANDARD(TVAL[E], FALSE)
    ELSE FEL(8)
  END
  ELSE FEL(5)
END;

(* Två tal där ett är REAL: båda blir REAL, och TRUE. *)
FUNCTION BLANDAT(T, U : INTEGER) : BOOLEAN;
BEGIN
  BLANDAT := FALSE;
  IF (T = REA) OR (U = REA) THEN
    IF ((T = HEL) OR (T = REA)) AND ((U = HEL) OR (U = REA)) THEN
    BEGIN
      IF U = HEL THEN KR(OFLT);
      IF T = HEL THEN KR(OFLO);
      BLANDAT := TRUE
    END
END;

FUNCTION TERM : INTEGER;
VAR T, U : INTEGER; O : CHAR; B : BOOLEAN;
BEGIN
  T := FAKTOR;
  WHILE (SYM = '*') OR (SYM = '/') OR (SYM = SDIV) OR (SYM = SMOD)
        OR (SYM = SAND) DO
  BEGIN
    O := SYM;
    NASTA;
    U := FAKTOR;
    IF O = SAND THEN BEGIN TA(T, BOO); TA(U, BOO); KB(OAND) END
    ELSE IF O = '/' THEN
    BEGIN
      (* alltid REAL *)
      IF (T = HEL) AND (U = HEL) THEN BEGIN KR(OFLT); KR(OFLO) END
      ELSE IF NOT BLANDAT(T, U) THEN BEGIN TA(T, REA); TA(U, REA) END;
      KR(ODVR);
      T := REA
    END
    ELSE
    BEGIN
      B := FALSE;
      IF O = '*' THEN B := BLANDAT(T, U);
      IF B THEN BEGIN KR(OMPR); T := REA END
      ELSE
      BEGIN
        TA(T, HEL); TA(U, HEL);
        IF O = '*' THEN KB(OMUL) ELSE IF O = SDIV THEN KB(ODIV) ELSE KB(OMOD)
      END
    END
  END;
  TERM := T
END;

FUNCTION ENKELT : INTEGER;
VAR T, U : INTEGER; O, TK : CHAR;
BEGIN
  TK := ' ';
  IF (SYM = '+') OR (SYM = '-') THEN BEGIN TK := SYM; NASTA END;
  T := TERM;
  IF TK <> ' ' THEN
    IF T = REA THEN BEGIN IF TK = '-' THEN KR(ONGR) END
    ELSE BEGIN TA(T, HEL); IF TK = '-' THEN KB(ONEG) END;
  WHILE (SYM = '+') OR (SYM = '-') OR (SYM = SOR) DO
  BEGIN
    O := SYM;
    NASTA;
    U := TERM;
    IF O = SOR THEN BEGIN TA(T, BOO); TA(U, BOO); KB(OOR) END
    ELSE IF BLANDAT(T, U) THEN
    BEGIN
      IF O = '+' THEN KR(OADR) ELSE KR(OSBR);
      T := REA
    END
    ELSE
    BEGIN
      TA(T, HEL); TA(U, HEL);
      IF O = '+' THEN KB(OADD) ELSE KB(OSUB)
    END
  END;
  ENKELT := T
END;

FUNCTION UTTRYCK;
VAR T, U, O : INTEGER;
BEGIN
  T := ENKELT;
  O := 0;
  IF SYM = '=' THEN O := OEQU;
  IF SYM = SNE THEN O := ONEQ;
  IF SYM = '<' THEN O := OLES;
  IF SYM = SLE THEN O := OLEQ;
  IF SYM = '>' THEN O := OGRT;
  IF SYM = SGE THEN O := OGEQ;
  IF O <> 0 THEN
  BEGIN
    NASTA;
    U := ENKELT;
    IF BLANDAT(T, U) THEN KR(OEQR + O - OEQU)
    ELSE IF SAMMASTR(T, U) THEN
    BEGIN
      (* CPS ger -1, 0 eller 1, som jämförs med 0 *)
      KB(OCPS); KW(SLEN(T)); KB(0); KB(O)
    END
    ELSE IF (T <> U) OR NOT SKALAR(T) THEN FEL(8)
    ELSE KB(O);
    T := BOO
  END;
  UTTRYCK := T
END;

(* -- satserna -------------------------------------------------------- *)

PROCEDURE SATS; FORWARD;

PROCEDURE SAMMANSATT;
BEGIN
  VANT(SBEGIN);
  SATS;
  WHILE SYM = ';' DO BEGIN NASTA; SATS END;
  VANT(SEND)
END;

PROCEDURE TILLDELNING(E : INTEGER);
VAR A, T : INTEGER;
BEGIN
  A := VARIABEL(E, T);
  VANT(STILL);
  IF T >= FTYP THEN
  BEGIN
    (* hela fältet eller strängen: MOV till från *)
    REFADRESS(A, E);
    TA(UTTRYCK, T);
    KB(OMOV);
    IF FPACK[T] THEN KW(SLEN(T)) ELSE KW(2 * ORDI(T))
  END
  ELSE
  BEGIN
    FORELAGRA(A, E);
    TA(UTTRYCK, T);
    LAGRA(A, E)
  END
END;

PROCEDURE FORSATS;
VAR E, T, GR, BORJAN, F : INTEGER; UPP : BOOLEAN;
BEGIN
  NASTA;
  E := SOK(NAMNET);
  IF NOT ARVAR(E) OR NOT SKALAR(TYPEN(E)) THEN FEL(12);
  T := TYPEN(E);
  VANT(STILL);
  FORELAGRA(0, E);
  TA(UTTRYCK, T);
  LAGRA(0, E);
  IF (SYM <> STO) AND (SYM <> SDOWNTO) THEN FEL(5);
  UPP := SYM = STO;
  NASTA;
  (* gränsen i ett gömt ord i posten *)
  IF TEMPDJUP = TEMPANT THEN
  BEGIN
    IF TEMPANT > TEMPMAX THEN FEL(26);
    TEMP[TEMPANT] := NYTTORD(1);
    TEMPANT := TEMPANT + 1
  END;
  GR := TEMP[TEMPDJUP];
  TEMPDJUP := TEMPDJUP + 1;
  TA(UTTRYCK, T);
  LAGRAORD(NIVA, GR);
  VANT(SDO);
  BORJAN := KLEN;
  LAD(0, E);
  LADORD(NIVA, GR);
  IF UPP THEN KB(OLEQ) ELSE KB(OGEQ);
  F := FRAMAT(OFJP);
  SATS;
  FORELAGRA(0, E);
  LAD(0, E);
  IF UPP THEN KB(OINC1) ELSE KB(ODEC1);
  LAGRA(0, E);
  BAKAT(OUJP, BORJAN);
  FYLL(F);
  TEMPDJUP := TEMPDJUP - 1
END;

PROCEDURE FALLSATS;
VAR T, KV, TAB, BORJAN, F0, U0, LO, HI, J, K, SLUT, P : INTEGER;
    KLAR, KOMMA : BOOLEAN;
BEGIN
  NASTA;
  T := UTTRYCK;
  IF NOT SKALAR(T) THEN FEL(8);
  VANT(SOF);
  TAB := FRAMAT(OUJP);
  F0 := FALLTOP; U0 := UTTOP;
  KLAR := FALSE;
  REPEAT
    BORJAN := KLEN;
    REPEAT
      TA(KONSTANT(KV), T);
      FOR J := F0 + 1 TO FALLTOP DO IF FVAL[J] = KV THEN FEL(17);
      IF FALLTOP = FALLMAX THEN FEL(26);
      FALLTOP := FALLTOP + 1;
      FVAL[FALLTOP] := KV; FPOS[FALLTOP] := BORJAN;
      KOMMA := SYM = ',';
      IF KOMMA THEN NASTA
    UNTIL NOT KOMMA;
    VANT(':');
    SATS;
    IF UTTOP = FALLMAX THEN FEL(26);
    UTTOP := UTTOP + 1;
    UTPOS[UTTOP] := FRAMAT(OUJP);
    IF SYM <> ';' THEN KLAR := TRUE
    ELSE
    BEGIN
      NASTA;
      IF SYM = SEND THEN KLAR := TRUE
    END
  UNTIL KLAR;
  VANT(SEND);
  FYLL(TAB);
  LO := FVAL[F0 + 1]; HI := LO;
  FOR J := F0 + 2 TO FALLTOP DO
  BEGIN
    IF FVAL[J] < LO THEN LO := FVAL[J];
    IF FVAL[J] > HI THEN HI := FVAL[J]
  END;
  IF LO <= 32512 THEN IF HI > LO + 255 THEN FEL(18);
  SLUT := KLEN + 5 + 2 * (HI - LO + 1);
  KB(OXJP); KW(LO); KW(HI);
  FOR J := 0 TO HI - LO DO
  BEGIN
    P := SLUT;
    FOR K := F0 + 1 TO FALLTOP DO IF FVAL[K] = LO + J THEN P := FPOS[K];
    KW(P - SLUT)
  END;
  KB(OERR); KB(4);
  FOR J := U0 + 1 TO UTTOP DO FYLL(UTPOS[J]);
  FALLTOP := F0; UTTOP := U0
END;

PROCEDURE SATS;
VAR E, F, U, BORJAN, T : INTEGER;
BEGIN
  IF SYM = SNAMN THEN
  BEGIN
    E := SOK(NAMNET);
    IF ARVAR(E) THEN TILLDELNING(E)
    ELSE IF (SLAGET(E) = KFUNK) AND (SYM = STILL) THEN
    BEGIN
      IF NIVAN(E) <> NIVA THEN FEL(22);
      NASTA;
      KB(OLFA);
      TA(UTTRYCK, TYPEN(E));
      IF TYPEN(E) = REA THEN KR(OSTOR) ELSE KB(OSTO)
    END
    ELSE IF SLAGET(E) = KPROC THEN ANROP(E)
    ELSE IF SLAGET(E) = KSTD THEN T := STANDARD(TVAL[E], TRUE)
    ELSE FEL(14)
  END
  ELSE IF SYM = SBEGIN THEN SAMMANSATT
  ELSE IF SYM = SIF THEN
  BEGIN
    NASTA;
    TA(UTTRYCK, BOO);
    VANT(STHEN);
    F := FRAMAT(OFJP);
    SATS;
    IF SYM = SELSE THEN
    BEGIN
      NASTA;
      U := FRAMAT(OUJP);
      FYLL(F);
      SATS;
      FYLL(U)
    END
    ELSE FYLL(F)
  END
  ELSE IF SYM = SWHILE THEN
  BEGIN
    NASTA;
    BORJAN := KLEN;
    TA(UTTRYCK, BOO);
    VANT(SDO);
    F := FRAMAT(OFJP);
    SATS;
    BAKAT(OUJP, BORJAN);
    FYLL(F)
  END
  ELSE IF SYM = SREPEAT THEN
  BEGIN
    NASTA;
    BORJAN := KLEN;
    SATS;
    WHILE SYM = ';' DO BEGIN NASTA; SATS END;
    VANT(SUNTIL);
    TA(UTTRYCK, BOO);
    BAKAT(OFJP, BORJAN)
  END
  ELSE IF SYM = SFOR THEN FORSATS
  ELSE IF SYM = SCASE THEN FALLSATS
END;

(* -- deklarationerna ------------------------------------------------- *)

FUNCTION TYP : INTEGER;
VAR LT, HT, LO, HI, EL, E, J, L : INTEGER; PK : BOOLEAN;
BEGIN
  PK := SYM = SPACKED;
  IF PK THEN BEGIN NASTA; IF SYM <> SARRAY THEN FEL(5) END;
  IF SYM = SARRAY THEN
  BEGIN
    NASTA;
    VANT('[');
    LT := KONSTANT(LO);
    VANT(SPP);
    HT := KONSTANT(HI);
    IF (LT <> HT) OR (LT = REA) OR (HI < LO) THEN FEL(11);
    VANT(']');
    VANT(SOF);
    EL := TYP;
    IF (EL >= FTYP) OR (EL = TXT) THEN FEL(10);
    IF PK AND (EL <> TKN) THEN FELFOR(20);
    L := 16000 DIV ORDI(EL);
    IF LO <= 32767 - L + 1 THEN IF HI > LO + L - 1 THEN FEL(16);
    (* samma gränser, element och PACKED är samma typ *)
    E := 0;
    FOR J := FTYP TO TYPER DO
      IF (FLO[J] = LO) AND (FHI[J] = HI) AND (FELEM[J] = EL)
         AND (FPACK[J] = PK) THEN E := J;
    IF E = 0 THEN
    BEGIN
      IF TYPER = TYPMAX THEN FEL(26);
      TYPER := TYPER + 1;
      FLO[TYPER] := LO; FHI[TYPER] := HI; FELEM[TYPER] := EL;
      FPACK[TYPER] := PK;
      E := TYPER
    END;
    TYP := E
  END
  ELSE
  BEGIN
    E := SOK(NAMNET);
    IF SLAGET(E) <> KTYP THEN FEL(10);
    TYP := TYPEN(E)
  END
END;

PROCEDURE KROPP(NR, NPAR : INTEGER);
VAR E, L, J : INTEGER;
BEGIN
  FOR E := SCOPE + 1 TO TOP DO
    IF (SLAGET(E) = KPROC) OR (SLAGET(E) = KFUNK) THEN
      IF PFORW[TVAL[E]] THEN FEL(19);
  KLEN := 0; TEMPANT := 0; TEMPDJUP := 0;
  KB(OENT); KB(0);
  SAMMANSATT;
  IF NIVA = 0 THEN KB(OHLT)
  ELSE
  BEGIN
    L := LOKALA - NPAR;
    IF L > 255 THEN FEL(15);
    SATT(1, L);
    KB(ORET)
  END;
  (* proceduren är klar och skrivs *)
  POFFS[NR] := UTLEN;
  FOR J := 0 TO KLEN - 1 DO WRITE(G, CHR(BYTEN(J)));
  UTLEN := UTLEN + KLEN
END;

PROCEDURE BLOCK(NR, NPAR : INTEGER); FORWARD;

PROCEDURE PROCEDUR;
VAR FUNK, VP, KLAR, FORW : BOOLEAN;
    N, P, E, J, K, Q, T, NRP, YSCOPE, YLOKALA, YPOOL : INTEGER;
BEGIN
  FUNK := SYM = SFUNCTION;
  NASTA;
  N := NAMNET;
  (* finns proceduren redan, med FORWARD? *)
  P := 0;
  E := HUVUD[HASH(N)];
  WHILE E > SCOPE DO
  BEGIN
    IF (SLAGET(E) = KPROC) OR (SLAGET(E) = KFUNK) THEN
      IF PFORW[TVAL[E]] THEN
        IF LIKA(TNAMN[E], N) THEN P := E;
    E := TLANK[E]
  END;
  YSCOPE := SCOPE; YLOKALA := LOKALA;
  FORW := FALSE;
  IF P = 0 THEN
  BEGIN
    IF FUNK THEN K := KFUNK ELSE K := KPROC;
    IF (PROCS > PROCMAX) OR (NIVA = 15) THEN FEL(26);
    NRP := PROCS;
    PROCS := PROCS + 1;
    P := NY(N, K, 0, NRP);
    TINFO[P] := TINFO[P] + 2048;     (* nivån är NIVA + 1 *)
    PFORST[NRP] := PARTOP + 1; PANT[NRP] := 0; PFORW[NRP] := FALSE;
    NIVA := NIVA + 1;
    SCOPE := TOP;
    YPOOL := POOLTOP;
    LOKALA := 0;
    IF SYM = '(' THEN
    BEGIN
      NASTA;
      KLAR := FALSE;
      REPEAT
        VP := SYM = SVAR;
        IF VP THEN NASTA;
        J := 1;
        NLISTA[1] := NAMNET;
        WHILE SYM = ',' DO
        BEGIN
          NASTA;
          IF J = LISTMAX THEN FEL(26);
          J := J + 1;
          NLISTA[J] := NAMNET
        END;
        VANT(':');
        E := SOK(NAMNET);
        IF SLAGET(E) <> KTYP THEN FEL(10);
        T := TYPEN(E);
        IF (T >= FTYP) AND NOT VP THEN FEL(21);
        IF VP THEN K := KVARP ELSE K := KVAR;
        FOR Q := 1 TO J DO
        BEGIN
          IF PARTOP = PARMAX THEN FEL(26);
          PARTOP := PARTOP + 1;
          PTYP[PARTOP] := T; PVAR[PARTOP] := VP;
          PANT[NRP] := PANT[NRP] + 1;
          E := NY(NLISTA[Q], K, T, LOKALA);
          IF VP THEN LOKALA := LOKALA + 1 ELSE LOKALA := LOKALA + ORDI(T)
        END;
        IF SYM = ';' THEN NASTA ELSE KLAR := TRUE
      UNTIL KLAR;
      VANT(')')
    END;
    IF FUNK THEN
    BEGIN
      VANT(':');
      E := SOK(NAMNET);
      IF (SLAGET(E) <> KTYP) OR (TYPEN(E) >= FTYP) OR (TYPEN(E) = TXT) THEN
        FEL(10);
      TINFO[P] := TINFO[P] + 32 * TYPEN(E)
    END;
    VANT(';');
    IF SYM = SFORWARD THEN
    BEGIN
      NASTA;
      VANT(';');
      FORW := TRUE;
      PFORW[NRP] := TRUE;
      PFPAR[NRP] := SCOPE + 1;
      (* parametrarna blir döda till den riktiga deklarationen; de
         ligger kvar i tabellen och i POOL *)
      FOR J := TOP DOWNTO SCOPE + 1 DO
      BEGIN
        HUVUD[HASH(TNAMN[J])] := TLANK[J];
        TINFO[J] := TINFO[J] + 10
      END;
      NIVA := NIVA - 1;
      SCOPE := YSCOPE; LOKALA := YLOKALA
    END
  END
  ELSE
  BEGIN
    (* den riktiga deklarationen efter FORWARD, utan parametrar *)
    POOLTOP := N;
    NRP := TVAL[P];
    IF FUNK THEN
    BEGIN
      IF SYM = ':' THEN NASTA;
      IF SYM = SNAMN THEN NASTA
    END;
    VANT(';');
    PFORW[NRP] := FALSE;
    NIVA := NIVA + 1;
    SCOPE := TOP;
    YPOOL := POOLTOP;
    FOR J := PFPAR[NRP] TO PFPAR[NRP] + PANT[NRP] - 1 DO
    BEGIN
      IF TOP = TABMAX THEN FEL(26);
      TOP := TOP + 1;
      TNAMN[TOP] := TNAMN[J]; TINFO[TOP] := TINFO[J] - 10;
      TVAL[TOP] := TVAL[J];
      LANKA(TOP)
    END;
    LOKALA := PARORD(NRP)
  END;
  IF NOT FORW THEN
  BEGIN
    BLOCK(NRP, PARORD(NRP));
    VANT(';');
    TOM(SCOPE);
    POOLTOP := YPOOL;
    NIVA := NIVA - 1;
    SCOPE := YSCOPE; LOKALA := YLOKALA
  END
END;

PROCEDURE BLOCK;
VAR P, E, N, T, V, J : INTEGER;
BEGIN
  IF SYM = SCONST THEN
  BEGIN
    NASTA;
    REPEAT
      P := NAMNET;
      VANT('=');
      T := KONSTANT(V);
      E := NY(P, KKONST, T, V);
      VANT(';')
    UNTIL SYM <> SNAMN
  END;
  IF SYM = STYPE THEN
  BEGIN
    NASTA;
    REPEAT
      P := NAMNET;
      VANT('=');
      E := NY(P, KTYP, TYP, 0);
      VANT(';')
    UNTIL SYM <> SNAMN
  END;
  IF SYM = SVAR THEN
  BEGIN
    NASTA;
    REPEAT
      N := 1;
      NLISTA[1] := NAMNET;
      WHILE SYM = ',' DO
      BEGIN
        NASTA;
        IF N = LISTMAX THEN FEL(26);
        N := N + 1;
        NLISTA[N] := NAMNET
      END;
      VANT(':');
      T := TYP;
      FOR J := 1 TO N DO
        E := NY(NLISTA[J], KVAR, T, NYTTORD(ORDI(T)));
      VANT(';')
    UNTIL SYM <> SNAMN
  END;
  WHILE (SYM = SPROCEDURE) OR (SYM = SFUNCTION) DO PROCEDUR;
  KROPP(NR, NPAR)
END;

(* -- de fördefinierade namnen och orden ------------------------------ *)

PROCEDURE ORDEN;
VAR
  W : PACKED ARRAY [1..64] OF CHAR;     (* orden och namnen *)
  WP : INTEGER;

(* K namn i W från WP, skilda med blanka, med slaget SLAG, typen T och
   värdet V; för varje namn ökar typen med DT och värdet med DV. *)
PROCEDURE DEFS(K, SLAG, T, V, DT, DV : INTEGER);
VAR N, P : INTEGER;
BEGIN
  WHILE K > 0 DO
  BEGIN
    WHILE W[WP] = ' ' DO WP := WP + 1;
    N := 0;
    WHILE W[WP] <> ' ' DO BEGIN N := N + 1; ID[N] := W[WP]; WP := WP + 1 END;
    PACKA(N);
    P := LAGG;
    POOLTOP := P + (N + 1) DIV 2 + 1;
    P := NY(P, SLAG, T, V);
    T := T + DT; V := V + DV; K := K - 1
  END
END;

(* Orden har symbolerna 'a' och uppåt i ordning. *)
BEGIN
  W := 'AND ARRAY BEGIN CASE CONST DIV DO DOWNTO ELSE END FOR FORWARD   '; WP := 1;
  DEFS(12, KORD, 0, 97, 0, 1);
  W := 'FUNCTION IF MOD NOT OF OR PROCEDURE PROGRAM REPEAT THEN TO TYPE '; WP := 1;
  DEFS(12, KORD, 0, 109, 0, 1);
  W := 'UNTIL VAR WHILE PACKED WRITE WRITELN READ READLN RESET REWRITE  '; WP := 1;
  DEFS(4, KORD, 0, 121, 0, 1);
  DEFS(6, KSTD, 0, 1, 0, 1);
  W := 'CLOSE ABS SQR ODD ORD CHR SUCC PRED EOF EOLN HALT SQRT SIN COS  '; WP := 1;
  DEFS(14, KSTD, 0, 7, 0, 1);
  W := 'ARCTAN LN EXP TRUNC ROUND FALSE TRUE MAXINT INPUT OUTPUT        '; WP := 1;
  DEFS(5, KSTD, 0, 21, 0, 1);
  DEFS(2, KKONST, BOO, 0, 0, 1);
  DEFS(1, KKONST, HEL, MAXINT, 0, 0);
  DEFS(2, KSTDFIL, TXT, 0, 0, 0);
  W := 'INTEGER CHAR BOOLEAN TEXT REAL                                  '; WP := 1;
  DEFS(5, KTYP, HEL, 0, 1, 0);
END;

PROCEDURE ORDET(W : INTEGER);     (* lägsta byten först *)
BEGIN
  WRITE(G, CHR(W MOD 256), CHR(W DIV 256))
END;

(* -- huvudprogrammet ------------------------------------------------- *)

BEGIN
  (* filnamnen: HEJ.PAS ger HEJ.PK *)
  FOR I := 1 TO 24 DO BEGIN KNAMN[I] := ' '; UNAMN[I] := ' ' END;
  I := 0;
  WHILE NOT EOLN DO
  BEGIN
    READ(C);
    IF I < 20 THEN BEGIN I := I + 1; KNAMN[I] := C END
  END;
  P := 0;
  WHILE (P < I) AND (KNAMN[P + 1] <> '.') DO
  BEGIN
    P := P + 1;
    UNAMN[P] := KNAMN[P]
  END;
  UNAMN[P + 1] := '.'; UNAMN[P + 2] := 'P'; UNAMN[P + 3] := 'K';
  RESET(F, KNAMN);
  REWRITE(G, UNAMN);
  WRITE(G, 'PK', CHR(1), CHR(1));

  FOR I := 0 TO 60 DO HUVUD[I] := 0;
  TINFO[0] := 0;
  TOP := 0; SCOPE := 0; POOLTOP := 1; NIVA := 0; LOKALA := 0;
  TYPER := FTYP - 1; PROCS := 0; PARTOP := 0; GLOBALA := 0;
  UTLEN := 0; FALLTOP := 0; UTTOP := 0; FILREF := 0; REELL := FALSE;
  ORDEN;

  RAD := 1; KOL := 0; SYMRAD := 0; SYMKOL := 0;
  LAS; CH := CH2; LAS;
  NASTA;

  VANT(SPROGRAM);
  P := NAMNET; POOLTOP := P;
  IF SYM = '(' THEN
  BEGIN
    NASTA;
    P := NAMNET; POOLTOP := P;
    WHILE SYM = ',' DO BEGIN NASTA; P := NAMNET; POOLTOP := P END;
    VANT(')')
  END;
  VANT(';');
  SCOPE := TOP;
  PROCS := 1;
  BLOCK(0, 0);
  IF SYM <> '.' THEN FEL(23);

  (* tabellen över procedurerna, de globala och antalet *)
  FOR I := 0 TO PROCS - 1 DO ORDET(POFFS[I]);
  ORDET(2 * GLOBALA);
  IF REELL THEN WRITE(G, CHR(PROCS MOD 256), CHR(PROCS DIV 256 + 128))
  ELSE ORDET(PROCS);
  CLOSE(G)
END.
