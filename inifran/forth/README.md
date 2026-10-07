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
| `e7i.bas` | laddprogram i ABC80-BASIC med tolken som DATA-rader |

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
hamnat i minnet. Programmet slutar med `3167 BYTES`. Ta bort det med `NEW`
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

## Köra tolken i emulatorn

Med emulatorn i [`../verktyg/`](../verktyg/) går det i ett kommando;
`RUN` tar ett par minuter i ABC80:s takt, men emulatorn kör fort så
länge den har tangenter och pauser kvar att skriva:

    ../verktyg/abc80 -K 10,10 -k 'POKE 65052,0,208\rNEW\r' -f e7i.bas \
        -k 'RUN\r\w150000\.NEW\r\w50\.Z=CALL(49152,3)\r'

Sedan är tolken igång i terminalen; Ctrl-] avslutar emulatorn.

## Bygga om

    python3 e7fc.py      # e7.fs, e7i.fs, e7k.inc -> e7.asm, e7r.asm, e7i.asm
    ../verktyg/z80asm e7.asm   # och e7r.asm, e7i.asm -> .bin
    python3 e7bas.py     # e7i.bin -> e7i.bas

Bara de ord som nås från ingångarna (`ENTRY`) tas med i `e7.asm` och
`e7r.asm`; i `e7i.asm` tas alla ord med och får huvuden, så att tolken
hittar dem.
