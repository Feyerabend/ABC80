# ABC 802 och ABC 800-serien

*(English below.)*

Luxors ABC 800-serie var de svenska kontors- och skoldatorerna från 1981
till mitten av åttiotalet. Alla hade samma grund: en Z80A på 3 MHz, BASIC
II från Dataindustrier (DIAB) i ROM, ABC-bussen och samma skivkort.
Maskinerna skilde sig i bilden, minnet och lådan.

| Maskin | År | Vad som skiljer |
|---|---|---|
| ABC 800 C | 1981 | färg, 40 tecken per rad efter text-tv:s modell; datorkortet i tangentbordet |
| ABC 800 M | 1981 | svartvit, 80 tecken med bildkretsen 6845, högupplöst grafik som tillval |
| ABC 802 | 1983 | ”lilla ABC 800”: 64 KB RAM, DOS:en i ROM, datorkortet i bildskärmen; de nedre 32 KB är RAM-skiva eller plats för CP/M |
| ABC 806 | 1983 | ”stora ABC 800”: mer minne som RAM-skiva och bättre grafik |
| ABC 1600 | 1985 | en annan sorts maskin: Motorola 68008 och ABCenix, ett Unix; se [`cpm/`](cpm/) |

Det som finns här utgår från ABC 802, men det mesta gäller hela serien.
BASIC II är i stort sett densamma i ABC 800, 802 och 806, DOS:en i ABC
802 är byte för byte densamma som i ABC 800 M, och skivkortet är
detsamma; bara DOS-PROM:en skiljer (ABC 6-XX, 32-XX och 66-XX).
Forth och Pascal här är skrivna för ABC 802:s minne och går inte utan
vidare på de andra.

Börja gärna med boken *ABC 802 inifrån* i [`inifran/`](inifran/). Dess
README visar vilka mappar som hör till vilka kapitel: maskinen och
listningen, tangentbordshäftet, CP/M, Pascal och Forth med
bruksanvisningarna, och tidsproven. Varje mapp har en egen README (i
`roms/` heter den `LASMIG.md`).

| Mapp | Innehåll |
|------|----------|
| [`maskinen/`](maskinen/) | maskinen för programmeraren: kretsarna, minneskartan och bankväxlingen, portarna, avbrotten och kontakterna |
| [`listning/`](listning/) | hela ROM:en, BASIC II och DOS v.20, kommenterad instruktion för instruktion (PDF, 330 sidor) |
| [`emulator/`](emulator/) | emulatorn, i C: batchläget, ett fönster med SDL3 och en [webbsida](https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html) som går att pröva direkt; med skivor för tidsproven, CP/M 2.2, Forth och Pascal |
| [`roms/`](roms/) | `hamta.py`, som hämtar ROM:arna från abc80.net (de ligger inte här) |
| [`benchmark/`](benchmark/) | tidsproven: 35 BASIC-program (BM1–BM8 ur Personal Computer World, Ahl, Noel, Mandelbrot, primtal, såll, ormspel och fler), tiderna på ABC 802 mot BBC Micro, och vad som rättades 2026 |
| [`forth/`](forth/) | en Forth i RAM för BASIC II och DOS:en, med kontorsprogrammen ORD, KALKYL och VERK skrivna i den |
| [`pascal/`](pascal/) | en Pascal med p-kod: kompilatorn skriven i Pascal, tolken i Z80-assembler, för BASIC-läget och för CP/M; och anteckningar om Luxors Pascal 800 |
| [`verktyg/`](verktyg/) | `z80asm.c`, assemblern som Forth och Pascal byggs med |
| [`cpm/`](cpm/) | CP/M, DataBoard 4680 och Unix: ABC 802 bland samtidens standarder (engelska) |
| [`inifran/`](inifran/) | boken *ABC 802 inifrån*, 103 sidor: [`abc802.pdf`](inifran/abc802.pdf) |
| [`manualer/`](manualer/) | bruksanvisningarna till [ORD](manualer/ord.pdf), [KALKYL](manualer/kalkyl.pdf), [VERK](manualer/verk.pdf) och [Pascal](manualer/pascal.pdf) |
| [`tangentbord/`](tangentbord/) | häftet om tangentborden ABC 55, ABC 77 och ABC 99 och musen ABC R8: [`tangentbord.pdf`](tangentbord/tangentbord.pdf) |
| `bilder/` | bilderna till texterna |

Programmen i `benchmark/` är provade i emulatorn: de som är anpassade
till ABC 802 ligger på skivan `emulator/skivor/tidsprov.img`, som
webbsidan har i från början. Knappen *Program* sätter i programskivan
(`emulator/skivor/program.img`) med Forth och ordbehandlaren ORD
(`RUN ORD`).

## Bygga

Allt byggs med en C-kompilator och Python 3. ROM:arna hämtas först:

    python3 roms/hamta.py
    cd emulator && make          # emulatorn i batchläget
    sh forth/gor.sh              # Forth, KALKYL och VERK: forth.img och program.img
    sh pascal/bygg.sh            # Pascal: pascal.img

`forth/gor.sh` och `pascal/bygg.sh` bygger assemblern ur `verktyg/` första
gången och använder emulatorn för att skriva programmen på skivorna.
Skivorna blir desamma som de färdiga i `emulator/skivor/`.

---

# ABC 802 and the ABC 800 series (English)

Luxor's ABC 800 series were the Swedish office and school computers from
1981 to the mid-eighties. They shared a Z80A at 3 MHz, BASIC II by
Dataindustrier (DIAB) in ROM, the ABC bus and the same disk controller,
and differed in the display, the memory and the case.

| Machine | Year | What differs |
|---|---|---|
| ABC 800 C | 1981 | colour, 40 characters per line in teletext style; the board sits in the keyboard |
| ABC 800 M | 1981 | monochrome, 80 characters with the 6845 CRTC, optional high-resolution graphics |
| ABC 802 | 1983 | the "small ABC 800": 64 KB of RAM, the DOS in ROM, the board in the monitor; the lower 32 KB is a RAM disk or room for CP/M |
| ABC 806 | 1983 | the "large ABC 800": more memory as a RAM disk and better graphics |
| ABC 1600 | 1985 | a different kind of machine: Motorola 68008 and ABCenix, a Unix; see [`cpm/`](cpm/) |

The material starts from the ABC 802, but most of it applies to the
whole series. BASIC II is much the same in the ABC 800, 802 and 806, the
ABC 802's DOS is byte for byte that of the ABC 800 M, and the disk
controller is the same; only the DOS PROM differs. The Forth and Pascal
here are written for the ABC 802's memory map.

A good place to start is the book in [`inifran/`](inifran/); its README
maps the chapters to the other folders. Each folder has its own README
(in `roms/` it is `LASMIG.md`).

| Folder | Contents |
|--------|----------|
| [`maskinen/`](maskinen/) | the machine for the programmer: chips, memory map and bank switching, ports, interrupts, connectors (Swedish, with an English summary) |
| [`listning/`](listning/) | the whole ROM, BASIC II and DOS v.20, commented instruction by instruction (PDF, 330 pages, in Swedish) |
| [`emulator/`](emulator/) | the emulator in C: batch mode, an SDL3 window and a [web page](https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html) to try at once; with disks for the benchmarks, CP/M 2.2, Forth and Pascal |
| [`roms/`](roms/) | `hamta.py`, which fetches the ROMs from abc80.net (they are not stored here) |
| [`benchmark/`](benchmark/) | 35 BASIC benchmark programs, times on the ABC 802 against the BBC Micro, and the corrections made in 2026 |
| [`forth/`](forth/) | a Forth in RAM for BASIC II and the DOS, with the office programs ORD, KALKYL and VERK written in it |
| [`pascal/`](pascal/) | a p-code Pascal: the compiler in Pascal, the interpreter in Z80 assembly, for BASIC mode and CP/M; and notes on Luxor's Pascal 800 |
| [`verktyg/`](verktyg/) | `z80asm.c`, the assembler used to build Forth and Pascal |
| [`cpm/`](cpm/) | CP/M, DataBoard 4680 and Unix: the ABC 802 among the standards of its day |
| [`inifran/`](inifran/) | the book *ABC 802 inifrån* (PDF, in Swedish): [`abc802.pdf`](inifran/abc802.pdf) |
| [`manualer/`](manualer/) | the manuals for ORD, KALKYL, VERK and Pascal (PDF, in Swedish) |
| [`tangentbord/`](tangentbord/) | the booklet on the keyboards ABC 55, ABC 77 and ABC 99 and the ABC R8 mouse (PDF, in Swedish) |

To build: `python3 roms/hamta.py`, then `make` in `emulator/`, `sh
forth/gor.sh` and `sh pascal/bygg.sh`. The disks come out identical to
the ready-made ones in `emulator/skivor/`.
