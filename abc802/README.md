# ABC 802: tidsprov, emulator, kommenterad ROM, CP/M

*(English below.)*

Luxors ABC 802 (1983) var en ABC 800 med 64 KB RAM, BASIC II från DIAB och
DOS:en i ROM: en kontorsdator med bärnstensgul skärm, som också kunde köra
CP/M. Här finns det som hör till den: tidsprov, en emulator, den
kommenterade ROM:en och en text om CP/M och dess samtida.

| Mapp | Innehåll |
|------|----------|
| [`benchmark/`](benchmark/) | tidsproven: 35 BASIC-program (BM1–BM8 ur Personal Computer World, Ahl, Noel, Mandelbrot, primtal, såll, ormspel och fler), tiderna på ABC 802 mot BBC Micro, och vad som rättades 2026 |
| [`emulator/`](emulator/) | emulatorn, i C: batchläget, ett fönster med SDL3 och en [webbsida](https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html) som går att pröva direkt; med skivor för tidsproven, CP/M 2.2 och Forth |
| [`roms/`](roms/) | `hamta.py`, som hämtar ROM:arna från abc80.net (de ligger inte här) |
| [`listning/`](listning/) | hela ROM:en, BASIC II och DOS v.20, kommenterad instruktion för instruktion (PDF, 330 sidor) |
| [`cpm/`](cpm/) | CP/M, DataBoard 4680 och Unix: ABC 802 bland samtidens standarder (engelska) |
| `bilder/` | bilderna till texterna |

Programmen i `benchmark/` är provade i emulatorn: de som är anpassade
till ABC 802 ligger på skivan `emulator/skivor/tidsprov.img`, som
webbsidan har i från början.

---

# ABC 802 (English)

The Luxor ABC 802 (1983) was an ABC 800 with 64 KB of RAM, BASIC II by
DIAB and the DOS in ROM: an office machine with an amber screen, which
could also run CP/M. This folder holds benchmarks, an emulator, the
commented ROM and an essay on CP/M and its contemporaries.

| Folder | Contents |
|--------|----------|
| [`benchmark/`](benchmark/) | 35 BASIC benchmark programs, times on the ABC 802 against the BBC Micro, and the corrections made in 2026 |
| [`emulator/`](emulator/) | the emulator in C: batch mode, an SDL3 window and a [web page](https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html) to try at once; with disks for the benchmarks, CP/M 2.2 and Forth |
| [`roms/`](roms/) | `hamta.py`, which fetches the ROMs from abc80.net (they are not stored here) |
| [`listning/`](listning/) | the whole ROM, BASIC II and DOS v.20, commented instruction by instruction (PDF, 330 pages, in Swedish) |
| [`cpm/`](cpm/) | CP/M, DataBoard 4680 and Unix: the ABC 802 among the standards of its day |
