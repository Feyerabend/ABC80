# Forthaid II, manualen

Forthaid II är Forthaid (`../forthaid/`) på ett kort med banker, så att
den fungerar också med TKN80, MyAB:s kort för 80 tecken per rad. Den har
samma kommandon, samma skärmeditor och samma stegning som Forthaid, och
den är skriven i Forth på tolken från exempel 7 i band 2 av *ABC80
inifrån*, så att det går att skriva egna kommandon. Det som är nytt är
att editorn och LIST räknar med 80 kolumner när TKN80 står i
80-teckensläget, och att de egna orden kan sparas på skivan med SAVEW
och laddas med LOADW. Forthaid II är inte med i boken; hur den är byggd
och mätt står i `LASMIG.md`.

**Kortet har inte funnits.** Det är påhittat för försöket och finns bara
i emulatorn och på webbsidan. Forthaid II är inte prövad på en riktig
ABC80.

Den går i båda ROM-versionerna, 9913 och 11273, med och utan TKN80.


## Att få kretsen i maskinen

Forthaid II är två delar:

- **den fasta delen** på `$4000`–`$4FEF` (16384–20463), en vanlig krets
  med tolken, inkopplingen och stegningen;
- **fem banker** à 2 KB i ett fönster på `$5000`–`$57FF`, en i taget:
  bank 0 LIST, PEEK, FIND, SYS, START, ED och DEL, bank 1 VAR och CHANGE,
  bank 2 REN, AUTO, OLD och HELP, bank 3 kompilatorn (`:`, `;`, IF med
  flera) och bank 4 SAVEW och LOADW.

Fönstret slutar under `$5800`, där TKN80 har sitt bildminne. Kretsen
byter bank själv; den som använder den märker ingenting av det.

Bygg den med `./gor.sh`, som skriver `forthaid2.bin` och
`forthaid2_b0.bin` … `forthaid2_b4.bin`.

- **Emulatorn i terminalen**, i den här katalogen (på en rad; bankerna
  med komma emellan, i ordning):

  ```
  ../../emulator/abc80 -l forthaid2.bin@4000 -b forthaid2_b0.bin,forthaid2_b1.bin,forthaid2_b2.bin,forthaid2_b3.bin,forthaid2_b4.bin@5000
  ```

  Med TKN80 läggs `-tkn80 ../../emulator/roms/tkn80-iii.rom` till, och
  med ett skivminne (för SAVEW och LOADW) DOS:en och en diskett, till
  exempel `-l ../../emulator/roms/abcdos80.rom@6000 -D diskett.dsk@45`
  (som `prov/fil.sh`).
- **Webbsidan** (`emulator/webb/abc80.html`): välj *Forthaid II* under
  *Program och kretsar*, eller öppna sidan med `?krets=forthaid2`. Kryssa
  i *TKN80* (`&tkn80=pa`) för 80 tecken, och välj *Systemskivan*
  (`&skiva=system`) för ett skivminne.
- **SDL-fönstret** (`abc80f`) har inget bankkort och kan inte köra
  Forthaid II.

### TKN80

TKN80 startar i 80-teckensläget. `PRINT INP(3);CHR$(12)` byter till 40
tecken och tömmer skärmen, `PRINT INP(4);CHR$(12)` tillbaka till 80.
Forthaid II läser radlängden varje gång ett kommando körs, så läget kan
bytas mellan två kommandon. Utan TKN80 är det 40 tecken, som i Forthaid.


## Att starta

Det finns två sätt att använda kretsen, som i Forthaid.

```
    Z=CALL(16384,2)     kopplar in kretsen: kommandona skrivs direkt i BASIC
    Z=CALL(16384)       Forth-tolken (OK), med kommandona som Forth-ord
    Z=CALL(16384,1)     Forth-tolken, med de egna ord som redan finns
```

### Inkopplad

Efter `Z=CALL(16384,2)` tar kretsen över tangentbordet varje gång BASIC
står i direktläge och väntar på en rad. Raden skrivs med kretsens
skärmeditor (se nedan). Börjar den med ett av kretsens kommandon körs
kommandot, och sedan kommer `ABC80` som efter BASIC:s egna kommandon.
Alla andra rader, programrader och direkta satser, går till BASIC som
förut. Hela raden kan vara 120 tecken, men en rad med ett kommando får
vara högst 79.

```
    ABC80
    Z=CALL(16384,2)

    ABC80
    VAR
    A B¤ I

    ABC80
```

`EX` kopplar ur kretsen. Då är tangentbordet som förut, och kretsen
lämnar inget efter sig i BASIC:s minne, utom det minne som LOADW har
tagit (nedan).

### Forth-tolken

`Z=CALL(16384)` startar Forth-tolken, som svarar `OK` efter varje rad
som gick bra. Kommandona finns där också, som Forth-ord: skriv `LIST`,
`FIND PRINT` eller `REN 100,10` som i BASIC. Ett kommando läser resten
av raden, så det står sist. Raderna läses med ROM:ens radinmatning
(utan skärmeditorn) och får vara högst 79 tecken.

`BYE` går tillbaka till BASIC. Har ett kommando ändrat programmet går
BYE tillbaka som när ett program är slut, med CLEAR och `ABC80`:
raden med `CALL` kan ha ändrats, och dess variabel kan vara borta.
Annars får `CALL` talet överst på Forth-stacken som värde:

```
    Z=CALL(16384)
    6 7 * BYE
    ABC80
    PRINT Z
     42
```

`Z=CALL(16384)` och `FORTH` glömmer de egna orden, också de som LOADW
har laddat; `Z=CALL(16384,1)` behåller dem. Från inkopplat läge går
`FORTH` till tolken, och `BYE` därifrån tillbaka till BASIC med kretsen
fortfarande inkopplad.


## Skärmeditorn

I inkopplat läge läses raderna med en editor där markören går fritt
över skärmen, som i Smartaid. En rad på skärmen kan läsas in igen med
högerpilen, eventuellt ändrad. Med TKN80 i 80-teckensläget går
markören över alla 80 kolumner.

| Tangent       | Vad                                            |
|---------------|------------------------------------------------|
| högerpilen    | tecknet under markören läses in i raden        |
| vänsterpilen  | det sista tecknet i raden tas bort             |
| CTRL-X        | hela raden tas bort                            |
| CTRL-W        | markören upp                                   |
| CTRL-Z        | markören ned                                   |
| CTRL-A        | markören åt vänster                            |
| CTRL-S        | markören åt höger                              |
| CTRL-Q        | markören till övre vänstra hörnet              |
| CTRL-L        | skärmen rensas                                 |
| CTRL-D        | tecknet under markören tas bort ur skärmraden  |
| CTRL-P        | ett blanktecken sätts in i skärmraden          |
| RETURN        | raden är klar                                  |

Raden är det som har skrivits eller lästs in med högerpilen, inte det
som står på skärmen. Flyttar man markören med CTRL-W, A, S eller Z läses
ingenting in; det gör bara högerpilen. Så här ändras `10 PRINT "AB"` till
`10 PRINT "XY"` på skärmen: flytta markören till radens början, tryck
högerpilen tio gånger (`10 PRINT "`), skriv `XY` och tryck högerpilen
för resten. Ett tecken som skrivs ersätter det som stod på skärmen.

`ED` (nedan) gör det första steget själv.


## Kommandona

I beskrivningarna är *n* och *m* radnummer. Ett radintervall skrivs som
i BASIC:s LIST: `n` (bara den raden), `n-m`, `n-` (till slutet), `-m`
(från början) eller ingenting (allt). Komma går lika bra som bindestreck.
Ett kommando som inte förstår det som står efter det svarar `?`.

Kommandona och Forth-orden kan skrivas med versaler eller gemener:
`list`, `List` och `LIST` är samma kommando, och ett eget ord som
definierats som `Tre` hittas också som `tre` och `TRE`. Det som står
efter kommandot ändras inte, så `find a` söker efter ett litet a.

Listor som fyller skärmen (LIST, FIND, VAR och PEEK) stannar efter 22
skärmrader och väntar på en tangent: mellanslag ger en rad till, RETURN
en ny skärm, och en annan tangent avbryter. CTRL-C avbryter också. En
programrad som är längre än skärmraden räknas som två eller flera, med
40 eller 80 tecken per rad efter läget.

### LIST *intervall*

Listar programmet, eller raderna i intervallet, som BASIC:s LIST men
med paus vid full skärm.

```
    LIST 100-200
    LIST 500-
```

### FIND *text*

Listar de rader där texten står. Raden söks så som den listas, så
nyckelord, variabler, radnummer och text inom citattecken hittas
likadant. Texten är resten av raden, med blanktecken.

```
    FIND GOSUB 1000
    FIND "HEJ
```

### VAR

Listar alla variabler i programmet, i bokstavsordning: först efter
bokstaven, sedan siffran, typen (`%` heltal, `$` sträng) och sist om den
har index, som `A(`.

```
    VAR
    A A( A1 B% C$ I
```

VAR behöver 256 byte ledigt minne efter variablerna och svarar `?` om de
inte finns.

### VAR *namn*

Listar de rader där variabeln står. `A(` är fältet A, `A` den enkla
variabeln.

```
    VAR A1
    VAR C$
    VAR A(
```

### CHANGE *v1*,*v2*

Byter variabeln *v1* mot *v2* i hela programmet. Typen och om den har
index måste vara densamma (`CHANGE A,B$` ger `?`). Parametrarna i
`DEF FN` byts inte.

```
    CHANGE A1,K
    CHANGE C(,E(
```

### REN *a*,*s*,*intervall*

Numrerar om programmet med början på *a* och steget *s*, eller bara
raderna i intervallet. Utan *s* är steget 10, och utan *a* och *s* blir
det 10, 20, 30... Hänvisningarna i GOTO, GOSUB, THEN, ON, RESTORE och de
andra följer med, som i BASIC:s REN. Kommer de nya numren i fel ordning
mot raderna omkring svarar REN `?` och ändrar ingenting. En hänvisning
till en rad som inte finns ger ERR 6, också före ändringen.

```
    REN
    REN 100,5
    REN 1000,10,500-800
```

### DEL *intervall*

Tar bort raderna i intervallet. Utan intervall tas ingenting bort.

```
    DEL 100-150
```

### AUTO *a*,*s*

Skriver radnummer, från *a* med steget *s* (10 och 10 om de inte
anges), och lägger in varje rad som skrivs. En tom rad slutar. Raderna
läses med ROM:ens radinmatning, utan skärmeditorn.

```
    AUTO 500,5
    500 PRINT "HEJ"
    505
```

### ED *n*

Listar rad *n* och ställer markören på dess början. Sedan är skärmen
mallen: högerpilen tar tecknet under markören, ett annat tecken skriver
över det, CTRL-P sätter in och CTRL-D tar bort. RETURN ger BASIC raden
så långt den har lästs; det som står efter markören måste alltså tas med
högerpilen. Ändras radnumret blir raden en ny rad, och den gamla står
kvar. Finns inte rad *n* svarar ED `?`. En rad som tar mer än en
skärmrad går att ändra i båda lägena.

### START *n*

Kör programmet från rad *n*, eller från raden efter om *n* inte finns,
utan att variablerna nollställs, som GOTO i direktläge (som BASIC inte
tillåter). START utan nummer kör från början. Variablerna finns bara
kvar så länge programmet inte har ändrats: BASIC nollställer dem när en
rad läggs in eller tas bort, och när programmet tar slut.

```
    RUN
    STOP LINE 200
    PRINT A
    START 210
```

### OLD

Tar tillbaka ett program efter NEW. I Forth-tolken måste den startas
med `PRINT CALL(16384)`, inte `Z=CALL(16384)`: Z skulle hamna mitt i
programmets första rad, som NEW har lämnat i minnet. Inkopplad går OLD
direkt.

```
    NEW
    PRINT CALL(16384)
    OLD
    BYE
```

### PEEK *adress*

Visar minnet åtta byte per rad, hexadecimalt och som tecken. Adressen
skrivs decimalt eller med `$` hexadecimalt. Utan adress fortsätter PEEK
där den slutade.

```
    PEEK $FE1C
    FE1C 00 C0 1F C0 29 C0 FF FF .É.É)É
    FE24 01 00 FF 00 FB 20 C0 FF .. .ä É
```

PEEK på `$5000`–`$57FF` visar bank 0, där PEEK själv ligger, inte den
bank som ett annat kommando använder.

### SYS

Programmets gränser och det lediga minnet: BOFA (programmets början),
EOFA (dess slut), HEAP (variablernas slut), programmets och
variablernas storlek, det lediga för BASIC, och var nästa egna ord
hamnar och hur mycket plats som finns kvar för dem på `$FB70`.

```
    SYS
    BOFA C000 EOFA C01D HEAP C027
    PROGRAM 30 VAR 9 FRITT 15065
    ORDEN FB70 FRITT 272
```

### SAVEW *namn*

Skriver de egna orden till en fil på skivan, både de som definierats på
`$FB70` och de som LOADW har laddat. Namnet skrivs som i BASIC:s SAVE,
med enhet om det behövs, och får ingen filtyp om man inte anger en.
Utan namn, eller utan egna ord, svarar SAVEW `?`.

### LOADW *namn*

Glömmer de egna orden, som `FORTH`, och läser orden i filen. Sedan kan
de köras och nya ord definieras som vanligt. Inkopplad:

```
    FORTH
    : SQ DUP * ; : Q4 SQ SQ ; OK
    SAVEW LIB OK
    BYE
    ABC80
```

och senare, efter `EX` och `Z=CALL(16384,2)`:

```
    FORTH
    LOADW LIB OK
    3 Q4 . 81 OK
```

Orden laddas i BASIC:s minne, under stackens topp (STKTOP), som sänks
lika mycket; SYS visar att FRITT blir mindre. En ny LOADW tar samma
minne, så BASIC förlorar bara så mycket som den största filen behöver.

En fil som inte är en ordfil från samma Forthaid II, eller som inte
ryms, ger `?`, och ingenting ändras. En fil som inte finns ger fel 21
till BASIC, som när OPEN misslyckas annars.

SAVEW och LOADW kräver ett skivminne. Med kassetten svarar de `?`:
kassettens buffertar är samma minne som de egna orden. Efter
`Z=CALL(16384)` eller `Z=CALL(16384,1)` ligger BASIC:s stack under
STKTOP, och LOADW kan då bara ta det minne som en tidigare LOADW har
tagit; ryms filen inte där svarar den `?`. Inkopplad, eller i tolken
efter `FORTH`, går det alltid.

### HELP

Kommandona, och de egna orden.

```
    HELP
    SYS LIST PEEK FIND VAR DEL AUTO OLD ED
    START CHANGE REN WORDS BYE EX FORTH
    SAVEW LOADW
    KUB SQ
```

### FORTH, EX, BYE, WORDS

`FORTH` går till Forth-tolken och `EX` kopplar ur kretsen. I tolken går
`BYE` tillbaka till BASIC, och `WORDS` listar alla ord.


## Stegningen

Medan ett program kör i inkopplat läge ger CTRL-C en paus: programmet
står still, och ett utropstecken står där markören är. Nästa tangent
bestämmer:

| Tangent | Vad                                                          |
|---------|--------------------------------------------------------------|
| CTRL-T  | programmet kör med TRACE så länge tangenten hålls ned        |
| CTRL-S  | programmet kör utan TRACE så länge tangenten hålls ned       |
| CTRL-C  | programmet stannar, som med CTRL-C utan kretsen (STOP LINE)  |
| annan   | programmet kör vidare som förut                              |

När CTRL-T eller CTRL-S släpps blir det paus igen. Ett kort tryck kör
alltså några rader, och med TRACE syns radnumren. Tangenten som avslutar
pausen går inte vidare till programmet.

På webbsidan och i emulatorn hålls varje tangent nere en kort stund (40
ms) och släpps sedan. Ett tryck på CTRL-T kör därför en liten bit;
håller man tangenten nere upprepar webbläsaren den, och programmet kör
med korta pauser.


## Egna ord

I Forth-tolken kan nya ord definieras med `:` och `;`, och de är sedan
kommandon som de andra.

```
    : SQ DUP * ; OK
    7 SQ . 49 OK
```

Ett ord som räknar programmets rader, med kretsens ord för BASIC-raderna
(se tabellen nedan):

```
    : RADER  0 BOFA É
    BEGIN DUP END-PRG? 0= WHILE
    NEXTLINE SWAP 1+ SWAP REPEAT DROP . ; OK
    RADER 3 OK
```

ABC80 visar `@` som `É` och `$` som `¤` (samma koder; i emulatorn skrivs
de som `@` och `$`).
En definition kan gå över flera rader, men varje rad är högst 79 tecken.

Talen är 16 bitar, decimala eller hexadecimala med `$`, och med minus
framför negativa. Ett ord som inte finns, eller ett tal som inte går att
läsa, ger namnet och `?`, och raden avbryts; en halvfärdig definition
tas bort. Har ett ord tagit mer från stacken än där fanns svarar
tolken `STACK ?` och tömmer stackarna.

Kompilatorn ligger i bank 3. Det märks inte när man skriver, och de
färdiga orden behöver inte banken när de körs.

### Plats

De egna orden ligger i RAM på `$FB70`–`$FC7F`, 272 byte, i kassettens
blockbuffertar; SYS visar hur mycket som är kvar (`ORDEN` och `FRITT`).
Kompilatorn prövar inte gränsen. Ord som går förbi den skrivs över av
kretsens stackar när kretsen är inkopplad. Bufferten används av ROM:en
när en fil läses eller skrivs på kassett, och då skrivs de egna orden
över.

Ord som har laddats med LOADW ligger i stället under STKTOP och tar
inget av de 272 byten. Ett större förråd av ord byggs alltså så här:
definiera några ord, spara dem med SAVEW, ladda dem med LOADW, definiera
fler på `$FB70` och spara alltihop med SAVEW igen.

`Z=CALL(16384)` och `FORTH` glömmer orden. Vill man behålla dem startas
tolken med `Z=CALL(16384,1)`. Från inkopplat läge går de egna orden bara
att köra i tolken, inte direkt i BASIC, och `FORTH` glömmer dem, så där
behöver man först `EX`, sedan `Z=CALL(16384,1)`, eller LOADW efter
`FORTH`.

### Orden

Bokens tolk (exempel 7, band 2) har de vanliga orden:

| Slag             | Ord                                                                   |
|------------------|-----------------------------------------------------------------------|
| stacken          | `DUP DROP SWAP OVER ROT ?DUP >R R> R@`                                |
| räkning          | `+ - * U/MOD NEGATE 1+ 1- 2* 10*`                                     |
| jämförelser      | `= < U< 0= 0< AND OR`                                                 |
| minnet           | `@ ! C@ C! +! CMOVE HERE , C, ALLOT`                                  |
| in och ut        | `EMIT TYPE KEY ACCEPT CR SPACE . U. ."`                               |
| styrning         | `IF ELSE THEN BEGIN UNTIL AGAIN WHILE REPEAT DO LOOP I J UNLOOP EXIT` |
| definitioner     | `: ; VARIABLE CONSTANT IMMEDIATE ' EXECUTE LITERAL`                   |
| tolken           | `WORDS BYE PARSE-NAME LOOKUP`                                         |

Forthaid II lägger till, som Forthaid:

| Ord              | Stacken           | Vad                                                  |
|------------------|-------------------|------------------------------------------------------|
| `H.`             | ( u -- )          | talet hexadecimalt, fyra siffror                     |
| `BOFA EOFA HEAP` | ( -- a )          | adressen till BASIC:s pekare (läs med `@`)           |
| `FINDLN`         | ( n -- rad )      | den första raden med numret n eller högre            |
| `LINE#`          | ( rad -- n )      | radens nummer                                        |
| `NEXTLINE`       | ( rad -- rad' )   | nästa rad                                            |
| `END-PRG?`       | ( rad -- f )      | sant vid programmets slut                            |
| `LISTLINE`       | ( rad -- n )      | listar raden; n är skärmraderna den tog              |
| `DETOK`          | ( rad buf -- slut ) | raden som text i buf, med ROM:ens DETOK            |
| `ROMCALL`        | ( a -- )          | anropar rutinen på a i ROM:en                        |
| `I@ I!`          | ( -- n ), ( n -- ) | Z80:ns I-register, `$4F` när kretsen är inkopplad   |

En rad i programmet börjar med längden (en byte) och radnumret (två),
och programmet slutar med en byte 1. Mer om BASIC:s rader och pekare
står i band 1.

Kommandonas hjälpord i bankerna, som `PEEKC` och `RENUM`, syns inte för
tolken; bara de i tabellen ovan och kommandona går att skriva vid OK.
`FINDLN`, `LISTLINE`, `DETOK` och `H.` ligger i bank 0 och körs genom
den fasta delen, som byter bank och byter tillbaka när ordet är klart.


### VERKTYG.FTH på exempeldisketten

Exempeldisketten (`emulator/exempel/program/diskett/program.dsk`, på
webbsidan *Exempeldisketten*) har en ordfil med fyra ord efter
kommandon i Smartaid-kretsarna och deras like (band 2, kapitlet
Smartaid):

| Ord        | Vad                                                              |
|------------|------------------------------------------------------------------|
| `n CROSS`  | de rader som hoppar till rad n: `GOTO`, `GOSUB`, `THEN`, `ELSE` och listorna i `ON` (ABC80L:s CROSS, Schoolaids XREF) |
| `n RAM`    | rad n som den ligger i minnet, byte för byte i hex (Superbasics RAM) |
| `FIX`      | en referens till en rad som inte finns får numret på nästa rad som finns; de ändrade raderna listas (Superbasics FIX) |
| `PAC`      | tar bort raderna som börjar med `REM` och gör sedan `FIX` (Superbasics PAC) |

```
    LOAD GISSA
    Z=CALL(16384,2)
    FORTH
    LOADW VERKTYG.FTH OK
    60 CROSS
    100 GOTO 60
    OK
    80 RAM
    80 IF G=T THEN 110
    1C 50 00 88 C0 F0 47 44 69 D4
    C0 F0 54 73 D4 D4 E2 BA BD 87
    07 81 BF 6E 00 47 65 0D
    OK
```

I RAM:s rader syns radens inre form från band 1: längden `1C`, numret
`50 00`, `IF` (`88`), `87 07` som hoppar förbi `THEN`-delen och
radreferensen `BF 6E 00` med plats för målradens adress, som fylls i vid
`RUN`. CROSS och FIX letar efter just referenserna (`$BF`, och `$BE` med
en lista för `ON`). De går genom raden ett element i taget med ROM:ens
SKPELM (`$0EFB` i både 9913 och 11273), genom ett litet stycke Z80-kod i
ordet `STUB`, så att ett tal eller en adress som råkar innehålla `$BF`
inte räknas.

PAC och FIX på ett program med `REM`-rader och ett hopp till en rad som
saknas:

```
    10 REM PROV
    20 A=1
    30 IF A=2 THEN 10 ELSE 50
    40 REM GAMMAL
    50 ON A GOTO 70,40,10
    60 GOSUB 75
    70 REM SLUT
    80 PRINT "REM";A;
    90 END
```

`PAC` tar bort raderna 10, 40 och 70 och listar de rader som FIX sedan
ändrar:

```
    30 IF A=2 THEN 20 ELSE 50
    50 ON A GOTO 80,50,20
    60 GOSUB 80
```

Ett hopp förbi programmets sista rad står kvar. Adresserna i
referenserna fylls i på nytt vid varje `RUN`, så orden behöver bara
ändra radnumren; PAC flyttar resten av programmet ned som `DEL` och gör
sedan CLEAR, så att BASIC:s variabler börjar efter programmet. Efter
`BYE` kan programmet köras som vanligt.

Källan är `emulator/exempel/program/forth/VERKTYG.FS`. Orden
tillsammans är större än de 272 byten på `$FB70`, så
`emulator/exempel/program/forth/spara.sh` skriver in dem i tolken i
omgångar med SAVEW och LOADW emellan, som filens huvud beskriver, och
prövar sedan att filen laddas och att alla orden finns. Skriptet gör så
med varje `NAMN.FS` i samma mapp, så ett nytt ordförråd läggs bara där.
En ordfil hör till den Forthaid II den gjordes med, och LOADW svarar `?`
för en annan; därför kör `gor.sh` här skriptet varje gång kretsen byggs,
liksom exemplens `gor.sh`. Webbsidan görs sedan om med `make webb` i
`emulator/`.

## Minnet

| Var             | Vad                                                            |
|-----------------|----------------------------------------------------------------|
| `$4000`–`$4FEF` | den fasta delen; vektortabellen för avbrotten på `$4F34`       |
| `$5000`–`$57FF` | bankerna, en i taget; `$5040`+n väljer bank n                   |
| `$5800`–`$5FFF` | fritt för TKN80:s bildminne                                    |
| `$7CF8`–`$7EFD` | tolkens och kommandonas variabler, i bildminnets osynliga byte |
| `$7EFE`–`$7EFF` | stegningens flagga                                             |
| `$FB00`–`$FB4F` | radbufferten                                                   |
| `$FB50`–`$FB6F` | resten av variablerna                                          |
| `$FB70`–`$FC7F` | de egna orden                                                  |
| `$FC80`–`$FCFF` | stackarna, medan kretsen läser en rad eller kör ett kommando   |
| under STKTOP    | de ord som LOADW har laddat                                    |

Variablerna i bildminnet ligger på `$7CF8`–`$7CFF`, `$7DF8`–`$7DFF` och
`$7EF8`–`$7EFF`, de byte som är osynliga både med 40 och med 80 tecken
per rad. `$7FF8`–`$7FFF` används inte. Kretsen tar inget av BASIC:s
minne utom det LOADW tar. Ett program som själv använder bildminnets
osynliga byte, som vissa skrivarrutiner gör, krockar med den.


## Begränsningar

- Kortet med bankerna har inte funnits; Forthaid II går bara i
  emulatorn i terminalen och på webbsidan.
- Ingen automatisk start: kretsen kopplas in med `Z=CALL(16384,2)` efter
  varje RESET.
- Smartaid 3:s RESUME, DIR, LIB, FOR i direktläge och utskrift på
  skrivaren (PR:) finns inte.
- Forthaid II är lika snabb som Forthaid, 1,2–1,8 gånger långsammare än
  Smartaid 3 och med FIND 3,5 gånger (mätt i emulatorn på ett program om
  12 KB; se `LASMIG.md`). Med TKN80 i 80-teckensläget tar kommandona upp
  till 20 ms längre.
- SAVEW och LOADW kräver ett skivminne. En VARIABLE eller CONSTANT
  sparas som den är, så en konstant som håller adressen till ett eget
  ord pekar fel efter LOADW. En fil från en annan version av Forthaid II
  laddas inte.
- Kretsen känner igen BASIC:s kommandoloop på stacken och är prövad med
  9913 och 11273. Med en annan ROM-version tar den kanske inte över.
