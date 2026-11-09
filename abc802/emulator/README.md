# Emulatorn till ABC 802

*(English below.)*

En emulator till Luxors ABC 802 (1983): Z80:n på 3 MHz, 6845:an och
bildminnet, tangentbordet, DART:en och SIO:n med kassetten och
kontakterna, CTC:n, ABC-bussen med skivkortet (DOS v.20 i ROM:en) och
skrivarkortet. Kärnan är ren C och används av tre värdar: batchläget,
ett fönster med SDL3 och en webbsida (WebAssembly).

## Pröva utan att bygga

Webbsidan är färdigbyggd och behöver ingenting installerat:
<https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html>

Skivan med tidsproven ur [`../benchmark/`](../benchmark/) sitter i från
början: `RUN B1` kör det första, och `RUN LIB` (Luxors LIB) visar vad
som finns på skivan.
Knapparna CP/M och Forth sätter i de skivorna och startar om. `?cpm=pa`
startar med CP/M, `?sprak=en` på engelska, `?tecken=40` med 40 tecken per
rad och `?k=...` skriver tangenter efter starten.

## Bygga

ROM:arna är Luxors och ligger inte här. Hämta dem först (Python 3):

    python3 ../roms/hamta.py     # eller: make roms

Sedan, i den här mappen:

    make                 # abc802, batchläget
    make webb            # webb/abc802.html (kräver clang och wasm-ld)
    cd sdl && make       # abc802f, fönstret (kräver SDL3, se sdl/README.md)

Batchläget kör maskinen, skriver tangenterna och skriver sedan skärmen på
standard ut:

    ./abc802 -D skivor/tidsprov.img -t 5000 -k '\w1500RUN B1\r'
    ./abc802 -D cpm/a.img -t 4000 -k '\w2500DIR\r'
    ./abc802 -p bild.png -t 2000

Alla flaggor står överst i [`vard/abc802.c`](vard/abc802.c): `-k` och
`-f` för tangenter och textfiler, `-D`/`-DP`/`-DS` för skivorna, `-T`/`-U`
för kassetten, `-A`/`-B`/`-a`/`-b` för kontakterna, `-P` för
skrivarkortet, `-p` för en PNG och `-i` för antalet instruktioner och
tiden. Skivorna skrivs inte tillbaka utan `-DS`.

Nätet LUXNET: med ROM:en `../roms/abc802net.rom` (BASIC II och nätets
DOS-PROM) och `-N mapp` spelar emulatorn nodkortet och centralen, och
mappen blir kontots `UFD:`, med undermapparna som kataloger för `CD`.
Det som ändras skrivs tillbaka efter körningen, och `-NL` visar
trafiken:

    ./abc802 -r ../roms/abc802net.rom -N mapp -NL -k '10 PRINT "HEJ"\rSAVE UFD:PROV\r'

På webbsidan sätter panelen *Nätet* i nodkortet och startar om med
nät-ROM:en; filerna i `UFD:` ligger då i webbläsaren, där de läggs in,
hämtas och tas bort, och trafiken visas i panelen.

## Mapparna

| Mapp | Innehåll |
|---|---|
| `karna/` | kärnan: Z80 (`z80.c`), maskinen och minnet (`abc802.c`), skärmen (`skarm802.c`), CTC, SIO/DART, kassetten, ABC-bussen, skivkortet och skrivarkortet |
| `vard/` | batchläget (`abc802.c`) och PNG-bilden |
| `sdl/` | fönstret med SDL3 (`fonster.c`) |
| `webb/` | webbsidan: `mall.html`, `abc802.js` och värden i C (`webb.c`, `libc.c`); `bygg.sh` sätter ihop `abc802.html` |
| `skivor/` | `tidsprov.img` (tidsproven och Luxors systemprogram BASICINI.SYS, CMDINT.SYS, LIB), `tom.img` (tom), `forth.img` (Forth för ABC 802) |
| `cpm/` | CP/M 2.2: start-PROM:en (`start.asm`), BIOS:en (`bios.asm`), Digital Researchs CCP och BDOS (`dr/`), systemskivan `a.img` med DR:s program, editorn SKRIV, BBC BASIC och Zork I, och en tom `b.img` |

Skivorna är färdiga. Källorna till CP/M-delen finns att läsa, men verktygen
som byggde skivorna (assemblern och skivskripten) ligger inte här.

## Vad som kommer varifrån

- ROM:arna: abc80.net, se [`../roms/LASMIG.md`](../roms/LASMIG.md).
- CP/M 2.2: Digital Research, fritt att använda och sprida enligt DRDOS,
  Inc. (2022), se [`cpm/dr/LASMIG.md`](cpm/dr/LASMIG.md).
- BBC BASIC (Z80): R. T. Russell, zlib-licensen, [`cpm/bbc/licence.txt`](cpm/bbc/licence.txt).
- Zork I: Microsoft, MIT-licensen, [`cpm/zork/LICENSE`](cpm/zork/LICENSE).
- Luxors systemprogram på `tidsprov.img`: ur Luxors egna skivor.
- Resten (kärnan, värdarna, BIOS:en, ZTOLK, SKRIV, Forth): skrivet för
  ABC 802 inifrån. Delar bygger på ABC80-emulatorn i
  [`../../inifran/emulator/`](../../inifran/emulator/), som källorna nämner
  som `abc80/emulator`.

## Hur nära maskinen

Tidsproven går i stort sett lika fort som på en riktig ABC 802; tabellen
står i [`../benchmark/README.md`](../benchmark/README.md#the-machine-and-the-emulator).

---

# The ABC 802 emulator (English)

An emulator of the Luxor ABC 802 (1983): the 3 MHz Z80, the 6845 and video
memory, the keyboard, DART and SIO with cassette and serial ports, the CTC,
and the ABC bus with the disk card (DOS v.20 in ROM) and the printer card.
The core is plain C, shared by three hosts: a batch mode, an SDL3 window
and a web page (WebAssembly).

**Try it** without building:
<https://feyerabend.github.io/ABC80/abc802/emulator/webb/abc802.html>
(`?sprak=en` for English). The benchmark disk from
[`../benchmark/`](../benchmark/) is inserted at start; the CP/M and Forth
buttons insert those disks. The *Network* panel puts in the LUXNET node card
and restarts with the network ROM; the files in `UFD:` are then kept in the
browser.

**Build:** the ROMs belong to Luxor and are not stored here; fetch them with
`python3 ../roms/hamta.py` (or `make roms`). Then `make` builds the batch
emulator `abc802`, `make webb` the web page (needs clang and wasm-ld), and
`make` in `sdl/` the window (needs SDL3). The command-line options are
listed at the top of [`vard/abc802.c`](vard/abc802.c); the comments and
documentation are in Swedish.

The disk images in `skivor/` and `cpm/` are ready-built. CP/M 2.2 is Digital
Research's (free to use and distribute per DRDOS, Inc., 2022); BBC BASIC is
R. T. Russell's (zlib licence); Zork I is Microsoft's (MIT licence).
