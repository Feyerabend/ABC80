# Exempel 1–6: program i en krets

Exempel 1–6 i *ABC80 inifrån*, band 2 (kapitlet Exempel). Programmen är
skrivna för boken, inte tagna ur någon krets, och visar hur kretsar som
Smartaid och Superbasic tar sig in i ABC80: genom avbrottsvektorerna, i
tangentrutinen och i BASIC:ens satser. Alla går i båda versionerna av
ABC80:s ROM.

| Fil | Adress | Från BASIC | Vad programmet gör |
|---|---|---|---|
| `e1.asm` | $4000 | `CALL(16384)` in, `CALL(16387)` ur | räknar tangenttryckningarna i skärmens hörn och går vidare till ROM:ens tangentrutin |
| `e2.asm` | $4000 | som e1 | gör om gemener till versaler i direktläge; INPUT och GET får tecknen som de skrivs |
| `e3.asm` | $4000 | som e1 | CTRL-L i direktläge blir LIST och RETURN |
| `e4a.asm`, `e4b.asm` | $4000, två banker | `CALL(16384)` | anropar från bank 0 till bank 1 och tillbaka; skrivning till $4040/$4041 väljer bank |
| `e5.asm` | $7000 | `CMD stränguttryck` | en egen sats på IEC-kretsens plats: en statusrad överst på skärmen |
| `e6.asm` | $4000 | `CALL(16384)` in, `CALL(16387)` ur, `A=CALL(16390)` | tar emot på V24 i program (1200 baud, 8N1), som texttelefonen; -1 om inget har kommit |

Varje `.asm` beskriver sig själv i sina första rader. `.bin` är den
assemblerade bilden från filens `ORG`, klar att lägga i en EPROM eller
läsa in i en emulator.

## Kretsen

Exempel 1–3 och 6 behöver en EPROM på 4 KB på $4000–$4FFF, där Smartaid 3
och Superbasic sitter. Kortet avkodar de fyra översta adressbitarna och
lägger ut sina bytes när datorn ger XMEMFL på ABC-bussen. Skrivningar
struntar det i, så programmen sparar det de behöver i RAM eller bland
bildminnets osynliga bytes.

Exempel 4 behöver en EPROM på 8 KB, där den översta adressbiten kommer
från en vippa på kortet. En skrivning till $4040 väljer bank 0 och en
skrivning till $4041 bank 1, som i Super Smartaid. Det är adressen som
väljer, inte värdet.

Exempel 5 är en EPROM på 1 KB (2708) i platsen för IEC-kretsen på Luxors
minneskort, $7000–$73FF.

## Bygga och köra

Med verktygen i [`../verktyg/`](../verktyg/) (`make` där först):

    ../verktyg/z80asm -l e1.lst e1.asm      # -> e1.bin

och i emulatorn, med kretsen på sin plats:

    ../verktyg/abc80 -l e1.bin@4000         # också e2, e3 och e6
    ../verktyg/abc80 -b e4a.bin,e4b.bin@4000
    ../verktyg/abc80 -l e5.bin@7000

Skriv `Z=CALL(16384)` för att koppla in e1–e4 och e6, och `CMD "TEXT"`
för e5. Utan terminal skriver emulatorn skärmen efteråt:

    $ ../verktyg/abc80 -l e2.bin@4000 -k 'Z=CALL(16384)\rprint 6*7\r' -t 300
    ABC80
    Z=CALL(16384)

    ABC80
    PRINT 6*7
     42
    ...

Hur emulatorn tar emot V24 för e6 (`-s`) står i dess manual,
[`../verktyg/abc80.md`](../verktyg/abc80.md).

## I webbläsaren

Emulatorn finns också som en webbsida,
[`../emulator/webb/abc80.html`](../emulator/webb/abc80.html), en enda fil
som går att öppna direkt. Länkarna nedan öppnar den på GitHub Pages med
kretsen på sin plats och skriver kommandona åt en:

| Länk | Vad som händer |
|---|---|
| [Exempel 1](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e1&k=Z%3DCALL%2816384%29%5Cr) | räknaren står i skärmens övre högra hörn; skriv något och se den räkna; `Z=CALL(16387)` kopplar ur |
| [Exempel 2](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e2&k=Z%3DCALL%2816384%29%5Cr%5Cw50print%20%22hej%22%5Cr) | `print "hej"` blir versaler i direktläge; i `INPUT` får gemenerna vara kvar |
| [Exempel 3](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e3&k=Z%3DCALL%2816384%29%5Cr%5Cw50%2010%20PRINT%20%22HEJ%22%5Cr20%20GOTO%2010%5Cr%5Cx0c) | ett program skrivs in, och CTRL-L listar det |
| [Exempel 4](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e4&k=Z%3DCALL%2816384%29%5Cr) | 0, 1 och 0 överst till höger: bank 0, ett anrop till bank 1 och tillbaka i bank 0 |
| [Exempel 5](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e5&k=10%20FOR%20I%3D1%20TO%203%5Cr20%20CMD%20%22VARV%20%22%2BNUM%24%28I%29%5Cr30%20NEXT%20I%5Cr40%20CMD%20%22SLUT%22%5CrRUN%5Cr) | `CMD` skriver på en statusrad överst medan programmet går |
| [Exempel 6](https://feyerabend.github.io/ABC80/inifran/emulator/webb/abc80.html?krets=e6&k=10%20Z%3DCALL%2816384%29%5Cr20%20A%3DCALL%2816390%29%5Cr30%20IF%20A%3C0%20THEN%2020%5Cr40%20IF%20A%3D4%20THEN%2070%5Cr50%20PRINT%20CHR%24%28A%29%3B%5Cr60%20GOTO%2020%5Cr70%20Z%3DCALL%2816387%29%5Cr80%20PRINT%5Cr90%20PRINT%20%22KLART%22%5CrRUN%5Cr&s=HEJ%21%20DETTA%20KOM%20P%C3%85%20V24%20I%201200%20BAUD.%5Cx04) | programmet tar emot en text på V24 (länken sänder den med `?s=`) och skriver ut den |

Kretsen väljs med `?krets=e1` till `?krets=e6`, eller i listan
bredvid *Starta om*. `?k=` ger tangenterna med samma skrivsätt som
`-k` (`\r` för RETURN, `\w50` för en paus i ms, `\x0c` för
CTRL-L) och `?s=` en text till V24 som `-s`. Med `&rom=gammal` sist
i länken körs exemplet med den gamla ROM:en.
