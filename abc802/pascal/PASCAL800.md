# Pascal 800 inifrån

Anteckningar om Luxors Pascal 800 (Dataindustrier AB, ”ABC 800 - Pascal
System Ver. 1.04”, art. nr 68 29067-43), utredda 2026-11-11 ur skivavbilden
`pascal10.td0` i devili.iki.fi:s arkiv och genom körningar i emulatorn för
ABC 802 (DOS 6.20, CMDINT.SYS startad med START.BAC, MF1 som dataskiva).
Avbilden och filerna ur den ligger inte i förrådet. `pdis800.py` plockar isär
en COD-fil. (?) betyder att uppgiften inte är kontrollerad.

## 1. Filerna

| Fil | Byte | Vad |
|---|---|---|
| PASSYS.ABS | 8 960 | tolken för systemprogrammen (kompilatorn, länkaren) |
| PASCAL.ABS | 8 704 | tolken för färdiga program, med REAL och matematiken |
| PASCOMP.COD | 32 000 | kompilatorn P-1.01, p-kod, 12 segment |
| PASLINK.COD | 11 008 | länkaren P-1.02, p-kod, 2 segment |
| EDIT.ABS | 3 584 | radeditorn ”ABC800 Editor version 1.05”, \$C700–\$D47D |

COD-huvudet i PASCOMP säger ”OS.8 Pascal 3.01 File: PASPROG.ABC800COMP
1982-06-09” och i PASLINK ”…PASPROG.PASLINK800 1982-07-15”. Vad ”OS.8” står
för är okänt (?).

## 2. Två tolkar

Båda laddas på \$8000 och är samma p-maskin, men med olika standardprocedurer.

- **PASSYS.ABS** har kompilatorns hjälprutiner i maskinkod: CSP 7 och 8
  (IDSEARCH och TREESEARCH, sökningen bland de reserverade orden och i
  symbolträdet). Tabellen över de reserverade orden ligger på \$8FD5, åtta
  tecken per ord och två byte med ordets kod. Där finns bland annat
  EXTERNAL, FORWARD och SEGMENT, och tre platser med `********`, som ser ut
  att vara borttagna ord (?). Flyttalsinstruktionerna (129, 131, 135, 137,
  138, 144, 146, 150, 153) och CSP 14 och 24–31 går till \$8237, fel 11.
- **PASCAL.ABS** har REAL och CSP 24–31 (RND, SIN, COS, LOG, ATAN, LN, EXP,
  SQT) men saknar IDSEARCH och TREESEARCH. `PASCAL PASCOMP` ger därför
  `RUNTIME ERROR NR.11`.

Ett program utan REAL går med båda: `PASSYS MF1:PROV` och `PASCAL MF1:PROV`
(eller `PASCAL PROV`) ger samma utskrift. Utan argument svarar båda
`Code file does not exist!`.

## 3. P-maskinen

UCSD Pascal II.0-lik.

- BC är IPC, Z80:ans SP är beräkningsstacken.
- Hämtslingan på \$81FF: `LD A,(BC) / INC BC / ADD A,A / JP NC,SLDC`, annars
  H=\$80, L=A och hopp genom tabellen på \$8000 (opkod 128–255). Under 128 är
  det SLDC.
- SLDL 1–16 delar rutinen \$8465 (MP+2n), SLDO \$849F (BASE+2n), SIND
  \$851D/\$8525. 214 (XIT) går till \$FC6F, 215 (NOP) rakt tillbaka till \$81FF.
- Stort operandord (rutinen \$8457): en byte, eller två när den höga biten är
  satt.
- LDCN lägger 1 på stacken, alltså är NIL = 1.
- Variabler: \$8103 heapens topp (NP), \$8105 MP, \$8107 BASE, \$810B sparad IPC,
  \$816D segmentets bas, \$8119+4·seg referensräknare och adress för segmentet,
  \$818C segmenttabellen, sex byte per post.
- CSP på \$82A6 med tabellen på \$82BE+2n, UCSD:s numrering: 0 IOCHECK
  (\$FC72), 1 NEW, 7/8 IDSEARCH/TREESEARCH, 12 startparametrarnas flaggor
  (pekare till \$9995), 255 (\$825F) slut, laddar om CMDINT.
- CXP 0,n är segment 0, det inbyggda systemet, med tabellen på \$83E2: bland
  annat 13 skriv heltal, 19 skriv sträng, 22 WRITELN, 18 läs sträng, 21
  READLN.
- CXP seg≠0 går till \$8CD6: segmentet läses från filen in på stacken (SP
  minskas med längden) genom \$95C9 och räknas med referensräknaren. Ett
  segment som inte används tar alltså ingen plats; kompilatorn är ~29 KB
  p-kod men har bara ~21 KB fritt.

## 4. Minnet

Vid READLN i ett litet program (T2):

| Adress | Innehåll |
|---|---|
| \$FA00–\$FCEF | den bofasta delen (skyddsprovet, krypterad kod) |
| ~\$F600 | skivbufferten |
| \$EFBE– | kodsegmentet |
| \$EF5E | BASE; de globala på BASE+10 |
| \$EF48 | MP |
| nedåt | stacken |
| uppåt | heapen |
| \$999D | heapens början; startkoden skrivs över |
| \$8000–\$999C | tolken, ~6,5 KB |

Omkring 21 KB fritt mellan heapen och stacken.

## 5. In- och utmatning

- Text till skärmen: ROM:ens \$000B (HL = text, BC = längd).
- Filer går genom BASIC:s enhetsdrivrutiner: funktionskod i A, IX = FCB,
  ingången i (\$990F), anropad med PUSH DE/RET på \$98EE. FCB:n har flaggor i
  IX+\$1C och radbufferten i IX+\$28; den töms vid 253 byte (\$FD), CR följs av
  LF. Felkoden står i IY+\$44, \$22 är filslut. Uppslagningen av fil eller
  enhet görs med \$0063.
- Därför är en PAS-fil skriven med BASIC:s `PRINT #` läsbar för kompilatorn,
  och tvärtom.
- Slut: \$601B med DE → `CMDINT  SYS` och C = \$FF laddar om DOS:ens
  kommandotolk.

## 6. COD-filen

- Varje DOS-sektor har ett huvud om tre byte: typbyten (f0 för PASCOMP, 30
  för användarprogram, 01 för PASLINK, 20 för PAS-filer), sektornumret+1 och
  0. Resten, 253 byte, är data; filen läses som en ström av sådana bitar.
- Huvudsektorn: antal segment, 0000, sedan för varje segment (block, längd i
  byte). Kring 0xA8 namnet och en text, till exempel
  `PROV A800 Pascal 1.01 File: MF1:PROV 2000-00-00/00.00.00` (datumet är
  tomt eftersom klockan inte är ställd).
- Ett segment börjar på strömmens byte block·256. Sist i segmentet står
  segmentnumret och antalet procedurer, och före dem procedurordlistan med
  självrelativa pekare.
- Procedurens attribut, vid pekaren: ordet procnr | nivå<<8, sedan på -2
  ENTERIC (självrelativ), -4 EXITIC, sedan parametrarnas och datats storlek
  och ett ord till som inte är utrett (?).
- Huvudprogrammet är procedur 1. Segmentnumren börjar på 1; 0 är tolkens
  inbyggda.
- PASCOMP: 12 segment, 3 796 (nr 1, 28 procedurer), 316, 5 660, 6 374, 152,
  542, 364, 9 552 (42 procedurer), 858, 990, 940 och 230 byte.

## 7. Språket

UCSD-mängden av fördefinierade namn: CONCAT, LENGTH, INSERT, DELETE, COPY,
POS, MOVELEFT, MOVERIGHT, EXIT, FILLCHAR, SCAN, BLOCKREAD, BLOCKWRITE, SEEK,
IORESULT, MARK, RELEASE, HALT, PWROFTEN, MEMAVAIL, TIME. Dessutom STARTPAR,
SVC, OPTION, RENAME, DATE, GOTOXY, INP, OUT, PEEK, POKE, CALL, IXOR, IAND,
IOR, ISWAP, ISHIFT, DISPOSE, ADDRESS, ISAM, EOLNCHR och GRAPH. Kompilatorn
har inkluderingsfiler och en listfil (.LST).

## 8. Att köra

```
PASSYS PASCOMP,MF1:PROV       kompilera MF1:PROV.PAS till MF1:PROV.COD
PASSYS MF1:PROV               kör (utan REAL)
PASCAL MF1:PROV               kör (med REAL)
```

Kommandoraden står på \$C0B0. PASSYS startkod (\$99B4–\$9A3D) läser den så:
efter tolkens namn kan ett komma och flaggbokstäver stå; varje bokstav sätter
en bit i fyra byte på \$9996 (A = bit 0 i första byten och så vidare). Efter
mellanslaget kommer kodfilens namn fram till ett komma, och resten av raden
kopieras till \$999E som startparameter (STARTPAR). CSP 12 ger programmet
flaggorna.

```
PASSYS,LB PASCOMP,MF1:M       flaggorna L och B till kompilatorn
```

Kompilatorn läser flaggorna i segment 2, procedur 10: L (lista), B (”Link
info included...”), och E, D, O, R som inte är prövade (?).

## 9. Separat kompilering och länkning

Prövat:

```
program m;                       segment b;        (eller program b;)
procedure p(x: integer); external;   procedure p(x: integer);
begin                            begin writeln(x * 2) end;
 p(7)                            begin
end.                             end.
```

En modul måste vara en hel enhet med ett (tomt) huvudprogram; en lös
procedur ger `Line 1, error 3`. Båda kompileras med B:

```
PASSYS,B PASCOMP,MF1:M
PASSYS,B PASCOMP,MF1:C
PASSYS PASLINK
T MF1:MC
I MF1:M
I MF1:C
E
PASCAL MF1:MC                    skriver 14
```

Länkarens kommandon (första tecknet på raden, stora eller små bokstäver,
tabellen XJP på 232D i segment 1):

| | |
|---|---|
| `T fil[,B]` | målfilen (Destination file) |
| `P fil[,M]` | listan, till exempel `P CON:`, med modultabellen och de globala |
| `I fil` / `L fil` | ta med en modul (I) eller ett bibliotek (L) (?) ; `,R` |
| `C` | (?) skriver `No external modules.` om inget saknas |
| `R` | (?) |
| `A` | avbryt (`**  Linker aborted  **`) |
| `E` | länka och sluta (`Linking complete.`) |
| `*` eller tom | kommentar |

Annat ger `Command error!`. Kommandona kan också stå i en fil:
`PASSYS PASLINK,CMD=fil` (?; länkaren letar efter `CMD=` och `cmd=` i
startparametern). Länkaren lägger en tillfällig fil, PLKTMP.
Felen: `Invalid call to module X in module Y`, `Module X has allready been
included!`, `Variable X is undeclared!`, `Too many segments!`, `Too many
procedures!`, `End of memory!`.

## 10. Editorn

`EDIT MF1:M.PAS` ger prompten `>`. Kommandona är två bokstäver (tabellen i
EDIT.ABS): RE, KI, PR, EN, DL, IL, BT, WR, SV, CV, NU, OR, AB, UC, LC och ED.
Prövade: RE läser filen, PR skriver ut, DL n tar bort rad n, IL n infogar.
Gissningar (?): ED redigera, WR skriv, EN sluta och skriv (filen har texten
`BAK`, alltså en reservkopia), AB avbryt (`EDIT ABORTED`), UC/LC stora och
små bokstäver (efter UC visas det infogade med stora). RE läser filen och visar sista
raden; PR ger raderna numrerade `   1.   program m;`. IL 4 infogar med
decimala nummer 4.01, 4.02 …; hur infogningen avslutas är inte utrett (?).
Editorn finns också som ”ABC80 EDITOR VERSION 1.03” i samma fil.

## 11. Kopieringsskyddet

Tolken läser 64 sektorer och räknar dem som inte går att läsa; originalet
har ett 14-tal. På en kopia stämmer inte antalet och tolken tömmer minnet.
Provet och den bofasta delen på \$FA00–\$FCEF är krypterade. I emulatorn är
provet förbigånget, eftersom avbilden inte har de trasiga sektorerna.
