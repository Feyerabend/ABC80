<p>
<img src="assets/images/band1_framsida.jpg" alt="ABC80 inifrån, band 1: Maskinen och BASIC:en" width="240">
<img src="assets/images/band2_framsida.jpg" alt="ABC80 inifrån, band 2: Utbyggnaderna" width="240">
</p>

# ABC80

Material om den svenska hemdatorn ABC80 (Luxor, 1978): spelet
AIR-FIGHT från 1981, emulatorer, verktyg och texter, och materialet
till boken *ABC80 inifrån*.

Material on the Swedish home computer ABC80 (Luxor, 1978): the game
AIR-FIGHT from 1981, emulators, tools and essays, and the companion
material to the book *ABC80 inifrån* ("ABC80 from the inside").

## ABC80 inifrån

Boken *ABC80 inifrån* finns i två band hos BoD (Books on
Demand) samt hos flera traditionella bokhandlare för beställning.
Materialet till den ligger i [`inifran/`](inifran/).

- **Band 1: Maskinen och BASIC:en** (244 s., ISBN 978-91-8150-657-0):
  datorn själv och hela BASIC-tolken i ROM:en, rutin för rutin.
- **Band 2: Utbyggnaderna** (266 s., ISBN 978-91-8134-515-5):
  minneskort, skivminne, skrivare, IEC-bussen, Smartaid, CP/M och
  BASIC II för ABC80.

The book *ABC80 inifrån* (in Swedish) is published in two
volumes by BoD (Books on Demand): volume 1 on the machine and its BASIC
interpreter, volume 2 on the expansions. The companion material is in
[`inifran/`](inifran/). It is also available in traditional Swedish bookstores.

| Mapp / Folder | Innehåll | Contents |
|---|---|---|
| [`inifran/`](inifran/) | boken *ABC80 inifrån*: exempel, Forth, disassembleringar, assembler och emulator (i terminalen och i webbläsaren) | the book: examples, Forth, disassemblies, assembler and emulator (terminal and browser) |
| [`airfight/`](airfight/) | AIR-FIGHT 1981, MUSIC och KOL av Kristian Lidberg, uppsatsen om dem och portar till andra maskiner | AIR-FIGHT 1981, MUSIC and KOL by Kristian Lidberg, the essay about them, and ports to other machines |
| [`emulatorer/`](emulatorer/) | en enkel emulator i C och *ABC80 for Ants* för Raspberry Pi Pico 2 | a simple emulator in C and *ABC80 for Ants* for Raspberry Pi Pico 2 |
| [`kassett/`](kassett/) | verktyg mellan ABC80-band (WAV) och BASIC-text | tools between ABC80 tapes (WAV) and BASIC text |
| [`texter/`](texter/) | historik, minnen och en debattartikel från 1984 | history, memories and an opinion article from 1984 |
| [`abc802/`](abc802/) | benchmarks för ABC802 och BASIC II | benchmarks for ABC802 and BASIC II |
| [`abclisp/`](abclisp/) | *What if*: en Lisp för Z80 | *What if*: a Lisp for the Z80 |
| [`assets/`](assets/) | bilder och PDF:er som texterna visar | images and PDFs used by the texts |

Börja gärna med [uppsatsen om AIR-FIGHT](airfight/README.md) eller med
emulatorn i [`inifran/emulator/`](inifran/emulator/), som går att köra
direkt i webbläsaren: [https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html)
(ROM-filerna ligger inte i förrådet; `make webb` där hämtar dem, så att sidan går att bygga själv.)

A good place to start is [the essay on AIR-FIGHT](airfight/README.md),
or the emulator in [`inifran/emulator/`](inifran/emulator/), which runs
directly in the browser: [https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html)
(the ROM files are not in the repository; `make webb` there downloads them, so you can build the page yourself.)
