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
  förråd. `airfight.asm` är den källan, flyttad till $E000 så att den
  ryms i 16 KB, och med tangenterna lästa som på maskinen: bit 7 i port
  56 är nere-signalen, och koden ligger kvar när tangenten släpps
  (Pico-emulatorn gav 0, och då stannade spelet vid REDO!). `text/AIRFIGHT.BAS` är en laddare
  med koden i DATA-rader, som lägger den på $E000 och startar den med
  `CALL(57344)`. Laddningen tar en halv minut, och spelet går inte
  tillbaka till BASIC (RESET). Svaren på J/N, namnen och spelläget (P
  eller T) avslutas med RETURN. Spelare 1 har A och D (svänger) och X
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

## Som text

`text/NAMN.BAS` är programmet som vanlig text, som skrivs in rad för
rad som tangenter. I webbsidan: *Skriv in…* (eller dra filen till
sidan) och skriv `RUN`. I terminalen:

    ./abc80 -r roms/abc80new.rom -f exempel/program/text/KURVA.BAS -k 'RUN\r'

## Övriga filer

- `airfight.asm`: källan till AIRFIGHT.
- `airfight.bin`: den assemblerade koden (3 522 bytes).
- `gor.sh`: gör om banden, disketten och `text/AIRFIGHT.BAS` ur
  `text/` och `airfight.asm` (z80asm från `verktyg/` i det publika
  förrådet, och `../../abc80` måste vara byggd).
