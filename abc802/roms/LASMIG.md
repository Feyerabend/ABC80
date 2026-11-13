# ROM:arna

Emulatorn behöver programmen som satt i ABC 802:s ROM-kretsar: BASIC II
(skriven av DIAB), DOS:en och teckengeneratorn. De är Luxors, och därför
ligger de inte här. Du hämtar dem själv från arkivet där de redan finns.

## Vill du bara pröva

Använd webbsidan, som är färdigbyggd och inte behöver något installerat:
<https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html>

## Vill du bygga själv

Stå i `abc802/` och skriv

    python3 roms/hamta.py        # på Windows: py roms\hamta.py

eller `make roms` i `emulator/`. Skriptet laddar ner ROM:arna hit och
kontrollerar att varje fil är exakt den som emulatorn är provad med.
Emulatorn (`emulator/abc802`, `emulator/sdl/abc802f`) letar efter dem i
`../roms/` räknat från där programmet ligger, så de ska ligga just här.

## Vad som hämtas

| Fil | Innehåll | Varifrån |
|---|---|---|
| `abc802rom.rom` | BASIC II (02-11, 12-11, 22-11) och DOS v.20 (32-31, 1984-04-03), 32 KB | abc80.net, fyra kretsar à 8 KB efter varandra |
| `abc802net.rom` | BASIC II och nätets DOS-PROM i stället för DOS:en (LUXNET, ABC802-dos-LUXNET-322n02), 32 KB; för `-N` | abc80.net, fyra kretsar à 8 KB efter varandra |
| `orig/abc802/ABC802-char.6490191-01.bin` | teckengeneratorn 6490191-01, 8 KB | abc80.net |
| `luxnet/l.bas` | inte en ROM: Dataindustriers L.BAS (X.19, 1985), som listar `UFD:` på nätet, som text; `make webb` gör L.BAC av den, som webbsidan har i `UFD:` | ABC-klubbens arkiv hos devili.iki.fi |

Arkiven: <https://www.abc80.net/archive/luxor/Prom/fw/ABC802/> och
<https://www.devili.iki.fi/pub/Luxor/software/ABC-klubben/abc800/luxnet/>

---

# The ROMs (English)

The emulator needs the programs in the ABC 802's ROM chips: BASIC II (by
DIAB), the DOS and the character generator. They belong to Luxor, so they
are not stored here. Run `python3 roms/hamta.py` from `abc802/` (or
`make roms` in `emulator/`) to download them from the abc80.net archive;
each file is checked against its SHA-256. It also fetches Dataindustrier's
L.BAS for the network from ABC-klubben's archive at devili.iki.fi, which
`make webb` turns into the L.BAC that the web page has in `UFD:`. To just
try the machine, use the ready-built web page above.
