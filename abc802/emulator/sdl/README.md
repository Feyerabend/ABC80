# ABC 802 i ett eget fönster (SDL3)

`fonster.c` är en värd till emulatorns kärna (`../karna`), som
batchläget `../vard/abc802.c`, men med skärmen i ett fönster. Bilden
(480 x 250 punkter: 24 rader, eller 25 när R6 är 25) visas i 4:3,
bärnstensfärgad och som på en fosforskärm: punkterna har ett mjukt
sken, och det som släcks klingar av under några hundradelar. Runt bilden finns en svart kant, så att
tecknen i kanterna syns. Tangentbordet och pipet fungerar. Kärnan och `make` i `../`
berörs inte.

## SDL3 hämtas separat

Som för ABC80-fönstret: `brew install sdl3` (macOS), `libsdl3-dev`
(Debian/Ubuntu), `SDL3-devel` (Fedora). Makefilen frågar `pkg-config`
och använder annars `/opt/homebrew`; andra platser går att ange med
`make SDL_CFLAGS=-I/väg/include SDL_LIBS="-L/väg/lib -lSDL3"`.

## Bygga och köra

    cd emulator/sdl
    make
    ./abc802f

Utan `-r` och `-c` läses ROM:en och teckengeneratorn från
`../../roms/`, räknat från var programmet ligger.

| Flagga     | Vad                                                          |
|------------|--------------------------------------------------------------|
| `-r fil`   | ROM:en, 32 KB                                                |
| `-c fil`   | teckengeneratorn, 4 eller 8 KB                               |
| `-40`      | bygeln S3 på 40 tecken per rad                               |
| `-60`      | bygeln 50/60 Hz på 60 Hz                                     |
| `-k text`  | tangenter efter starten (`\n` RETURN, `\wN` paus, `\xHH` kod) |
| `-f fil`   | en textfil att skriva som tangenter (ett BASIC-program)      |
| `-D fil[@kort[.enhet]]` | en skivavbild, standard kort 44 enhet 0 (MF0, `DR0:`); kort 36 är HD0 |
| `-DP fil`  | samma, skrivskyddad                                          |
| `-DS`      | skriv tillbaka avbilderna när fönstret stängs                |
| `-z n`     | förstoring: fönstret är n x 528 x n x 396 (standard 2)       |
| `-tyst`    | utan pip                                                     |
| `-skarp`   | skarpa punkter, utan sken och efterlysning                   |

Tangenterna: RETURN, ← och → (backsteg är ←), ESC, TAB, DEL och
Ctrl-A till Ctrl-Z; å ä ö é ü och Å Ä Ö É Ü blir ABC 802:s koder. F12
är RESET. Cmd-V (macOS) eller Shift-Insert klistrar in text.

Skivminnet är DOS v.20 i ROM:en och skivkortet på ABC-bussen
(`../karna/skivkort.c`). `../skivor/` har en tom skiva och en
med tidsproven: `./abc802f -D ../skivor/tidsprov.img`, sedan
`LOAD DR0:B1` och `RUN`. Utan `-DS` skrivs ingenting tillbaka.

Pipet kommer från tangentbordets högtalare när datorn sänder kod 7 på
DART B. Tonen och längden är inte kända; 1 kHz i 100 ms antas (?).
