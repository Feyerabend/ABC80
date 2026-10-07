; Exempel 3: en snabbtangent som skriver en hel rad. Skrivet för boken,
; inte ur någon krets.
;
; Kretsen sitter på $4000. Från BASIC:
;   Z=CALL(16384)   koppla in (I = $40)
;   Z=CALL(16387)   koppla ur (I = 0)
; Inkopplad gör CTRL-L i direktläge detsamma som att skriva LIST och
; trycka RETURN. I INPUT gör CTRL-L ingenting, som förut.

KEYINT: EQU  $031E          ; ROM:ens tangentrutin (båda versionerna)
CASINT: EQU  $0594          ; ROM:ens kassettrutin (båda versionerna)
PRTXT:  EQU  $000B          ; skriv BC tecken från (HL) på skärmen
EFTGK:  EQU  $02BF          ; i INLINE, efter CALL GETKEY
RADSLT: EQU  $02E5          ; i INLINE: lägg CR i bufferten och återvänd
EFTINL: EQU  $00F0          ; i direktlägets slinga, efter CALL INLINE
SNABB:  EQU  $0C            ; CTRL-L

        ORG  $4000

        JP   PA             ; $4000
        JP   AV             ; $4003

PA:     LD   A,$40          ; koppla in
        LD   I,A
        RET

AV:     XOR  A              ; koppla ur
        LD   I,A
        RET

        DEFS $4034-$

        DEFW TANGNT         ; $4034 tangentbordet
        DEFW CASINT         ; $4036 kassetten, ROM:ens

; Tangentbordets avbrott. Stacken som i exempel 2; är tangenten CTRL-L och
; väntar BASIC:en på en rad i direktläge byts GETKEY:s återhopp mot
; RAD.
TANGNT: PUSH AF
        PUSH HL
        PUSH DE
        IN   A,($38)        ; tangenten, med strobebiten
        AND  $7F
        CP   SNABB
        JR   NZ,T9
        LD   HL,8
        ADD  HL,SP          ; HL -> GETKEY:s återhopp
        LD   DE,EFTGK
        CALL CPW
        JR   NZ,T9
        PUSH HL
        LD   DE,4
        ADD  HL,DE          ; HL -> INLINE:s återhopp
        LD   DE,EFTINL
        CALL CPW
        POP  HL
        JR   NZ,T9
        LD   DE,RAD
        LD   (HL),E
        INC  HL
        LD   (HL),D
T9:     POP  DE
        POP  HL
        POP  AF
        JP   KEYINT         ; ROM:en läser tangenten som vanligt

; Jämför ordet i (HL) med DE. Z = lika. Ändrar A och flaggorna.
CPW:    LD   A,(HL)
        CP   E
        RET  NZ
        INC  HL
        LD   A,(HL)
        DEC  HL
        CP   D
        RET

; Hit kommer GETKEY:s RET. HL pekar på nästa plats i radbufferten och
; B är antalet platser som är kvar. Ryms texten läggs den i bufferten
; och på skärmen, och raden avslutas som om RETURN hade tryckts. Annars
; får INLINE tecknet, och ett styrtecken hoppar den över.
RAD:    CP   SNABB
        JP   NZ,EFTGK
        LD   A,B
        CP   LANGD+1
        JP   C,EFTGK        ; ryms inte
        EX   DE,HL          ; DE = bufferten
        LD   HL,TEXT
        LD   BC,LANGD
        LDIR                ; in i bufferten
        PUSH DE             ; slutet
        LD   HL,TEXT
        LD   BC,LANGD
        CALL PRTXT          ; och på skärmen
        POP  HL
        JP   RADSLT         ; CR efter texten, INLINE återvänder

TEXT:   DEFM "LIST"
LANGD:  EQU  $-TEXT
