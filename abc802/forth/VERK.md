# VERK: upplägget

VERK är text och kalkyl i ett och samma program för ABC 802: en
ordbehandlare där ett ställe i texten kan vara ett fält med en formel.
Det bygger på ORD (editorn) och KALKYL (formlerna) och skrivs helt i
Forth. Här står upplägget. Överlägget är provat i KALKYL, och VERK.ABS
(`verk.fs`) har editorn med KALKYL:s celler, fälten, statusfältet
(CTRL-K) och filerna med utskriften. På programskivan finns brevet
OFFERT.VRK som exempel.

## Förebilden

Jef Raskins Canon Cat (1987) såldes som en "work processor", en
arbetsbehandlare i stället för en ordbehandlare; därav namnet VERK,
som ORD efter ordbehandlare. Cat var byggd i Forth, och allt var en
enda text: i stället för program fanns det kommandon som verkade på
texten, och ett uttryck i texten kunde räknas ut på plats.

VERK tar den idén ett steg till med KALKYL:s celler: ett fält i texten
har ett namn och en formel, och formlerna kan läsa varandra. Ett brev
med en offert räknar alltså om sig självt när priset ändras.

## Det användaren ser

```
Stockholm 1984-03-12

Offert på 12 bordslampor à <   1250.00> kr. Med 5 % rabatt blir
nettot <  14250.00> kr, momsen <   3562.50> kr och att betala
<  17812.50> kr.

VERK  NETTO = PRIS*ANTAL*(1-RABATT)    ######.##
```

- **Texten** skrivs och rättas som i ORD, med tangenterna som när
  BASIC läser en rad: <- suddar tecknet före markören, och CTRL-S går
  bakåt (som i WordStar; vänsterpilen är samma kod 8 som CTRL-H). ->
  eller CTRL-D går framåt, CTRL-G tar bort tecknet efter markören, och
  DEL suddar som <- med en PC:s tangentbord. CTRL-Q går till menyn, i
  stället för ESC som ABC 802 inte har.
- **Ett fält** visar bara sitt värde, i mallens bredd (`FORMAT` i
  KALKYL), inverterat. Värdet har en fast bredd, så att stycket inte
  bryts om när värdet ändras. På ABC 802 är bit 7 i ett tecken i
  skärmminnet inverterat, utan att ta en teckenplats.
- **Statusfältet** (rad 24, som i ORD) visar namnet, formeln och mallen
  för fältet vid markören. Där skrivs och rättas formeln, som `SATT` i
  KALKYL i dag, med samma parser och felmeddelanden. Texten står still
  medan formeln skrivs.
- **Ett nytt fält** läggs in vid markören med en tangent; statusfältet
  frågar efter namnet och formeln.
- **Omräkningen** sker när en formel ändras, i beroendeordning som i
  KALKYL, och bara de fält som syns ritas om.

## Fälten

Så här är fälten gjorda (milstolpe 3), enklare än först tänkt:

- Ett fält tar lika många bytes i texten som det är brett på skärmen:
  först 128 + cellens nummer, sedan bredden − 1 bytes 255. Då är en
  plats i texten fortfarande en kolumn på skärmen, så radbrytningen,
  markören och raderna i ORD:s ord fungerar som de är.
- Bredden (`FBREDD`) är rubrikens längd för en text och annars mallens
  (`CMSK`), eller 10 utan mall; högst 40.
- Ingen fälttabell: fälten hittas när en rad ritas. Raden hämtas ändå
  med `RMOVE` till skärmen, och `HSCAN` (maskinkod) letar där efter
  byte med bit 7. `FALTEN` skriver värdet över bytena, i mallens bredd
  och med bit 7 satt, så att det syns inverterat; ett tal som inte får
  plats blir `*`. Värdet räknas alltså fram varje gång raden ritas och
  ligger aldrig i texten.
- Markören går över fältet i ett steg (`STEG`, `UTUR`), och <- och
  CTRL-G tar bort hela fältet. Statusraden visar cellens namn när
  markören står på ett fält. 6845:ans markör (en understrykning) syns
  inte mot ett inverterat fält, så namnet är det som visar var den är.
- `FALT NAMN` i Forth lägger in ett fält för cellen vid markören. Med
  `SATT` och `FORMAT` som i KALKYL är det sättet att göra fält tills
  statusfältet finns.
- Formeln, typen, värdet och mallen ligger i KALKYL:s celltabell och
  formeltexterna på RAM-skivan, som i KALKYL. Namnen gäller i hela
  texten, ett namn kan ha ett fält utan att synas, och samma cell kan
  visas på flera ställen.

När `SATT` med `NYPOS` eller `TABORT` flyttar cellerna följer fälten
med, och när en mall eller en rubrik ändras får fälten den nya bredden
(`FIXA`, milstolpe 4).

## Statusfältet

Milstolpe 4. CTRL-K skriver formeln på sista raden:

- Vid ett fält visas cellens rad som i en .KAL-fil, `PRIS 1250` eller
  `SUMMA PRIS*ANTAL`, och den kan rättas (`RADED`, en radeditor i
  bufferten `EB`). Tangenterna är som när BASIC läser en rad: <- suddar
  tecknet före markören, -> går framåt och CTRL-X suddar raden. Dessutom
  går CTRL-S bakåt och CTRL-D framåt, CTRL-G tar bort tecknet efter
  markören och CTRL-A och CTRL-F går till radens början och slut, som i
  texten. RETURN gör `SATT`, och
  en rad som börjar med `%` gör `FORMAT`: `%PRIS ######.##`.
- Utan fält vid markören läggs ett nytt fält in för cellen på raden
  (`FINS`), också för en cell som redan finns.
- Ett fel visas på rad 24 med KALKYL:s meddelande, till exempel
  "tal eller namn saknas vid tecken 6". Nästa tangent tar bort det, och
  raden kan rättas. CTRL-Q (eller ESC) ångrar, och då blir det inget
  fält.
- Felet fångas så här (`KORA`): kärnans `EMIT`, `TYPE` och `U.`
  anropar ROM:ens `PRTXT` [000B], och under `SATT` och `FORMAT` byter
  `UT` de tre anropen mot `BTXT` (adresserna i `UTA`), som lägger
  tecknen från 32 i `SB`, högst 80. Det som skrivs är alltid ett fel,
  så om inget skrevs gick det bra. Förut skrev `SATT` på skärmens
  första rad (24 rader och `PAGE`), som lästes till `SB` innan skärmen
  ritades om.
- Efteråt får alla fält rätt bredd och nummer (`FIXA`), och sidan ritas
  om.

`FINS`, `FIXA`, radeditorn, introduktionen, menyn och tangenterna
(`TANGENT`) är överlägg 3 (2331 bytes), se Minnet. CTRL-K tas därför i
`TANG` i den fasta delen: `KALK` gör `SATT`, som hämtar överlägg 1, och
ett överlägg får inte hämta ett annat.

## Filerna

En VERK-fil är en textfil som PRINT# skriver, så att BASIC och ORD kan
läsa den. Först kommer texten, med fälten skrivna `<NAMN>`, och sedan
en rad `%%` och cellerna som i en .KAL-fil: `NAMN formel` och
`%NAMN mall`.

```
Offert på 12 bordslampor à <PRIS> kr. Med 5 % rabatt blir
nettot <NETTO> kr, momsen <MOMS> kr och att betala <SUMMA> kr.
%%
PRIS 1250
ANTAL 12
RABATT 0.05
NETTO PRIS*ANTAL*(1-RABATT)
MOMS NETTO*0.25
SUMMA NETTO+MOMS
%PRIS ######.##
```

- `{}` och `[]` går inte som markörer: i ABC 802:s teckenuppsättning är
  $7B och $7D ä och å, och $5B och $5D Ä och Å. Ett cellnamn får
  innehålla Ä Ö Å (`BOKST?` tar $41-$5D). `<` och `>` är inga
  bokstäver.
- Vid läsningen blir bara `<NAMN>` med en cell efter `%%` ett fält; ett
  annat `<` i texten är text.
- Filen heter NAMN.VRK om ingen typ ges.

Så blev det (milstolpe 5): 6 i menyn sparar, 7 läser och 8 skriver ut
till PR:, och i Forth gör `SPARA namn`, `LADDA namn` och `SKRIV`
detsamma.

- `SPARA` skriver texten rad för rad med fälten som `<NAMN>`, sedan
  `%%` och cellerna med KALKYL:s ord, och säger "Sparad: NAMN.VRK".
- `LADDA` läser texten till `%%` in i en ny text och sedan cellerna
  som KALKYL:s `LADDA`; formlerna kompileras efteråt (överlägg 1), och
  `<NAMN>` blir fält i cellens bredd. Ett `<NAMN>` utan cell blir kvar
  som text, en formel som inte går att räkna visar `fel`, och en fil
  utan `%%` är bara text. Efter läsningen går `VERK` direkt till
  texten. Om filen inte finns ("FEL: filen finns inte") är texten
  kvar som den var.
- `SKRIV` skriver texten till PR: med värdena i mallens bredd, i
  stället för fälten, och med radbrytningen som på skärmen.
- `VISA` finns inte i VERK, eftersom texten är visningen; `FORMAT`
  finns i Forth som i KALKYL.

## Minnet

Uppmätt 2026-11-09 i emulatorn, med HERE efter varje del:

| Del | Bytes |
|---|---|
| Forth-kärnan med flyttalen | 6 764 |
| ORD:s ord (textbufferten, skärmen, tangenterna, buffertar) | 6 616 |
| KALKYL: formelkompilatorn med celltabellen och mallarna | 5 416 |
| KALKYL: cellerna och omräkningen, SATT, TABORT, NY | 2 818 |
| KALKYL: PRINT USING och radbufferten | 2 024 |
| KALKYL: SPARA och LADDA | 1 906 |
| KALKYL: PR: med VISA och SKRIV | 489 |
| KALKYL: DOS-anropet | 183 |

Med KALKYL laddat ligger returstacken på $EE94 (BASIC:s SP vid CALL)
och datastacken på $EE52. $D800-$ECFF fylldes med ett mönster, och
ingen del av KALKYL rörde det. Forth kan alltså ha $8000 till omkring
$ED00, cirka 27,9 KB.

Allt på en gång blir för mycket (cirka 29 KB med fälten och
formelkoden), så formelkompilatorn blir ett överlägg:

| Område | KB | |
|---|---|---|
| kärnan och flyttalen | 6,8 | fast |
| editorn (ORD:s ord, skrivna om utan BASIC) | 6,6 | fast |
| cellerna, omräkningen, PRINT USING, DOS-anropet | 5,0 | fast |
| fälten och statusfältet | ~1 | fast |
| överlägget: formelkompilatorn | 5,5 | från RAM-skivan |
| formlernas kod | ~2 | |
| **summa** | **~27** | av 27,9 |

Filerna och utskriften (cirka 2,5 KB) används sällan och kan i stället
dela överläggets område och hämtas från skivan, eller också krympas:
VISA och cellistan behövs inte, eftersom texten är visningen.

RAM-skivan ($0000-$77FF, 30 KB) får texten (cirka 22 KB, 7-8 sidor),
formeltexterna (40 celler × 80 bytes) och kompilatorns kod.

## Överlägget

Formelkompilatorn behövs bara när en formel skrivs i statusfältet och
när en fil läses. Omräkningen använder bara den kompilerade koden och
`RAKNA`. Kompilatorn ligger därför på RAM-skivan och hämtas med `RMOVE`
till ett fast område när den behövs, på cirka 40 ms.

- Orden i överlägget nås via en tabell med fasta ingångar; deras
  huvuden får inte ligga i ordlistans kedja, eftersom området ibland
  har något annat.
- Formlernas kod läggs ovanför överlägget, från `KBAS` som i KALKYL.
- `gor.sh` kompilerar överlägget på sin fasta adress och tar ut det ur
  minnet, som `kabs.py` gör KALKYL.ABS. VERK.ABS får överlägget med
  sig och lägger det på RAM-skivan vid starten.

### Uppmätt med KALKYL

Överlägget är gjort i KALKYL (milstolpe 1), och kompilatorn som går att
flytta är bara 1694 bytes. Resten av de 5416 bytes ovan är tabeller som
omräkningen behöver: `CELLER` (1160 bytes), `MALLAR` (516), `FT` (562)
och bitmängderna. Med bara kompilatorn och filerna i överläggen blev
vinsten 868 bytes, eftersom hämtningen, summan och de fasta ingångarna
tar cirka 800 bytes.

Sedan lades allt som bara används när något ändras i överlägg 1
(`SATT`, `TABORT`, cirkelprovet, `ORDNA`, `KOMPALLA`: 3490 bytes) och
`VISA` och `FORMAT` med filerna i överlägg 2 (3381 bytes). Då blev
vinsten 2401 bytes: 5208 bytes fritt mot 2811. Det fasta i KALKYL är nu
cellerna, talen med mallarna och omräkningen.

För VERK betydde det cirka 27 KB fast plus ett område på 3,5 KB,
alltså något för mycket av 27,9 KB. Så blev det (milstolpe 3):

- Gränsen mot BASIC är $ED00 (TIB $ECB0), som mätningen ovan tillåter.
- Överlägg 2 följer med i VERK.ABS i `PB` (1920 bytes), `CELLER` och
  `MALLAR`, som inte behöver något innehåll vid starten (`OVBIT`,
  `OVDEL`); `VSTART` lägger det på RAM-skivan och nollar `MALLAR`.
  I KALKYL ligger kopian efter `KBAS` och tar plats av formelkoden.
- VERK.ABS är 26022 bytes ($8000-$E5A5), och 1802 bytes är fria till
  $ECB0 för formlernas kod. `KPLATS` kompilerar om alla formler när
  det är 700 bytes kvar, så det blir cirka 1,1 KB i taget.
- Texten får 20521 bytes på RAM-skivan, upp till överlägg 1.

Med statusfältet (milstolpe 4) fick den fasta delen inte plats, så det
som bara används med CTRL-K och i menyn blev överlägg 3:

- Överlägg 3 är `FINS`, `FIXA`, radeditorn, `INTRO`, `MENY` och
  tangenterna (`TANGENT` med `UPP`, `NED` och de andra), 2331 bytes.
  Det kompileras på `OVB` (överlägg 1 läggs tillbaka efteråt) och
  ligger på RAM-skivan under överlägg 1, där texten lämnar 2560 bytes
  (`OV3R`). `OVT` har 6 bytes per överlägg, 18 i stället för 12.
- Överlägg 2 och 3 följer med i VERK.ABS efter varandra i åtta bitar
  (`OVBIT`): `PB`, `CELLER`, `MALLAR`, `MB`, `EB`, `RB`, `SB` och sist
  det som är fritt efter `KBAS` upp till $ECB0. `OVDEL` flyttar dem
  mellan bitarna och RAM-skivan, och `kabs.py` läser kedjorna ur
  bitarna.
- Det som bara används vid starten (`OVBIT`, `OVDEL`, `VSTART`) ligger
  efter `KBAS`, där formlernas kod hamnar. `VSTART` lägger överläggen på
  RAM-skivan, tar bort orden ur ordlistan (`'LAST` till `VLAST`) och
  sätter HERE till `KBAS`; `VERK` når det via `'VSTART`.
- VERK.ABS är 27764 bytes ($8000-$EC73). I gång är HERE $E379, så 2358
  bytes är fria till $ECB0 och 1659 innan `KPLATS` kompilerar om alla
  formler (700 bytes under TIB, $E9F4). I milstolpe 4 var det 594;
  provat med 17 formler som `C17 C16+17*2+SUMMA(C1..C16)` och CTRL-K som
  kompilerar om. När det inte räcker säger `SATT` "minnet räcker inte".
- Returstacken har 64 bytes under BASIC:s SP, och CTRL-K som kompilerar
  om alla formler går djupare (`VERK`, `ORD`, `TANG`, `KALK`, `KORA`,
  `SATT`, `KPLATS`, `KOMPALLA` och formelkompilatorn); det gav
  STACK ?. `VERK` flyttar därför datastackens botten (`S0`, $8005) till
  160 bytes under SP ($8003) och tömmer stackarna med `RESET`. Det går
  eftersom `VERK` aldrig går tillbaka. Med VERK laddat är SP $EFDA, så
  datastacken får cirka 500 bytes ned till BASIC:s variabler
  (HEAP $ED00 + 64).
- Texten får 17961 bytes.

Med filerna (milstolpe 5) blev det fyra överlägg, alla på `OVB` och
med plats för fyra i `OVT` (24 bytes):

- Överlägg 2 är KALKYL:s skrivning till fil och VERK:s `SPARA` och
  `SKRIV`, 2051 bytes; VISA och cellistan behövs inte.
- Överlägg 4 är läsningen (`LADDA`) och `FORMAT`, 2523 bytes, och
  ligger på RAM-skivan under överlägg 3, där texten lämnar 2560 bytes
  (`OV4R`). Det får inte plats i bitarna i VERK.ABS, så det är en egen
  fil, VERK.OVL. `VSTART` läser den med DOS:s `DRUN` till `OVB` (sist i
  filen är en RET, som är startadressen), provar summan och lägger den
  på RAM-skivan. Om det inte går säger VERK det, och då går läsa och
  `FORMAT` inte.
- Det delade i `kalkyl.fs` (filnamnet, `SOPPNA`, `LOPPNA` och raderna)
  är märkt med `----`-rader, och `gor.sh` lägger det i både överlägg 2
  och 4, där det behövs. Rader som bara är en kommentar skrivs inte
  in, eftersom emulatorns tangentkö tar 65536 tecken.
- `kabs.py` läser överlägg 4 från RAM-skivan i minnet, provar summan
  och kedjan, och skriver det till `verkovl.bin` med adresserna i
  `verkovl.adr`, som `abs.py` gör VERK.OVL av.
- VERK.ABS är 27274 bytes ($8000-$EA89) och VERK.OVL 2524. I gång är
  HERE $E514, så 1948 bytes är fria till $ECB0 (2358 i milstolpe 4)
  och cirka 1250 innan `KPLATS` kompilerar om alla formler.
- Texten får 15256 bytes ($3B98).
- När radeditorn i statusfältet fick BASIC:s tangenter blev överlägg 3 2573
  bytes, och `OV3R` är nu 2592, så texten får 15224 bytes ($3B78).
  `kabs.py` stoppar bygget om överlägg 3 eller 4 är större än sin plats.
- Med samma tangenter i texten (<- suddar, CTRL-S bakåt) är överlägg 3
  2589 bytes, bara 3 under `OV3R`. VERK.ABS är 27421 bytes med 1894
  bytes fria.
- Felens texter är en tabell (`FTT` med räknade texter, `TXT"` från
  `verk.fs` flyttat till `kalkyl.fs`) i stället för 17 `IF ." ..." THEN`.
  Det gav 223 bytes: VERK.ABS är 27198 bytes med 2117 bytes fria, och
  överläggens område börjar på $B460.
- Introduktionen är filen VERK.HLP (`verk.hlp`), som `(INTRO)` läser
  till `PB` när den visas (sidan ritas om efteråt). `XOVL` blev
  `XDRUN`, som läser den ABS-fil vars namn är i `DNAMN` (`OVLN` eller
  `HLPN`) och ligger i den fasta delen; saknas filen kommer `DRUN`
  tillbaka med CY i stället för via `DERRX`, och `SBC A,A` gör det till
  -1, så att VERK säger "VERK.HLP kunde inte läsas". `hlp.py` gör
  filens data: ABC 802:s koder för å ä ö, 13 för radslut, 1 där
  textens längd (`TS`) skrivs och 0 sist, med en RET efter; `kabs.py`
  skriver `PB`:s adress i `verkhlp.adr`. Överlägg 3 gick från 2589 till
  1950 bytes, och den fasta delen blev 160 bytes större (`XOVL` låg
  förut efter `KBAS`): VERK.ABS är 26682 bytes med 1957 bytes fria.
  Överläggens område är fortfarande 3490 bytes, eftersom överlägg 1 är
  störst; vinsten är plats i överlägg 3 för nya tangenter.
- `KORA` fångar felen med `BTXT` i stället för via skärmen (se
  Statusfältet). Det kostar 68 bytes: VERK.ABS är 26750 bytes med 1889
  bytes fria.
- `OV3R` är sänkt från 2592 till 2080, eftersom överlägg 3 nu är 1950
  bytes (130 kvar för nya tangenter). Texten får 512 bytes mer, 15736
  ($3D78), och överlägg 4 ligger på RAM-skivan $3D78.

Det som kan göras om det blir trångt:

- Ha `CELLER` och `MALLAR` på RAM-skivan och hämta en post i taget, som
  formeltexterna.
- Färre celler än 40 (`CMAX`), eller texten på färre sidor.

## Hastigheten

Uppmätt i emulatorn: `C@` tar 21 µs per byte och `RC@` (RAM-skivan) 54
µs, medan `RMOVE` och `CMOVE` båda tar 7,5 µs per byte. Ett varv i en
DO-LOOP i Forth tar ensamt cirka 190 µs.

- RAM-skivan sölar inte ned, om texten flyttas i block: en skärm tar 14
  ms att hämta och att flytta luckan 10 KB tar 75 ms.
- Det som är långsamt är loopar i Forth tecken för tecken: att gå
  igenom 27 KB så tar över 5 sekunder. Omritningen hämtar därför sidan
  med `RMOVE` (som ORD:s `LAYOUT`), sökningen görs med maskinkod (som
  `MSCAN`) och fälten hittas i raden som redan är hämtad (`HSCAN`).

## Milstolparna

1. **Överlägget (klart 2026-11-09).** Formelkompilatorn på RAM-skivan,
   hämtad med `RMOVE` och nådd via tabellen, provad med KALKYL (se
   Överläggen i KALKYL.md). Vinsten blev mindre än räknat, se nedan.
2. **Editorn i Forth (klart 2026-11-09).** ORD:s ord utan BASIC-delen:
   menyn och starten i Forth, och programmet som VERK.ABS, se nedan.
3. **Fälten.** Markören i texten, värdena i mallens bredd
   och omräkningen (klart 2026-11-09). Fälten i texten i cellens bredd,
   utan fälttabell; se Fälten.
4. **Statusfältet (klart 2026-11-10).** CTRL-K: formeln vid
   markören, nya fält och felen på rad 24; fälten följer cellerna.
   Se Statusfältet. Efteråt mer plats för formlerna: 1659 bytes i
   stället för 594 innan `KPLATS` kompilerar om, se Minnet.
5. **Filerna och utskriften (klart 2026-11-10).** `<NAMN>` och `%%`
   i NAMN.VRK, och utskrift till PR: med värdena i texten. Läsningen
   är överlägg 4 i filen VERK.OVL; se Filerna och Minnet.
6. **Programskivan och webbsidan (klart 2026-11-10)**, med en offert
   skriven som brev: OFFERT.VRK (`offert.vrk`), som `gor.sh` låter
   BASIC skriva med `PRINT #1`, 50 tecken i taget. Introduktionen
   säger "Ett exempel: 7 och OFFERT i menyn". à finns inte i ABC 802:s
   tecken, så brevet säger "för 1250.00 kr".

## Editorn

`verk.fs` är ORD:s ord, tagna ur REM FORTH-raderna i `ord.bas`, sedan
fälten och sist menyn och introduktionen, som i ORD var BASIC. `gor.sh`
tar `kalkyl.fs` till raden "Hit är KALKYL och VERK lika" och sedan
`verk.fs`, skriver in det i Forth med gränsen mot BASIC på $ED00 och
gör VERK.ABS med `kabs.py ... VERK`: ingång 1 blir ordet `VERK`.
Laddaren VERK (`verk.bas`) läser in filen som KALKYL:s laddare, med
rutinen på $ECB0.

- Första gången visar `VERK` introduktionen och börjar en ny text;
  sedan menyn: 1 fortsätt, 2 ny text, 3 introduktionen, 4 slut (BASIC)
  och 5 Forth, där `VERK` ger menyn igen och `BYE` går till BASIC;
  6 spara, 7 läsa och 8 skriva ut (se Filerna).
- Från BASIC går `Z=CALL(32768,1)` tillbaka till menyn med texten kvar
  på RAM-skivan.
- VERK.ABS är 27274 bytes med KALKYL:s celler (14657 med bara
  editorn). Ingen kompilering vid starten: ORD tar cirka 25 s.
- Introduktionen visar tangenterna och hur ett fält görs med CTRL-K.
  Texten är filen VERK.HLP (`verk.hlp`, se Minnet), så den kan ändras
  utan att VERK byggs om, bara skivan.
- `gor.sh` skriver in VERK med 500 ms efter varje rad: med 300 tappades
  ibland det första tecknet på raden efter en lång definition, eftersom
  ordlistan är lång och raden inte hunnit kompileras. Ett tappat `:`
  gör att resten tolkas, och `;` förstör då ordlistan.
- Forths radbuffert tar 79 tecken, och resten av en längre rad tappas
  utan att något märks. `gor.sh` stoppar därför om en rad i
  `kalkyl.fs` eller `verk.fs` är längre.
- Statusraden börjar med VERK i stället för ORD.

Namnen som krockade med `kalkyl.fs`: editorns `KLAR` heter `RITAD`,
`TNY` gör också `0 TOP !`, och `LB`, `VISA` och `RS` finns bara i
överlägg 2, som editorn inte använder.

## Att ta reda på

- Cats LEAP-tangenter (sök framåt och bakåt medan man skriver) kunde
  vara ett sätt att flytta sig i texten; det kan komma senare.
