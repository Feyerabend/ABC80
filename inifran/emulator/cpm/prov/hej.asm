; hej.asm -- provet till start-PROM:en (cpm/start.asm), till boken ABC80 inifrån
;
; skiva.py lägger den här sektorn sist på provskivan, efter nio
; sektorer där sektor n är fylld med byten n. Startblocket säger att
; tio sektorer ska till $F600 och att starten är på $FF80, i det
; gemensamma minnet. Programmet ser efter i CP/M-kartan att de nio
; sektorerna kom rätt (spår 0 sektor 1-7 och spår 1 sektor 0-1), byter
; till systemkartan och skriver svaret överst på ABC80:s skärm.

          ORG  $FF00
          DEFS $80,0
START:    LD   SP,0             ; stacken i det gemensamma minnet, som
                                ; finns kvar i systemkartan
          LD   HL,$F600
          LD   C,1              ; sektorn och dess byte
SEKTOR:   LD   B,0
BYTE:     LD   A,(HL)
          CP   C
          JR   NZ,FEL
          INC  HL
          DJNZ BYTE
          INC  C
          LD   A,C
          CP   10
          JR   NZ,SEKTOR
          LD   HL,RATT
          JR   SKRIV
FEL:      LD   HL,FELTEXT
SKRIV:    LD   A,2              ; systemkartan: ABC80:s skärm
          OUT  ($EF),A
          PUSH HL
          LD   HL,$7C00         ; blanka bildminnet
          LD   DE,$7C01
          LD   BC,$3FF
          LD   (HL),' '
          LDIR
          POP  HL
          LD   DE,$7C00
          LD   BC,16
          LDIR
STOPP:    JR   STOPP
RATT:     DEFM "STARTEN GICK BRA"
FELTEXT:  DEFM "FEL I SEKTORERNA"
          END
