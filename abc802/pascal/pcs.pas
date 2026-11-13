PROGRAM PCS(INPUT, OUTPUT);
(* pcs.pas -- kompilatorn för det större språket (PASCAL.md, avsnitt 9),
   skriven i delmängden. Den läser källans namn från tangentbordet, till
   exempel HEJ.PAS, och skriver p-koden till HEJ.PK.

   Den bygger på pc.pas och ger samma bytes som pc.py: RECORD och WITH,
   fält av fält, fält och poster som värdeparametrar, uppräknade typer,
   delomfång med CHK, pekare med NEW, MARK och RELEASE, och mängder.

   Typerna står i en tabell (TK, T1, T2, T3), och fälten i posterna i
   FNAMN, FOFF, FTY och FNXT. Versionsbyten i huvudet blir 2 när koden
   har mängder eller högen, och det vet man först på slutet; koden går
   därför till en fil HEJ.TMP med två bokstäver per byte, och HEJ.PK
   skrivs när källan är läst och stängd. PVM.COM har bara två filer
   öppna åt gången. *)

CONST
  KODMAX = 2559;    (* bytes i en procedurs kod *)
  KODORD = 1279;    (* två bytes i varje ord *)
  TABMAX = 560;     (* poster i symboltabellen *)
  SKRAP = 32;       (* fält i en sats, som poster över TABMAX *)
  TABHI = 592;      (* TABMAX + SKRAP *)
  POOLMAX = 2100;   (* ord för namnen *)
  PROCMAX = 150;    (* procedurer *)
  PARMAX = 250;     (* parametrar i alla procedurer *)
  TYPMAX = 200;     (* typer *)
  FALTMAX = 300;    (* fält i posterna *)
  FALLMAX = 64;     (* fall i CASE i varandra *)
  TEMPMAX = 16;     (* FOR och WITH i varandra *)
  LISTMAX = 64;     (* namn i grupper i varandra *)
  DIMMAX = 8;       (* index i ett fält *)

  (* symbolerna *)
  SAND = 'a'; SARRAY = 'b'; SBEGIN = 'c'; SCASE = 'd'; SCONST = 'e';
  SDIV = 'f'; SDO = 'g'; SDOWNTO = 'h'; SELSE = 'i'; SEND = 'j';
  SFOR = 'k'; SFORWARD = 'l'; SFUNCTION = 'm'; SIF = 'n'; SMOD = 'o';
  SNOT = 'p'; SOF = 'q'; SOR = 'r'; SPROCEDURE = 's'; SPROGRAM = 't';
  SREPEAT = 'u'; STHEN = 'v'; STO = 'w'; STYPE = 'x'; SUNTIL = 'y';
  SVAR = 'z'; SWHILE = '{'; SPACKED = '|';
  SINN = 'A'; SNIL = 'B'; SRECORD = 'C'; SSET = 'D'; SWITH = 'E';
  SNAMN = '1'; STAL = '2'; STECKEN = '3'; SSTRANG = '4'; SSLUT = '5';
  STILL = '6'; SLE = '7'; SGE = '8'; SNE = '9'; SPP = '!'; SREELL = '#';

  (* namnens slag; en död parameter (FORWARD) har slaget + 10 *)
  KKONST = 1; KTYP = 2; KVAR = 3; KVARP = 4; KPROC = 5; KFUNK = 6;
  KSTD = 7; KSTDFIL = 8; KORD = 9;

  (* typerna; de nya är NYTYP och uppåt, och en strängkonstant med
     längden n har typen -n. TOMM är den tomma mängden []. *)
  HEL = 1; TKN = 2; BOO = 3; TXT = 4; REA = 5; NILT = 6; TOMM = 7;
  NYTYP0 = 8;

  (* typernas slag i TK *)
  KFALT = 1; KSTR = 2; KPOST = 3; KMANGD = 4; KPEK = 5; KUPPR = 6;
  KDEL = 7;

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
  OLDSE = 144; OSGS = 145; OSRS = 146; OUNI = 147; OINT = 148;
  ODIF = 149; OINN = 150; OEQS = 151; ONES = 152; OLSS = 153;
  OGSS = 154; OINDS = 155; OSTOS = 156;
  OLDCR = 160; OINDR = 161; OSTOR = 162; OFLT = 163; OFLO = 164;
  OTRC = 165; OADR = 167; OSBR = 168; OMPR = 169; ODVR = 170; ONGR = 171;
  OABR = 172; OSQRR = 173; OEQR = 174; OSQT = 180; OWRR = 186; ORDR = 187;

VAR
  F, G, H : TEXT;
  KNAMN, UNAMN, XNAMN : PACKED ARRAY [1..24] OF CHAR;

  (* läsaren *)
  CH, CH2 : CHAR;                       (* tecknet och nästa *)
  RAD, KOL, SYMRAD, SYMKOL, FORRAD, FORKOL : INTEGER;
  SYM : CHAR;
  VAL : INTEGER;                        (* talet eller tecknet *)
  SBUF : PACKED ARRAY [1..80] OF CHAR;       (* strängen *)
  SLANGD : INTEGER;
  STORT, LANG, REELL, STOR : BOOLEAN;   (* REELL: koden har REAL;
                                           STOR: mängder eller högen *)
  ID : PACKED ARRAY [1..33] OF CHAR;         (* namnet *)
  NBUF : ARRAY [0..17] OF INTEGER;      (* namnet packat *)

  (* namnen *)
  POOL : ARRAY [1..POOLMAX] OF INTEGER;
  POOLTOP : INTEGER;
  HUVUD : ARRAY [0..60] OF INTEGER;
  TNAMN, TINFO, TTYP, TVAL, TLANK, TX : ARRAY [0..TABHI] OF INTEGER;
  (* TINFO är slaget + 32 * nivån. TX är 0, 1 för en post eller ett fält
     som värdeparameter före BLOCK, 2 + ordet med adressen efter, och
     -1 - det gömda ordet med postens adress för ett fält i WITH (TVAL
     är då fältets ord i posten). *)
  TOP, SCOPE, SKTOP : INTEGER;
  NLISTA : ARRAY [1..LISTMAX] OF INTEGER;
  NTOP : INTEGER;

  (* typerna och fälten *)
  TK, T1, T2, T3 : ARRAY [1..TYPMAX] OF INTEGER;
  (* fält: T1 lo, T2 hi, T3 elementet; post: T1 det första fältet, T2
     orden; mängd: T3 grundtypen (0 i TOMM); pekare: T3 typen, och T1
     namnet och T2 = 1 medan den väntar på typen i TYPE; uppräkning: T1
     antalet; delomfång: T1 lo, T2 hi, T3 grundtypen *)
  TYPER : INTEGER;
  OPPNA : BOOLEAN;                      (* i TYPE *)
  FNAMN, FOFF, FTY, FNXT : ARRAY [1..FALTMAX] OF INTEGER;
  FTOP : INTEGER;

  (* procedurerna *)
  PFORST, PANT, PFPAR, POFFS : ARRAY [0..PROCMAX] OF INTEGER;
  PFORW : ARRAY [0..PROCMAX] OF BOOLEAN;
  PTYP : ARRAY [1..PARMAX] OF INTEGER;
  PVAR : ARRAY [1..PARMAX] OF BOOLEAN;
  PARTOP, PROCS, NIVA, LOKALA, GLOBALA : INTEGER;

  (* koden *)
  KOD : ARRAY [0..KODORD] OF INTEGER;  (* byte - 128 högst *)
  KLEN, UTLEN : INTEGER;
  TEMP : ARRAY [0..TEMPMAX] OF INTEGER; (* FOR:s gränser, WITH:s adresser *)
  TEMPANT, TEMPDJUP : INTEGER;
  FVAL, FPOS, UTPOS : ARRAY [1..FALLMAX] OF INTEGER;
  FALLTOP, UTTOP : INTEGER;
  FILREF : INTEGER;                     (* filen i WRITE och READ *)
  I, P : INTEGER;
  C, D : CHAR;

(* -- posterna i symboltabellen --------------------------------------- *)

FUNCTION SLAGET(E : INTEGER) : INTEGER;
BEGIN SLAGET := TINFO[E] MOD 32 END;

FUNCTION NIVAN(E : INTEGER) : INTEGER;
BEGIN NIVAN := TINFO[E] DIV 32 END;

(* -- felen ----------------------------------------------------------- *)

PROCEDURE FELPOS(N, R, K : INTEGER);
BEGIN
  WRITELN('rad ', R, ', kolumn ', K + 1, ': fel ', N);
  CLOSE(H);
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
         OR (CH = ':') OR (CH = '.') OR (CH = '^') THEN
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

(* En ny post utan att pröva namnet: fälten i WITH. *)
PROCEDURE NYA(P, SLAG, TYP, VARDE, NV : INTEGER);
BEGIN
  IF TOP = TABMAX THEN FEL(26);
  TOP := TOP + 1;
  TNAMN[TOP] := P; TINFO[TOP] := SLAG + 32 * NV; TTYP[TOP] := TYP;
  TVAL[TOP] := VARDE; TX[TOP] := 0;
  LANKA(TOP)
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
  NYA(P, SLAG, TYP, VARDE, NIVA);
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

(* Ett fält i en hel post blir en egen variabel i en post över TABMAX,
   som inte finns i hashtabellen och används igen i nästa sats. *)
FUNCTION SKRAPET : INTEGER;
BEGIN
  IF SKTOP = SKRAP THEN FEL(26);
  SKTOP := SKTOP + 1;
  SKRAPET := TABMAX + SKTOP
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

PROCEDURE KS(X : INTEGER);        (* en opkod för mängderna eller högen *)
BEGIN
  STOR := TRUE;
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

(* -- typerna --------------------------------------------------------- *)

FUNCTION UTTRYCK : INTEGER; FORWARD;

FUNCTION KIND(T : INTEGER) : INTEGER;   (* typens slag, 0 för de enkla *)
BEGIN
  IF T < 1 THEN KIND := 0 ELSE KIND := TK[T]
END;

FUNCTION GRUND(T : INTEGER) : INTEGER;
BEGIN
  IF KIND(T) = KDEL THEN GRUND := T3[T] ELSE GRUND := T
END;

FUNCTION ORDINAL(T : INTEGER) : BOOLEAN;
BEGIN
  T := GRUND(T);
  ORDINAL := (T >= HEL) AND (T <= BOO) OR (KIND(T) = KUPPR)
END;

FUNCTION ORDI(T : INTEGER) : INTEGER;
VAR K : INTEGER;
BEGIN
  K := KIND(T);
  IF T = REA THEN ORDI := 3
  ELSE IF T < 0 THEN ORDI := (1 - T) DIV 2
  ELSE IF K = KMANGD THEN ORDI := 16
  ELSE IF K = KSTR THEN ORDI := (T2[T] - T1[T] + 2) DIV 2
  ELSE IF K = KFALT THEN ORDI := (T2[T] - T1[T] + 1) * ORDI(T3[T])
  ELSE IF K = KPOST THEN ORDI := T2[T]
  ELSE ORDI := 1
END;

FUNCTION SLEN(T : INTEGER) : INTEGER;   (* strängens längd, eller 0 *)
BEGIN
  SLEN := 0;
  IF T < 0 THEN SLEN := -T
  ELSE IF KIND(T) = KSTR THEN SLEN := T2[T] - T1[T] + 1
END;

FUNCTION BYTES(T : INTEGER) : INTEGER;
BEGIN
  IF SLEN(T) > 0 THEN BYTES := SLEN(T) ELSE BYTES := 2 * ORDI(T)
END;

FUNCTION SAMMASTR(T, U : INTEGER) : BOOLEAN;
BEGIN SAMMASTR := (SLEN(T) > 0) AND (SLEN(T) = SLEN(U)) END;

(* Ett fält eller en post: tilldelas med MOV och laddas som adressen. *)
FUNCTION BLOCKTYP(T : INTEGER) : BOOLEAN;
VAR K : INTEGER;
BEGIN
  K := KIND(T);
  BLOCKTYP := (T < 0) OR (K = KFALT) OR (K = KSTR) OR (K = KPOST)
END;

(* Samma typ: fält och pekare jämförs led för led, mängder genom
   grundtypen, och de andra är bara sig själva. *)
FUNCTION SAMMA(T, U : INTEGER) : BOOLEAN;
VAR S : BOOLEAN; K : INTEGER;
BEGIN
  S := T = U;
  IF NOT S THEN
  BEGIN
    K := KIND(T);
    IF (K <> 0) AND (K = KIND(U)) THEN
    BEGIN
      IF (K = KFALT) OR (K = KSTR) THEN
      BEGIN
        IF (T1[T] = T1[U]) AND (T2[T] = T2[U]) THEN S := SAMMA(T3[T], T3[U])
      END
      ELSE IF K = KPEK THEN S := SAMMA(T3[T], T3[U])
      ELSE IF K = KMANGD THEN S := T3[T] = T3[U]
    END
  END;
  SAMMA := S
END;

(* Två mängder som passar ihop: mängdtypen, annars 0. *)
FUNCTION MPAR(T, U : INTEGER) : INTEGER;
BEGIN
  MPAR := 0;
  IF (KIND(T) = KMANGD) AND (KIND(U) = KMANGD) THEN
    IF T3[T] = 0 THEN MPAR := U
    ELSE IF (T3[U] = 0) OR (T3[U] = T3[T]) THEN MPAR := T
END;

FUNCTION NYTYP(K : INTEGER) : INTEGER;
BEGIN
  IF TYPER = TYPMAX THEN FEL(26);
  TYPER := TYPER + 1;
  TK[TYPER] := K; T1[TYPER] := 0; T2[TYPER] := 0; T3[TYPER] := 0;
  NYTYP := TYPER
END;

(* Typen med slaget K och T1, T2, T3, en gammal om den finns. *)
FUNCTION GAMMAL(K, A, B, C : INTEGER) : INTEGER;
VAR J, E : INTEGER;
BEGIN
  E := 0;
  FOR J := NYTYP0 TO TYPER DO
    IF (TK[J] = K) AND (T1[J] = A) AND (T2[J] = B) AND (T3[J] = C) THEN
      E := J;
  IF E = 0 THEN
  BEGIN
    E := NYTYP(K);
    T1[E] := A; T2[E] := B; T3[E] := C
  END;
  GAMMAL := E
END;

FUNCTION SETTYP(EL : INTEGER) : INTEGER;
BEGIN
  IF EL = 0 THEN SETTYP := TOMM ELSE SETTYP := GAMMAL(KMANGD, 0, 0, EL)
END;

(* Typen T ska vara ONSKAD; ett INTEGER blir REAL med FLT. *)
PROCEDURE TA(T, ONSKAD : INTEGER);
BEGIN
  IF (T = HEL) AND (ONSKAD = REA) THEN KR(OFLT)
  ELSE IF NOT SAMMA(T, ONSKAD) THEN IF NOT SAMMASTR(T, ONSKAD) THEN
    IF NOT ((KIND(ONSKAD) = KDEL) AND (T3[ONSKAD] = T)) THEN
      IF NOT ((T = NILT) AND (KIND(ONSKAD) = KPEK)) THEN
        IF MPAR(T, ONSKAD) = 0 THEN FELFOR(8)
END;

(* Som TA, när värdet ska till en variabel: ett delomfång prövas. *)
PROCEDURE TATILL(T, ONSKAD : INTEGER);
BEGIN
  TA(T, ONSKAD);
  IF KIND(ONSKAD) = KDEL THEN BEGIN KB(OCHK); KW(T1[ONSKAD]); KW(T2[ONSKAD]) END
END;

(* -- variablerna ----------------------------------------------------- *)

FUNCTION ARVAR(E : INTEGER) : BOOLEAN;
BEGIN ARVAR := (SLAGET(E) = KVAR) OR (SLAGET(E) = KVARP) END;

FUNCTION PARORD(NR : INTEGER) : INTEGER;   (* parametrarnas ord *)
VAR J, N : INTEGER;
BEGIN
  N := 0;
  FOR J := PFORST[NR] TO PFORST[NR] + PANT[NR] - 1 DO
    IF PVAR[J] OR BLOCKTYP(PTYP[J]) THEN N := N + 1
    ELSE N := N + ORDI(PTYP[J]);
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

PROCEDURE FORSKJUT(OFF : INTEGER);   (* adressen OFF ord fram *)
BEGIN
  IF OFF <> 0 THEN BEGIN LDC(2 * OFF); KB(OADD) END
END;

PROCEDURE ADRESSEN(E : INTEGER);
VAR B : INTEGER;
BEGIN
  IF TX[E] < 0 THEN BEGIN LADORD(NIVAN(E), -1 - TX[E]); FORSKJUT(TVAL[E]) END
  ELSE IF SLAGET(E) = KVARP THEN LADORD(NIVAN(E), TVAL[E])
  ELSE IF NIVAN(E) = 0 THEN BEGIN KB(OLGA); KW(TVAL[E]) END
  ELSE
  BEGIN
    B := TVAL[E] + ORDI(TTYP[E]) - 1;
    IF NIVAN(E) = NIVA THEN BEGIN KB(OLLA); KB(B) END
    ELSE BEGIN KB(OLDA); KB(NIVA - NIVAN(E)); KB(B) END
  END
END;

PROCEDURE REFADRESS(A, E : INTEGER);
BEGIN
  IF A = 0 THEN ADRESSEN(E)
END;

(* A är 0 för en hel variabel E (ingen kod än), 1 när adressen ligger
   på stacken och 2 för ett tecken i en sträng; T är typen. Ett REAL
   laddas och lagras genom sin adress (INDR och STOR), en mängd med INDS
   och STOS, och ett tecken i en sträng med INDB och STB. *)
PROCEDURE LAD(A, E, T : INTEGER);
BEGIN
  IF T = REA THEN BEGIN REFADRESS(A, E); KR(OINDR) END
  ELSE IF KIND(T) = KMANGD THEN BEGIN REFADRESS(A, E); KS(OINDS) END
  ELSE IF A = 2 THEN KB(OINDB)
  ELSE IF A = 1 THEN KB(OIND)
  ELSE IF SLAGET(E) = KVARP THEN BEGIN LADORD(NIVAN(E), TVAL[E]); KB(OIND) END
  ELSE LADORD(NIVAN(E), TVAL[E])
END;

PROCEDURE FORELAGRA(A, E, T : INTEGER);
BEGIN
  IF A = 0 THEN
    IF (SLAGET(E) = KVARP) OR (T = REA) OR (KIND(T) = KMANGD) THEN
      ADRESSEN(E)
END;

PROCEDURE LAGRA(A, E, T : INTEGER);
BEGIN
  IF T = REA THEN KR(OSTOR)
  ELSE IF KIND(T) = KMANGD THEN KS(OSTOS)
  ELSE IF A = 2 THEN KB(OSTB)
  ELSE IF (A = 1) OR (SLAGET(E) = KVARP) THEN KB(OSTO)
  ELSE LAGRAORD(NIVAN(E), TVAL[E])
END;

(* Fältet F i posten med P i POOL, eller 0. *)
FUNCTION FALTET(T, P : INTEGER) : INTEGER;
VAR F : INTEGER; FUNNEN : BOOLEAN;
BEGIN
  F := T1[T];
  FUNNEN := FALSE;
  WHILE (F <> 0) AND NOT FUNNEN DO
    IF LIKA(FNAMN[F], P) THEN FUNNEN := TRUE ELSE F := FNXT[F];
  FALTET := F
END;

(* Ordet för fältet F i den hela posten E, som inte är en VAR. *)
FUNCTION FADR(E, F : INTEGER) : INTEGER;
BEGIN
  IF NIVAN(E) = 0 THEN FADR := TVAL[E] + FOFF[F]
  ELSE FADR := TVAL[E] + ORDI(TTYP[E]) - FOFF[F] - ORDI(FTY[F])
END;

(* Variabeln E är läst; efter namnet kan [i, j], .fält och ^ följa i
   vilken ordning som helst. Ger A som i LAD, och E och T för det som
   ska laddas eller lagras. *)
FUNCTION VARIABEL(VAR E, T : INTEGER) : INTEGER;
VAR A, FT, F, P, S : INTEGER; KLAR : BOOLEAN;
BEGIN
  A := 0;
  T := TTYP[E];
  IF TX[E] < 0 THEN BEGIN ADRESSEN(E); A := 1 END;
  WHILE (SYM = '[') OR (SYM = '.') OR (SYM = '^') DO
    IF SYM = '[' THEN
    BEGIN
      IF (KIND(T) <> KFALT) AND (KIND(T) <> KSTR) THEN FEL(8);
      NASTA;
      IF A = 0 THEN ADRESSEN(E);
      KLAR := FALSE;
      REPEAT
        FT := T;
        IF NOT ORDINAL(UTTRYCK) THEN FEL(8);
        KB(OCHK); KW(T1[FT]); KW(T2[FT]);
        IF T1[FT] = 1 THEN KB(ODEC1)
        ELSE IF T1[FT] <> 0 THEN BEGIN LDC(T1[FT]); KB(OSUB) END;
        T := T3[FT];
        KB(OIXA);
        IF TK[FT] = KSTR THEN BEGIN KW(1); A := 2 END
        ELSE BEGIN KW(2 * ORDI(T)); A := 1 END;
        IF SYM <> ',' THEN KLAR := TRUE
        ELSE
        BEGIN
          NASTA;
          IF (KIND(T) <> KFALT) AND (KIND(T) <> KSTR) THEN FEL(8)
        END
      UNTIL KLAR;
      VANT(']')
    END
    ELSE IF SYM = '.' THEN
    BEGIN
      IF KIND(T) <> KPOST THEN FEL(8);
      NASTA;
      P := NAMNET;
      F := FALTET(T, P);
      IF F = 0 THEN FELFOR(7);
      POOLTOP := P;
      IF (A = 0) AND (SLAGET(E) <> KVARP) THEN
      BEGIN
        S := SKRAPET;
        TNAMN[S] := FNAMN[F]; TINFO[S] := KVAR + 32 * NIVAN(E);
        TTYP[S] := FTY[F]; TVAL[S] := FADR(E, F); TX[S] := 0;
        E := S
      END
      ELSE
      BEGIN
        IF A = 0 THEN ADRESSEN(E);
        FORSKJUT(FOFF[F]);
        A := 1
      END;
      T := FTY[F]
    END
    ELSE
    BEGIN
      IF KIND(T) <> KPEK THEN FEL(8);
      NASTA;
      IF A = 0 THEN LAD(A, E, T) ELSE KB(OIND);
      A := 1;
      T := T3[T]
    END;
  VARIABEL := A
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
      IF (TTYP[E] <> HEL) AND (TTYP[E] <> REA) THEN FEL(9);
      V := S * TVAL[E];
      KONSTANT := TTYP[E]
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
    KONSTANT := TTYP[E]
  END
  ELSE FEL(9)
END;

(* Ett gömt ord i posten, för FOR:s gräns och WITH:s adress. *)
FUNCTION TEMPORD : INTEGER;
BEGIN
  IF TEMPDJUP = TEMPANT THEN
  BEGIN
    IF TEMPANT > TEMPMAX THEN FEL(26);
    TEMP[TEMPANT] := NYTTORD(1);
    TEMPANT := TEMPANT + 1
  END;
  TEMPORD := TEMP[TEMPDJUP];
  TEMPDJUP := TEMPDJUP + 1
END;

(* -- anropen och standardprocedurerna -------------------------------- *)

PROCEDURE ANROP(E : INTEGER);
VAR NR, N, A, V, T, AD : INTEGER; KLAR : BOOLEAN;
BEGIN
  NR := TVAL[E];
  IF SLAGET(E) = KFUNK THEN FOR N := 1 TO ORDI(TTYP[E]) DO KB(0);
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
        IF NOT SAMMA(T, PTYP[A]) THEN FEL(8);
        IF AD = 2 THEN FEL(12);       (* ett tecken i en sträng *)
        REFADRESS(AD, V)
      END
      ELSE TATILL(UTTRYCK, PTYP[A]);
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
    IF (TTYP[E] = TXT) AND (ARVAR(E) OR (SLAGET(E) = KSTDFIL)) THEN
    BEGIN
      NASTA;
      IF SLAGET(E) = KSTDFIL THEN KB(0) ELSE LAD(0, E, TXT);
      R := TRUE
    END
  END;
  FILEN := R
END;

PROCEDURE FILIGEN;
BEGIN
  IF FILREF = 0 THEN KB(0) ELSE LAD(0, FILREF, TXT)
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
VAR E, T, A, G : INTEGER;
BEGIN
  IF SYM <> SNAMN THEN FEL(12);
  E := SOK(NAMNET);
  IF NOT ARVAR(E) THEN FEL(12);
  A := VARIABEL(E, T);
  G := GRUND(T);
  IF (G <> HEL) AND (G <> TKN) AND (G <> REA) THEN FEL(8);
  FORELAGRA(A, E, T);
  FILIGEN;
  IF G = REA THEN KR(ORDR)
  ELSE BEGIN KB(OCSP); IF G = HEL THEN KB(6) ELSE KB(5) END;
  IF KIND(T) = KDEL THEN BEGIN KB(OCHK); KW(T1[T]); KW(T2[T]) END;
  LAGRA(A, E, T)
END;

(* N: 1 WRITE, 2 WRITELN, 3 READ, 4 READLN, 5 RESET, 6 REWRITE, 7 CLOSE,
   8 ABS, 9 SQR, 10 ODD, 11 ORD, 12 CHR, 13 SUCC, 14 PRED, 15 EOF,
   16 EOLN, 17 HALT, 18 SQRT, 19 SIN, 20 COS, 21 ARCTAN, 22 LN, 23 EXP,
   24 TRUNC, 25 ROUND, 26 NEW, 27 MARK, 28 RELEASE. Ger funktionens
   typ. *)
FUNCTION STANDARD(N : INTEGER; SATSEN : BOOLEAN) : INTEGER;
VAR E, T, Q, A : INTEGER; FORSTA, KLAR : BOOLEAN;
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
          IF (TTYP[E] = TXT) AND (ARVAR(E) OR (SLAGET(E) = KSTDFIL)) THEN
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
    IF NOT ARVAR(E) OR (TTYP[E] <> TXT) THEN FEL(12);
    ADRESSEN(E);
    VANT(',');
    IF SYM = SNAMN THEN
    BEGIN
      Q := SOK(NAMNET);
      T := TTYP[Q];
      IF NOT ARVAR(Q) OR (KIND(T) <> KFALT) AND (KIND(T) <> KSTR) THEN FEL(5)
      ELSE IF T3[T] <> TKN THEN FEL(5);
      ADRESSEN(Q);
      LDC(T2[T] - T1[T] + 1);
      KB(OCSP);
      IF KIND(T) = KSTR THEN KB(N + 5) ELSE KB(N + 8)
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
  ELSE IF N >= 26 THEN
  BEGIN
    (* högen, bara i den stora versionen *)
    IF NOT SATSEN THEN FEL(8);
    VANT('(');
    IF SYM <> SNAMN THEN FEL(12);
    E := SOK(NAMNET);
    IF NOT ARVAR(E) THEN FEL(12);
    A := VARIABEL(E, T);
    IF KIND(T) <> KPEK THEN FELFOR(8);
    IF N = 28 THEN BEGIN LAD(A, E, T); KB(OCSP); KS(17) END
    ELSE
    BEGIN
      REFADRESS(A, E);
      IF N = 26 THEN BEGIN LDC(2 * ORDI(T3[T])); KB(OCSP); KS(15) END
      ELSE BEGIN KB(OCSP); KS(16) END
    END;
    VANT(')')
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
        IF NOT ORDINAL(T) THEN FEL(8);
        STANDARD := HEL
      END
      ELSE IF N = 12 THEN BEGIN TA(T, HEL); STANDARD := TKN END
      ELSE IF (N = 13) OR (N = 14) THEN
      BEGIN
        IF NOT ORDINAL(T) THEN FEL(8);
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

(* [a, b..c]: SGS för ett element, SRS för lo..hi, och UNI. *)
FUNCTION MANGD : INTEGER;
VAR T, EL : INTEGER; FORSTA, KLAR : BOOLEAN;
BEGIN
  NASTA;
  IF SYM = ']' THEN BEGIN NASTA; KS(OLDSE); MANGD := TOMM END
  ELSE
  BEGIN
    EL := 0; FORSTA := TRUE; KLAR := FALSE;
    REPEAT
      T := UTTRYCK;
      IF NOT ORDINAL(T) THEN FELFOR(8);
      IF (EL <> 0) AND (EL <> T) THEN FELFOR(8);
      EL := T;
      IF SYM = SPP THEN BEGIN NASTA; TA(UTTRYCK, T); KS(OSRS) END
      ELSE KS(OSGS);
      IF NOT FORSTA THEN KS(OUNI);
      FORSTA := FALSE;
      IF SYM = ',' THEN NASTA ELSE KLAR := TRUE
    UNTIL KLAR;
    VANT(']');
    MANGD := SETTYP(EL)
  END
END;

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
  ELSE IF SYM = SNIL THEN BEGIN NASTA; KB(0); FAKTOR := NILT END
  ELSE IF SYM = '[' THEN FAKTOR := MANGD
  ELSE IF SYM = SNAMN THEN
  BEGIN
    E := SOK(NAMNET);
    IF SLAGET(E) = KKONST THEN
    BEGIN
      IF TTYP[E] = REA THEN LDCR(TVAL[E]) ELSE LDC(TVAL[E]);
      FAKTOR := TTYP[E]
    END
    ELSE IF ARVAR(E) THEN
    BEGIN
      A := VARIABEL(E, T);
      IF T = TXT THEN FEL(8);
      (* ett fält eller en post: adressen *)
      IF BLOCKTYP(T) THEN REFADRESS(A, E) ELSE LAD(A, E, T);
      FAKTOR := GRUND(T)
    END
    ELSE IF SLAGET(E) = KFUNK THEN BEGIN ANROP(E); FAKTOR := GRUND(TTYP[E]) END
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
VAR T, U, M : INTEGER; O : CHAR; B : BOOLEAN;
BEGIN
  T := FAKTOR;
  WHILE (SYM = '*') OR (SYM = '/') OR (SYM = SDIV) OR (SYM = SMOD)
        OR (SYM = SAND) DO
  BEGIN
    O := SYM;
    NASTA;
    U := FAKTOR;
    IF O = SAND THEN BEGIN TA(T, BOO); TA(U, BOO); KB(OAND) END
    ELSE IF (O = '*') AND (KIND(T) = KMANGD) THEN
    BEGIN
      M := MPAR(T, U);
      IF M = 0 THEN FELFOR(8);
      KS(OINT);
      T := M
    END
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
VAR T, U, M : INTEGER; O, TK : CHAR;
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
    ELSE IF KIND(T) = KMANGD THEN
    BEGIN
      M := MPAR(T, U);
      IF M = 0 THEN FELFOR(8);
      IF O = '+' THEN KS(OUNI) ELSE KS(ODIF);
      T := M
    END
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
VAR T, U, O, I : INTEGER;
BEGIN
  T := ENKELT;
  IF SYM = SINN THEN
  BEGIN
    NASTA;
    U := ENKELT;
    IF NOT ORDINAL(T) OR (KIND(U) <> KMANGD) THEN FELFOR(8);
    IF (T3[U] <> 0) AND (T3[U] <> T) THEN FELFOR(8);
    KS(OINN);
    T := BOO
  END
  ELSE
  BEGIN
    I := -1;
    IF SYM = '=' THEN I := 0;
    IF SYM = SNE THEN I := 1;
    IF SYM = '<' THEN I := 2;
    IF SYM = SLE THEN I := 3;
    IF SYM = '>' THEN I := 4;
    IF SYM = SGE THEN I := 5;
    IF I >= 0 THEN
    BEGIN
      O := OEQU + I;
      NASTA;
      U := ENKELT;
      IF MPAR(T, U) <> 0 THEN
      BEGIN
        (* = <> <= >=; <= är "ingår i" *)
        IF (I = 2) OR (I = 4) THEN FELFOR(8);
        IF I = 0 THEN KS(OEQS) ELSE IF I = 1 THEN KS(ONES)
        ELSE IF I = 3 THEN KS(OLSS) ELSE KS(OGSS)
      END
      ELSE IF ((KIND(T) = KPEK) OR (T = NILT))
              AND ((KIND(U) = KPEK) OR (U = NILT)) THEN
      BEGIN
        IF I > 1 THEN FELFOR(8);
        IF NOT SAMMA(T, U) AND (T <> NILT) AND (U <> NILT) THEN FELFOR(8);
        KB(O)
      END
      ELSE IF BLANDAT(T, U) THEN KR(OEQR + I)
      ELSE IF SAMMASTR(T, U) THEN
      BEGIN
        (* CPS ger -1, 0 eller 1, som jämförs med 0 *)
        KB(OCPS); KW(SLEN(T)); KB(0); KB(O)
      END
      ELSE IF NOT SAMMA(T, U) OR NOT ORDINAL(T) THEN FEL(8)
      ELSE KB(O);
      T := BOO
    END
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
  IF BLOCKTYP(T) THEN
  BEGIN
    (* hela fältet, strängen eller posten: MOV till från *)
    REFADRESS(A, E);
    TA(UTTRYCK, T);
    KB(OMOV); KW(BYTES(T))
  END
  ELSE
  BEGIN
    FORELAGRA(A, E, T);
    TATILL(UTTRYCK, T);
    LAGRA(A, E, T)
  END
END;

PROCEDURE FORSATS;
VAR E, T, GR, BORJAN, F : INTEGER; UPP : BOOLEAN;
BEGIN
  NASTA;
  E := SOK(NAMNET);
  IF NOT ARVAR(E) OR NOT ORDINAL(TTYP[E]) OR (TX[E] < 0) THEN FEL(12);
  T := TTYP[E];
  VANT(STILL);
  FORELAGRA(0, E, T);
  TA(UTTRYCK, T);
  LAGRA(0, E, T);
  IF (SYM <> STO) AND (SYM <> SDOWNTO) THEN FEL(5);
  UPP := SYM = STO;
  NASTA;
  (* gränsen i ett gömt ord i posten *)
  GR := TEMPORD;
  TA(UTTRYCK, T);
  LAGRAORD(NIVA, GR);
  VANT(SDO);
  BORJAN := KLEN;
  LAD(0, E, T);
  LADORD(NIVA, GR);
  IF UPP THEN KB(OLEQ) ELSE KB(OGEQ);
  F := FRAMAT(OFJP);
  SATS;
  FORELAGRA(0, E, T);
  LAD(0, E, T);
  IF UPP THEN KB(OINC1) ELSE KB(ODEC1);
  LAGRA(0, E, T);
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
  IF NOT ORDINAL(T) THEN FEL(8);
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

(* WITH p, q DO sats: postens fält blir namn under satsen. En hel post
   får fälten som egna variabler; annars ligger postens adress i ett
   gömt ord. *)
PROCEDURE MEDSATS;
VAR N0, D0, E, T, A, F, G : INTEGER; KOMMA : BOOLEAN;
BEGIN
  NASTA;
  N0 := TOP; D0 := TEMPDJUP;
  REPEAT
    IF SYM <> SNAMN THEN FEL(12);
    E := SOK(NAMNET);
    IF NOT ARVAR(E) THEN FEL(12);
    A := VARIABEL(E, T);
    IF KIND(T) <> KPOST THEN FELFOR(8);
    F := T1[T];
    IF (A = 0) AND (SLAGET(E) <> KVARP) THEN
      WHILE F <> 0 DO
      BEGIN
        NYA(FNAMN[F], KVAR, FTY[F], FADR(E, F), NIVAN(E));
        F := FNXT[F]
      END
    ELSE
    BEGIN
      IF A = 0 THEN ADRESSEN(E);
      G := TEMPORD;
      LAGRAORD(NIVA, G);
      WHILE F <> 0 DO
      BEGIN
        NYA(FNAMN[F], KVAR, FTY[F], FOFF[F], NIVA);
        TX[TOP] := -1 - G;
        F := FNXT[F]
      END
    END;
    KOMMA := SYM = ',';
    IF KOMMA THEN NASTA
  UNTIL NOT KOMMA;
  VANT(SDO);
  SATS;
  TOM(N0);
  TEMPDJUP := D0
END;

PROCEDURE SATS;
VAR E, F, U, BORJAN, T : INTEGER;
BEGIN
  SKTOP := 0;
  IF SYM = SNAMN THEN
  BEGIN
    E := SOK(NAMNET);
    IF ARVAR(E) THEN TILLDELNING(E)
    ELSE IF (SLAGET(E) = KFUNK) AND (SYM = STILL) THEN
    BEGIN
      IF NIVAN(E) <> NIVA THEN FEL(22);
      NASTA;
      KB(OLFA);
      T := TTYP[E];
      TATILL(UTTRYCK, T);
      IF T = REA THEN KR(OSTOR)
      ELSE IF KIND(T) = KMANGD THEN KS(OSTOS)
      ELSE KB(OSTO)
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
  ELSE IF SYM = SWITH THEN MEDSATS
END;

(* -- deklarationerna ------------------------------------------------- *)

PROCEDURE NAMNIN;                 (* namnet till NLISTA *)
BEGIN
  IF NTOP = LISTMAX THEN FEL(26);
  NTOP := NTOP + 1;
  NLISTA[NTOP] := NAMNET
END;

PROCEDURE NAMNLISTA;              (* a, b, c *)
BEGIN
  NAMNIN;
  WHILE SYM = ',' DO BEGIN NASTA; NAMNIN END
END;

(* Den ordinala typens lo och hi, för ett index och en mängd. *)
PROCEDURE GRANSER(T : INTEGER; VAR LO, HI : INTEGER);
VAR G : INTEGER;
BEGIN
  G := GRUND(T);
  IF KIND(T) = KDEL THEN BEGIN LO := T1[T]; HI := T2[T] END
  ELSE IF G = BOO THEN BEGIN LO := 0; HI := 1 END
  ELSE IF G = TKN THEN BEGIN LO := 0; HI := 255 END
  ELSE IF KIND(G) = KUPPR THEN BEGIN LO := 0; HI := T1[G] - 1 END
  ELSE FELFOR(11)
END;

(* lo..hi med konstanter, eller namnet på en ordinal typ; ger
   grundtypen. *)
FUNCTION OMFANG(VAR LO, HI : INTEGER) : INTEGER;
VAR E, LT, HT : INTEGER;
BEGIN
  E := 0;
  IF SYM = SNAMN THEN
  BEGIN
    E := SOKA(LAGG);
    IF SLAGET(E) <> KTYP THEN E := 0
  END;
  IF E <> 0 THEN
  BEGIN
    NASTA;
    GRANSER(TTYP[E], LO, HI);
    OMFANG := GRUND(TTYP[E])
  END
  ELSE
  BEGIN
    LT := KONSTANT(LO);
    VANT(SPP);
    HT := KONSTANT(HI);
    IF (LT <> HT) OR (LT = TXT) OR (LT = REA) OR (HI < LO) THEN FEL(11);
    OMFANG := LT
  END
END;

FUNCTION TYP : INTEGER;
VAR LO, HI, EL, E, J, L, N, N0, U, SIST : INTEGER; PK, P2, KLAR : BOOLEAN;
    GL, GH : ARRAY [1..DIMMAX] OF INTEGER;
BEGIN
  PK := SYM = SPACKED;
  IF PK THEN
  BEGIN
    NASTA;
    IF (SYM <> SARRAY) AND (SYM <> SRECORD) AND (SYM <> SSET) THEN FEL(5)
  END;
  IF SYM = SARRAY THEN
  BEGIN
    NASTA;
    VANT('[');
    N := 0;
    KLAR := FALSE;
    REPEAT
      IF N = DIMMAX THEN FEL(26);
      N := N + 1;
      U := OMFANG(GL[N], GH[N]);
      IF SYM = ',' THEN NASTA ELSE KLAR := TRUE
    UNTIL KLAR;
    VANT(']');
    VANT(SOF);
    EL := TYP;
    IF EL = TXT THEN FEL(10);
    (* ARRAY [a, b] OF T är ARRAY [a] OF ARRAY [b] OF T; PACKED gäller
       det innersta *)
    FOR J := N DOWNTO 1 DO
    BEGIN
      LO := GL[J]; HI := GH[J];
      P2 := PK AND (J = N);
      IF P2 AND (EL <> TKN) THEN
      BEGIN
        IF SLEN(EL) = 0 THEN FELFOR(20);
        P2 := FALSE
      END;
      L := 16000 DIV ORDI(EL);
      IF LO <= 32767 - L + 1 THEN IF HI > LO + L - 1 THEN FEL(16);
      IF P2 THEN EL := GAMMAL(KSTR, LO, HI, EL)
      ELSE EL := GAMMAL(KFALT, LO, HI, EL)
    END;
    TYP := EL
  END
  ELSE IF SYM = SRECORD THEN
  BEGIN
    NASTA;
    E := NYTYP(KPOST);
    SIST := 0;
    KLAR := SYM <> SNAMN;
    WHILE NOT KLAR DO
    BEGIN
      N0 := NTOP;
      NAMNLISTA;
      VANT(':');
      U := TYP;
      IF U = TXT THEN FELFOR(10);
      FOR J := N0 + 1 TO NTOP DO
      BEGIN
        IF FALTET(E, NLISTA[J]) <> 0 THEN FELFOR(6);
        IF FTOP = FALTMAX THEN FEL(26);
        FTOP := FTOP + 1;
        FNAMN[FTOP] := NLISTA[J]; FOFF[FTOP] := T2[E]; FTY[FTOP] := U;
        FNXT[FTOP] := 0;
        IF SIST = 0 THEN T1[E] := FTOP ELSE FNXT[SIST] := FTOP;
        SIST := FTOP;
        T2[E] := T2[E] + ORDI(U)
      END;
      NTOP := N0;
      IF T2[E] > 16000 THEN FEL(16);
      IF SYM <> ';' THEN KLAR := TRUE
      ELSE BEGIN NASTA; KLAR := SYM <> SNAMN END
    END;
    VANT(SEND);
    TYP := E
  END
  ELSE IF SYM = SSET THEN
  BEGIN
    NASTA;
    VANT(SOF);
    U := TYP;
    IF NOT ORDINAL(U) THEN FELFOR(10);
    IF (GRUND(U) = HEL) AND (KIND(U) <> KDEL) THEN FELFOR(28);
    GRANSER(U, LO, HI);
    IF (LO < 0) OR (HI > 255) THEN FELFOR(28);
    TYP := SETTYP(GRUND(U))
  END
  ELSE IF SYM = '^' THEN
  BEGIN
    NASTA;
    N := NAMNET;
    E := SOKA(N);
    IF SLAGET(E) = KTYP THEN BEGIN POOLTOP := N; TYP := GAMMAL(KPEK, 0, 0, TTYP[E]) END
    ELSE IF NOT OPPNA THEN FELFOR(7)
    ELSE
    BEGIN
      (* ^T där T kommer senare i TYPE; namnet blir kvar i POOL *)
      U := NYTYP(KPEK);
      T1[U] := N; T2[U] := 1;
      TYP := U
    END
  END
  ELSE IF SYM = '(' THEN
  BEGIN
    (* (a, b, c) *)
    U := NYTYP(KUPPR);
    KLAR := FALSE;
    REPEAT
      NASTA;
      E := NY(NAMNET, KKONST, U, T1[U]);
      T1[U] := T1[U] + 1;
      IF SYM <> ',' THEN KLAR := TRUE
    UNTIL KLAR;
    VANT(')');
    TYP := U
  END
  ELSE
  BEGIN
    E := 0;
    IF SYM = SNAMN THEN
    BEGIN
      E := SOKA(LAGG);
      IF SLAGET(E) <> KTYP THEN E := 0
    END;
    IF E <> 0 THEN BEGIN NASTA; TYP := TTYP[E] END
    ELSE IF (SYM = SNAMN) OR (SYM = STAL) OR (SYM = STECKEN) OR (SYM = '+')
            OR (SYM = '-') THEN
    BEGIN
      (* lo..hi *)
      U := OMFANG(LO, HI);
      E := NYTYP(KDEL);
      T1[E] := LO; T2[E] := HI; T3[E] := U;
      TYP := E
    END
    ELSE FEL(10)
  END
END;

PROCEDURE UTBYTE(B : INTEGER);    (* en byte till HEJ.TMP *)
BEGIN
  WRITE(H, CHR(65 + B DIV 16), CHR(65 + B MOD 16));
  UTLEN := UTLEN + 1;
  IF UTLEN MOD 32 = 0 THEN WRITELN(H)
END;

PROCEDURE KROPP(NR, NPAR : INTEGER);
VAR E, L, J : INTEGER;
BEGIN
  FOR E := SCOPE + 1 TO TOP DO
    IF (SLAGET(E) = KPROC) OR (SLAGET(E) = KFUNK) THEN
      IF PFORW[TVAL[E]] THEN FEL(19);
  KLEN := 0; TEMPANT := 0; TEMPDJUP := 0;
  KB(OENT); KB(0);
  (* fälten och posterna som värdeparametrar kopieras *)
  FOR E := SCOPE + 1 TO TOP DO
    IF TX[E] >= 2 THEN
    BEGIN
      ADRESSEN(E);
      LADORD(NIVA, TX[E] - 2);
      KB(OMOV); KW(BYTES(TTYP[E]))
    END;
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
  FOR J := 0 TO KLEN - 1 DO UTBYTE(BYTEN(J))
END;

PROCEDURE BLOCK(NR, NPAR : INTEGER); FORWARD;

PROCEDURE PROCEDUR;
VAR FUNK, VP, KLAR, FORW, KOPIA : BOOLEAN;
    N, P, E, J, K, Q, T, NRP, N0, YSCOPE, YLOKALA, YPOOL, YTYPER,
    YFTOP : INTEGER;
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
    TINFO[P] := TINFO[P] + 32;       (* nivån är NIVA + 1 *)
    PFORST[NRP] := PARTOP + 1; PANT[NRP] := 0; PFORW[NRP] := FALSE;
    NIVA := NIVA + 1;
    SCOPE := TOP;
    YPOOL := POOLTOP; YTYPER := TYPER; YFTOP := FTOP;
    LOKALA := 0;
    IF SYM = '(' THEN
    BEGIN
      NASTA;
      KLAR := FALSE;
      REPEAT
        VP := SYM = SVAR;
        IF VP THEN NASTA;
        N0 := NTOP;
        NAMNLISTA;
        VANT(':');
        E := SOK(NAMNET);
        IF SLAGET(E) <> KTYP THEN FEL(10);
        T := TTYP[E];
        (* ett fält eller en post som värdeparameter är adressen, och
           proceduren kopierar den *)
        KOPIA := BLOCKTYP(T) AND NOT VP;
        IF VP OR KOPIA THEN K := KVARP ELSE K := KVAR;
        FOR Q := N0 + 1 TO NTOP DO
        BEGIN
          IF PARTOP = PARMAX THEN FEL(26);
          PARTOP := PARTOP + 1;
          PTYP[PARTOP] := T; PVAR[PARTOP] := VP;
          PANT[NRP] := PANT[NRP] + 1;
          E := NY(NLISTA[Q], K, T, LOKALA);
          IF KOPIA THEN TX[E] := 1;
          IF VP OR KOPIA THEN LOKALA := LOKALA + 1
          ELSE LOKALA := LOKALA + ORDI(T)
        END;
        NTOP := N0;
        IF SYM = ';' THEN NASTA ELSE KLAR := TRUE
      UNTIL KLAR;
      VANT(')')
    END;
    IF FUNK THEN
    BEGIN
      VANT(':');
      E := SOK(NAMNET);
      IF (SLAGET(E) <> KTYP) OR BLOCKTYP(TTYP[E]) OR (TTYP[E] = TXT) THEN
        FEL(10);
      TTYP[P] := TTYP[E]
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
    YPOOL := POOLTOP; YTYPER := TYPER; YFTOP := FTOP;
    FOR J := PFPAR[NRP] TO PFPAR[NRP] + PANT[NRP] - 1 DO
    BEGIN
      IF TOP = TABMAX THEN FEL(26);
      TOP := TOP + 1;
      TNAMN[TOP] := TNAMN[J]; TINFO[TOP] := TINFO[J] - 10;
      TTYP[TOP] := TTYP[J]; TVAL[TOP] := TVAL[J]; TX[TOP] := TX[J];
      LANKA(TOP)
    END;
    LOKALA := PARORD(NRP)
  END;
  IF NOT FORW THEN
  BEGIN
    BLOCK(NRP, PARORD(NRP));
    VANT(';');
    TOM(SCOPE);
    POOLTOP := YPOOL; TYPER := YTYPER; FTOP := YFTOP;
    NIVA := NIVA - 1;
    SCOPE := YSCOPE; LOKALA := YLOKALA
  END
END;

PROCEDURE BLOCK;
VAR P, E, N0, T, V, J, T0 : INTEGER;
BEGIN
  (* fälten och posterna som värdeparametrar får sina egna ord *)
  FOR E := SCOPE + 1 TO TOP DO
    IF TX[E] = 1 THEN
    BEGIN
      TX[E] := TVAL[E] + 2;
      TVAL[E] := NYTTORD(ORDI(TTYP[E]));
      TINFO[E] := KVAR + 32 * NIVAN(E)
    END;
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
    T0 := TYPER;
    OPPNA := TRUE;
    REPEAT
      P := NAMNET;
      VANT('=');
      E := NY(P, KTYP, TYP, 0);
      VANT(';')
    UNTIL SYM <> SNAMN;
    OPPNA := FALSE;
    (* ^T där T kom senare i TYPE *)
    FOR J := T0 + 1 TO TYPER DO
      IF (TK[J] = KPEK) AND (T2[J] = 1) THEN
      BEGIN
        E := SOKA(T1[J]);
        IF SLAGET(E) <> KTYP THEN FELFOR(7);
        T3[J] := TTYP[E]; T1[J] := 0; T2[J] := 0
      END
  END;
  IF SYM = SVAR THEN
  BEGIN
    NASTA;
    REPEAT
      N0 := NTOP;
      NAMNLISTA;
      VANT(':');
      T := TYP;
      FOR J := N0 + 1 TO NTOP DO
        E := NY(NLISTA[J], KVAR, T, NYTTORD(ORDI(T)));
      NTOP := N0;
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

(* Orden har symbolerna 'a' och uppåt i ordning, och de i den stora
   versionen 'A' och uppåt. *)
BEGIN
  W := 'AND ARRAY BEGIN CASE CONST DIV DO DOWNTO ELSE END FOR FORWARD   '; WP := 1;
  DEFS(12, KORD, 0, 97, 0, 1);
  W := 'FUNCTION IF MOD NOT OF OR PROCEDURE PROGRAM REPEAT THEN TO TYPE '; WP := 1;
  DEFS(12, KORD, 0, 109, 0, 1);
  W := 'UNTIL VAR WHILE PACKED IN NIL RECORD SET WITH NEW MARK RELEASE  '; WP := 1;
  DEFS(4, KORD, 0, 121, 0, 1);
  DEFS(5, KORD, 0, 65, 0, 1);
  DEFS(3, KSTD, 0, 26, 0, 1);
  W := 'WRITE WRITELN READ READLN RESET REWRITE CLOSE ABS SQR ODD ORD   '; WP := 1;
  DEFS(11, KSTD, 0, 1, 0, 1);
  W := 'CHR SUCC PRED EOF EOLN HALT SQRT SIN COS ARCTAN LN EXP TRUNC    '; WP := 1;
  DEFS(13, KSTD, 0, 12, 0, 1);
  W := 'ROUND FALSE TRUE MAXINT INPUT OUTPUT INTEGER CHAR BOOLEAN TEXT  '; WP := 1;
  DEFS(1, KSTD, 0, 25, 0, 1);
  DEFS(2, KKONST, BOO, 0, 0, 1);
  DEFS(1, KKONST, HEL, MAXINT, 0, 0);
  DEFS(2, KSTDFIL, TXT, 0, 0, 0);
  DEFS(4, KTYP, HEL, 0, 1, 0);
  W := 'REAL                                                            '; WP := 1;
  DEFS(1, KTYP, REA, 0, 0, 0)
END;

PROCEDURE ORDET(W : INTEGER);     (* lägsta byten först *)
BEGIN
  WRITE(G, CHR(W MOD 256), CHR(W DIV 256))
END;

(* -- huvudprogrammet ------------------------------------------------- *)

BEGIN
  (* filnamnen: HEJ.PAS ger HEJ.PK, och koden går först till HEJ.TMP *)
  FOR I := 1 TO 24 DO BEGIN KNAMN[I] := ' '; UNAMN[I] := ' '; XNAMN[I] := ' ' END;
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
    UNAMN[P] := KNAMN[P]; XNAMN[P] := KNAMN[P]
  END;
  UNAMN[P + 1] := '.'; UNAMN[P + 2] := 'P'; UNAMN[P + 3] := 'K';
  XNAMN[P + 1] := '.'; XNAMN[P + 2] := 'T'; XNAMN[P + 3] := 'M';
  XNAMN[P + 4] := 'P';
  RESET(F, KNAMN);
  REWRITE(H, XNAMN);

  FOR I := 0 TO 60 DO HUVUD[I] := 0;
  TINFO[0] := 0; TTYP[0] := 0; TX[0] := 0;
  FOR I := 1 TO NYTYP0 - 1 DO
  BEGIN TK[I] := 0; T1[I] := 0; T2[I] := 0; T3[I] := 0 END;
  TK[TOMM] := KMANGD;
  TOP := 0; SCOPE := 0; POOLTOP := 1; NIVA := 0; LOKALA := 0; SKTOP := 0;
  NTOP := 0; TYPER := NYTYP0 - 1; FTOP := 0; OPPNA := FALSE;
  PROCS := 0; PARTOP := 0; GLOBALA := 0;
  UTLEN := 0; FALLTOP := 0; UTTOP := 0; FILREF := 0;
  REELL := FALSE; STOR := FALSE;
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
  IF UTLEN MOD 32 <> 0 THEN WRITELN(H);
  CLOSE(F);
  CLOSE(H);

  (* huvudet, koden från HEJ.TMP, tabellen över procedurerna, de
     globala och antalet *)
  REWRITE(G, UNAMN);
  IF STOR THEN WRITE(G, 'PK', CHR(2), CHR(1)) ELSE WRITE(G, 'PK', CHR(1), CHR(1));
  RESET(H, XNAMN);
  FOR I := 0 TO UTLEN - 1 DO
  BEGIN
    IF (I > 0) AND (I MOD 32 = 0) THEN READLN(H);
    READ(H, C, D);
    WRITE(G, CHR((ORD(C) - 65) * 16 + ORD(D) - 65))
  END;
  CLOSE(H);
  FOR I := 0 TO PROCS - 1 DO ORDET(POFFS[I]);
  ORDET(2 * GLOBALA);
  IF REELL THEN WRITE(G, CHR(PROCS MOD 256), CHR(PROCS DIV 256 + 128))
  ELSE ORDET(PROCS);
  CLOSE(G);
  (* HEJ.TMP töms *)
  REWRITE(H, XNAMN);
  CLOSE(H)
END.
