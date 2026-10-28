# Storleken: hade fönstren fått plats på kortet?

Steg 7 i [PLAN.md](PLAN.md), 2026-10-28. Enligt annonsen har HR-80
4 KB EPROM och 18 KB RAM, varav bilden tar 16 KB. Frågan är om tolken
med de påhittade fönstren och musen hade fått plats där.
Ingen kompilator ger det som skrevs för hand 1984, så tolken
(`emulator/karna/hr80.c`) kompileras för fyra processorer och jämförs
med tolken före fönstren (4a0221c, efter steg 1). Siffrorna kommer från
[storlek.sh](storlek.sh), som kräver clang från LLVM (Homebrew `llvm`)
och SDCC (Homebrew `sdcc`). `hr80_rita` är inte medräknad, eftersom den
gör värdens bild; på kortet gör bildutmatningen det, och pekaren är
en överlagring där.

## Koden

Byte kod och konstanter, med ABC80:s teckensnitt (928 byte):

| processor | före fönstren | med fönster och mus | gånger |
|---|---:|---:|---:|
| Z80, SDCC | 7 903 | 19 694 | 2,5 |
| AVR (8 bitar), clang -Oz | 6 568 | 15 677 | 2,4 |
| MSP430 (16 bitar), clang -Oz | 4 370 | 9 678 | 2,2 |
| Cortex-M0 (Thumb), clang -Oz | 3 784 | 8 370 | 2,2 |

Fördelat på delarna (funktion för funktion, utan inlining i MSP430):

| del | Z80, SDCC | MSP430 |
|---|---:|---:|
| tolken (annonsens kommandon, med kapningen vid fönstren) | 8 906 | 4 262 |
| teckensnittet | 928 | 928 |
| fönstren (öppna, välja, lyfta, stänga, ramarna, kapningen) | 5 884 | 2 774 |
| musen, kön och frågan | 3 933 | 1 886 |

De största nya funktionerna i Z80 är ramen (`rita_fonster`, 1 440),
flytten med innehållet (`flytta`, 1 147), stängningen (`stang`, 973),
trycket med musen (`tryck`, 810) och lyftet (`lyft`, 699).

Tolken före fönstren är en bra kontroll. Den gör ungefär det annonsen
säger att HR-80 gjorde, och HR-80:s tolk fick plats i 4 KB.
Kompilerad blir den 3,8 KB (Thumb) till 4,4 KB (MSP430), så en
handskriven tolk av den tiden låg nära de siffrorna, ungefär hälften av
SDCC:s. Med samma förhållande tar fönstren och musen 4,5-5 KB till, och
hela tolken blir 9-10 KB.

**Svaret är nej.** Fönstren och musen mer än fördubblar tolken. Den hade
inte fått plats i 4 KB och knappast i ett 2764 (8 KB) heller, utan
behövt två 2764 eller ett 27128 (16 KB), som fanns 1984 men kostade
mer. För att få ned fönstren till ett 8 KB-kort hade man kunnat
- låta bli att flytta innehållet när ett fönster dras (`flytta`) och i
  stället be ABC80 rita om det med `R n`;
- rita ramen enklare, utan stängningsruta och fylld titelrad;
- stänga med ett kommando från ABC80 i stället för med musen.

## Arbetsminnet

De 2 KB som blir kvar bredvid bilden har räknats så här (hr80.h): 384
byte till fönstertabellen och kön, och resten, 1 664 byte, till
makrona. Men C-tolken har mer i arbetsminnet än så:

| | i C-tolken | på ett kort (uppskattning) |
|---|---:|---:|
| tillståndet (läge, färger, kommando, mus) | 106 | 106 |
| processorns stack (makron i åtta nivåer) | värdens | omkring 250 |
| fyllningens stack | 16 384 (4 096 poster) | 384 (128 poster om 3 byte) |
| makrotabellen (start, längd, slag för 126) | 630 | 252 (bara starten) |
| fönstren och kön | 396 | 384 |
| makrona | 1 664 | det som blir kvar, omkring 670 |

Fyllningens stack är den stora skillnaden. I proven (hr80prov a-h) används
högst två poster. En kam med ett streck i varannan punkt, där varje
mellanrum blir en egen gren, ger 129. Med 128 poster om tre byte (x med
nio bitar, y med åtta) räcker det för allt utom elaka figurer, och då
blir ytor ofyllda (C-tolken kastar redan grenar när stacken är full).
Makrotabellen behöver bara starten: längden är avståndet till nästa
start, och slaget kan vara makrots första byte.

**På ett riktigt kort hade makrona alltså fått omkring 670 byte, inte
1 664.** Före fönstren hade de fått omkring 1 050, så redan där var
"resten av de 2 KB" för generöst räknat, eftersom stackarna och
tabellen saknades i räkningen.

## Emulatorns kort

C-tolken har inte ändrats: den har fortfarande 1 664 byte makron och
4 096 poster i fyllningens stack, och proven är oförändrade. Emulatorn
visar alltså hur kortet kunde ha varit med 16 KB EPROM och lite mer
arbetsminne än annonsens 18 KB, inte ett kort som hade gått att köpa
1984.
