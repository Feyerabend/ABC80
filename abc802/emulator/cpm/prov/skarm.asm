; skarm.asm -- provet till BIOS:ens skärmkoder (cpm/prov.sh): 22 rader A-V,
; sedan ESC = med ESC T, ESC R, ESC E, ^^ och ^L, BS och ESC Y.

          ORG  $0100
          LD   B,22
          LD   A,'A'
RADER:    PUSH BC
          PUSH AF
          LD   E,A
          LD   C,2
          CALL 5
          LD   DE,LINJE
          LD   C,9
          CALL 5
          POP  AF
          INC  A
          POP  BC
          DJNZ RADER
          LD   HL,KODER
UT:       LD   A,(HL)
          CP   $FF
          RET  Z
          PUSH HL
          LD   E,A
          LD   C,2
          CALL 5
          POP  HL
          INC  HL
          JR   UT
LINJE:    DB   "0123456789012345678901234567890123456789",13,10,"$"
KODER:    DB   27,"=",32+2,32+5,"T"          ; rad C kolumn 5: X sedan
          DB   "X",27,"T"                    ; radera resten
          DB   27,"=",32+4,32+0,27,"R"       ; rad E bort
          DB   27,"=",32+6,32+0,27,"E","NY RAD" ; ny rad före G
          DB   30,12,12,11,"#"               ; hem, höger 2, upp (ingen), #
          DB   27,"=",32+10,32+20,8,"<",27,"Y"  ; rad K kol 19 <, radera till slut
          DB   27,"=",32+12,32+0,"SLUT",13,10
          DB   $FF
