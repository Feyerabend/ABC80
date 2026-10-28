# Fönster

Utanför boken. Textfönster på ABC80 i BASIC, efter ett demo som
jag gjorde för Olle Wolkert på 80-talet. Demot finns inte kvar,
och `FONSTER.BAS` är en ny version av det. Samma koder fungerar sedan
på grafikkortet i HR-80:s anda (`emulator/karna/hr80.c`), som har fått
fönster, en mus och händelser tillbaka till ABC80; `FONSTER2.BAS` visar
dem. **Kortets fönster och mus är påhittade.** Planen står i
[PLAN.md](PLAN.md), vad som är ändrat och var i [LASMIG.md](LASMIG.md),
och om det hade fått plats på kortet i [STORLEK.md](STORLEK.md).

## FONSTER.BAS

Programmet ligger bland exemplen, i
`emulator/exempel/program/text/FONSTER.BAS`, och på exempeldisketten:
`RUN DR0:FONSTER` (på webbsidan *Exempeldisketten*). I terminalen:

    cd emulator
    ./abc80 -f exempel/program/text/FONSTER.BAS -k 'RUN\r'

Fönstren ritas med `+` i hörnen, `-` för vågräta och `!` för lodräta
streck. Det aktiva fönstret har `=` i överkanten och ligger alltid
överst. Det som skrivs hamnar i det aktiva fönstret, radbryts vid ramen,
och när fönstret är fullt rullar det. Tangenterna:

| Tangent             | Gör                                          |
|---------------------|----------------------------------------------|
| Tab eller högerpil  | lyfter det understa fönstret, som blir aktivt |
| Ctrl-X              | stänger det aktiva fönstret                  |
| Ctrl-A              | öppnar det första stängda fönstret igen      |
| RETURN, vänsterpil  | ny rad, tar bort det sista tecknet           |
| Ctrl-C              | avbryter programmet (fönstren blir kvar)     |

Högst åtta fönster går att öppna. Ett fönster som öppnas igen är tomt.

## Styrkoderna

Programmet skriver allt genom en rutin, `UT` på rad 1000, som tar emot
`A$` och tolkar den tecken för tecken, som en skrivare tolkar sina
styrkoder: ett `CHR$(27)` och en bokstav med argument mitt i texten.
ABC80 har ingen ESC-tangent, och koderna skickas bara av programmet.
Det som skrivs på tangentbordet går till `UT` bara om det är vanliga
tecken, RETURN eller vänsterpil, så att ingen tangent kan starta en kod.

| Koden                         | Gör                                          |
|-------------------------------|----------------------------------------------|
| `CHR$(27)` `O` n r k h b titel CR | öppnar fönster n på rad r, kolumn k, h rader högt och b tecken brett (med ramen), med titeln |
| `CHR$(27)` `V` n              | väljer fönster n och lyfter det överst        |
| `CHR$(27)` `S` n              | stänger fönster n                             |
| `CHR$(12)` (FF)               | rensar det aktiva fönstret                    |
| `CHR$(13)` (CR)               | ny rad                                        |
| `CHR$(8)` (BS)                | tar bort det sista tecknet på raden           |

Talen efter bokstaven skrivs som tecken, talet + 32, som i markörkoden
till grafikkortet (`CHR$(n+32)`), så att de aldrig blir ett styrtecken.
Ett öppet fönster som öppnas igen stängs först. Fönstret flyttas in på
skärmen om det inte ryms (rad 0–22, kolumn 0–39; rad 23 är hjälpraden).

## Inget sparas under fönstren

Programmet sparar inte det som ligger under ett fönster. Varje fönster
har i stället sina rader i `L$`, en strängvektor med plats för 80 rader
om 38 tecken som fönstren får en bit var av när de öppnas. När ett
fönster stängs suddas dess ruta, och de fönster som ligger kvar ritas om
nerifrån och upp, men bara den del av dem som ligger inom rutan (rad
3000 ritar ett fönster inom en rektangel). Att lyfta ett fönster kräver
ingen omritning under: det ritas helt ovanpå, och det förra aktiva får
bara sin överkant ritad om, med `-` i stället för `=`. Samma sätt behövs
på grafikkortet, där minnet inte räcker till att spara något.

## Heltalen

Alla variabler är heltal (`I%`, `1%`). Med flyttal tar en slinga med
`ASC(MID$(...))` omkring 4 ms per varv i emulatorn, med heltal omkring
1 ms, och starten med de tre fönstren tog mer än åtta sekunder i stället
för omkring fyra. Vanlig text skrivs dessutom en bit i taget, fram till
ramen eller nästa styrtecken (rad 2300), i stället för tecken för tecken.

## JSP

Programmet beskrivet med Jacksons strukturerade programmering, som
demot ritades på 80-talet: varje ruta är en del av programmet, rutorna
under en ruta utan tecken är en sekvens (från vänster till höger), `*`
betyder upprepning och `o` val. Radnumren står under rutorna.

Huvudprogrammet:

                              FONSTER
                                 |
           +---------------------+---------------------+
           |                     |                     |
        Början              Tre fönster           Tangenterna
        40-90               100-210               300-370
                                                       |
                                                  Tangent  *
                                                  310-370
                                                       |
                +-------------+-------------+----------+-----+
                |             |             |                |
             Tab   o     Ctrl-X  o     Ctrl-A  o        Annat  o
             400          450           500             360-370
                                                             |
                                                  +----------+---------+
                                                  |                    |
                                           Styrtecken  o      Till UT  o
                                           (bort) 360         370

Tolken, rutinen `UT`:

                                 UT
                              1000-1040
                                 |
                              Bit  *
                             1020-1040
                                 |
                +----------------+-----------------+
                |                                  |
          Text  o                           Ett tecken  o
          2300                              1100
          (till ramen eller                       |
          nästa styrtecken)                       |
         +---------------+-------------+----------+---------+
         |               |             |                    |
     S=0  o          S=1  o        S=2  o               S=3  o
     1200            1300          1400                 1500
         |                             |                    |
   +-----+----+----+----+      +-------+------+      +------+------+
   |     |    |    |    |      |              |      |             |
  ESC o CR o FF o BS o Annat o Argument  Sista?  o  CR  o       Tecken  o
  1210  2400 2500 2600 (bort)  1410         |      ÖPPNA 2200  i titeln
                                     +------+------+------+
                                     |      |      |      |
                                    V o    S o    O o   Inte o
                                   VÄLJ   STÄNG  S=3
                                   2000   2100   1440

ÖPPNA, VÄLJ och STÄNG använder samma tre delar: `2700` tar bort ett
fönster ur ordningen `Z%` (understa först), `2800` lägger det överst och
ritar det, och `3000` ritar ett fönster inom en rektangel.

## Fönstren på grafikkortet

Grafikkortet i HR-80:s anda (`emulator/karna/hr80.c`, utanför boken)
tar emot kommandon på V24. Fönstren lades till med samma bokstäver som
i FONSTER.BAS, i ESC-koder som annonsen inte använder. De är påhittade.
Talen skrivs + 32, som ovan:

| Koden                         | Gör                                          |
|-------------------------------|----------------------------------------------|
| `ESC O n r k h b titel CR`    | öppnar fönster n (0-7) på textrad r, kolumn k, h rader högt och b tecken brett med ramen; det blir överst och valt |
| `ESC V n`                     | väljer fönster n och lyfter det överst        |
| `ESC T n`                     | väljer fönster n utan att lyfta det; 8 är skrivbordet, ytan utanför fönstren |
| `ESC S n`                     | stänger fönster n                             |
| `ESC ?`                       | frågar efter en händelse; svaret är fyra byte på V24 |

Kortet ritar ramen med en titelrad och en stängningsruta; det översta
fönstrets titelrad är fylld. Efter `V` eller `T` gäller annonsens
kommandon i det valda fönstret: grafikens (0, 0) är fönstrets nedre
vänstra hörn innanför ramen, markören räknas innanför ramen, FF rensar
bara fönstret, texten bryts och rullar där, och allt kapas vid ramen och
vid fönstren ovanför. Ett program som ritar på hela kortet ritar alltså
i ett fönster utan ändringar.

Inget sparas under fönstren, av samma skäl som i FONSTER.BAS: bilden
tar 16 av kortets 18 kilobyte. När ett fönster stängs eller lyfts får
det som blev synligt bakgrundsfärgen, och kortet kommer ihåg att
fönstret ska ritas om.

### Musen

Musen sitter på kortet, inte på ABC80, som en styrspak med en knapp.
Kortet ritar pekaren ovanpå bilden, inte i bildminnet, och sköter
fönstren själv:

- ett tryck i stängningsrutan stänger fönstret;
- ett tryck i titelraden lyfter fönstret, och medan knappen hålls dras
  en streckad ram; där knappen släpps flyttas fönstret, med innehållet;
- ett tryck i fönstret lyfter det och ger ett klick, på skrivbordet bara
  klicket.

Det valda fönstret byts inte, och ABC80 märker inget förrän det frågar.

### Händelserna

ABC80 läser V24 bit för bit i programvaran och tappar det som kommer
medan BASIC gör annat. Därför sänder kortet bara när ABC80 frågar med
`ESC ?`, en millisekund efter frågan, i 9600 baud. Svaret är alltid fyra
byte:

| Svaret      | Betyder                                                    |
|-------------|------------------------------------------------------------|
| `K n x y`   | klick i fönster n (8 skrivbordet), grafikens x och y i fönstret |
| `L n 0 0`   | fönster n lyftes                                           |
| `F n r k`   | fönster n flyttades till textrad r, kolumn k               |
| `S n 0 0`   | fönster n stängdes                                         |
| `R n 0 0`   | fönster n (8 skrivbordet) ska ritas om, när kön är tom     |
| `0 0 0 0`   | inget                                                      |

Kön på kortet rymmer 16 händelser. Läsrutinen i ABC80 är `hr80in.asm`
(`emulator/exempel/program/`), 64 byte på $FFBB direkt efter
sändrutinen `hr80ut.asm`. `Z=CALL(65467)` skickar `ESC ?`, letar upp
startbiten med avbrotten av och läser varje bit mitt i; svaret läses med
`PEEK(65532)` till `PEEK(65535)`. Kommer inget svar på en sekund blir
den första byten 0. Rutinen ligger där UFD-DOS:en har sin korttabell
(`$FFC0`–`$FFE0`) och UFDUN (`$FFF9`), så den går inte med UFD-DOS,
bara utan DOS eller med ABC-DOS, som lämnar `$FFB0`–`$FFFF` orört.

### FONSTER2.BAS

`emulator/exempel/program/text/FONSTER2.BAS`, på exempeldisketten
(`RUN DR0:FONSTER2`) och som band, öppnar tre fönster på kortet: BILD
med HR80-bilden, TEXT och RÄKNARE. Slingan frågar kortet hela tiden.
Inget svar räknar upp RÄKNARE, `R n` ritar fönster n igen, ett klick i
BILD lägger en stjärna där, och ett klick på skrivbordet öppnar de
stängda fönstren igen. Händelserna skrivs också på ABC80:s skärm.

Musen finns i alla tre värdarna:

- på webbsidan: koppla in grafikkortet och använd musen på kortets bild;
- i SDL-fönstret: `abc80f -H`, datorns mus i HR-80-fönstret
  (`emulator/sdl/README.md`);
- i terminalen: `abc80 -H bild.png -Hm fil` läser musen ur en fil, en rad
  `ms dx dy knapp` per rörelse, ms räknat från det att tangenterna
  skrivits (`emulator/exempel/program/fonster2.mus`):

      cd emulator
      ./abc80 -r roms/abc80new.rom -f exempel/program/text/FONSTER2.BAS \
          -k 'RUN\r' -H fonster2.png -Hm exempel/program/fonster2.mus -t 50000

### Storleken

Med fönstren och musen blir kortets tolk 2,2-2,5 gånger så stor. Den
hade behövt 16 kilobyte EPROM i stället för annonsens 4, och mer
arbetsminne ([STORLEK.md](STORLEK.md)). Emulatorn visar alltså ett kort
som kunde ha byggts, inte HR-80.
