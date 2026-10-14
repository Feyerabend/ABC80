# Kommenterade disassembleringar

Två ROM-program för ABC80, genomgångna instruktion för instruktion, med
namn och kommentarer på svenska. Dokumenten byggs ur samma anteckningar
som boken *ABC80 inifrån*, men bokens text är inte med: här finns bara
listningarna och tabellerna till dem.

Här finns inte själva koden, bara mallarna: listningarna med adresser,
namn och kommentarer, men utan byten och instruktionerna. Koden är
upphovsrättsligt skyddad och hör till sina upphovsmän. `bygg.sh` hämtar
ROM:en, fyller i koden ur den och bygger PDF:en.

## Bygga

    ./bygg.sh abc80        # ut/abc80rom.pdf, ABC80:s BASIC och operativsystem
    ./bygg.sh basicii80    # ut/basicii80.pdf, BASIC II för ABC80

Det kräver `python3`, `curl` och [tectonic](https://tectonic-typesetting.github.io)
(`brew install tectonic`). Första gången hämtar tectonic de paket och
typsnitt som behövs.

Utan ROM på kommandoraden hämtas den:

| Dokument | ROM | Hämtas från | sha256 |
|---|---|---|---|
| `abc80rom.pdf` | ABC80:s BASIC-ROM, version 11273, 16 KB, $0000–$3FFF | kretsarna ZA3506–ZA3509 ur [abc80.net:s arkiv](https://www.abc80.net/archive/luxor/Prom/fw/ABC80/) | `b1e5e574…4b534396` |
| `basicii80.pdf` | BASIC II för ABC80, 32 KB, koden på $0000–$736F | `basicii_abc80_1.bin` och `_2.bin` ur [abc80.net:s arkiv](https://www.abc80.net/archive/luxor/Prom/fw/ABC80/) | `778e7c8e…09a31fe26` |

Har du ROM:en själv går det att ange den: `./bygg.sh abc80 min.rom`.
Den måste vara exakt samma version som mallen gäller; `fyll.py` jämför
kontrollsumman och stannar annars.

BASIC II för ABC80 är ABC800:s BASIC II anpassad till ABC80:s skärm,
tangentbord och minneskarta. Skärmminnet på $7C00–$7FFF är kvar, så
koden upptar 30 KB. ROM-filen i abc80sim har samma kod, men i den
oanvända svansen $7379–$77FF rester av äldre kod där arkivets har $FF.

## Filerna

| Fil | Innehåll |
|---|---|
| `bygg.sh` | hämtar ROM:en, fyller i mallen och bygger PDF:en |
| `fyll.py` | fyller i mallen: avkodar instruktionen eller datan på varje rads adress |
| `z80dec.py` | Z80-avkodaren som `fyll.py` använder |
| `abc80-mall.tex`, `basicii80-mall.tex` | mallarna |
| `abc80rom.tex`, `kapitel/`, `*.sty` | ramen till ABC80-dokumentet: inledningen och tabellerna till listningen (systemvariabler, felkoder, tokens, symboler) |
| `kwfix.py` | sätter kod i citat i skrivmaskinsstil, som i boken |

I mallen har varje kodrad kvar adress, namn och kommentar, medan byten
står som `\By{?}` och instruktionen som `\Fy{slag}{tips}`. Slaget säger om
det är en instruktion eller data (DEFB, DEFW, DEFM), och tipsen vilka namn
som ska stå i stället för talen. Se `fyll.py`.

## Köra ROM:arna

Emulatorn i [`../verktyg/`](../verktyg/) startar båda; ROM:arna ligger i
[`../emulator/roms/`](../emulator/roms/):

    ../verktyg/abc80 -r ../emulator/roms/abc80old.rom
    ../verktyg/abc80 -r ../emulator/roms/basicii80.rom -m 32

---

## In English

Commented disassemblies (in Swedish) of two ABC80 ROMs, built from the
same notes as the listings in the book *ABC80 inifrån*. Only templates are
published here: the listings with addresses, names and comments, but
without the code itself, which is copyrighted by its authors. `bygg.sh`
downloads the ROM, fills the code in from it and builds the PDF:
`./bygg.sh abc80` gives `ut/abc80rom.pdf` (the ABC80 BASIC and system
ROM, revision 11273), `./bygg.sh basicii80` gives `ut/basicii80.pdf`
(BASIC II for the ABC80). It needs `python3`, `curl` and tectonic. A ROM
of your own can be given as a second argument; it must match the
template's SHA-256.
