# COMAL, Compis och ABC 80 (bakgrund till försöket)

*Inte med i boken.* Den här texten berättar varför ett försök med
COMAL:s strukturer i ABC80-BASIC:en är mer än en lek. I början av
1980-talet valde svensk skola mellan att bygga vidare på BASIC-datorerna
från Luxor och att börja om med en egen skoldator och ett nytt språk.
Den egna skoldatorn blev Compis och språket COMAL. Compis räknas i dag
som ett av de stora misslyckandena i svensk datorhistoria, och ABC 80
som en av de stora framgångarna. Källorna står sist. Det som inte är
säkert är märkt.


## COMAL

COMAL (COMmon Algorithmic Language) gjordes i Danmark av Børge R.
Christensen och Benedict Løfstedt. Christensen var matematiklärare på
Tønder Seminarium, och Løfstedt var datalog vid Aarhus universitet.
Seminariet fick en Data General Nova 1200 år 1972, och språket arbetades
fram brevledes 1973-74. Det första COMAL kom på hålremsa i februari
1975 (DDHF).

Tanken var att behålla det som gjorde BASIC lätt och ta med det som
gjorde Pascal tydligt. Radnummer, direktläge och en tolk som gick att
lära sig på en lektion fanns kvar. Från Pascal kom block som stängs med
namn (ENDIF, ENDWHILE), procedurer med parametrar och lokala variabler
samt långa variabelnamn. Redan COMAL 75 hade IF/ELSE/ENDIF på flera
rader och PROC/ENDPROC. COMAL 80 blev färdigt omkring 1980 på Z80. Ett
standardiseringsarbete pågick 1979-1989, med ett förslag till en
gemensam kärna ("COMAL Kernel" eller "Nucleus") i mars 1980.

I Danmark fick språket ett stöd som inget annat skolspråk fick. Danska
skolor krävde att COMAL skulle finnas på varje mikrodator de köpte
(Wikipedia). Därför finns det COMAL för nästan allt som såldes där:

| Dator                       | COMAL                                                            |
|-----------------------------|------------------------------------------------------------------|
| Commodore PET               | COMAL 80, Mogens Kjær (senare UniComal)                          |
| Commodore 64                | COMAL 80 på diskett och som instickskassett (Commodore Data A/S) |
| RC Piccolo (Regnecentralen) | RC-Comal 80                                                      |
| BBC Micro                   | Acornsoft COMAL (1984)                                           |
| Apple II med CP/M           | i irländska gymnasieskolor                                       |
| Compis                      | Telenovas egen COMAL, för CP/M-86 och senare MS-DOS (1984-87)    |


## Compis

Compis kom ur TUDIS, en teknikupphandling som Styrelsen för teknisk
utveckling (STU) startade 1981. Målet var en svensk skoldator med
läromedel och programvara. Den skulle i princip ersätta ABC 80, som då
ansågs föråldrad. Kraven var bland annat grafik på minst 500 × 500 punkter och
ett enkelt språk (M3).

Konceptet kom enligt Jan Boström (Liber, senare Luxor) från "ett litet
källarföretag", Olesen och Lindgren, som gick till Esselte med det.
Konsortiet bakom budet var Esselte Studium och Svenska Datorer AB
(SDAB). Telis fabrik i Nynäshamn skulle tillverka datorn. Budet var
mycket billigt, 6 800 kr plus moms (M3). Thomas Kaiserfeld har skrivit
att det var det enda bud som byggde på en starkare processor än de som
då fanns på marknaden, och att det var därför det vann (enligt Martin
Emanuel i intervjun med Boström). Processorn var Intel 80186, med
CP/M-86, 128 KB minne och 640 × 400 punkters grafik i färg.

Luxor och Liber ansåg att skolmarknaden var deras. De anlitade enligt
Boström ett lobbyföretag, som granskade om Esselte och Teli höll
leveranstiderna. Uppgifterna gick vidare till tidningar. Dagens Nyheter
skrev den 25 september 1983 "Företag i storgräl om skoldatorer". I
samma artikel stod det att Olesen och Lindgren först hade erbjudit
Luxor tillverkningen. Luxor sade nej, enligt Boström för att Luxor
vilade på avtalet med DIAB.

Svenska Datorer gick i konkurs, och Televerkets bolag Telenova tog över.
Datorn blev minst ett år försenad, och priset steg från omkring 7 000 kr
till 9 000 kr. Uppgifterna om när den var klar skiljer sig åt:

| Uppgift                                   | Källa                            |
|-------------------------------------------|----------------------------------|
| de första 75 levererades i september 1983 | df.lth.se                        |
| klar våren 1984                           | vittnesseminariet, not 54        |
| tillverkad 1985-88                        | flera webbsidor, ej kontrollerat |

Compis såldes aldrig till privatpersoner. Under namnet Scandis gick den
till skolor i Norge och Danmark, och den gick också till Finland och
Italien. Compis II var delvis IBM-kompatibel och hade en bärnstensgul
skärm. Computer Sweden skrev 1988 "Compis med många ovänner", och enligt
den artikeln hade skolorna lagt 120 miljoner kronor på datorn på tre år
(M3).

Folk som var med säger samma sak, fast med olika ord:

- *Lars Bolander* var huvudsekreterare i Datautbildningsgruppen 1985.
  Han förstod inte varför man skulle "ge sig på att skapa en datamaskin
  i Sverige … när de stora jättarna höll på att producera datorer till
  sannolikt helt andra kostnader. Det där projektet ledde till
  ingenting, och det var ju bra det."
- *Hans O. Åkeson* (Utbildningsradion) säger "Vi drabbades ju av den här
  Compis-frossan, men avstod från att överhuvudtaget blanda oss i det."
- *Jan Boström* (Liber och Luxor, alltså motparten) kallar Compis "en
  utav de största flopparna i svensk teknikutvecklingshistoria". Han
  säger också: "Man borde ha fortsatt med BASIC-tolken istället, byggt
  vidare på det konceptet. Nu förlorade man väldigt, väldigt mycket
  pengar, man förlorade utvecklingsmöjligheter och kunskap försvann,
  därför att man gick in i det här sidospåret [med COMAL]." Han lägger
  till att man inte kunde veta det då.

Det som gick fel var alltså inte i första hand språket. Det var att en
dator som skulle konstrueras från början mötte IBM PC och MS-DOS, och
att den bara fanns för skolor. *Ett datorsystem som ingen hade hemma
saknade program, och den som inte valde den kunde inte heller ta med
sig något därifrån.* Artikeln på df.lth.se kallar CP/M-86 det stora
misstaget.

## ABC 80

Skillnaden från ABC 80 mot Compis var inte tekniken. ABC 80 var svagare
än Compis på alla sätt. Skillnaden var att den kom tidigt, att den
fanns att köpa för både skolor och hem, att BASIC:en fanns i ROM:en
från början och att programmen, böckerna och kunnandet växte runt den.
BASIC II i ABC 800 byggde vidare på samma maskin och samma program.

## Vad som hände med COMAL

COMAL dog inte på en gång. Det försvann som skolspråk när skolorna köpte
PC med MS-DOS i slutet av 1980-talet.
UniComal i Danmark och Telenova i Sverige gjorde MS-DOS-versioner, men
den danska skolans krav upphörde, och utanför Danmark hade COMAL aldrig
fått en egen marknad. Standardiseringen slutade 1989. I dag finns
OpenComal, som går på Linux och Windows, och enligt Wikipedia användes
COMAL fortfarande i några brittiska skolor 2016.

Språkets idéer lever kvar på annat sätt. BBC BASIC (1981) hade redan
REPEAT/UNTIL och procedurer (DEF PROC, DEF FN). QuickBASIC (1985)
fick block-IF, DO/LOOP och SUB, och Visual Basic ärvde dem.
Mot slutet hade BASIC svalt det som COMAL hade lagt till.

## Försöket här

Försöket i den här mappen svarar på Boströms tanke, "man borde ha
fortsatt med BASIC-tolken istället". COMAL:s styrstrukturer ryms i ABC
80:s BASIC-tolk med en krets på 2215 bytes och 23 bytes ändrade i ROM:en.
Det gäller WHILE, REPEAT, block-IF med ELIF och ELSE, PROC/EXEC med
parametrar och LOCAL, CASE och LIST med indrag. Gamla program går som
förut, och strukturerna kostar några procent i tid mot GOTO (se
`LASMIG.md`). Det som inte ryms är det som gör COMAL till ett eget språk:
långa variabelnamn, FUNC på flera rader och REF-parametrar.

Det säger inte att Compis kunde ha undvikits. Frågan i TUDIS var en ny
skoldator, inte ett nytt språk. Försöket säger bara att språket inte var
skälet att börja om från noll. Strukturerna hade gått att få på en krets
till de datorer som redan stod i skolorna. Det var en sådan krets som
Superbasic (`$4000-$57FF`) var för ABC 80.


## Det som inte är säkert

- När Compis var klar (se tabellen ovan). Boström säger att TUDIS
  startade 1983, men det var nog beslutet det året (DN-artikeln). STU
  startade upphandlingen 1981 (M3).
- Hur Olesen och Lindgren hörde ihop med Svenska Datorer AB.
- Boström säger att Børge Christensen hade "tagit fram tolken" till
  Compis. Christensen gjorde språket, men COMAL i Compis var Telenovas
  egen (datamuseum.dk).

## Källor

- Jan Boström, intervju av Martin Emanuel 14 november 2008, Tekniska museet och KTH, s. 14-17.
- *Folkbildning kring datorn 1978-85*, vittnesseminarium på Tekniska museet 9 oktober 2008, red. Ulla Riis. diva-portal.org, diva2:214187. Bolander s. 22 med not 54, Åkeson s. 44.
- "Telenova Compis", M3, januari 2008.
- Dagens Nyheter 25 september 1983, "Företag i storgräl om skoldatorer" (citerad i Boström-intervjun, inte läst).
- Computer Sweden 1988, "Compis med många ovänner" (citerad i M3, inte läst).
- www.df.lth.se/~pi/compis/compis_pdf2.html
- sv.wikipedia.org/wiki/Compis
- internetmuseum.se/tidslinjen/compis-introducerar-datorn-i-skolan/
- datamuseum.dk/wiki/TeleNova_COMPIS
- ddhf.dk/wiki/COMAL
- en.wikipedia.org/wiki/COMAL
- en.wikipedia.org/wiki/ABC_80
