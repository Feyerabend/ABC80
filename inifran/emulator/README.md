# Emulatorn

En ABC80 i C11, skriven till boken *ABC80 inifrån*. Den består av tre
delar:

- **`karna/`**: själva datorn, utan in- och utmatning. `z80.c` är
  processorn, `maskin.c` minnet, portarna, avbrotten, tangentkön, V24
  och labplattan, med tiden i T-cykler (2,9952 MHz), `bussen.c`
  ABC-bussen med korten (`skivkort.c`, `skrivarkort.c`, `ieckort.c`,
  `p40.c`),
  `uni80.c` CP/M-kortet UNI-80,
  `kassett.c` bandspelaren som en följd av flanker, `ljud.c` SN76477,
  `tecken.c` gör om bildminnet till text i UTF-8 och `bild.c` till
  punkter. Kärnan läser inga filer och
  frågar inte efter klockan; den kan byggas för vilken plattform som
  helst.
- **`vard/`**: värdlagren. `abc80.c` läser flaggorna och filerna,
  `terminal.c` kör ABC80 i en POSIX-terminal, `batch.c` kör utan
  terminal och skriver skärmen efteråt, `anrop.c` är anropsläget och
  `tstenhet.c` filenheten TST: och `kassettfil.c` kassettbandet som
  WAV-fil (omvandlingen i `wav.c`); `png.c` skriver P40:s papper.
- **`webb/`**: emulatorn i webbläsaren, se nedan.
- **`sdl/`**: emulatorn i ett eget fönster med SDL3 (Linux, Windows,
  macOS). SDL3 hämtas separat; se `sdl/README.md`. Den byggs med `make`
  i `sdl/`, inte med `make` här.

Flaggorna är de från `verktyg/abc80` och `dis/04/host/abc80host`; se
huvudet i `vard/abc80.c`. Utan `-t` körs terminalen, om standard in är
en; annars batchläget med 500 ms efter tangenterna, som abc80host.
`-i n` räknar tiden i instruktioner, n per 20 ms, som abc80host gör.
`-a fil.wav` skriver ljudet (OUT 6 till SN76477, `karna/ljud.c`) till en
WAV-fil i batchläget, 22 050 prov/s, mono, 16 bitar:

    ./abc80 -r rom -k 'PRINT CHR$(7)\r' -t 1000 -a pip.wav

Kassetten (PIO B, `karna/kassett.c`) läser och skriver WAV-filer
(`vard/kassettfil.c`): `-T band.wav` är bandet som spelas upp (LOAD,
OPEN), `-U inspelning.wav` spelar in (SAVE, PREPARE). Bandet går bara
när ROM:en har motorn på, och i batchläget körs maskinen så länge
motorn går innan tiden i `-t` börjar. Skriv inga tangenter medan
blocken läses: tangentavbrottet kommer då mitt i kassettavbrottet och
ger ERR 35, så lägg en lång paus (`\w`) före nästa kommando:

    ./abc80 -r rom -f prog.bas -k 'SAVE CAS:PROG\r' -U prog.wav -t 100
    ./abc80 -r rom -k 'LOAD CAS:PROG\r\w20000\.LIST\r' -T prog.wav -t 300

`exempel/program/` har fyra exempelprogram (GISSA, KURVA, MUSIK och
AIRFIGHT) i tre mappar: `kassett/` (ett band per program), `diskett/`
(alla fyra för ABC-DOS) och `text/`. `exempel/malare/` har MÅLARE, ett
ritprogram av Mikael Bonnier från 1982 (GPL-3.0), som band och text,
med förbehållen om var det kommer ifrån. `exempel/genesis/`, när den
finns med, är ett demo från 2015 som kassett-WAV och diskett; kommandona
står i mapparnas `LASMIG.md`. `exempel/krets/forth.bin` är Forth-tolken
från bokens exempel 7 som en krets på $4000, som Smartaid
(`-l exempel/krets/forth.bin@4000`, sedan `POKE 65052,0,208`, `NEW` och
`Z=CALL(16384,3)`), och `forth_cmos.bin` samma tolk med de egna orden i
CMOS-minnet (`-M`, nedan), som finns kvar till nästa gång.

`-CP papper.png` kopplar in nålskrivaren P40 på kort 60
(`karna/p40.c`). ABC80 kör själv vagnens motor och nålarna med
drivrutinen på $7800 (`-l roms/p40.rom@7800`), och var punkterna hamnar
avgörs av tiden; papperet skrivs som en bild när emulatorn slutar.
Vagnens mekanik är antagen (se `karna/p40.h`). `exempel/p40/` har ett
provprogram och vad det visar:

    ./abc80 -r roms/abc80new.rom -l roms/p40.rom@7800 -CP papper.png \
            -f exempel/p40/PROV.BAS -k 'RUN\r' -t 9000

I webbläsaren finns skrivaren i gruppen *Skrivaren P40*.

`-M cmos.bin` sätter i 2 KB CMOS-minne med batteri på $5000–$57FF, som på
Super Smartaid: det läses ur filen, om den finns, och skrivs tillbaka när
emulatorn slutar (`-M fil@adr` för en annan adress). I webbläsaren sparas
det i webbläsaren själv (gruppen *CMOS-minnet*).

`-tkn80 roms/tkn80-iii.rom` sätter i MyAB:s kort TKN80 för 80 tecken per
rad, med 2 KB bildminne på $5800–$5FFF. Kortet startar i 80-teckensläget;
`INP(3)` ger 40 tecken och `INP(4)` 80. `-B skarm.png` skriver skärmen
som en bild (640 × 480) när emulatorn slutar:

    ./abc80 -r roms/abc80new.rom -tkn80 roms/tkn80-iii.rom \
            -k 'PRINT PEEK(885)\r' -B skarm.png

I webbläsaren är det rutan *TKN80* under *Maskinen*, eller `?tkn80=pa`.
Markören blinkar där som på kortet; rutan *fast markör* (`?fast=pa`) är
kortet med MyAB:s bygling för fast markör, och tecken med bit 7 visas då
inverterade utan att blinka. Terminalen och `-B` visar markören fast.

`-cpm cpm/system.dsk` sätter i MyAB:s kort UNI-80 (`karna/uni80.c`) och
startar CP/M 2.2 från skivan, som när RESET hålls nere på den riktiga
maskinen. Start-PROM:en (`cpm/start.asm`) och BIOS:en (`cpm/bios.asm`)
är skrivna här; ingenting kommer från MyAB. CCP:n, BDOS:en och
programmen är Digital Researchs fria CP/M 2.2 (`cpm/dr/`). `make`
assemblerar dem med `z80asm` ur `../verktyg/` och bygger systemskivan
`cpm/system.dsk` (`cpm/systemskiva.py`): A: och B: är FD2, 80 KB per
enhet, C: en RAM-skiva i ABC80:s eget minne och D: kort 44 (ABC 832,
640 KB) om en skiva sitter där (`-D fil`). Med `-tkn80` blir
skärmen 80 tecken bred. `cpm/b.dsk` är en skiva till B: med R. T.
Russells BBC BASIC (Z80), fri under zlib-licensen, med en egen första
sida för vår BIOS (`cpm/bbc/`, se `LASMIG.md` där). `cpm/d.dsk` är en
skiva till D: med Infocoms Zork I, fri under MIT-licensen, som körs med
vår Z-maskintolk ZTOLK på systemskivan (`cpm/zork/`, se `LASMIG.md`
där).

    ./abc80 -cpm cpm/system.dsk -k 'DIR\r'
    ./abc80 -cpm cpm/system.dsk -D cpm/b.dsk@45.1 -k 'B:\rBBCBASIC\r'
    ./abc80 -cpm cpm/system.dsk -D cpm/d.dsk -k 'ZTOLK D:ZORK1\r'

I webbläsaren är det rutan *CP/M* under *Maskinen*, eller `?cpm=pa`, och
Zork I knappen *Zork I* under *Disketten*, eller `?zork=pa`.

Kortets tre minneskartor (port `$EF`: CP/M, ABC80 och start-PROM:en) är
vår egen modell, byggd på vad en BIOS gör med porten. Kortets bevarade
avkodnings-PROM, UNI80-13, avgör dem inte: den sitter i ABC80 och har
ROM:en på `$0000-$3FFF` i alla sina fyra kartor (`roms/prom13.py`).
Den visar däremot ABC80-läget med 16 KB extra: med `-uni80` och utan
`-cpm` startar maskinen som ABC80 med kortets RAM på `$8000-$BFFF`, där
en ABC80 med 16 KB inte har något, tills porten skrivs första gången.
BASIC hittar då 32 KB:

    ./abc80 -r roms/abc80new.rom -uni80 -k '10 DIM A(6000)\r20 PRINT "OK"\rRUN\r'

SKRIV (`cpm/skriv/skriv.asm`) är en egen editor för hela skärmen på
systemskivan, med WordStars tangenter, i 40 och 80 tecken. `SKRIV
fil.typ` startar den; filen läses in i minnet i sin helhet (omkring
50 KB ryms) och den gamla blir `fil.BAK` när den sparas. Ctrl-J visar
tangenterna på tre sidor, och `SKRIV SKRIV.TXT` visar bruksanvisningen
(`cpm/skriv/handbok.txt`, som Makefile skriver om till ABC80:s tecken).
ESC och en bokstav gör samma sak som Ctrl och bokstaven, för
webbläsare som tar Ctrl-W, Ctrl-N och Ctrl-T själva; ESC är Ctrl-Ä på
ABC80. Nedersta raden visar filen, `*` om den är ändrad, raden och
kolumnen.

| Ctrl och          | gör                                        |
|-------------------|--------------------------------------------|
| E X S D           | upp, ned, vänster, höger (och pilarna)     |
| A F               | ett ord vänster, höger                     |
| R C               | en sida upp, ned                           |
| W Z               | rulla en rad upp, ned                      |
| Q S, Q D          | radens början, slut                        |
| Q R, Q C          | filens början, slut                        |
| Q F, L            | sök (RETURN, ESC avbryter), sök igen       |
| G, DEL            | radera tecknet vid markören, före          |
| T, Y, Q Y         | radera ordet, raden, resten av raden       |
| U                 | sätt in raderna från Y (i följd) igen      |
| N                 | dela raden                                 |
| V                 | infoga eller skriv över                    |
| P                 | nästa tangent som tecken (P I ger TAB)     |
| K S               | spara                                      |
| K X, K D          | spara och sluta                            |
| K Q               | sluta utan att spara (två om ändrad)       |
| J                 | hjälpen                                    |

    ./abc80 -cpm cpm/system.dsk -k 'SKRIV SKRIV.TXT\r'

    make roms       # hämta ROM:arna (roms/hamta.py); de ligger inte i förrådet
    make            # abc80
    make prov       # jämför med verktyg/abc80 i en rad fall
    make webb       # webb/abc80.html
    make cpmprov    # CP/M: start-PROM:en, BIOS:en, programmen och BBC BASIC
    make clean

`make prov` behöver den gamla emulatorn i `../verktyg/` och ROM:arna i
`../emulator/roms/`, som i det publika förrådet (de hämtas om de saknas); fall 4 får skilja (labplattan,
en ms räknas lite olika). I bokens arkiv går också provningarna till
abc80host att köra mot den här (569 fall):

    PROGRAM="$PWD/abc80 -i 6000" ../dis/04/host/test.sh   # alla lika
    PROGRAM="$PWD/abc80" ../dis/04/host/test.sh           # tiden skiljer

## I webbläsaren

`webb/abc80.html` är hela emulatorn i en fil: öppna den i webbläsaren,
direkt från disken; den behöver ingen server och inget installerat. Den
ligger också på webben, i den version som finns i kodförrådet:
[https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html)
Den går att bygga om med `make webb`, som först hämtar ROM:arna
(`roms/hamta.py`, se [`roms/LASMIG.md`](roms/LASMIG.md)).
WebAssembly-koden, de två ROM:arna och ABC-DOS ligger i filen, och
exempeldisketten (`exempel/program/diskett/program.dsk`), MÅLARE-bandet
och Genesis-demot (band med musiken och diskett) om de finns; med båda blir filen drygt
4 MB, med bara exempeldisketten 330 KB. Sidan har ljudet
(Web Audio), kassetten (lägg i en WAV, spela in och spara som WAV,
snabbt medan motorn går; PLAY, spolning åt båda hållen och ett
räkneverk; bandets eget ljud hörs, och utan fjärrstyrning går bandet
vidare när motorn slås av, så att Genesis-demots musik kommer från
bandet), disketten i FD2 med ABC-DOS, Forth-kretsarna (en lista, eller
`?krets=forth` och `?krets=cmos`), bokens exempel 1–6 i den tänkta
kretsen (i samma lista, eller `?krets=e1` till `?krets=e6`), CMOS-minnet, skrivaren P40 (papperet
växer på sidan och sparas som PNG, `?p40=pa`), exempeldisketten, MÅLARE och Genesis-demot på
knappar (från bandet, med musiken, eller från disketten). Filer går
att dra till sidan, text att klistra in, och en länk kan ge tangenter
med samma syntax som `-k`:

    abc80.html?k=10 PRINT "HEJ"\r20 GOTO 10\rRUN\r

och `?s=` en text till V24, med samma syntax som `-s`, när tangenterna
är skrivna.

Knapparna ligger i paneler (Maskinen, Disketten, Kassetten, Program och
kretsar, Skrivaren P40, CMOS-minnet, Demot) som fälls ut och ihop; listen
visar läget också ihopfälld, och *i* visar panelens hjälptext. Sidan
finns på svenska och engelska: knappen *EN*/*SV* överst byter, valet
sparas i webbläsaren, och `?sprak=en` eller `?sprak=sv` går före. Utan
något val gäller webbläsarens språk.

`make webb` bygger om den med clang och wasm-ld (lld), utan Emscripten
och utan C-bibliotek: kärnan, `vard/wav.c` och `webb/webb.c` med
`webb/libc.c` blir `abc80.wasm`, som `webb/bygg.sh` lägger i mallen
`mall.html` med `abc80.js`. Mallen går inte att öppna som den är; bara
`abc80.html` behöver delas ut.

## Kärnan i ett eget program

Värden använder kärnan så här:

    Maskin m;                                 /* stor: lägg den statiskt */
    maskin_starta(&m, 16);                    /* 16 KB RAM */
    maskin_rom(&m, rom, storlek);             /* bytes som värden har läst */
    maskin_aterstall(&m);
    tangentko_paus(&m, 1000);
    tangentko_lagg(&m, 'A');
    maskin_kor(&m, MASKIN_BILD);              /* 20 ms */
    /* bildminnet: m.minne + MASKIN_BILDMINNE, rader med tecken_rad */
