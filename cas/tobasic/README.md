
## tobasic

ABC80: cassette recordings (WAV) → files → BASIC text.
ABC80: kassettinspelningar (WAV) → filer → BASIC-text.

[English](#english) · [Svenska](#svenska)

---

### English

Two independent tools, one step each:

| Tool | In | Out |
|------|----|-----|
| `wav2basic.py` | cassette recording (WAV) | the files on the tape: `.BAS` (text), `.BAC` (compiled), `.BIN` |
| `bac2bas` | a `.BAC` file (from tape or disk) | the program as text, the way `LIST` shows it |

```sh
make bac2bas
python3 wav2basic.py abcklubben-kassett-8.wav          # -> ut/abcklubben-kassett-8/
./bac2bas ut/abcklubben-kassett-8/13-RPN.BAC > RPN.BAS
./bac2bas < ~/Documents/abc80/FILES/DIS.BAC             # filter: stdin -> stdout
```

#### wav2basic.py

Needs Python 3 and numpy. It reads 8-, 16- or 32-bit PCM, mono or stereo,
and tries both channels.

- Edges are found with a Schmitt trigger relative to the local amplitude,
  so noise around zero gives no false edges.
- The bit cell length is tracked, so changes in tape speed are followed.
- It makes several passes with different settings. A block is accepted
  when its checksum is right, and the passes fill each other's gaps.

For each tape it writes a directory with the files, and a report on
stdout: time on the tape, name, number of blocks, kind, and missing blocks.

- *Text files* (`LIST`) become UTF-8 with ÄÖÅ and LF. The raw file is kept as `.raw`.
- *BAC files* (`SAVE`) are written as 256-byte sectors, the disk format.
  That is why `bac2bas` reads files from both tape and disk. A missing
  block becomes a sector of zeros, and `bac2bas` warns about it.

Options:

| Option | Meaning |
|--------|---------|
| `-o dir` | where the files are written (default `ut`) |
| `-a` | keep the ABC80 7-bit characters (`[\]{|}$`) |
| `-q` | quick: one pass per channel |
| `-v` | show each pass |

The tape format:

- FM coding at about 740 bit/s. Bytes are sent least significant bit first.
- Each block contains, in order:
  - 256 zero bits
  - 3 × SYN ($16)
  - STX ($02)
  - 256 bytes
  - ETX ($03)
  - a 2-byte checksum
- The name block begins with `$FF $FF $FF`, followed by the name (8+3 characters).
- Data blocks begin with `$00`, a block number (2 bytes) and 253 bytes of data.

#### bac2bas

`bac2bas [-a] [-c] [file.BAC ...]` is ANSI C (C89) with no other
dependencies. It reads the files given, or stdin, and writes the listing
to stdout.

| Option | Meaning |
|--------|---------|
| `-a` | keep the ABC80 7-bit characters instead of UTF-8 (ÄÖÅäöå¤ …) |
| `-c` | end lines with CR, as on the ABC80, instead of LF |

The program does not emulate the Z80. It is a parallel of the listing code
in the ABC80 BASIC ROM (the old ROM, checksum 11273, `abc80old.asm`),
routine by routine and with the same names:

| Routine | Job |
|---------|-----|
| `DETOK` | line → text |
| `SPACE` | spaces |
| `DETEXP` | expression code (reverse Polish) → infix |
| `DETOP`, `OPTEXT` | operators |
| `FNTEXT` | functions |
| `JOINC`/`JOINA` | `,` or `=` in front of the operand |
| `DETVAR` | variable names |
| `ROTTXT` | moves text backwards |
| `PRNUM` | integers |
| `FLTASC` | BCD floating point |

The keyword tables (STKW1, STKW2, KWEQV, FNKW) are copied from the ROM in
the ROM's order, so the search works the same way.

The BAC format:

- The file is made of 256-byte sectors. The first one begins with `$82`.
- Each line is stored as `[length][line lo][hi][code …][$0D]`.
- A length of `$00` means the rest of the sector is unused. `$01` means the program ends.

*Tested* against the ROM's own `DETOK` ($1195), run in `abc80host -r 11273`.
All 3,901 lines in FILES and on the tapes come out byte for byte the same,
with two exceptions:

- *List protection.* Some programs (HLPA/HLPB, ASSTRAN*, GOLV, SÄNK …)
  have two string operands with no operator. The `RET` in `DETEXP` then
  jumps into the listed text and runs the string as machine code, and that
  code rewrites the listing (to `1 GOLV+`, for example). `bac2bas` writes
  the line as stored up to the jump and warns on stderr.
- *Broken lines.* One example is line 90 of CAS80, where a binary
  operator has only one operand. The ROM then writes garbage outside the
  buffer. `bac2bas` writes `nnn REM ** RADEN KUNDE INTE LISTAS **` and
  warns.

#### Older tools

`wav2bin.c` and `bin2basic.py` (2023) were the first approach. `wav2bin`
reads only the first byte of each sample, as unsigned 8-bit, with a fixed
threshold, so it does not work on the 16-bit recordings. `wav2basic.py`
replaces both.

---

## Svenska

Två fristående verktyg som gör var sitt steg:

| Verktyg | In | Ut |
|---|---|---|
| `wav2basic.py` | kassettinspelning (WAV) | filerna på bandet: `.BAS` (text), `.BAC` (kompilerad), `.BIN` |
| `bac2bas` | en `.BAC`-fil (från band eller skiva) | programmet som text, som `LIST` visar det |

```sh
make bac2bas
python3 wav2basic.py abcklubben-kassett-8.wav          # -> ut/abcklubben-kassett-8/
./bac2bas ut/abcklubben-kassett-8/13-RPN.BAC > RPN.BAS
./bac2bas < ~/Documents/abc80/FILES/DIS.BAC             # filter: stdin -> stdout
```

#### wav2basic.py

Kräver Python 3 och numpy. Läser 8-, 16- eller 32-bitars PCM, mono eller
stereo. Båda kanalerna prövas.

- Flankerna hittas med en Schmitt-trigger i förhållande till den lokala
  amplituden, så att brus kring noll inte ger falska flanker.
- Bitcellens längd följs, så att variationer i bandhastigheten hanteras.
- Flera pass görs med olika inställningar. Ett block godtas när
  kontrollsumman stämmer, och passen fyller varandras luckor.

För varje band skrivs en katalog med filerna och en rapport på stdout:
tid på bandet, namn, antal block, typ och vilka block som saknas.

- *Textfiler* (`LIST`) blir UTF-8 med ÄÖÅ och LF. Råfilen ligger som `.raw`.
- *BAC-filer* (`SAVE`) skrivs som 256-bytes sektorer, samma format som på
  skiva. Därför läser `bac2bas` filer från både band och skiva. Ett block
  som saknas blir en sektor med nollor, och `bac2bas` varnar för den.

Flaggor:

| Flagga | Betydelse |
|--------|-----------|
| `-o katalog` | var filerna skrivs (standard `ut`) |
| `-a` | behåll ABC80:s 7-bitarstecken (`[\]{|}$`) |
| `-q` | snabbt: ett pass per kanal |
| `-v` | visa varje pass |

Kassettformatet:

- FM-kodning med ungefär 740 bit/s. Byte skrivs med minst signifikant bit först.
- Varje block innehåller, i ordning:
  - 256 nollbitar
  - 3 × SYN ($16)
  - STX ($02)
  - 256 byte
  - ETX ($03)
  - en kontrollsumma på 2 byte
- Namnblocket börjar med `$FF $FF $FF` och följs av namnet (8+3 tecken).
- Datablocken börjar med `$00`, blocknummer (2 byte) och 253 byte data.

#### bac2bas

`bac2bas [-a] [-c] [fil.BAC ...]` är ett ANSI C-program (C89) utan andra
beroenden. Det läser filerna som anges, annars stdin, och skriver
listningen på stdout.

| Flagga | Betydelse |
|--------|-----------|
| `-a` | behåll ABC80:s 7-bitarstecken i stället för UTF-8 (ÄÖÅäöå¤ …) |
| `-c` | radslut CR, som i ABC80, i stället för LF |

Programmet emulerar inte Z80:n. Det är en parallell till listningskoden i
ABC80:s BASIC-ROM (den gamla ROM:en, kontrollsumma 11273, `abc80old.asm`),
rutin för rutin och med samma namn:

| Rutin | Uppgift |
|-------|---------|
| `DETOK` | rad → text |
| `SPACE` | mellanslag |
| `DETEXP` | uttryckskod (omvänd polsk notation) → infix |
| `DETOP`, `OPTEXT` | operatorer |
| `FNTEXT` | funktioner |
| `JOINC`/`JOINA` | `,` eller `=` framför operanden |
| `DETVAR` | variabelnamn |
| `ROTTXT` | flyttar text bakåt |
| `PRNUM` | heltal |
| `FLTASC` | flyttal i BCD |

Nyckelordstabellerna (STKW1, STKW2, KWEQV, FNKW) är kopierade ur ROM:en i
ROM:ens ordning, så sökningen fungerar som där.

BAC-formatet:

- Filen består av 256-bytes sektorer. Den första börjar med `$82`.
- Varje rad lagras som `[längd][radnr lo][hi][kod …][$0D]`.
- Längden `$00` betyder att resten av sektorn är tom. `$01` betyder att programmet är slut.

*Provat* mot ROM:ens egen `DETOK` ($1195), körd i `abc80host -r 11273`.
Alla 3 901 rader i FILES och på banden blir byte för byte samma, med två
undantag:

- *Listskydd.* Några program (HLPA/HLPB, ASSTRAN*, GOLV, SÄNK …) har två
  strängoperander utan operator. Då hoppar `RET` i `DETEXP` in i den listade
  texten och kör strängen som maskinkod, och koden skriver om listningen
  (till exempel till `1 GOLV+`). `bac2bas` skriver raden som den är lagrad
  fram till hoppet och varnar på stderr.
- *Trasiga rader.* Ett exempel är rad 90 i CAS80, där en binär operator
  har bara en operand. ROM:en skriver då skräp utanför bufferten. `bac2bas`
  skriver `nnn REM ** RADEN KUNDE INTE LISTAS **` och varnar.

#### Äldre verktyg

`wav2bin.c` och `bin2basic.py` (2023) var den första vägen. `wav2bin` läser
bara den första byten i varje sampel, som 8 bitar utan tecken, och har en
fast tröskel. Därför fungerar den inte på 16-bitarsinspelningarna.
`wav2basic.py` ersätter båda.
