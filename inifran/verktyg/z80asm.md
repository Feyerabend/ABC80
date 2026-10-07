# z80asm – manual

`z80asm` översätter en källfil i Z80-assembler till bytes. Den skrevs för
boken *ABC80 inifrån*, och allt i bokens listningar och exempel går
igenom den. Skrivsättet är Zilogs, som i bokens listningar.

    z80asm [-o fil.bin] [-x fil.hex] [-l fil.lst] källa.asm

| Flagga | |
|---|---|
| `-o fil.bin` | binärfilen; utan `-o` källans namn med `.bin` |
| `-x fil.hex` | samma bytes som Intel HEX |
| `-l fil.lst` | listning: adress, bytes och källrad, sist symbolerna |

Binärfilen börjar på den lägsta adress som har fått en byte och slutar
på den högsta. Luckor mellan två `ORG` fylls med $FF, som i en tom EPROM.
En krets som är assemblerad från `ORG $4000` blir alltså en fil som ska
ligga på $4000, och en hel ROM från `ORG 0` blir en ROM-avbild.

Vid fel skrivs varje fel på en rad, `fil:rad: text`, sedan antalet fel,
och programmet slutar med status 1 utan att skriva några filer:

    fel.asm:2: förskjutningen 200 är utanför -128..127
    fel.asm:3: hoppet når inte (203 bytes)
    fel.asm:4: okänd symbol: OKAND
    fel.asm:5: okänd instruktion: FOO
    fel.asm:6: START är redan definierad
    fel.asm: 5 fel

## Källraderna

    ETIKETT: INSTR operand,operand   ; kommentar

- En **etikett** står först på raden; kolon efter den behövs inte där,
  men en etikett med kolon får stå efter blanktecken. Namnen består av
  bokstäver (A–Z, a–z), siffror, `_` och `.`, börjar inte med en siffra
  och skiljer på versaler och gemener.
- **Instruktionerna och registren** skrivs med versaler eller gemener,
  som det passar.
- **Kommentarer** börjar med `;` och gäller till radens slut. En rad
  kan vara tom eller bara ha en etikett.
- Filen är text i UTF-8 eller ASCII. Svenska tecken går bra i
  kommentarerna och i strängarna, men i strängarna blir de flera bytes,
  så skriv ABC80:s egna koder när det är dem som ska in i minnet
  (`]` för Å, `[` för Ä, `\` för Ö).

## Direktiven

| Direktiv | |
|---|---|
| `ORG uttr` | nästa byte hamnar på adressen |
| `namn EQU uttr` | ger namnet ett värde (också `namn = uttr` och `namn: EQU uttr`) |
| `DEFB uttr,"text",...` | bytes; en sträng ger en byte per tecken. Också `DB`, `DEFM` och `DM` |
| `DEFW uttr,...` | ord på 16 bitar, den låga byten först. Också `DW` |
| `DEFS antal[,byte]` | `antal` bytes med värdet `byte` (standard 0). Också `DS` |
| `END` | resten av filen läses inte |

En sträng står inom `"`; ett citattecken inne i den skrivs dubbelt:
`DEFM "SA ""HEJ"""`. Inom `'` står ett enda tecken, som är ett tal
(`'A'` = 65) och kan stå i uttryck.

## Tal och uttryck

| Skrivsätt | Värde |
|---|---|
| `255` | decimalt |
| `$FF`, `0FFH`, `0xFF` | hexadecimalt (med `H` måste talet börja med en siffra) |
| `%1010` | binärt |
| `'A'` | tecknets kod |
| `$` | adressen där raden börjar |

Operatorerna, svagast först (samma nivå räknas från vänster):

| Nivå | Operatorer |
|---|---|
| 1 | `\|` eller |
| 2 | `^` antingen eller |
| 3 | `&` och |
| 4 | `<<` `>>` skift |
| 5 | `+` `-` |
| 6 | `*` `/` `%` (heltal; `%` är resten) |
| 7 | framför ett värde: `-` `+` `~` (ettkomplement) |

Parenteser grupperar, men en operand som helt står inom parentes är en
minnesadress, som i Zilogs skrivsätt:

    LD   A,(BUF+1)       ; byten på adressen BUF+1
    LD   A,(BUF+1)*2     ; talet (BUF+1)*2 – parentesen omfattar inte allt

Värdet räknas med minst 32 bitar och kontrolleras sedan: en byte ska
vara mellan -128 och 255, ett ord mellan -32768 och 65535 (negativa
värden blir tvåkomplement).

## Instruktionerna

Alla dokumenterade Z80-instruktioner finns, med Zilogs namn och
operander:

    LD   A,(IX+5)        ; (IX+d) och (IY+d), d mellan -128 och 127
    LD   (HL),N
    JP   NZ,ADR          ; villkoren NZ Z NC C PO PE P M
    JR   C,SLINGA        ; JR och DJNZ: målet ska ligga inom -126..129
    JP   (HL)            ; också JP (IX) och JP (IY)
    EX   AF,AF'
    IN   A,($38)         ; och IN r,(C), OUT (C),r
    RST  $38             ; 0, 8, $10 ... $38
    IM   2
    BIT  7,(HL)          ; BIT, SET och RES med bitnummer 0-7

ALU-instruktionerna med A skrivs både med och utan `A,`: `SUB B` och
`SUB A,B`, `ADD A,B`, `CP (HL)` och `CP A,(HL)`.

De odokumenterade instruktionerna (`SLL`, halva indexregister som
`IXH`, `OUT (C),0` …) finns inte; skriv dem med `DEFB` där de behövs.

## Passen

En etikett kan användas innan den är definierad. Det första passet
räknar ut alla etiketters adresser, och det sista skriver koden. Beror
en `EQU` eller ett `DEFS` på en symbol längre ned, och därmed adresserna
efter den, går assemblern fler varv tills inget längre ändras (högst
20). Ett namn som har fått ett annat värde i det sista passet än i det
förra är ett fel.

    TEXT:   DEFM "LIST"
    LANGD:  EQU  $-TEXT       ; 4, och användas före sin rad
            ...
            LD   BC,LANGD

## Listningen

`-l` ger en rad per källrad: adressen, upp till fyra bytes (fler fortsätter
på nästa rader, utan källtext) och källraden. En `EQU` visar värdet efter `=`. Sist
kommer symbolerna i bokstavsordning med sina värden.

    4000                   ORG  $4000
    4000 C30640            JP   PA             ; $4000
         = 7FF8     ANTAL:  EQU  $7FF8
    4006 210000    PA:     LD   HL,0

    Symboler:
    ANTAL            7FF8   PA               4006   ...

## Koden

Allt finns i `z80asm.c`, uppdelat i avsnitt i den ordning delarna
används:

1. **minnet och utdata** – en bild av hela adressrymden, med en
   markering av vilka bytes som har skrivits
2. **symbolerna** – en hashtabell; varje symbol minns i vilket pass den
   definierades och med vilket värde
3. **uttrycken** – en rekursiv parser med en funktion per nivå i
   tabellen ovan
4. **operanderna** – varje operand blir en typ (register, registerpar,
   `(HL)`, `(IX+d)`, tal, minnesadress …) med ett nummer, samma nummer
   som i operationskoden
5. **instruktionerna** – för varje instruktion de giltiga
   kombinationerna av operandtyper och hur bytena byggs av numren
6. **raderna, direktiven och passen** – och sist `main`

Ett fel avbryter raden med `longjmp` och skrivs bara i det sista passet,
så att varje fel kommer en gång.
