# Emulatorn

En ABC80 i C11, skriven till boken *ABC80 inifrån*. Den består av tre
delar:

- **`karna/`**: själva datorn, utan in- och utmatning. `z80.c` är
  processorn, `maskin.c` minnet, portarna, avbrotten, tangentkön, V24
  och labplattan, med tiden i T-cykler (2,9952 MHz), `bussen.c`
  ABC-bussen med korten (`skivkort.c`, `skrivarkort.c`, `ieckort.c`),
  `kassett.c` bandspelaren som en följd av flanker, `ljud.c` SN76477,
  `tecken.c` gör om bildminnet till text i UTF-8 och `bild.c` till
  punkter. Kärnan läser inga filer och
  frågar inte efter klockan; den kan byggas för vilken plattform som
  helst.
- **`vard/`**: värdlagren. `abc80.c` läser flaggorna och filerna,
  `terminal.c` kör ABC80 i en POSIX-terminal, `batch.c` kör utan
  terminal och skriver skärmen efteråt, `anrop.c` är anropsläget och
  `tstenhet.c` filenheten TST: och `kassettfil.c` kassettbandet som
  WAV-fil (omvandlingen i `wav.c`).
- **`webb/`**: emulatorn i webbläsaren, se nedan.

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

`-M cmos.bin` sätter i 2 KB CMOS-minne med batteri på $5000–$57FF, som på
Super Smartaid: det läses ur filen, om den finns, och skrivs tillbaka när
emulatorn slutar (`-M fil@adr` för en annan adress). I webbläsaren sparas
det i webbläsaren själv (gruppen *CMOS-minnet*).

    make            # abc80
    make prov       # jämför med verktyg/abc80 i en rad fall
    make webb       # webb/abc80.html
    make clean

`make prov` behöver den gamla emulatorn i `../verktyg/` och ROM:arna i
`../disass/`, som i det publika förrådet; fall 4 får skilja (labplattan,
en ms räknas lite olika). I bokens arkiv går också provningarna till
abc80host att köra mot den här (569 fall):

    PROGRAM="$PWD/abc80 -i 6000" ../dis/04/host/test.sh   # alla lika
    PROGRAM="$PWD/abc80" ../dis/04/host/test.sh           # tiden skiljer

## I webbläsaren

`webb/abc80.html` är hela emulatorn i en fil: öppna den i webbläsaren,
direkt från disken; den behöver ingen server och inget installerat.
WebAssembly-koden, de två ROM:arna och ABC-DOS ligger i filen, och
exempeldisketten (`exempel/program/diskett/program.dsk`), MÅLARE-bandet
och Genesis-demot (band med musiken och diskett) om de finns; med båda blir filen drygt
4 MB, med bara exempeldisketten 330 KB. Sidan har ljudet
(Web Audio), kassetten (lägg i en WAV, spela in och spara som WAV,
snabbt medan motorn går; PLAY, spolning åt båda hållen och ett
räkneverk; bandets eget ljud hörs, och utan fjärrstyrning går bandet
vidare när motorn slås av, så att Genesis-demots musik kommer från
bandet), disketten i FD2 med ABC-DOS, Forth-kretsarna (en lista, eller
`?krets=forth` och `?krets=cmos`), CMOS-minnet, exempeldisketten, MÅLARE och Genesis-demot på
knappar (från bandet, med musiken, eller från disketten). Filer går
att dra till sidan, text att klistra in, och en länk kan ge tangenter
med samma syntax som `-k`:

    abc80.html?k=10 PRINT "HEJ"\r20 GOTO 10\rRUN\r

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
