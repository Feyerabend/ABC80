# Hårdvaran: från kassettkontakten till dagens dator

Det här är två förslag på hur en ABC80 eller en dator i ABC 800-serien kan
kopplas till en dagens dator i stället för till en bandspelare. Det första
är en kabel till ljudkortet. Det andra är en Raspberry Pi Pico som spelar
bandspelare med filer på ett SD-kort. Inget av dem är byggt och provat
ännu. Där en uppgift inte är kontrollerad står (?).

Verktygen i `c/` och `python/` gör redan det som behövs på datorsidan:
`kas2wav` gör ljud av en `.kas`, och `wav2kas` gör `.kas` av en inspelning.
Pico-förslaget flyttar samma kod till bandspelarens plats.

---

## 1 Kontakterna

**ABC 800-serien** (800 C/M, 802, 806). TAPE (P1) är en femstiftig DIN-kontakt
(schemat, blad 7):

| Stift | Signal | Riktning | Kretsen i datorn |
|-------|--------|----------|------------------|
| 1 | DFDOUT | ut | 74LS07 (öppen kollektor) med uppdragning till +5 V |
| 2 | GND | | |
| 3 | DFDIN | in | C44 (470 pF) till två LM339 med hysteres |
| 4, 5 | MOTOR | | kontakten i reläet RE1, sluten när motorn ska gå |

Ut kommer alltså en fyrkantvåg mellan 0 och 5 V. Kondensatorn C44 på
ingången gör att DFDIN bara ser flankerna och inte bryr sig om
likspänningsnivån. Komparatorerna behöver en tydlig flank, men hur stor
den måste vara går inte att läsa i schemat (?). En linjeutgång med
omkring 1 V topp bör räcka (?).

**ABC80.** Signalen och motorn har var sin kontakt: en femstiftig DIN för
data och ett uttag för motorn (Luxors servicemanual). I datorn sitter de på
PIO B, port $3A: bit 6 skriver, bit 7 läser genom en låskrets, och bit 5
drar motorreläet, som bryter högst 1 W vid 24 V. Vilket stift i
DIN-kontakten som är vilket, och vilka nivåer bandspelaren ger och tar
emot, står i Markesjö 1978 (s. 112–124, 164–167) men är inte kontrollerat
här (?). Luxors bandspelare var anpassad för datorn och kan själv ha
förstärkaren och pulsformaren för läsningen (?). I så fall vill ABC80 ha
TTL-nivå in, och då räcker inte en passiv kabel; se 2.3.

---

## 2 En kabel till ljudkortet

### 2.1 Från datorn till ljudkortets ingång (spara)

Fyrkantvågen på 5 V delas ned till linjenivå och skiljs från likspänningen:

```
DFDOUT (1) --┬-- 47 kΩ --┬-- 1 µF ---- linje in (vänster)
             │           │
          (redan       2,2 kΩ
          uppdragen)     │
GND (2) -----------------┴------------ linje in (jord)
```

Delaren ger 5 × 2,2 / 49,2 ≈ 0,22 V topp till topp, väl under vad en
linjeingång tål. Är ljudkortet en mikrofoningång kan 2,2 kΩ bytas mot
220 Ω. `wav2kas` söker själv upp tröskeln och bryr sig inte om polariteten,
så nivån är inte kritisk. 44,1 kHz räcker: en halv bitcell vid 2 400 baud
är 0,21 ms, ungefär 9 sampel.

### 2.2 Från ljudkortets utgång till datorn (läsa), ABC 800-serien

```
linje ut (vänster) -- 1 kΩ ---- DFDIN (3)
linje ut (jord) --------------- GND (2)
```

Motståndet skyddar bara utgången. C44 i datorn tar bort likspänningen.
Spela upp med full volym och höj eller sänk tills `LOAD CAS:` går igenom
(?). `kas2wav` skriver en fyrkantvåg; ett ljudkort med hårt lågpassfilter
gör om den till något rundare, men flankerna är det enda datorn ser.

### 2.3 Läsa, ABC80

Behöver ABC80 TTL-nivå på ingången (se 1) blir det en aktiv krets, till
exempel en komparator med hysteres, en LM393 med +5 V från datorn eller ett
batteri:

```
linje ut -- 10 nF --┬-------- (+) LM393 --┬-- ut till DIN (in)
                    │                     │
                  10 kΩ               4,7 kΩ till +5 V
                    │          (−) till 2,5 V via 2 × 10 kΩ
GND ----------------┴---- och 100 kΩ från ut till (+) för hysteresen
```

Det är samma sorts koppling som LM339 i ABC 800 (?).

### 2.4 Motorn

Motorstiften (4–5 i ABC 800-serien, det egna uttaget i ABC80) är en
reläkontakt utan spänning. Kabeln kan lämna dem öppna: då är det
användaren som startar inspelningen eller uppspelningen när datorn väntar.
Vid läsning har ABC80 ungefär 8,5 s på sig från anropet, så uppspelningen
ska startas strax efter `LOAD`. Den som vill se när datorn vill ha bandet
kan sätta en lysdiod, ett motstånd och ett batteri över kontakten.

---

## 3 En Pico som bandspelare

En Raspberry Pi Pico (RP2040) med ett SD-kort sitter där bandspelaren satt.
Den tar emot blocken som datorn skriver och sparar dem som `.kas`, och
spelar upp `.kas` när datorn vill läsa. Inget ljud behöver spelas in och
avkodas i efterhand, och motorreläet styr det hela som på en bandspelare.

### 3.1 Kopplingen

```
              Pico                           ABC
GP2 (in)  -- 10 kΩ --┬--------------------- DFDOUT / skrivsignal
                    18 kΩ
                     │
GND -----------------┴--------------------- GND
GP3 (ut)  -- 1 kΩ ------------------------- DFDIN / läsning
GP4 (in, uppdragen) ----------------------- motorkontakten, ena sidan
GND --------------------------------------- motorkontakten, andra sidan
SPI0 (GP16–19) ---- SD-kort
```

- DFDOUT är dragen upp till 5 V. Delaren 10 k / 18 k ger 3,2 V, som
  Picons ingång tål.
- Picons 3,3 V räcker för DFDIN, eftersom C44 bara ser flanken (?). För
  ABC80 måste nivån passa det som bandspelaren gav (se 1).
- Motorkontakten drar GP4 till jord när datorn startar motorn.
- Picon kan få ström från USB. Jord ska vara gemensam.

### 3.2 Programmet

En PIO-tillståndsmaskin i RP2040 tar tid på flankerna med 1 µs
upplösning, och en annan skriver flankerna. Resten är samma kod som i
`c/abckas.c`, som är skriven i C89 och inte behöver något operativsystem:

- **Spara.** När motorn startar och flanker kommer in läggs tiderna i en
  ringbuffert. `edges_to_bits` och `find_blocks` gör om dem till block,
  som skrivs till en `.kas` på SD-kortet. Tröskeln mellan halv och hel
  cell blir 3/4 av den nominella cellen. För ABC 800-serien är cellen
  32 × tk T, med tk 129 för filhuvudet och sedan tk ur byte 14 i det, som
  i `decode_wav`. Ett block per 3 s ger gott om tid att skriva till kortet.
- **Läsa.** När motorn startar och ingen signal kommer spelas nästa fil:
  `block_bytes` och `block_edges` ger flankerna, med samma tider som
  `kas2wav` (ABC80: 3993 och 4065 T vid 2,9952 MHz; ABC 800-serien:
  32 × tk T vid 3 MHz). Före första blocket väntar Picon ungefär en
  sekund, och när motorn stannar stannar uppspelningen, så att blocken
  inte går förlorade medan datorn är upptagen.
- **Välja fil.** Över USB-serieporten: `lista`, `spela FIL`, `spela in FIL`.
  Utan kommando sparas inspelningen under namnet i filhuvudet, och
  uppspelningen tar den fil på kortet vars namn står först.
- **Läge.** ABC80 eller ABC 800 väljs med en bygel på en GPIO eller med
  ett kommando, eftersom tiderna och synken skiljer sig. Vid läsning
  provar Picon båda, som `wav2kas` gör.

Filerna på kortet är samma `.kas` som verktygen använder, så det går att
flytta dem mellan kortet och `kas upp`, `bac2bas` och `abctext` utan att
ljud är inblandat. Emulatorerna i `emulatorer/pico` i ABC80-arkivet
visar hur projekt för Picon byggs med CMake och Pico SDK, och hur ett
SD-kort läses med FatFs.

---

## 4 I vilken ordning

1. Kabeln i 2.1 och `wav2kas` mot ett ABC 800 eller 802: datorn skriver
   `SAVE CAS:`, ljudkortet spelar in. Det kontrollerar bitcellen
   32 × tk T mot en riktig maskin, vilket inte är gjort.
2. Kabeln i 2.2 och `kas2wav --abc800`: datorn läser.
3. ABC80: ta reda på stiften och nivåerna (1) och bygg 2.1 och 2.3.
4. Picon, först bara med spara, sedan läsa och motorn.
