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
