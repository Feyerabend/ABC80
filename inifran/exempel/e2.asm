; Exempel 2: versaler i direktläge. Skrivet för boken, inte ur någon
; krets.
;
; Kretsen sitter på $4000. Från BASIC:
;   Z=CALL(16384)   koppla in (I = $40)
;   Z=CALL(16387)   koppla ur (I = 0)
; Inkopplad gör den om a-z och ä ö å ü till versaler när BASIC:en väntar
; på en rad i direktläge. INPUT och GET får tecknen som de skrivs.

KEYINT: EQU  $031E          ; ROM:ens tangentrutin (båda versionerna)
CASINT: EQU  $0594          ; ROM:ens kassettrutin (båda versionerna)
EFTGK:  EQU  $02BF          ; i INLINE, efter CALL GETKEY
EFTINL: EQU  $00F0          ; i direktlägets slinga, efter CALL INLINE

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

; Tangentbordets avbrott. Väntar BASIC:en i direktläge på en rad ser
; stacken ut så här när avbrottsrutinen har sparat AF, HL och DE:
;   SP+0   DE
;   SP+2   HL
;   SP+4   AF
;   SP+6   den avbrutna adressen i GETKEY
;   SP+8   EFTGK   GETKEY:s återhopp till INLINE
;   SP+10  IX      sparat av INLINE
;   SP+12  EFTINL  INLINE:s återhopp till direktlägets slinga
; Då byts GETKEY:s återhopp mot STORA.
TANGNT: PUSH AF
        PUSH HL
        PUSH DE
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
        LD   DE,STORA
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

; Hit kommer GETKEY:s RET med tecknet i A. Koderna $61-$7E (a-z och
; ä ö å ü) blir $41-$5E; resten lämnas. Sedan fortsätter INLINE.
STORA:  CP   $61
        JR   C,S1
        CP   $7F
        JR   NC,S1
        SUB  $20
S1:     JP   EFTGK
