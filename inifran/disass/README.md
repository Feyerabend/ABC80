# Kommenterade disassembleringar

Två ROM-program för ABC80, genomgångna instruktion för instruktion, med
namn och kommentarer på svenska. Dokumenten byggs ur samma anteckningar
som listningarna i boken *ABC80 inifrån*.

## `abc80/`

| Fil | Innehåll |
|---|---|
| `abc80rom.pdf` | ABC80:s BASIC och operativsystem, kommenterad listning |
| `abc80old.rom` | ROM:en som dokumentet följer, 16 KB, $0000–$3FFF |
| `abc80new.rom` | den senare versionen 9913 av samma ROM; skillnaderna står sist i dokumentet |

## `basicii/`

| Fil | Innehåll |
|---|---|
| `basicii80.pdf` | BASIC II för ABC80, kommenterad listning |
| `basicii80.rom` | ROM:en, 32 KB; koden ligger i $0000–$77FF, resten är $FF |
| `abc800mrom.rom` | ABC800 M:s BASIC II, som BASIC II för ABC80 bygger på (för jämförelse) |

BASIC II för ABC80 är ABC800:s BASIC II anpassad till ABC80:s skärm,
tangentbord och minneskarta. Skärmminnet på $7C00–$7FFF är kvar, så
ROM:en upptar 30 KB.

## Köra ROM:arna

Emulatorn i [`../verktyg/`](../verktyg/) startar båda:

    ../verktyg/abc80 -r abc80/abc80old.rom
    ../verktyg/abc80 -r basicii/basicii80.rom

---

## In English

Commented disassemblies (in Swedish) of two ABC80 ROMs, built from the
same notes as the listings in the book *ABC80 inifrån*:
`abc80/abc80rom.pdf` covers the original ABC80 BASIC and system ROM
(`abc80old.rom`, 16 KB; `abc80new.rom` is the later revision 9913), and
`basicii/basicii80.pdf` covers BASIC II for the ABC80 (`basicii80.rom`,
32 KB), an adaptation of the ABC800 M BASIC II (`abc800mrom.rom`,
included for comparison). The emulator in `../verktyg/` runs both.
