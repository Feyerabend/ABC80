; Exempel 4, bank 0: anrop mellan två banker på samma adresser.
; Skrivet för boken, inte ur någon krets. Bank 1 står efter.
;
; Kretsen har två banker à 4 KB på $4000-$4FFF. En skrivning till $4040
; väljer bank 0 och en skrivning till $4041 bank 1; värdet spelar ingen
; roll. Från BASIC:
;   Z=CALL(16384)   skriver 0, 1 och 0 överst till höger på skärmen:
;                   bank 0, ett anrop till bank 1, tillbaka i bank 0.

BANK0:  EQU  $4040          ; skrivning hit: bank 0
BANK1:  EQU  $4041          ; skrivning hit: bank 1
B1RUT:  EQU  $4100          ; rutinen i bank 1
HORN:   EQU  $7C24          ; rad 0, kolumn 36

        ORG  $4000

        LD   (BANK0),A      ; $4000 i båda bankerna: välj bank 0
        JP   START          ; $4003 bara i bank 0

START:  LD   A,'0'          ; bank 0 skriver en nolla ...
        LD   (HORN),A
        CALL ANR1           ; ... bank 1 en etta ...
        LD   A,'0'
        LD   (HORN+2),A     ; ... och bank 0 en nolla till
        RET

        DEFS $4080-$

; Stubben. Bara de bytes som står här hämtas ur bank 0; $4083-$4088
; hämtas ur bank 1.
ANR1:   LD   (BANK1),A      ; $4080 byt till bank 1
        DEFS 6,$FF          ; $4083 (bank 1: CALL B1RUT, LD (BANK0),A)
        RET                 ; $4089 tillbaka till anroparen i bank 0
