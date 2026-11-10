# Blanketten

Utanför boken. En självdeklaration för 1982 på ABC80 i BASIC, efter ett
program som jag skrev omkring 1981–82: blanketten på skärmen så likt
papperet som det gick, med sidorna "swappade" till och från skivan.
Programmet finns inte kvar, och kalkyldelen blev aldrig färdig. Det här
är en ny version. Förebilden för formulären var kapitel 5 i Isaksson och
Kärrsgård, *Avancerad programmering på ABC80* (1980), med fälten mellan
`<` och `>`.

Blanketten är exemplet i kapitlet *Deklarationen på ABC80* i häftet
*ABC80 inifrån. Utanför boken* (`bok/extra/kapitel/deklaration.tex`):
vad ABC80 redan klarade på kontoret, var maskinen tog slut (40 tecken,
minnet, sex siffror), programmet som vara med en ny årgång varje år, och
vägen fram till kontorsprogrammen och ABC 802.

**Blanketten är efterbildad och inte RSV:s, och skattereglerna är
påhittade och förenklade, inte 1982 års.** Det står också på sidorna.

## Programmen

De ligger bland exemplen, i `emulator/exempel/program/text/`:

| Program      | Gör                                                       |
|--------------|-----------------------------------------------------------|
| `DSKAPA.BAS` | gör den tomma blanketten, DR1:DEKL.DAT (en gång)          |
| `DEKL.BAS`   | visar sidorna och låter en fylla i dem                    |
| `DKALK.BAS`  | räknar rutorna och skriver in summorna, och går tillbaka  |

Programmen ligger på programskivan i DR0 och blanketten på en egen
dataskiva i DR1. `gor.sh` lägger dem på exempeldisketten (men inte på
band), och webbsidan sätter en tom ABC-DOS-skiva, `data.dsk`, i DR1 när
en diskett sitter i DR0. På webbsidan: Exempeldisketten, `RUN DR0:DSKAPA`
en gång och sedan `RUN DR0:DEKL`. Katalogen visar båda skivorna, och
Spara disketten laddar ner `data.dsk` också, om något har sparats på den.

## Dataskivan och direktåtkomsten

DEKL.DAT är 17 block om 253 bytes. Den skrivs först till full längd med
`PRINT #` (en rad om 252 tecken blir ett block), och sedan läses och
skrivs blocken direkt med ABC-DOS:s rutiner, som i *ABC80 inifrån*
(band 2, Direktåtkomst):

- `CALL(28666,1)` låter `Q0$` peka på buffert 1, 253 tecken;
- `CALL(28668,B)` läser block B till bufferten;
- `CALL(28670,B)` skriver bufferten som block B.

En sida är skärmraderna 0–22, och de ligger i fyra block om sex rader
(240 tecken). Sida S är block 4S−4 till 4S−1, och block 16 har numret på
sidan som visades sist. DEKL läser ett block i taget direkt till
skärmen och sparar sidan genom att läsa skärmminnet med `PEEK`. Det är
alltså skärmbilden som sparas, och fälten hittas igen genom att leta
efter `<` på raderna.

En rad som börjar med ett rutnummer, i kolumn 0–1, är en ruta med ett
belopp. Med `=` före `<` räknas rutan, och DEKL hoppar över den.

## Tangenterna i DEKL

| Tangent            | Gör                                              |
|--------------------|--------------------------------------------------|
| RETURN             | nästa fält; ett belopp ställs till höger         |
| vänster- och högerpil | flyttar markören i fältet                     |
| CTRL-N, CTRL-P     | nästa och förra sidan (sidan sparas om den är ändrad) |
| CTRL-K             | sparar, kör DKALK och kommer tillbaka            |
| CTRL-X             | sparar och slutar                                |

I rutorna går det bara att skriva siffror, blanktecken och minus.

## Räkningen i DKALK

Eftersom beloppen står på fasta platser i skärmbilderna kan räkningen
ligga i ett eget program. DKALK läser de 16 blocken, tar beloppet efter
`<` på varje rad med ett rutnummer, och räknar med reglerna i DATA från
rad 900:

    "6 =1 +2 +3 -4 -5"           ruta 6 är 1 + 2 + 3 - 4 - 5
    "15 =14 -12 -13 P"           P: blir det minus blir det noll
    "26 =24 -#40000 P %25"       #C är ett tal, %C gånger C procent

Stegen görs från vänster till höger. Summorna skrivs sedan in i de block
där rutorna står, med blanktecken mellan grupperna om tre siffror, och
DKALK går tillbaka till DEKL med `CHAIN`.

ABC80:s flyttal har bara sex siffror (1 234 567 blir 1 234 560), och
avrundningen kunde ge en krona för mycket i varje steg. Därför räknar
DKALK med talen som ASCII-siffror: tio tecken i varje tal, minus som
tiokomplement, addition siffra för siffra från höger med en
minnessiffra, och procenten som multiplikation med ett heltal och
division med 100 (två siffror bort) efter att 50 har lagts till.

## Att köra den i emulatorn

I `emulator/`, med en tom ABC-DOS-skiva i varje enhet (som `tom_skiva`
i `gor.sh` gör):

    ./abc80 -r roms/abc80new.rom -l roms/abcdos80.rom@6000 \
      -D p.dsk@45 -D d.dsk@45.1 -DS

Spara de tre programmen med `SAVE DR0:DSKAPA` och så vidare, kör
`RUN DR0:DSKAPA` en gång, och sedan `RUN DR0:DEKL`. Med exemplet som
provades (lön 142 000, resor 6 200, övriga avdrag 1 500, ränta 8 400,
skuldräntor 12 300, pensionsförsäkring 1 800, bank 25 000, fastighet
450 000, skulder 180 000 och preliminärskatt 39 000) blir kvarskatten
19 380 kronor.
