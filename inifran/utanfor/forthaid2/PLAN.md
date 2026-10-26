# Forthaid II (utanför boken), planen

Föreslaget 2026-10-26 efter en fråga från författaren. Alla åtta steg
klara 2026-10-26 (resultatet i LASMIG.md).
Forthaid (`../forthaid/`) är klart och ändras inte av det här; Forthaid
II är ett nytt försök i en egen katalog, så att det första programmet
förblir kort nog att läsa.

## Vad det är
En Forthaid som fungerar med TKN80 i 80-teckensläget och som får mer
plats genom bankning, löst på Forths sätt och inte som i Smartaid.
Samma kommandon och samma öppenhet som Forthaid (egna ord, skärmeditor,
stegning), och kommandon och ord skrivs med versaler eller gemener som
i Forthaid sedan 2026-10-26.

Frågorna försöket svarar på:
1. Vad kostar det att flytta Forthaid ur TKN80:s väg?
2. Hur ser bankning ut i en trådad tolk, där varje ord redan är en
   adress i en lista, jämfört med Smartaids stubbar i assembler? Vad
   kostar ett anrop över bankgränsen i tid?

Ingen kod lånas från Smartaid (OWOCO:s rättigheter). Band 2:s kapitel
om Smartaid används bara för att veta hur Super Smartaid och Magnum
väljer bank, inte hur de är skrivna.

## Varför Forthaid kraschar med TKN80 (kontrollerat 2026-10-26)
Inget fel i koden utan adresser som krockar:
- TKN80 har bildminnet på $5800–$5FFF, och $5C00–$5FFF är samma minne
  som $7C00–$7FFF. Forthaid ligger på $4000–$5FFF (8 101 byte), så
  kretsens sista 2 KB och TKN80:s bildminne ligger på samma adresser.
- Vektortabellen KTAB ligger på $5F34 (I=$5F), mitt i TKN80:s minne.
- Tolkens variabler ligger i bildminnets osynliga byte ($7C78, $7C7A,
  $7F7E med flera). I 80-teckensläget är $7C78 mitt i rad 12 (figuren
  tkn80rader i band 2); bara $7xF8–$7xFF är osynliga där.
- Skärmeditorn räknar med 40 kolumner.

## Uppläggningen
- **Fast kärna på $4000–$4FFF (4 KB):** den inre tolken, maskinkodsorden,
  (LOOKUP) utan skillnad på versaler och gemener, den yttre tolken,
  tangentbordsrutinen och avbrottsrutinen med KTAB på en sida under
  $5000. Det som alltid måste finnas.
- **Bankat fönster på $5000–$57FF (2 KB):** kommandona (FIND, LIST, REN,
  editorn, stegningen) och ordlistans huvuden för dem, en bank i taget.
  Fönstret slutar under $5800, så TKN80:s minne lämnas i fred.
- **Variablerna i kortets eget RAM** i stället för bildminnet, i den fasta
  delen eller i ett litet RAM på kortet (Smartaid hade CMOS-RAM på
  $5000–$57FF; här tas det ur fönstret eller läggs ovanför BASIC). Det
  avgörs i steg 1.
- **Bankbytet i tolken:** ett ord i en annan bank anropas genom ett
  fjärrord i den fasta delen: kodfältet byter bank, lägger den gamla
  banken på returstacken ihop med returadressen och hoppar in. När ordet
  är klart (EXIT) tar ett eget ord tillbaka den gamla banken. Ett anrop
  inom banken är som i dag. Korskompilatorn (en kopia av e7fc.py, som i
  Forthaid) vet vilken bank varje ord hamnar i och lägger in fjärrorden
  själv.
- **Kompilatorn för sig:** `:` och de ord som bara behövs när man
  definierar nya ord kan ligga i en egen bank; den som bara kör kommandon
  betalar inte för dem i den fasta delen.
- **Färdigkompilerade ord från skivan:** trådad kod med relativa eller
  omräknade adresser, laddad till RAM ovanför BASIC, som en bank man kan
  fylla själv. Prövas sist och bara om det blir plats.
- **Ett bankkort i emulatorn:** ett påhittat men enkelt kort, bank vald
  med en skrivning till en adress i kretsens område (som Super Smartaids
  $4040/$4041), i `emulator/` och på webbsidan (`?krets=forthaid2`).
  Hur kortet fungerar står i LASMIG.md, så att ingen tror att det har
  funnits.

## Stegen
1. **Minneskartan:** var variablerna, stackarna och KTAB hamnar när
   $5800–$7FFF inte får röras utom $7xF8–$7xFF. Prövas först utan
   bankning: Forthaid som den är, nerkortad till under $5800 (vilka
   kommandon får plats?). Det är också svaret på fråga 1.
   *Klart 2026-10-26:* VAR, CHANGE, REN och OLD bort, 6 128 byte, KTAB
   på $5734, variablerna på $7CF8–$7EFF och $FB50–$FB6F, nya ord från
   $FB70; 114 prov ok med och utan TKN80 (LASMIG.md).
2. **Bankkortet i emulatorn:** kärnan (`emulator/`), ett prov som byter
   bank och läser tillbaka, sedan webbsidan.
   *Klart 2026-10-26:* emulatorns `-b` används som den är (skrivning
   till `$5040`+n), utan RAM på kortet; `kort/prov.sh` i båda ROM:arna
   och med TKN80. Webbsidan får den fasta delen bredvid bankerna i
   steg 4.
3. **Fjärrorden i korskompilatorn:** bank per källfil, fjärrord
   automatiskt, ett prov som anropar fram och tillbaka mellan banker och
   ett som mäter tiden för ett anrop över gränsen.
   *Klart 2026-10-26:* `bank/fc2.py` (bokens `e7fc.py` med ett nytt
   bygge), `bank/fb.inc` (DOCOL för fjärrord och tillbakavägen),
   `bank/prov.sh` med exempel 7:s tolk i den fasta delen och två banker;
   ok i 9913 och 11273, med och utan TKN80. Banken har sitt nummer på
   `$5000` och en tabell över sina ord, så den fasta delen behöver inga
   adresser ur bankerna. Ett anrop över gränsen tar 754 T mot 335.
4. **Kommandona i bankerna:** Forthaids kommandon fördelade på bankerna,
   alla Forthaids prov körda med TKN80 i båda lägena och utan TKN80.
   *Klart 2026-10-26:* `banker.txt` lägger kommandona i tre banker
   (126, 903 och 701 byte lediga), den fasta delen på
   `$4000`–`$4FEF` med KTAB på `$4F34`; ord i maskinkod kan ligga i en
   bank, och `fc2.py -h` ger huvud bara åt ord som behöver det (22
   fjärrord); 150 prov ok med och utan TKN80; webbsidan
   `?krets=forthaid2` (LASMIG.md).
5. **Editorn med 80 kolumner:** EDLIN och LIST räknar med radlängden från
   TKN80 (40 eller 80).
   *Klart 2026-10-26:* `gor.sh` ändrar kopiorna: EDW i `fak.inc` och
   COLS i `fa.fs` läser radlängden ur radtabellen (`$0375` = `$58` i
   80-teckensläget); EDLIN, CTRL-S, CTRL-D, CTRL-P och LISTLINE räknar
   med den. Bara med bankerna (bank 0 77 byte lediga). Nya prov `ed4`,
   `ed80` och `list80`, 160 ok (LASMIG.md).
6. **Mätningen:** som i Forthaid (`matning.sh`), med tiden för
   bankbytena; jämförelsen med Forthaid och Smartaid i LASMIG.md.
   *Klart 2026-10-26:* `matning.sh`; Forthaid II lika snabb som
   Forthaid (med TKN80 0–20 ms mer), FIND, VAR, REN och CHANGE gör ett
   anrop över gränsen oavsett programmets längd (VAR med namn två till
   per rad med träff); 8 436 byte mot 8 101, med 1,8 KB lediga
   (LASMIG.md).
7. **Kompilatorn i egen bank och ord från skivan**, om det finns tid och
   plats.
   *Kompilatorn klar 2026-10-26:* `e7i.fs`:s kompilator (`:`, `;`,
   kontrollorden, VARIABLE, CONSTANT med flera) i bank 3 med två rader i
   `banker.txt`; den fasta delen har 443 byte lediga i stället för 107.
   Nytt prov `komp`, 166 ok (LASMIG.md).
   *Orden från skivan klara 2026-10-26:* SAVEW och LOADW i bank 4
   (`bank/fil.fs`, FCALL i `bank/fil.inc`) med ROM:ens filrutiner;
   filen har adresserna räknade från början och en bitkarta, och LOADW
   lägger orden under STKTOP, i samma minne varje gång. Kräver ett
   skivminne. Nytt prov `prov/fil.sh` med DOS:en, ok i båda ROM:arna och
   med TKN80; webbsidan med systemskivan (LASMIG.md).
8. **README.md, MANUAL.md och webbsidan**, och kopian till publika
   förrådet (författaren).
   *Klart 2026-10-26:* `MANUAL.md` (på svenska, med SAVEW, LOADW, TKN80
   och minnet), `README.md` (på engelska, som Forthaids) och webbsidans
   hjälptext (fem banker, 80 kolumner, SAVEW och LOADW, manualen);
   manualens exempel med SAVEW och LOADW prövat i emulatorn och på
   webbsidan i Chromium och WebKit, med och utan TKN80.

## Inte med
- Smartaids kod, stubbar eller bankadresser utöver det som behövs för
  att förklara skillnaden.
- Ändringar i bok/ eller i `../forthaid/`.
- Ett riktigt kort i hårdvara.
