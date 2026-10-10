# En liten Forth för ABC80

Källorna till exempel 7 i *ABC80 inifrån*, band 2 (kapitlet Exempel,
"En liten Forth"). Boken beskriver hur Forth-systemet är byggt; här finns
filerna, så att det kan köras och ändras.

Forth-systemet har två delar:

- färdigkompilerade ord som anropas från BASIC med `CALL`, som en krets:
  `MAIN` (ingång 0), `TIO` (1) och `FIND` (2), som skriver numren på de
  BASIC-rader där en text finns
- en tolk som läser rader från tangentbordet och låter användaren
  definiera egna ord

Det är en direkt trådad Forth (DTC). Datastacken är Z80:s stack (SP), och
det översta värdet ligger i BC. Returstacken pekas ut av IX, och DE pekar
på nästa adress i tråden.

## Filerna

| Fil | Innehåll |
|---|---|
| `e7k.inc` | kärnan i Z80-assembler (NEXT, DOCOL, orden i maskinkod) |
| `e7.fs` | Forth-källan till de färdiga orden |
| `e7i.fs` | Forth-källan till tolken |
| `e7fc.py` | korskompilatorn: läser kärnan och Forth-källan, skriver `.asm` |
| `e7bas.py` | skriver laddprogrammet `e7i.bas` ur `e7i.bin` |
| `e7.asm`, `e7.bin` | de färdiga orden på $4000 (för en krets) |
| `e7r.asm`, `e7r.bin` | de färdiga orden på $C000 (i RAM) |
| `e7i.asm`, `e7i.bin` | orden och tolken på $C000, 3167 bytes |
| `e7ip.asm`, `e7ip.bin` | samma tolk på $4000, för en krets (ett 2732-EPROM) |
| `e7c.fs` | tillägg till tolken: de egna orden i CMOS-minne (inte med i boken) |
| `e7c.asm`, `e7c.bin` | kretsen med tillägget, på $4000 |
| `e7i.bas` | laddprogram i ABC80-BASIC med tolken som DATA-rader |
| `FORTH.wav` | tolken på band, sparad med `SAVE CAS:FORTH` (2 min 15 s) |

`.asm`- och `.bin`-filerna skrivs av verktygen, men de ligger med så att
inget behöver byggas för att prova.

## Köra tolken på en ABC80

Flytta upp BASIC-programmens början (BOFA) till $D000, så att RAM:et
från $C000 blir ledigt:

    POKE 65052,0,208
    NEW

Skriv sedan in `e7i.bas` (eller ladda det i en emulator) och kör det med
`RUN`. Varje DATA-rad har 30 bytes i hex och deras summa, och en
felskriven rad ger `FEL I RAD` och radens nummer innan något av den har
hamnat i minnet. Har BOFA inte flyttats upp slutar programmet direkt med
`SKRIV POKE 65052,0,208 OCH NEW`, i stället för att lägga tolken över
sig självt. Annars slutar det med `3167 BYTES`. Ta bort det med `NEW`
och starta tolken:

    Z=CALL(49152,3)

    : KV DUP + ;
    7 KV . 14 OK
    BYE

`BYE` går tillbaka till BASIC, och `Z=CALL(49152,4)` fortsätter med de
egna orden kvar. De färdiga orden körs med `CALL(49152,n)`, n = 0–2.
Med `POKE 65052,0,192` och `NEW` får BASIC tillbaka minnet.

På ABC80 skrivs `@` som `É` (den svenska varianten av ASCII), så hämtordet
skrivs `É` i tolken: `VARIABLE V 42 V ! V É .`

## Från bandet

`FORTH.wav` är tolken sparad som ett BASIC-program, med BOFA flyttad
till $D000. Programmet flyttar sig när det laddas, men BOFA måste flyttas
upp först, så att tolken får plats (annars skriver programmet
`SKRIV POKE 65052,0,208 OCH NEW`):

    POKE 65052,0,208
    NEW
    RUN CAS:

Spela upp bandet (i webbläsarens emulator: *Lägg i band…*, i terminalen
`-T FORTH.wav`). Det tar 2 min 15 s att läsa in bandet, och sedan
lägger programmet tolken i minnet på knappt en minut i ABC80:s takt.
När `3167 BYTES` har skrivits ut skriver du `NEW` och `Z=CALL(49152,3)`
som ovan. Skriv inte för fort till tolken: den hinner med en människa
men inte alltid en klistrad rad.

## Som en krets

`e7ip.bin` är samma tolk på $4000, där en krets som Smartaid satt, i
den del av minneskartan som annars är tom. Den ryms i ett 2732-EPROM
(4 KB). Kretsen kan inte skrivas i, så nya ord läggs i RAM från $C000
och uppåt. De variabler som tolken behöver ligger redan i de dolda
bytena i bildminnet. BOFA flyttas därför upp som förut, men inget behöver
laddas:

    POKE 65052,0,208
    NEW
    Z=CALL(16384,3)

`Z=CALL(16384,4)` fortsätter med de egna orden kvar, och
`CALL(16384,n)`, n = 0–2, kör de färdiga orden. Smartaid från 1981
startades också med `CALL`. Senare kretsar tog över maskinen redan vid
start, genom att sätta I-registret så att tangentbordsavbrottet gick
till kretsen. Det gör inte den här.

I emulatorn (byggd med `make` i `../emulator/`) sätts kretsen i med `-l`:

    ../emulator/abc80 -l e7ip.bin@4000 -k 'POKE 65052,0,208\rNEW\rZ=CALL(16384,3)\r'

I webbläsarens emulator (`../emulator/webb/abc80.html`) väljer du
*Forth-kretsen*, eller öppnar sidan med `?krets=forth`:
[abc80.html?krets=forth](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=forth).

## Med CMOS-minne

Super Smartaid hade 2 KB CMOS-minne med batteri på $5000–$57FF, så att det
som stod där fanns kvar när datorn slogs av. `e7c.bin` är kretsen med ett
tillägg, `e7c.fs`, som lägger de egna orden där. Det är en
vidareutveckling som inte finns med i boken. `e7c.fs` läses efter `e7.fs`
och `e7i.fs` och definierar om `TIB`, `QUIT` och `FORTH`. Radbufferten
ligger också i CMOS-minnet, så ingen `POKE` behövs:

    Z=CALL(16384,3)
    NY ORDLISTA
    : DUBBEL DUP + ;  OK

Efter varje rad utan fel skrivs ett huvud först i minnet: ett märke (det
sista huvudet i kretsen), DP, LAST och en kontrollsumma. Nästa gång
fortsätter tolken med orden kvar, om huvudet stämmer; annars börjar den
med en tom ordlista. `EMPTY` tömmer ordlistan, och en rad som inte får
plats tas bort med `CMOS FULLT`.

Emulatorn sparar CMOS-minnet i en fil med `-M`:

    ../emulator/abc80 -l e7c.bin@4000 -M cmos.bin -k 'Z=CALL(16384,3)\r'

I webbläsaren väljer du *Forth med CMOS* (eller `?krets=cmos`). Där
sparas minnet i webbläsaren och finns kvar när sidan laddas om.

## Köra tolken i emulatorn

Med emulatorn i [`../verktyg/`](../verktyg/) går det i ett kommando;
`RUN` tar ett par minuter i ABC80:s takt, men emulatorn kör fort så
länge den har tangenter och pauser kvar att skriva:

    ../verktyg/abc80 -K 10,10 -k 'POKE 65052,0,208\rNEW\r' -f e7i.bas \
        -k 'RUN\r\w150000\.NEW\r\w50\.Z=CALL(49152,3)\r'

Sedan är tolken igång i terminalen; Ctrl-] avslutar emulatorn.

## Bygga om

    python3 e7fc.py      # e7.fs, e7i.fs, e7c.fs, e7k.inc -> e7.asm, e7r.asm, e7i.asm, e7ip.asm, e7c.asm
    ../verktyg/z80asm e7.asm   # och e7r.asm, e7i.asm, e7ip.asm, e7c.asm -> .bin
    python3 e7bas.py     # e7i.bin -> e7i.bas

Bara de ord som nås från ingångarna (`ENTRY`) tas med i `e7.asm` och
`e7r.asm`; i `e7i.asm`, `e7ip.asm` och `e7c.asm` tas alla ord med och får huvuden, så att tolken
hittar dem.
