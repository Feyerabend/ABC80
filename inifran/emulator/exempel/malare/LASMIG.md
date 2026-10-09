# MÅLARE av Mikael Bonnier (1982)

Ett ritprogram för ABC80 i BASIC. Mikael Bonnier skrev det i april 1982,
15 år gammal, på grundskolans ABC80, och hade det på sin kassett som
`MÅLARE.BAC`. Markören flyttas i åtta riktningar med tangenterna runt F,
Caps Lock avgör om den ritar eller suddar, S tömmer skärmen, och varje
riktning har sitt eget ljud (port 6).

| Fil          | Vad                                                         |
|--------------|-------------------------------------------------------------|
| `MALARE.BAS` | programmet som text, ISO-646-SE (`]` är Å, `\|` är ö)       |
| `MALARE.wav` | bandet, sparat med `SAVE CAS:` i emulatorn (`gor.sh`)       |
| `COPYING`    | GNU General Public License, version 3                       |
| `gor.sh`     | gör `MALARE.wav` av `MALARE.BAS`                            |

## Upphov och licens

Rad 10 i programmet: `REM ***COPYRIGHT 1982.04.02    Mikael Bonnier***`.
Bonnier har själv lagt ut programmet, med de andra från samma kassett,
i förrådet <https://github.com/mobluse/abc80-mob> under GNU General
Public License version 3 (GPL-3.0). `COPYING` är förrådets `LICENSE`,
oförändrad. Programmet är alltså fri programvara: det får köras,
ändras och spridas vidare under samma licens, och källan ska följa med.
Här är den `MALARE.BAS`.

`MALARE.BAS` är `malare.bas` från Bonniers sida
<https://orbin.se/df.lth.se/~mikaelb/abc/80/>, hämtad 2026-10-10,
byte för byte, bara med nytt namn. Där finns programmet i ABC80:s egen
teckenkodning, som emulatorn läser. I abc80-mob står det i UTF-8.

## Förbehåll

- **Licensen gäller förrådet.** GPL-3.0 står för abc80-mob som helhet.
  Programfilen har bara copyrightraden från 1982 och ingen egen
  licensrad. Vi läser det som att Bonnier har gett ut programmet
  under GPL-3.0. Vill man vara helt säker får man fråga Bonnier.
- **Bara MÅLARE.** Samma kassett hade också EDIT, MASKEN, MUZAK, ROBOT,
  SCHACK, SERIE, STARTREK och TENNIS. De finns i abc80-mob, men vem som
  skrev dem framgår inte (STARTREK kommer ur boken *Dataspel i Basic*),
  så de är inte med här.
- **Inte originalbandet.** Bonnier läste kassetten med H. Peter Anvins
  `mfmdecode`, lade filerna på en diskett (`muzak.dsk`) och gjorde om
  `MÅLARE.BAC` till text med `LOAD` och `LIST`. `MALARE.wav` är den
  texten sparad igen med `SAVE CAS:M]LARE` i den här emulatorn. Namnet
  på bandet är originalets, `MÅLARE.BAC`, men den semikompilerade koden
  är gjord på nytt och behöver inte vara lika byte för byte. Inspelningen
  av originalkassetten (`muzak96k.wav`) nämns i Bonniers README, men
  finns inte kvar på sidan.
- **Oförändrat.** Raderna är som Bonnier lämnade dem, också `K%=79` utan
  `%` på rad 130 och de fyllda rutorna (tecken 127) i kommentarerna på
  rad 170–250.
- **Annan licens än resten.** GPL-3.0 gäller filerna i den här mappen.
  Webbsidan `webb/abc80.html` har bandet inbyggt som data, som emulatorn
  spelar upp. Programmet körs på den emulerade maskinen och är inte
  länkat till emulatorns kod, så sidan och bandet är skilda verk som
  ligger tillsammans (”aggregate”, GPL-3.0 avsnitt 5). Sidans hjälptext
  säger att programmet är GPL-3.0 och hänvisar hit.

## Köra det

I webbläsaren (`webb/abc80.html`): knappen *MÅLARE* i kassettgruppen
lägger i bandet och skriver `RUN CAS:`. Svara på `Tidsfördröjning?` med
ett tal (till exempel 10) och RETURN. Gemener ritar, versaler flyttar
markören och suddar där den har varit:

    E R T        upp-vänster  upp     upp-höger
    D   G        vänster              höger
    C V B        ned-vänster  ned     ned-höger

Ett tryck i emulatorn är kort men räcker för några steg, eftersom
programmet läser tangenten (port 56) så länge den är nere.

I terminalen, från emulatorns mapp (laddningen tar en halv minut,
Ctrl-C två gånger avslutar):

    ./abc80 -r roms/abc80new.rom -k 'RUN CAS:\r' -T exempel/malare/MALARE.wav

Bandet görs om med `exempel/malare/gor.sh`.
