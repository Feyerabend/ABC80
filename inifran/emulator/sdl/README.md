# ABC80 i ett eget fönster (SDL3)

`fonster.c` är en värd till emulatorns kärna (`../karna`), som
webbsidan, men som ett vanligt program. Skärmen visas i ett fönster i
4:3 och liknar en vit fosforskärm: punkterna har ett mjukt sken, det
som släcks klingar av under några hundradelar, och runt bilden finns en
svart kant, så att tecknen i kanterna syns (`-skarp` ger skarpa
punkter utan sken). Tangentbordet och ljudet fungerar som på
webbsidan. Programmet går på Linux, Windows och macOS.
Kärnan, `abc80` och `make` i `../` berörs inte.

## SDL3 hämtas separat

Fönstret och ljudet går genom biblioteket SDL3, som inte följer med här.
Installera det först:

- **macOS:** `brew install sdl3`
- **Debian/Ubuntu** (25.04 och senare): `sudo apt install libsdl3-dev`
- **Fedora:** `sudo dnf install SDL3-devel`
- **Windows:** MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-SDL3`), eller
  utvecklingspaketet från <https://github.com/libsdl-org/SDL/releases>.
  `SDL3.dll` ska ligga bredvid programmet.
- **Annars:** bygg SDL3 från källkoden, <https://libsdl.org>.

Makefilen frågar `pkg-config` efter SDL3. Finns inte `pkg-config`
används `/opt/homebrew` (Homebrew på Apple Silicon). Andra platser går
att ange med `make SDL_CFLAGS=-I/väg/include SDL_LIBS="-L/väg/lib -lSDL3"`.

## Bygga och köra

    cd emulator/sdl
    make
    ./abc80f

Utan `-r` läses ROM:en från `../roms/abc80new.rom`, räknat från var
programmet ligger. CP/M-skivorna (`../cpm/system.dsk`, `b.dsk`,
`d.dsk`) finns inte i förrådet; de byggs av `make` i `emulator/`.

| Flagga                  | Vad                                                    |
|-------------------------|--------------------------------------------------------|
| `-r fil`                | ROM:en, 16 KB eller 32 KB (BASIC II)                   |
| `-l fil@adr`            | en krets på adressen (hex); flera går bra              |
| `-m kb`                 | RAM: 16 (standard), 32 eller 48                        |
| `-tkn80 fil`            | TKN80 med EPROM:en TKN80-III, 80 tecken per rad        |
| `-cpm fil.dsk`          | UNI-80 och CP/M från systemskivan i A:; B: får `../cpm/b.dsk` (BBC BASIC) och D: `../cpm/d.dsk` (Zork I), som på webbsidan, om inte `-D` sätter något där |
| `-D fil[@kort[.enhet]]` | en skivavbild, kort 44 (standard), 45 eller 36         |
| `-DS`                   | skriv tillbaka skivorna när fönstret stängs            |
| `-T band.wav`           | ett band i bandspelaren                                |
| `-U fil.wav`            | spela in det som sparas på band                        |
| `-k text`               | tangenter efter starten (`\r` RETURN, `\wN` paus N ms) |
| `-z n`                  | förstoring, n gånger 352 × 264 (standard 3)            |
| `-tyst`                 | utan ljud                                              |
| `-fast`                 | fast markör (TKN80 med byglingen)                      |
| `-skarp`                | skarpa punkter, utan sken och efterlysning             |

Exempel:

    ./abc80f -k 'PRINT 2+2\r'
    ./abc80f -r ../roms/abc80new.rom -tkn80 ../roms/tkn80-iii.rom
    ./abc80f -cpm ../cpm/system.dsk
    ./abc80f -r ../../utanfor/comal/ut/comal.rom -l ../../utanfor/comal/ut/comal.krets@4000

Det sista är COMAL-försöket ur `utanfor/comal`, ABC80-BASIC med
COMAL:s WHILE, REPEAT, PROC och CASE.

## Tangenterna

RETURN, ← och → (backsteg är ←), ESC, TAB, DEL och Ctrl-A till Ctrl-Z.
å ä ö é ü och Å Ä Ö É Ü blir ABC80:s koder för dem. **F12** är RESET;
i CP/M startar det om från skivan. **Cmd-V** (macOS) eller
**Shift-Insert** klistrar in text, med en paus efter varje rad.

## Tiden

Maskinen körs en bild (20 ms) i taget. Med ljud bestämmer ljudkortet
takten: programmet håller ungefär 60 ms ljud i kö. Utan ljud följer
takten klockan. Så länge det finns tangenter kvar i kön, från `-k`
eller inklistrad text, körs maskinen så fort datorn hinner och ljudet
kastas.
