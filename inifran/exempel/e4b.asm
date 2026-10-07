; Exempel 4, bank 1. Skrivet för boken, inte ur någon krets. Bank 0
; står före.

BANK0:  EQU  $4040
HORN:   EQU  $7C24

        ORG  $4000

        LD   (BANK0),A      ; $4000 i båda bankerna: välj bank 0

        DEFS $4083-$,$FF

; Stubbens mitt. Skrivningen till $4041 på $4080 i bank 0 har just valt
; den här banken, och nästa instruktion hämtas härifrån.
        CALL B1RUT          ; $4083 anropa rutinen i bank 1
        LD   (BANK0),A      ; $4086 byt tillbaka; $4089 hämtas ur bank 0

        DEFS $4100-$,$FF

B1RUT:  LD   A,'1'          ; $4100
        LD   (HORN+1),A
        RET
