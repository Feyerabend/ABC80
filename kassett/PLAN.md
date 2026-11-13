# Plan för kassett/

Målet är att föra filer fram och tillbaka mellan kassettband (ljud) och
dagens filer, för ABC80, ABC 800 C/M, ABC 802 och ABC 806. Allt finns i
både C89 och Python 3, med samma namn och samma flaggor. Dessutom ska det
finnas förslag på hårdvara och en PDF som förklarar hur kassetten fungerar.

Mappen utvecklas här, i `~/Documents/abc802/kassett/`. När den är klar
kopieras den till `GitHub/ABC80/kassett/`, och då ersätter den de gamla
verktygen (`tobasic/` och `towave/`). Det som finns där incheckas inte
härifrån.

## Verktygen, lager för lager (se FORMAT.md)

| Verktyg | Från → till | Lager |
|---------|-------------|-------|
| `kas2wav` | blockfil eller fil → WAV (ABC80-FM; ABC 800 när signalen är belagd) | 1–3 |
| `wav2kas` | WAV → filer + rapport per block (summa, nummer, luckor) | 1–3 |
| `kas` | blockfil (`.kas`: rå block, 256 byte styck) ↔ filer, list, kontroll | 2–3 |
| `bac2bas` | BAC (ABC80 $82, BASIC II $8F) → BAS-text | 4 |
| `abctext` | teckenkod och radslut: 7-bit ↔ CP437/850 ↔ UTF-8, CR ↔ CRLF ↔ LF | 4 |

`.kas` är mellanformatet: blocken utan synk och utfyllnad, i bandets
ordning. Det kan provas byte för byte och skrivas av emulatorerna.

## Ordningen

1. FORMAT.md, med de öppna frågorna besvarade så långt källorna räcker
   (klart i första version)
2. Kärnan i Python: block, fil, ABC80-FM in och ut, `.kas` (klart)
3. Samma kärna i C89, och prov som jämför C och Python byte för byte (klart)
4. Prov mot facit: de 11 riktiga WAV-filerna i `~/Documents/abc80`, och
   band skrivna av ROM:en i emulatorerna (ABC80 och ABC 802)
   (klart: `c/prov.sh`, `prov/abc80.sh` och `prov/abc802.sh`)
5. `bac2bas` (den befintliga C-versionen som utgångspunkt) och `abctext`
6. ABC 800-serien: schemat, ROM 0 för ABC 800, och sedan lager 1
7. Hårdvaran: en passiv nivåanpassning till ljudkortet, och en Pico som
   "bandspelare" (DIN-kontakten, motorreläet, se `emulatorer/pico`)
8. PDF:en, med nyritade figurer: signalen, FM, blocket, summan, ROM-rutinen
   steg för steg (efter Markesjö), ABC 800-serien, verktygen och hårdvaran
9. Kopiera till `GitHub/ABC80/kassett/`, och ta bort `tobasic/` och `towave/`
   där. Användaren checkar in själv.

## Det som de gamla verktygen gjorde fel (ska inte upprepas)

- `abc2wav`: sista biten saknade avslutande flank (rättat där), ett extra
  block vid 253·n byte, `printf` i WAV-utdata, ingen ETX i textfiler.
- `wav2basic.py`: flaggorna saknade `--` (rättat där).
- `wav2bin`/`bin2basic` var redan ersatta men byggdes fortfarande.
