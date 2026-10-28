# Fönster: textfönster i BASIC och fönster på grafikkortet

Plan 2026-10-28. Utanför boken, som de andra mapparna här: bokens filer
ändras inte, och det som kopieras står i LASMIG.md när det finns.

## Bakgrunden

Författaren pratade på 80-talet med Olle Wolkert om ”windows” för
ABC80 och ABC800, och gjorde då ett enkelt demo i BASIC för Olle:
fönster i textläget, ritade med `-` för vågräta streck, `!` för lodräta
och `+` i hörnen. Man flyttade kanske mellan dem med TAB eller någon
annan tangent; det minns författaren inte säkert. Demot finns inte kvar.

Två saker minns författaren (2026-10-28): programmet ritades och
förklarades med strukturerad programmering, JSP (Jacksons metod, med
trädet av sekvens, val och upprepning), och fönstren styrdes med koder
i texten, inspirerade av styrkoderna man skickade till skrivare: ett
ESC och en bokstav med argument mitt i det som skrevs ut, så att
programmet bara skriver ut och koderna väljer fönster, ritar ramen
eller rensar.

Försöket tar tanken två steg: först demot igen, i BASIC och i
textläget, sedan fönster på grafikkortet i HR-80:s anda (emulator/,
karna/hr80.c) med en mus på kortet och händelser tillbaka till ABC80
på V24. Valen (författaren 2026-10-28): tolken i C först, och en enkel
mus i stället för styrspak.

## Det som sätter gränserna

- **Kortets minne.** Annonsen säger 18 KB RAM. Bilden i färgläget är
  256 x 256 punkter à 2 bitar, 16 KB, så 2 KB blir kvar, och de har
  makrona i dag (HR80_MAKROMINNE). Det som ligger under ett fönster kan
  alltså inte sparas. Fönstren får överlappa, men när ett fönster
  flyttas, lyfts eller stängs ritar kortet bara ramarna och fyller det
  som blev synligt med bakgrundsfärgen, och lägger ”rita om fönster n”
  i kön till ABC80, som får rita innehållet igen (som expose i X).
  Fönstertabellen och kön tas ur de 2 KB; makrona får det som blir kvar.
- **V24 tillbaka.** ABC80 har en V24, och den läses bit för bit i
  programvaran (PIO B, port $3A bit 0). Ett tecken som kommer medan
  BASIC gör annat går förlorat. Därför sänder kortet aldrig på eget
  initiativ: händelserna väntar i kortets kö, och ABC80 frågar med ett
  kommando och läser svaret direkt efter, med avbrotten av.
- **Musen sitter på kortet**, inte på ABC80: den enda V24:an används
  redan av kortet, och kortet har en egen processor. Kortet läser musen,
  ritar pekaren och sköter dragning, lyft och stängning själv, så att
  ABC80 inte märker något förrän det frågar. Musen tänks som Ataris
  styrspak: några linjer och en knapp in på kortet. I emulatorn ger
  värden bara dx, dy och knappen, så det exakta slaget spelar ingen roll
  förrän ett riktigt kort byggs.
- **Läsrutinen i ABC80.** `hr80ut.asm` tar 59 av de 128 byte på $FF80
  som bruksanvisningen lämnar till egen maskinkod. Läsrutinen ska få
  plats i de 69 som är kvar (frågan om den ”får plats”).

## Steg

1. **Textfönstren i BASIC.** `FONSTER.BAS` för ABC80 (40 kolumner)
   gör om demot: två eller tre fönster ritade med `+`, `-` och `!`,
   ett aktivt i taget, byte med en tangent (TAB eller CTRL-tangenter,
   ABC80 har bara vänster- och högerpil), text skriven i det aktiva
   fönstret och kapad vid ramen. Det som ligger under ett fönster ritas
   om av programmet, för det finns inget att spara det i. Fönstren
   styrs som en skrivare: en rutin tar emot det programmet skriver ut,
   tecken för tecken, och tolkar egna styrkoder (ESC och en bokstav med
   argument; vilka bestäms i steget) som skapar, väljer, rensar och
   ramar in fönster. Programmet beskrivs med ett JSP-diagram, ritat för
   hand, som visar strukturen (sekvens, val, upprepning) och vilka rader
   som hör till vilken ruta. Samma program i BASIC II med 80 kolumner om det går utan att bli ett annat program.
   Körs i emulatorn och på webbsidan (exempeldisketten).
   **Klart 2026-10-28:** `emulator/exempel/program/text/FONSTER.BAS`,
   på exempeldisketten och som band; koderna och JSP-diagrammet i
   README.md. Tab (högerpilen) byter, Ctrl-X stänger, Ctrl-A öppnar
   igen, högst åtta fönster. Koderna är `CHR$(27)` och O, V eller S med
   talen + 32; ABC80 har ingen ESC-tangent, så de kommer bara från
   programmet, och styrtecken från tangentbordet (utom RETURN och
   vänsterpil) går inte till tolken. Heltalsvariabler, annars tar starten
   dubbelt så lång tid. Ingen BASIC II-version: där ska IF stå sist på
   raden, och nästan varje rad i tolken skulle få en annan form, så det
   blir ett annat program; BASIC II på ABC80 har dessutom bara 40
   kolumner utan TKN80.
2. **Fönstren i kortets tolk** (karna/hr80.c, fortfarande ren C).
   Samma styrkoder som i steg 1 där det går, så att BASIC-programmet
   kan flyttas från textfönstren till kortet med små ändringar.
   Nya ESC-kommandon, i koder som annonsen inte använder: skapa fönster
   n (x, y, bredd, höjd, titel), välj fönster n, lyft, stäng. Kortet
   ritar ramen, titelraden och en stängningsruta. Efter ”välj” gäller de
   gamla ritkommandona inom fönstret: koordinaterna räknas från dess
   hörn och kapas vid kanten, så att `HR80.BAS` kan ritas i ett fönster
   utan ändringar. Allt nytt står som påhittat i hr80.c och här. Prov i
   hr80prov/ som de gamla.
3. **Pekaren och musen.** `hr80_mus(h, dx, dy, knapp)` i tolken.
   Pekaren ritas ovanpå bilden i `hr80_rita`, inte i bildminnet, så att
   den inte behöver spara något under sig (på ett riktigt kort en
   överlagring i bildutmatningen). Klick i ett fönster lyfter det,
   dragning i titelraden flyttar det (med ramen som kontur medan det
   dras), klick i stängningsrutan stänger. Varje sådan sak ger en
   händelse i kön.
4. **Händelserna tillbaka på V24.** Ett kommando ”finns något?” som
   kortet svarar på med ett kort, fast format (händelsen, fönstret,
   x, y; eller ”inget”). I emulatorn går svaret till maskinens RxD med
   `v24_lagg` och `v24_starta`, i 9600 baud. `hr80in.asm` på $FF80
   efter sändrutinen läser svaret; `A=CALL(...)` ger det till BASIC.
   Byte räknade och prövade mot en riktig bitström i emulatorn.
5. **Exemplet.** `FONSTER2.BAS` (namnet bestäms då): några fönster på
   kortet, ett med HR80-bilden, ett med text och ett som räknar;
   BASIC-slingan frågar efter händelser och ritar om det som behövs.
6. **Värdarna.** `abc80 -H` med musens rörelser ur en fil (för proven),
   SDL-fönstret HR-80 med datorns mus, webbsidans blad med Grafikkortet
   med webbläsarens mus. Webbsidan prövas i Chromium och WebKit med
   Playwright.
7. **Storleken.** Räkna vad fönstren och musen tar i hr80.c, och om
   tolken med dem hade fått plats i 4 KB EPROM och 2 KB arbetsminne på
   en processor av den tiden. Skrivs om till Z80 bara om det visar sig
   intressant (författaren 2026-10-28: C först).
8. **LASMIG.md och README.md**, raden i `utanfor/README.md`, och
   eventuellt ett kapitel i häftet (bok/extra/PLAN.md).

Varje steg committas för sig.

## Öppna frågor

- Tangenten och antalet: besvarat 2026-10-28, Tab och högst åtta.
- Händelsernas format och vilka koder kommandona får (bestäms i steg 2
  och 4, efter att annonsens kodlista har gåtts igenom igen).
