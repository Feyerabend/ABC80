# Fönster: textfönster i BASIC och fönster på grafikkortet

Plan 2026-10-28. Utanför boken, som de andra mapparna här: bokens filer
ändras inte, och det som kopieras står i LASMIG.md när det finns.

## Bakgrunden

Jag pratade på 80-talet med Olle Wolkert, som ledde OWOCO (OW efter
hans namn), om fönster för ABC80 och ABC800, med Macintoshen som vi
hade på OWOCO (den som jag typsatte Smartaids manual på) och
Lisp-maskinerna på Uppsala universitet, med fönster och mus, som
förebilder, och gjorde då ett enkelt demo i BASIC för Olle:
fönster i textläget, ritade med `-` för vågräta streck, `!` för lodräta
och `+` i hörnen. Man flyttade kanske mellan dem med TAB eller någon
annan tangent; det minns jag inte säkert. Demot finns inte kvar.

Två saker minns jag (2026-10-28): programmet ritades och
förklarades med strukturerad programmering, JSP (Jacksons metod, med
trädet av sekvens, val och upprepning), och fönstren styrdes med koder
i texten, inspirerade av styrkoderna man skickade till skrivare: ett
ESC och en bokstav med argument mitt i det som skrevs ut, så att
programmet bara skriver ut och koderna väljer fönster, ritar ramen
eller rensar.

Försöket tar tanken två steg: först demot igen, i BASIC och i
textläget, sedan fönster på grafikkortet i HR-80:s anda (emulator/,
karna/hr80.c) med en mus på kortet och händelser tillbaka till ABC80
på V24. Valen (mina, 2026-10-28): tolken i C först, och en enkel
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
   **Klart 2026-10-28:** `ESC O n r k h b titel CR`, `ESC V n` (välj
   och lyft) och `ESC S n` som i FONSTER.BAS, och `ESC T n` som väljer
   utan att lyfta (n = 8 är skrivbordet, ytan utanför fönstren). Ramen
   tar en textruta på varje sida: titelrad med stängningsruta, fylld i
   det översta fönstret. Allt efter V eller T ritas i det valda fönstret
   och kapas vid ramen och vid fönstren ovanför; fyllningen stannar vid
   kanterna, texten bryts och rullar inom fönstret. Inget sparas under:
   det som blir synligt fylls med bakgrunden, och `omrita` får en bit
   per fönster som tappade innehåll (till händelserna i steg 4).
   Fönstertabellen räknas till 320 byte, så makrona får 1 728 byte i
   stället för 2 KB (häftets kapitel om HR-80 säger 2 KB; rättas i
   steg 8). De fyra gamla provbilderna är oförändrade; nya prov e
   (text, lyft, stäng) och f (grafik och fyllning kapade).
3. **Pekaren och musen.** `hr80_mus(h, dx, dy, knapp)` i tolken.
   Pekaren ritas ovanpå bilden i `hr80_rita`, inte i bildminnet, så att
   den inte behöver spara något under sig (på ett riktigt kort en
   överlagring i bildutmatningen). Klick i ett fönster lyfter det,
   dragning i titelraden flyttar det (med ramen som kontur medan det
   dras), klick i stängningsrutan stänger. Varje sådan sak ger en
   händelse i kön.
   **Klart 2026-10-28:** `hr80_mus(h, dx, dy, knapp)` med rörelsen i
   bildens punkter (512 x 256) och `hr80_handelse(h, ut)` som tar den
   äldsta händelsen ur kön. Pekaren är en pil ovanpå bilden, och den
   dragna ramen är streckad ovanpå; inget av dem i bildminnet. Ett tryck
   i stängningsrutan stänger (som ESC S), i titelraden lyfter och börjar
   en dragning, i fönstret lyfter och ger ett klick, på skrivbordet bara
   klicket. Fönstret flyttas där knappen släpps, i hela textrutor; det
   ligger överst och syns helt, så innehållet flyttas med i bildminnet.
   Det valda fönstret byts inte. Händelserna är fyra byte: `K n x y`
   (klick, grafikens x och y i fönstret, n = 8 skrivbordet), `L n`
   (lyft), `F n r k` (flyttat till rad r, kolumn k) och `S n`
   (stängt); kön rymmer 16. Med kön räknas fönstertabellen till 384
   byte, och makrona får 1 664. `omrita` kan ha kvar bitar för stängda
   fönster; steg 4 frågar bara efter de öppna. `hr80 -u dx,dy,k` ger
   musen och `-e` skriver kön, och provet g jämför både bilden och
   händelserna.
4. **Händelserna tillbaka på V24.** Ett kommando ”finns något?” som
   kortet svarar på med ett kort, fast format (händelsen, fönstret,
   x, y; eller ”inget”). I emulatorn går svaret till maskinens RxD med
   `v24_lagg` och `v24_starta`, i 9600 baud. `hr80in.asm` på $FF80
   efter sändrutinen läser svaret; `A=CALL(...)` ger det till BASIC.
   Byte räknade och prövade mot en riktig bitström i emulatorn.
   **Klart 2026-10-28:** `ESC ?` (63, oanvänd i annonsen). Svaret är
   alltid fyra byte: den äldsta händelsen i kön, annars `R n 0 0` om
   fönster n (8 skrivbordet) har en bit i `omrita` (biten tas bort;
   bitarna för stängda fönster kastas), annars fyra nollor. Kortet
   sänder bara efter frågan, en millisekund efter, i 9600 baud på RxD
   (hr80v24.c med maskinen, `v24_ny` tömmer listan). `hr80in.asm`
   (emulator/exempel/program/) är 64 byte på $FFBB: sänder ESC ? med
   SKICKA, letar upp startbiten med avbrotten av och läser varje bit
   mitt i, 310 T-cykler isär; svaret hamnar på $FFFC-$FFFF (65532-65535)
   och läses med PEEK. 59 + 64 + 4 av de 128 byte. NMI-klockan (omkring
   110 T) flyttar bitarna men de läses inom sin bit. Får rutinen inget
   svar på en sekund slutar den med 0 först. Prövat i emulatorn: 1 000
   svar med alla bytevärden (en provkopia av kortet som svarar med en
   räknare) utan fel, och mot det riktiga kortet med två fönster där det
   översta stängs (`R 0`, `R 8`, sedan 0). `hr80 -e` skriver svaren
   (`svar a b c d`), och provet h frågar tolv gånger efter musen i g.
5. **Exemplet.** `FONSTER2.BAS` (namnet bestäms då): några fönster på
   kortet, ett med HR80-bilden, ett med text och ett som räknar;
   BASIC-slingan frågar efter händelser och ritar om det som behövs.
   **Klart 2026-10-28:** `emulator/exempel/program/text/FONSTER2.BAS`,
   på exempeldisketten och som band. Tre fönster: BILD (HR80-bilden
   med solen, kullarna och två hus från makrot), TEXT och RÄKNARE (varv
   i slingan). Slingan anropar `Z=CALL(65467)` och läser svaret med
   PEEK; inget svar räknar upp RÄKNARE, `R n` ritar fönster n igen
   (8: texten på skrivbordet), ett klick i BILD lägger en stjärna där
   (de åtta senaste sparas och ritas om), ett klick på skrivbordet
   öppnar de stängda fönstren, och den senaste händelsen står i
   RÄKNARE och på ABC80:s skärm. Båda rutinerna som DATA, 123 byte på
   65408-65530; gor.sh prövar dem mot källorna. Det första ur steg 6
   gjordes här för provens skull: `abc80 -Hm fil` läser musen, en rad
   `ms dx dy knapp` per rörelse, ms räknat från det att tangenterna
   (`-k`, `-f`) skrivits; `fonster2.mus` klickar, drar RÄKNARE, stänger
   TEXT och öppnar det igen, och gor.sh jämför händelserna.
6. **Värdarna.** `abc80 -H` med musens rörelser ur en fil (för proven,
   gjort i steg 5: `-Hm`),
   SDL-fönstret HR-80 med datorns mus, webbsidans blad med Grafikkortet
   med webbläsarens mus. Webbsidan prövas i Chromium och WebKit med
   Playwright.
   **Klart 2026-10-28:** i `abc80f -H` är datorns mus i HR-80-fönstret
   kortets mus (läget räknas om till bildens punkter, vänsterknappen,
   datorns pekare göms över fönstret). På webbsidan är musen på kortets
   bild kortets mus, med pointer capture så att en dragning fortsätter
   utanför bilden; skärmen behåller fokus, så tangenterna går till
   ABC80. `hr80_mus` i webb.c tar läget, inte rörelsen. Prövat med
   Playwright i Chromium och WebKit: FONSTER2 med samma klick och
   dragning som `fonster2.mus` ger samma händelser som i gor.sh.
   Hjälpen under Grafikkortet nämner fönstren och musen.
7. **Storleken.** Räkna vad fönstren och musen tar i hr80.c, och om
   tolken med dem hade fått plats i 4 KB EPROM och 2 KB arbetsminne på
   en processor av den tiden. Skrivs om till Z80 bara om det visar sig
   intressant (jag 2026-10-28: C först).
   **Klart 2026-10-28:** [STORLEK.md](STORLEK.md) och `storlek.sh`
   (clang för AVR, MSP430 och Cortex-M0, SDCC för Z80). Fönstren och
   musen gör tolken 2,2-2,5 gånger större: utan värdens bild 3,8-4,4 KB
   före och 8,4-9,7 KB efter i Thumb och MSP430 (7,9 och 19,7 KB med
   SDCC). Tolken före fönstren ligger nära annonsens 4 KB, men med
   fönstren behövs 16 KB EPROM. I arbetsminnet saknades fyllningens
   stack, makrotabellen och processorns stack i räkningen; på ett kort
   hade makrona fått omkring 670 byte, inte 1 664. C-tolken är
   oförändrad.
8. **LASMIG.md och README.md**, raden i `utanfor/README.md`, och
   eventuellt ett kapitel i häftet (bok/extra/PLAN.md).
   **Klart 2026-10-28 utom kapitlet:** [LASMIG.md](LASMIG.md) med
   filerna som ändrats i emulatorn; README.md har kortets koder, musen,
   händelserna, FONSTER2 och storleken; raden i `utanfor/README.md`.
   Häftets kapitel om HR-80 (`bok/extra/kapitel/hr80.tex` och figuren
   `hr80vag.tex`) säger nu att makrona delar de 2 kilobyte med fönstren
   (1 664 byte), att 4 212 byte var tolken före fönstren, och att
   fönsterförsöket är gjort och hade behövt 16 kilobyte EPROM. C-tolken
   är oförändrad: emulatorn visar ett kort med 16 KB EPROM och mer
   arbetsminne. Om försöket blir ett eget kapitel i häftet är inte
   bestämt.

Varje steg committas för sig.

## Öppna frågor

- Tangenten och antalet: besvarat 2026-10-28, Tab och högst åtta.
- Händelsernas format och vilka koder kommandona får (bestäms i steg 2
  och 4, efter att annonsens kodlista har gåtts igenom igen).
