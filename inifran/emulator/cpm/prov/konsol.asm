; konsol.asm -- provet till BIOS:ens konsol och LST: (cpm/bios.asm), till boken ABC80 inifrån
;
; Ett CP/M-program (KONSOL.COM) som skriver skärmkoderna genom BDOS:en
; och tre tecken till LST: och stannar sedan. prov.sh jämför skärmen
; med prov/konsol.txt (40 tecken per rad).

BDOS      EQU  5
ESC       EQU  27

          ORG  $100
          LD   HL,TEXTEN
SKRIV:    LD   A,(HL)
          OR   A
          JR   Z,SKRIVARE
          PUSH HL
          LD   E,A
          LD   C,2              ; tecken till konsolen
          CALL BDOS
          POP  HL
          INC  HL
          JR   SKRIV
SKRIVARE: LD   HL,LST
NASTA:    LD   A,(HL)
          OR   A
          JR   Z,$              ; stanna, så att skärmen står kvar
          PUSH HL
          LD   E,A
          LD   C,5              ; tecken till LST:
          CALL BDOS
          POP  HL
          INC  HL
          JR   NASTA

TEXTEN:   DEFB 26                              ; radera skärmen
          DEFB ESC,"=",32+10,32+0,"BORT",13,10,"BORT",13,10,"BORT"
          DEFB ESC,"=",32+10,32+2,ESC,"Y"      ; rad 10: BO, rad 11-12 tomma
          DEFB ESC,"=",32+23,32+0,"SIST",13,10,"NY" ; skärmen en rad upp
          DEFB 30,"ABCDEFGH",8,8,8,ESC,"T"     ; rad 0: ABCDE
          DEFB ESC,"=",32+2,32+4,"XYZ"         ; rad 2 kolumn 4
          DEFB 11,"1",12,12,"2"                ; rad 1 kolumn 7 och 10
          DEFB 30,"*"                          ; överst till vänster
          DEFB ESC,"=",32+5,32+38,"ABCD"       ; rad 5-6 över kanten
          DEFB ESC,"=",32+99,32+30,"!"         ; rad 23, kolumn 30
          DEFB ESC,"=",32+20,32+99,"?"         ; rad 20, kolumn 39
          DEFB ESC,"=",32+20,32+0,"[\]{|}"     ; ÄÖÅäöå
          DEFB 0
LST:      DEFB "LST",0
          END
