# kassett

Verktyg och beskrivning för kassettbanden i ABC80 och ABC 800-serien
(800 C/M, 802 och 806).

- [FORMAT.md](FORMAT.md): signalen, blocken, filerna och teckenkoderna,
  med källor och öppna frågor
- [HARDVARA.md](HARDVARA.md): en kabel till ljudkortet och en Pico som
  bandspelare (förslag, inte byggda)
- `pdf/kassett.pdf`: allt detta med figurer (byggs med `pdf/bygg.sh`,
  som behöver tectonic)

## Python (python/)

Python 3. Avkodningen av WAV behöver numpy.

| Verktyg | Gör |
|---------|-----|
| `abckas.py` | kärnan: block, filer, text som ROM:en skriver den, FM-signalen in och ut (ABC80 och ABC 800) |
| `kas.py lista BAND.kas` | filerna och blocken i en blockfil |
| `kas.py packa -o BAND.kas FIL ...` | filer -> blockfil (se nedan om text; `-t`/`-b` tvingar, `--tk N` för ABC 800-serien) |
| `kas.py upp -d KATALOG BAND.kas` | blockfil -> filer |
| `kas2wav.py -o BAND.wav BAND.kas` | blockfil eller filer -> WAV (`-t`, `-b` och `--tk` som i `packa`; `--abc800` skriver som ABC 800-serien, och `--tk` slår på det) |
| `wav2kas.py [-r] BAND.wav` | WAV -> blockfil, med rapport block för block (ABC80 och ABC 800) |
| `bac2bas.py FIL.BAC` | BAC (ABC80 $82, BASIC II $8F) -> text, som `LIST` skriver den |
| `abctext.py FIL` | ABC-text -> dagens fil; `--abc` åt andra hållet |

`.kas` är blocken efter varandra, 256 byte styck, i bandets ordning.

En fil blir text om namnet slutar på `.BAS`, `.TXT` eller `.ASC`, eller
om den bara har skrivbara 7-bitarstecken och radslut; annars binär.
Text packas som ROM:en gör (FORMAT.md 3.1): radsluten blir CR, flera
blanksteg blir `$09 n`, högst 252 byte och ETX i varje block, och sist
ett block med sex nollor. Med `--tk` följer packningen BASIC II, som
fyller blocket lite annorlunda. BAC packas upp till 256-bytes sektorer
som på skivan (253 byte från bandet och tre nollor), och packas tillbaka
på samma sätt. ÅÄÖ i filnamn blir `]` `[` `\`.

Provat 2026-11-13:

- Text, binärt och lång text -> WAV -> blockfil -> filer: byte för byte
  lika, vid 44,1, 22,05, 11,025 och 8 kHz, 8 och 16 bitar.
- De tio riktiga banden i `~/Documents/abc80` (FORTH, MÅLARE, Genesis,
  AIRFIGHT, FONSTER, FONSTER2, KURVA, MUSIK, HR80 och GISSA): alla block
  lästa. GISSA, FORTH och MÅLARE ger samma BAC som den gamla
  `wav2basic.py`, och filerna packade igen ger samma block som bandet.
- Blocken från FORTH och Genesis -> ny WAV -> samma block.

Ännu inte provat: text från ett riktigt band (LIST); inget av banden har
någon.

### Text: teckenkod och radslut

`bac2bas` och `abctext` har samma två flaggor:

| Flagga | Värden |
|--------|--------|
| `--kod` | `7`: ABC:s 7-bitarskod som den är; `dos`: ÄÖÅÜÉ äöåüé i teckentabell 850 (och 437, där de tio är desamma); `utf8` (standard) |
| `--rad` | `cr` som på ABC, `crlf` som i MS-DOS och Windows, `lf` (standard) |

`abctext --abc` gör om dagens fil till ABC-text: radsluten blir CR, och
ÄÖÅÜÉ blir `[` `\` `]` `^` `@` och äöåüé `{` `|` `}` `~` `` ` ``. Utan `--kod` gissar den: UTF-8 om
filen är giltig UTF-8, annars 850 om där finns byte över $7F, annars 7.
Tecken som ABC inte har, och trasig UTF-8, blir `?`. `--rad` är då
radslutet i filen som skrivs (standard `cr`). `$` blir aldrig ¤.

`bac2bas` kör inte Z80-koden utan gör `LIST` rutin för rutin som ROM:en,
med nyckelorden och tabellerna kopierade ur ROM:erna (ABC80 och ABC 802).
Med `--kod 7 --rad cr` blir texten densamma som `LIST CAS:`, byte för
byte.

## ABC 800-serien

`kas2wav --abc800` (eller `--tk N`) skriver blocken som `CASWR` i ROM:en
(SYN STX, data, ETX, summan och FF FF) med FM-kodningen i processorkortets
TTL-logik: bitcellen är 32 x tk T vid 3 MHz. Filhuvudet går med tk 129
(ungefär 700 baud), och datablocken med tidkonstanten i filhuvudet (40 ger
ungefär 2 400 baud). Vägen fram och tillbaka är provad, C och Python lika,
men inte mot en riktig ABC 800 eller ett riktigt band: emulatorn för ABC 802
spelar in tecken, inte ljud. Se FORMAT.md, avsnitt 2.

## Prov mot ROM:en (prov/)

`sh prov/abc80.sh` och `sh prov/abc802.sh` låter ROM:en i emulatorerna
(`~/Documents/abc80/emulator/abc80` och `../emulator/abc802`) skriva ett
program med `SAVE CAS:`, samma program med `LIST CAS:` och en datafil
med `PRINT #`, och läsa tillbaka det verktygen gör av dem.

Provat 2026-11-13, allt lika:

- ABC80: wav2kas läser ROM:ens band (Python och C lika). BAC, text och
  datafil packade igen ger samma block som ROM:en, byte för byte, och
  programmet packat direkt ur källan ger samma block som `LIST CAS:`.
  ROM:en läser band från kas2wav (Python 44,1 kHz 16 bitar och C
  11 025 Hz 8 bitar) och ger samma skärm som med sina egna band.
- ABC 802 (bandet är tecken, inte signal, i emulatorn): BAC och text
  packade med `--tk 40` ger samma block som ROM:en. Datafilen ger samma
  text men inte samma block (FORMAT.md 3.1). ROM:en läser alla tre och
  ger samma skärm som med sina egna band.
- Båda: ett program till, med fler satser och uttryck, sparas med
  `SAVE CAS:` och listas med `LIST CAS:`. bac2bas (Python och C) på
  BAC-filerna ger samma text som ROM:en listade.

## C (c/)

C89 (`-std=c89 -pedantic`), bara standardbiblioteket och `mkdir`.
`make` bygger `kas`, `kas2wav`, `wav2kas`, `bac2bas` och `abctext`. De har samma namn,
underkommandon och flaggor som Python-verktygen och ger samma utdata
byte för byte: blockfiler, WAV-filer, uppackade filer, listor och
rapporter. Avkodningen behöver inte numpy.

`sh prov.sh [WAV ...]` (eller `make prov`) jämför C med Python:
påhittade filer (text med blanksteg, LF och långa rader, binärt, BAC,
tom fil, ÅÄÖ i namnet), WAV i tre hastigheter, WAV för ABC 800 med
fyra tidkonstanter (20, 40, 60 och 129) och de riktiga banden som
ges som argument. bac2bas och abctext provas i alla kombinationer av
`--kod` och `--rad` (abctext åt båda hållen, med UTF-8, 850 och trasig
UTF-8), också meddelandena och slutkoden. BAC-filer i katalogen `$BAC`
provas också.

Provat 2026-11-13: allt lika, också de elva riktiga banden med rapporten
block för block. Utan varningar med `-Wall -Wextra`, och utan fel med
AddressSanitizer och UBSan.

bac2bas och abctext provat 2026-11-13: 24 BAC-filer från skivorna (22 BASIC II,
en ABC80 och en tom) i alla nio kombinationer, och 600 BAC-filer med
slumpvis ändrade byte: C och Python lika, och inga krascher. abctext:
992 körningar och 990 filer med slumpvis trasig UTF-8, lika.
