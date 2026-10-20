# Forthaid, manualen

Forthaid är en verktygskrets för ABC80 i stil med Smartaid 3: kommandon
för att lista, söka, numrera om och ändra BASIC-program, en
skärmeditor och stegning av program som kör. Den är skriven i Forth på
tolken från exempel 7 i band 2 av *ABC80 inifrån*, och kommandona är
Forth-ord. Därför går det att skriva egna kommandon. Forthaid är inte
med i boken; hur den är byggd och mätt står i `LASMIG.md`.

Kretsen sitter på `$4000`–`$5FFF` (16384–24575), där Smartaid satt, och
går i båda ROM-versionerna, 9913 och 11273.


## Att få kretsen i maskinen

Bygg den med `./gor.sh`, som skriver `forthaid.bin` (8 176 byte).

- **Emulatorn i terminalen:**
  `emulator/abc80 -l utanfor/forthaid/forthaid.bin@4000`
- **SDL-fönstret:** samma `-l` till `abc80f`.
- **Webbsidan** (`emulator/webb/abc80.html`): välj *Forthaid* under
  *Program och kretsar*, eller öppna sidan med `?krets=forthaid`.

På en riktig ABC80 är den inte prövad.


## Att starta

Det finns två sätt att använda kretsen.

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
lämnar inget efter sig i BASIC:s minne.

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

`Z=CALL(16384)` och `FORTH` glömmer de egna orden; `Z=CALL(16384,1)`
behåller dem. Från inkopplat läge går `FORTH` till tolken, och `BYE`
därifrån tillbaka till BASIC med kretsen fortfarande inkopplad.


## Skärmeditorn

I inkopplat läge läses raderna med en editor där markören går fritt
över skärmen, som i Smartaid. En rad på skärmen kan läsas in igen med
högerpilen, eventuellt ändrad.

| Tangent       | Vad                                                    |
|---------------|--------------------------------------------------------|
| högerpilen    | tecknet under markören läses in i raden                |
| vänsterpilen  | det sista tecknet i raden tas bort                     |
| CTRL-X        | hela raden tas bort                                    |
| CTRL-W        | markören upp                                           |
| CTRL-Z        | markören ned                                           |
| CTRL-A        | markören åt vänster                                    |
| CTRL-S        | markören åt höger                                      |
| CTRL-Q        | markören till övre vänstra hörnet                      |
| CTRL-L        | skärmen rensas                                         |
| CTRL-D        | tecknet under markören tas bort ur skärmraden          |
| CTRL-P        | ett blanktecken sätts in i skärmraden                  |
| RETURN        | raden är klar                                          |

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

Listor som fyller skärmen (LIST, FIND, VAR och PEEK) stannar efter 22
rader och väntar på en tangent: mellanslag ger en rad till, RETURN en
ny skärm, och en annan tangent avbryter. CTRL-C avbryter också.

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
kvar. Finns inte rad *n* svarar ED `?`.

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

### SYS

Programmets gränser och det lediga minnet: BOFA (programmets början),
EOFA (dess slut), HEAP (variablernas slut), programmets och
variablernas storlek, det lediga för BASIC, och var nästa egna ord
hamnar och hur mycket plats som finns kvar för dem.

```
    SYS
    BOFA C000 EOFA C061 HEAP C06B
    PROGRAM 98 VAR 9 FRITT 14997
    ORDEN FB50 FRITT 304
```

### HELP

Kommandona, och de egna orden.

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

### Plats

De egna orden ligger i RAM på `$FB50`–`$FC7F`, 304 byte, i kassettens
blockbuffertar; SYS visar hur mycket som är kvar (`ORDEN` och `FRITT`).
Kompilatorn prövar inte gränsen. Ord som går förbi den skrivs över av
kretsens stackar när kretsen är inkopplad. Bufferten används av ROM:en
när en fil läses eller skrivs på kassett, och då skrivs de egna orden
över.

`Z=CALL(16384)` och `FORTH` glömmer orden. Vill man behålla dem startas
tolken med `Z=CALL(16384,1)`. Från inkopplat läge går de egna orden bara
att köra i tolken, inte direkt i BASIC, och `FORTH` glömmer dem, så där
behöver man först `EX`, sedan `Z=CALL(16384,1)`.

### Orden

Bokens tolk (exempel 7, band 2) har de vanliga orden:

| Slag             | Ord                                                              |
|------------------|------------------------------------------------------------------|
| stacken          | `DUP DROP SWAP OVER ROT ?DUP >R R> R@`                          |
| räkning          | `+ - * U/MOD NEGATE 1+ 1- 2* 10*`                                |
| jämförelser      | `= < U< 0= 0< AND OR`                                            |
| minnet           | `@ ! C@ C! +! CMOVE HERE , C, ALLOT`                             |
| in och ut        | `EMIT TYPE KEY ACCEPT CR SPACE . U. ."`                          |
| styrning         | `IF ELSE THEN BEGIN UNTIL AGAIN WHILE REPEAT DO LOOP I J UNLOOP EXIT` |
| definitioner     | `: ; VARIABLE CONSTANT IMMEDIATE ' EXECUTE LITERAL`              |
| tolken           | `WORDS BYE PARSE-NAME LOOKUP`                                    |

Forthaid lägger till:

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
| `I@ I!`          | ( -- n ), ( n -- ) | Z80:ns I-register, `$5F` när kretsen är inkopplad   |

En rad i programmet börjar med längden (en byte) och radnumret (två),
och programmet slutar med en byte 1. Mer om BASIC:s rader och pekare
står i band 1.


## Minnet

| Var             | Vad                                                         |
|-----------------|-------------------------------------------------------------|
| `$4000`–`$5FFF` | kretsen; vektortabellen för avbrotten på `$5F34`            |
| `$7C78`–`$7EFE` | tolkens och kommandonas variabler, i bildminnets osynliga byte |
| `$7F7E`–`$7F7F` | stegningens flagga                                          |
| `$FB00`–`$FB4F` | radbufferten                                                |
| `$FB50`–`$FC7F` | de egna orden                                               |
| `$FC80`–`$FCFF` | stackarna, medan kretsen läser en rad eller kör ett kommando |

Kretsen tar inget av BASIC:s minne. Ett program som själv använder
bildminnets osynliga byte, som vissa skrivarrutiner gör, krockar med
den.


## Begränsningar

- Ingen automatisk start: kretsen kopplas in med `Z=CALL(16384,2)` efter
  varje RESET.
- Smartaid 3:s RESUME, DIR, LIB, FOR i direktläge och utskrift på
  skrivaren (PR:) finns inte.
- Forthaid är 1,2–1,8 gånger långsammare än Smartaid 3, och FIND 3,5
  gånger (mätt i emulatorn på ett program om 12 KB; se `LASMIG.md`).
- Kretsen känner igen BASIC:s kommandoloop på stacken och är prövad med
  9913 och 11273. Med en annan ROM-version tar den kanske inte över.
