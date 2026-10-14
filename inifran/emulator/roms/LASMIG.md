# ROM:arna

Emulatorn behöver programmen som satt i ABC80:s ROM-kretsar: BASIC-tolken,
ABC-DOS och några tillbehör. De är skrivna av Luxor och andra, och därför
ligger de inte här. Du hämtar dem själv från de arkiv där de redan finns.

## Vill du bara pröva

Använd webbsidan. Den är färdigbyggd och behöver ingenting installerat:
<https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html>

## Vill du bygga själv

Stå i `emulator/` och skriv

    make roms

eller, utan make (Python 3 räcker):

    python3 roms/hamta.py        # på Windows: py roms\hamta.py

Skriptet laddar ner ROM:arna till den här mappen och kontrollerar att varje
fil är exakt den som emulatorn är provad med. Sedan fungerar `make`,
`make webb` (din egen webbsida) och exemplen i README. `make webb` och
`make prov` hämtar ROM:arna själva om de saknas. Assemblern och emulatorn i
`../../verktyg/` och försöken i `../../utanfor/` läser dem också härifrån.

## Vad som hämtas

| Fil | Innehåll | Varifrån |
|---|---|---|
| `abc80old.rom` | ABC80:s BASIC, den gamla versionen (11273), 16 KB | abc80.net, kretsarna ZA3506–ZA3509 |
| `abc80new.rom` | ABC80:s BASIC, den nya versionen (9913), 16 KB | abc80.net, kretsarna ZA3506–ZA3509 |
| `basicii80.rom` | BASIC II för ABC80, 32 KB | abc80sim |
| `abcdos80.rom` | ABC-DOS för disketten, 4 KB på $6000 | abc80.net (`ABCDOS80.bin`) |
| `p40.rom` | drivrutinen till nålskrivaren P40, 1 KB på $7800 | abc80.net (`P40.bin`) |
| `tkn80-iii.rom` | MyAB:s 80-teckenskort TKN80-III, 4 KB | abc80.net (`TKN80-III.bin`) |

Arkiven:

- abc80.net: <https://www.abc80.net/archive/luxor/Prom/fw/ABC80/>
- abc80sim: <https://github.com/hpax/abc80sim>

Den kommenterade listningen i `../../disass/` byggs ur den gamla ABC80-ROM:en
och ur BASIC II. Hur de används i emulatorn (`-r`, `-l`, `-tkn80`) står i
[`../README.md`](../README.md).
