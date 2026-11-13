# Bruksanvisningarna

*(English below.)*

Bruksanvisningar till programmen på skivorna i
[`../emulator/skivor/`](../emulator/skivor/). De är skrivna 2026 i den
stil som häftena till kontorsprogrammen hade i början av åttiotalet. Varken
häftena eller programmen är från den tiden. Programmen är skrivna för boken
*ABC 802 inifrån* och går både i emulatorn och på en riktig ABC 802.

| Häfte | Program | Sidor | Förebild |
|---|---|---|---|
| [`ord.pdf`](ord.pdf) | ORD, ordbehandling (`RUN ORD`) | 19 | Luxors ORD 800 (1981) |
| [`kalkyl.pdf`](kalkyl.pdf) | KALKYL, kalkylprogram (`RUN KALKYL`) | 27 | CALC och SPRIT ur ABC-klubbens programbank |
| [`verk.pdf`](verk.pdf) | VERK, text och kalkyl i samma dokument (`RUN VERK`) | 28 | Jef Raskins Canon Cat |
| [`pascal.pdf`](pascal.pdf) | Pascal, kompilator och p-kod | 25 | Wirths Pascal-P och UCSD Pascal |

ORD, KALKYL och VERK ligger på programskivan, `program.img`, och är
skrivna i Forth; källorna finns i [`../forth/`](../forth/). Pascal ligger
på Pascal-skivan, `pascal.img`, och källorna finns i
[`../pascal/`](../pascal/). På [webbsidan](https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html)
sätter knappen *Program* i programskivan.

Häftena gäller programmen som de är i november 2026. Skärmbilderna är
tagna i emulatorn.

---

## The manuals (English)

Manuals, in Swedish, for the programs on the disks in
[`../emulator/skivor/`](../emulator/skivor/), written in 2026 in the style
of early-eighties office software booklets. Neither the booklets nor the
programs are period documents: the programs were written for the book
*ABC 802 inifrån* and run both in the emulator and on a real ABC 802.

- `ord.pdf`: ORD, a word processor (`RUN ORD`).
- `kalkyl.pdf`: KALKYL, a spreadsheet (`RUN KALKYL`).
- `verk.pdf`: VERK, text and calculation in one document, after the Canon Cat (`RUN VERK`).
- `pascal.pdf`: a Pascal compiler and p-code interpreter.

ORD, KALKYL and VERK are written in Forth and live on `program.img`, with
the sources in [`../forth/`](../forth/). The Pascal is on `pascal.img`, with
the sources in [`../pascal/`](../pascal/).
