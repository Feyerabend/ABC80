
; Forthaid II: fjärrorden (steg 3 i ../PLAN.md, utanför boken).
;
; Bankkortet (../LASMIG.md): fönstret $5000-$57FF, en skrivning till
; $5040+n kopplar in bank n. Kortet går inte att läsa av, så varje bank
; har sitt nummer i sin första byte (BANKNR) och därefter antalet ord i
; tabellen och tabellen, med adresserna till de ord som anropas utifrån.
;
; Ett ord i en bank anropas utifrån genom ett fjärrord i den fasta
; delen, som fc2.py skriver:
;     F_ORD:  CALL DOFAR
;             DEFB bank,plats      ; plats = 2 + 2 × numret i tabellen
; Ett anrop inom banken och ett anrop av ett ord i den fasta delen är
; som vanligt.
;
; Returstacken efter DOFAR och ordets DOCOL, högre adresser överst:
;     IP i det anropande ordet
;     den bank som var inkopplad
;     FARX                         <- IX
; Ordets EXIT ger IP = FARX, och NEXT kör då FAREXIT, som tar tillbaka
; banken och gör EXIT till det anropande ordet.

;W (FAR) DOFAR EXIT
BANKNR: EQU  $5000          ; den inkopplade bankens nummer
BANKH:  EQU  $50            ; $5040+n kopplar in bank n
DOFAR:  DEC  IX
        LD   (IX+0),D
        DEC  IX
        LD   (IX+0),E       ; IP
        LD   A,(BANKNR)
        DEC  IX
        LD   (IX+0),0
        DEC  IX
        LD   (IX+0),A       ; den gamla banken
        POP  HL             ; fjärrordets två byte
        LD   A,(HL)         ; den nya banken
        INC  HL
        LD   E,(HL)         ; platsen i dess tabell
        OR   $40
        LD   L,A
        LD   H,BANKH
        LD   (HL),A         ; koppla in den
        LD   L,E
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        EX   DE,HL          ; HL = ordet i banken
        LD   DE,FARX
        JP   (HL)
FARX:   DEFW FAREXIT
FAREXIT: LD  A,(IX+0)
        INC  IX
        INC  IX
        OR   $40
        LD   L,A
        LD   H,BANKH
        LD   (HL),A         ; den gamla banken igen
        JP   EXIT
