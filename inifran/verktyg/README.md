# Verktygen: assemblern och emulatorn

Två små program i C, skrivna till boken *ABC80 inifrån*, som bokens
exempel byggs och provas med:

- **`z80asm`** – en assembler för Z80. Den läser en källfil och skriver
  bytena som en binärfil (för en EPROM eller en emulator), som Intel HEX
  och som en listning. Manual: [`z80asm.md`](z80asm.md).
- **`abc80`** – en ABC80 i terminalen. Den kör ABC80:s ROM (eller BASIC II
  för ABC80) med egna kretsar, med skärmen ritad i terminalen och
  tangentbordet från den, eller utan terminal i ett batchläge som skriver
  skärmen efteråt. Manual: [`abc80.md`](abc80.md).

Båda är skrivna för att kunna läsas: en fil för assemblern, en för
processorn och en för datorn runt den, och inga bibliotek utöver C:s
standardbibliotek och POSIX (för terminalen).

## Bygga

Med en C-kompilator (C11) och `make`, på Linux, macOS eller i WSL:

    make            # z80asm och abc80
    make prov       # assemblera exemplen och Forth, jämför med .bin-filerna
    make clean

Utan `make`:

    cc -O2 -o z80asm z80asm.c
    cc -O2 -o abc80 abc80.c z80.c

## Kom igång

    ./abc80                                   # ABC80, Ctrl-] avslutar
    ./abc80 -r ../disass/basicii/basicii80.rom -m 32    # BASIC II för ABC80

    ./z80asm -l e1.lst ../exempel/e1.asm      # -> ../exempel/e1.bin
    ./abc80 -l ../exempel/e1.bin@4000         # med kretsen på $4000
                                              # (Z=CALL(16384) kopplar in den)

    ./abc80 -t 500 -k 'PRINT 6*7\r'           # utan terminal: skriver skärmen

ROM:arna finns i [`../disass/`](../disass/); utan `-r` startar `abc80`
med `../disass/abc80/abc80new.rom`, räknat från där programmet ligger.

## Filerna

| Fil | Innehåll |
|---|---|
| `z80asm.c` | assemblern |
| `z80.h`, `z80.c` | Z80-processorn: registren, instruktionerna och T-cyklerna |
| `abc80.c` | ABC80: minnet, portarna, avbrotten, tangentbordet, skärmen och terminalen |
| `Makefile` | bygger och provar |
| `z80asm.md`, `abc80.md` | manualerna |

## Så har de provats

- `z80asm` ger samma bytes som assemblern bokens listningar byggdes med,
  för alla ROM:ar och kretsar som boken listar, och för en fil med alla
  dokumenterade instruktioner med alla operander.
- Z80-processorn klarar alla prov i *zexdoc* och alla i *zexall* utom
  `BIT n,(HL)`: där kommer flaggbitarna 3 och 5 från ett inre register i
  processorn (MEMPTR), som emulatorn inte har med.
- `abc80` ger samma skärm som emulatorn bokens prov kördes med, för de
  prov som bara använder det som finns här; de få som skiljer gör det
  för att `abc80` räknar processorns tid i T-cykler och därför hinner
  olika långt på en given tid.

---

## In English

Two small C programs written for the book *ABC80 inifrån* ("ABC80 from
the inside"), used to build and try out the book's examples. The code,
comments and manuals are in Swedish.

- **`z80asm`** is a Z80 assembler (Zilog syntax, documented instructions,
  `ORG`/`EQU`/`DEFB`/`DEFW`/`DEFS`/`END`). It writes a raw binary image,
  optionally Intel HEX (`-x`) and a listing with a symbol table (`-l`).
  Usage: `z80asm [-o file.bin] [-x file.hex] [-l file.lst] source.asm`.
- **`abc80`** emulates a Luxor ABC80 in a terminal: Z80 at 2.9952 MHz
  with T-state timing, 50 Hz NMI, the keyboard on PIO A with its
  interrupt, the 40×24 teletext-style screen (mosaic graphics are drawn
  with Unicode sextant characters, so use a font that has them), extra
  ROMs (`-l file@addr`), a bank-switched ROM card (`-b`), and serial input
  (`-s`). With `-t ms` it runs without a terminal and prints the screen,
  which is how the book's examples are tested. `Ctrl-]` quits.

Build with `make`; `make prov` assembles the examples and the Forth
system and checks the result against the `.bin` files. The ROM images are
in `../disass/`.
