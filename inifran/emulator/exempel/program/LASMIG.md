# Exempelprogram till ABC80 inifrån

Fyra program för ABC80. Alla fyra finns i tre former, en mapp för var:

| Mapp        | Vad                                  | Laddas med                  |
|-------------|--------------------------------------|-----------------------------|
| `kassett/`  | ett band per program, `NAMN.wav`     | `RUN CAS:` (kassetten)      |
| `diskett/`  | alla fyra på `program.dsk`           | `RUN DR0:NAMN` (ABC-DOS)    |
| `text/`     | programmen som text, `NAMN.BAS`      | skrivs in som tangenter     |

Webbsidan har disketten inbyggd, på knappen *Exempeldisketten*. Banden
är för stora för att följa med där, men de går att lägga i med *Lägg i
band…* eller att dra till sidan.

## Programmen

- `GISSA`: gissa ett tal mellan 1 och 100. Nytt till boken.
- `KURVA`: axlar, en sinuskurva och en dämpad cosinus med `SETDOT`, i
  grafikläget (koden `CHR$(151)` först på varje rad). Nytt till boken.
- `MUSIK`: en orgel på tangentbordet, av Kristian Lidberg 1981,
  inskriven av Set Lonnert (`airfight/MUSIC.TXT` i
  <https://github.com/Feyerabend/ABC80>), oförändrad utom att `¤` är
  skrivet `$`. De vita tangenterna är Z, X, C, V, B, N, M, `,`, `.`
  och `-`, de svarta S, D, G, H, J, L och Ö; RETURN tömmer skärmen.
  Programmet väntar sig versaler, så slå på Caps Lock. Tonen ljuder
  medan tangenten hålls nere; i emulatorn är ett tryck kort, så tonen
  blir kort. De två talen som skrivs efter varje ton kommer från
  originalet.
- `AIRFIGHT`: spelet för två av Kristian Lidberg och Set Lonnert 1981,
  i den Z80-version som finns under `emulatorer/pico/04/` i samma
  förråd. `airfight.asm` är den källan, med tangenterna lästa som på
  maskinen: bit 7 i port 56 är nere-signalen, och koden ligger kvar när
  tangenten släpps (Pico-emulatorn gav 0, och då stannade spelet vid
  REDO!). Det som beror på ROM:en står i en maskinfil som gor.sh lägger
  efter källan: `airfight_abc80.asm` lägger spelet på $E000, så att det
  ryms i 16 KB, och `airfight_b2.asm` på $9000 för BASIC II (se nedan).
  `text/AIRFIGHT.BAS` är en laddare med koden i DATA-rader, som lägger
  den på $E000 och startar den med `CALL(57344)`. Laddningen tar en halv
  minut, och Ctrl-C går tillbaka till BASIC. Svaren på J/N, namnen och
  spelläget (P eller T) avslutas med RETURN. Spelare 1 har A och D
  (svänger) och X
  (skjuter), spelare 2 J, L och M.

## Från kassetten

`kassett/NAMN.wav` är programmet sparat med `SAVE CAS:NAMN`, i 8 bitar
och 11 025 prov/s, ett band per program. I webbsidan: *Lägg i band…*,
välj bandet och skriv `RUN CAS:`. Skriv inget medan blocken läses (det
ger ERR 35). I terminalen, från emulatorns mapp (Ctrl-C två gånger
avslutar):

    ./abc80 -r roms/abc80new.rom -k 'RUN CAS:\r' -T exempel/program/kassett/GISSA.wav

## Från disketten

`diskett/program.dsk` har alla fyra, som `NAMN.BAC`, på en diskett för
ABC-DOS (FD2, 160 KB). I webbsidan: *Exempeldisketten* (eller *Lägg i
diskett…* med den här filen) och skriv `RUN DR0:GISSA`, `RUN DR0:KURVA`,
`RUN DR0:MUSIK` eller `RUN DR0:AIRFIGHT`. I terminalen, med ABC-DOS på
FD2-kortet:

    ./abc80 -r roms/abc80new.rom -l roms/abcdos80.rom@6000 \
        -D exempel/program/diskett/program.dsk@45 -k '\w1500\.RUN DR0:GISSA\r'

## Forthaid II:s ord

Disketten har också `VERKTYG.FTH`, egna ord till Forthaid II
(`utanfor/forthaid2`): `n CROSS` listar de rader som hoppar till rad n,
`n RAM` visar rad n byte för byte, `FIX` ger en referens till en rad som
inte finns numret på nästa rad, och `PAC` tar bort raderna med `REM` och
gör sedan `FIX`. Källan är `forth/VERKTYG.FS`, och `forth/spara.sh`
lägger varje `forth/NAMN.FS` på disketten som `NAMN.FTH`, om Forthaid II
är byggd. Både `gor.sh` här och Forthaid II:s `gor.sh` kör det, eftersom
LOADW bara laddar en ordfil som gjordes med samma Forthaid II; efter en
ombyggd Forthaid II görs webbsidan om med `make webb`. I webbsidan:
*Forthaid II* i listan, *Exempeldisketten*, `LOAD GISSA`,
`Z=CALL(16384,2)`, `FORTH`, `LOADW VERKTYG.FTH` och `60 CROSS`. Mer i
Forthaid II:s `MANUAL.md`.

## Som text

`text/NAMN.BAS` är programmet som vanlig text, som skrivs in rad för
rad som tangenter. I webbsidan: *Skriv in…* (eller dra filen till
sidan) och skriv `RUN`. I terminalen:

    ./abc80 -r roms/abc80new.rom -f exempel/program/text/KURVA.BAS -k 'RUN\r'

## För BASIC II

`text_b2/` har programmen skrivna om för BASIC II (ABC 800-tolken, se
?rom=b2 på webbsidan): `GISSA`, `KURVA`, `MUSIK`, `CHIP8` (en CHIP-8-tolk
med spelet Mini Lights Out av tobiasvl, CC0) och `SNAKE3` (en orm som
styrs med A, S, W och Z). `CHIP8Z` och `SNAKE3Z` är samma två spel i
Z80-kod, med laddare som `AIRFIGHT`: koden läggs på $9000 och startas
med `CALL(36864)`. Laddningen tar tio sekunder, och spelen går mycket
fortare än BASIC-versionerna. Ctrl-C går tillbaka till BASIC, och
`SNAKE3Z` skriver poängen. `AIRFIGHT` är samma källa som för ABC80,
assemblerad med `airfight_b2.asm`: koden på $9000 och variablerna på
$9F00 (BASIC II har laddaren högst upp, där $E000 ligger), klockan
$FFF5 i stället för $FDF0 och BREAK genom `BRKPTR`. Laddningen tar en
halv minut. gor.sh sparar alla åtta på
`diskett/program_b2.dsk`, som webbsidan har som exempeldiskett när
BASIC II är vald. Skriv `RUN MO0:CHIP8Z`, `RUN MO0:AIRFIGHT` och så vidare. I terminalen:

    ./abc80 -r roms/basicii80.rom -m 32 \
        -D exempel/program/diskett/program_b2.dsk@45 -DS -k 'RUN MO0:SNAKE3Z\r'

## Övriga filer

- `airfight.asm`: källan till AIRFIGHT, och maskinfilerna
  `airfight_abc80.asm` och `airfight_b2.asm` (adresserna, klockan och
  BREAK).
- `airfight.bin` och `airfight_b2.bin`: den assemblerade koden för
  ABC80 (3 584 bytes) och BASIC II (3 589 bytes).
- `chip8.asm`, `snake3.asm`: källorna till CHIP8Z och SNAKE3Z, och
  `chip8.bin` (1 891 bytes) och `snake3.bin` (725 bytes).
- `gor.sh`: gör om banden, disketterna och laddarna `text/AIRFIGHT.BAS`,
  `text_b2/AIRFIGHT.BAS`, `text_b2/CHIP8Z.BAS` och `text_b2/SNAKE3Z.BAS` ur `text/`, `text_b2/`
  och källorna (z80asm från `verktyg/` i det publika
  förrådet, och `../../abc80` måste vara byggd).
