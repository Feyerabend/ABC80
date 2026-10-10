# Kretsarna

`forth.bin` är Forth-tolken från exempel 7 i band 2 av *ABC80 inifrån*,
assemblerad för $4000, där en krets som Smartaid satt (3167 bytes, ryms i
ett 2732-EPROM). Den byggs i bokens förråd (`bok/band2/exempel/e7ip.bin`,
med `e7fc.py` och `test.sh`) och kopieras hit.

Kretsen kan inte skrivas i, så nya ord läggs i RAM från $C000. BOFA flyttas
därför upp först, och tolken startas med `CALL`:

    POKE 65052,0,208
    NEW
    Z=CALL(16384,3)

    : KV DUP + ;  OK
    7 KV . 14 OK
    BYE

`BYE` går tillbaka till BASIC, `Z=CALL(16384,4)` fortsätter med de egna
orden kvar och `CALL(16384,n)`, n = 0–2, kör de färdiga orden MAIN, TIO och
FIND. `WORDS` listar orden. `@` skrivs `É` på ABC80.

I terminalen sätts kretsen i med `-l`:

    ./abc80 -l exempel/krets/forth.bin@4000

I webbläsaren: välj *Forth-kretsen* (eller öppna `abc80.html?krets=forth`).

## Forth med CMOS

`forth_cmos.bin` är samma tolk med en tillagd källa
(`bok/band2/exempel/e7c.fs`, inte med i boken; bilden är `e7c.bin`): de
egna orden läggs i CMOS-minnet på $5000–$57FF, det batteriförsedda RAM
som Super Smartaid hade, och finns kvar när datorn slås av. Radbufferten
ligger där också, så ingen `POKE` behövs:

    ./abc80 -l exempel/krets/forth_cmos.bin@4000 -M cmos.bin

    Z=CALL(16384,3)
    NY ORDLISTA
    : DUBBEL DUP + ;  OK
    BYE

Nästa gång, med samma `cmos.bin`, finns `DUBBEL` kvar. Efter varje rad
utan fel skrivs ett huvud först i minnet: ett märke (kretsens sista
huvud), DP, LAST och en kontrollsumma. Stämmer det inte, t.ex. med en
annan version av kretsen, börjar tolken med en tom ordlista. `EMPTY`
tömmer den, och en rad som inte får plats (2 KB, utom huvudet och
radbufferten) tas bort med `CMOS FULLT`.

I webbläsaren: välj *Forth med CMOS* (eller `abc80.html?krets=cmos`);
minnet sparas i webbläsaren och finns kvar när sidan laddas om.

## Bokens exempel 1–6

`e1.bin`–`e6.bin` (exempel 4 som två banker, `e4a.bin` och `e4b.bin`)
är programmen i band 2, kapitlet Egna exempel, assemblerade för sina
adresser. De byggs i bokens förråd (`bok/band2/exempel/`, med `test.sh`)
och kopieras hit; källtexterna finns i kodförrådets `inifran/exempel/`.

    ./abc80 -l exempel/krets/e1.bin@4000          # också e2, e3 och e6
    ./abc80 -b exempel/krets/e4a.bin,exempel/krets/e4b.bin@4000
    ./abc80 -l exempel/krets/e5.bin@7000

I webbläsaren: välj *Exempel 1*–*6* i listan (eller
`abc80.html?krets=e1` till `?krets=e6`). `?s=` sänder en text på V24 till
exempel 6 när tangenterna är skrivna, som `-s`.
