# Fönster

Utanför boken. Textfönster på ABC80 i BASIC, efter ett demo som
författaren gjorde för Olle Wolkert på 80-talet. Demot finns inte kvar,
och det här är en ny version av det. Planen, med fönster och mus på
grafikkortet som nästa steg, står i [PLAN.md](PLAN.md).

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
på grafikkortet i steg 2, där minnet inte räcker till att spara något.

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
