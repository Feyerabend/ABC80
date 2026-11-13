# kassett

Verktyg och beskrivning för kassettbanden i ABC80 och ABC 800-serien
(800 C/M, 802 och 806). Under arbete.

- [FORMAT.md](FORMAT.md): signalen, blocken, filerna och teckenkoderna,
  med källor och öppna frågor..

## Python (python/)

Python 3. Avkodningen av WAV behöver numpy.

| Verktyg | Gör |
|---------|-----|
| `abckas.py` | kärnan: block, filer, text som ROM:en skriver den, ABC80:s FM-signal in och ut |
| `kas.py lista BAND.kas` | filerna och blocken i en blockfil |
| `kas.py packa -o BAND.kas FIL ...` | filer -> blockfil (se nedan om text; `-t`/`-b` tvingar, `--tk N` för ABC 800-serien) |
| `kas.py upp -d KATALOG BAND.kas` | blockfil -> filer |
| `kas2wav.py -o BAND.wav BAND.kas` | blockfil eller filer -> WAV (`-t`, `-b` och `--tk` som i `packa`) |
| `wav2kas.py [-r] BAND.wav` | WAV -> blockfil, med rapport block för block |

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

## C (c/)

C89 (`-std=c89 -pedantic`), bara standardbiblioteket och `mkdir`.
`make` bygger `kas`, `kas2wav` och `wav2kas`. De har samma namn,
underkommandon och flaggor som Python-verktygen och ger samma utdata
byte för byte: blockfiler, WAV-filer, uppackade filer, listor och
rapporter. Avkodningen behöver inte numpy.

`sh prov.sh [WAV ...]` (eller `make prov`) jämför C med Python:
påhittade filer (text med blanksteg, LF och långa rader, binärt, BAC,
tom fil, ÅÄÖ i namnet), WAV i tre hastigheter och de riktiga banden som
ges som argument.

Provat 2026-11-13: allt lika, också de tio riktiga banden med rapporten
block för block. Utan varningar med `-Wall -Wextra`, och utan fel med
AddressSanitizer och UBSan.

`c/bac2bas.c` är den gamla, som utgångspunkt för `bac2bas`; den byggs
inte än.
