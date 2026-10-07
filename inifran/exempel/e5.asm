; Exempel 5: en egen sats på IEC-kretsens plats. Skrivet för boken,
; inte ur någon krets.
;
; Kretsen sitter på $7000, där Luxors IEC-krets annars sitter. ROM:en
; kompilerar satsen CMD genom att hoppa till $7000 och kör den genom att
; hoppa till $7003. Här blir
;   CMD stränguttryck
; en statusrad: strängen skrivs på skärmens översta rad, fylls ut med
; blanktecken till 40 tecken, och markören flyttas inte.

ERRA:   EQU  $0012          ; fel med koden i A
RST38:  EQU  $0038          ; beräkna ett kompilerat uttryck
CMPSTX: EQU  $003B          ; kompilera ett stränguttryck
RAD0:   EQU  $7C00          ; skärmens översta rad

        ORG  $7000

        JP   CMDKMP         ; $7000 kompilera CMD
        JP   CMDKOR         ; $7003 kör CMD
        JP   INTE           ; $7006 IEC$
        JP   INTE           ; $7009 enheten "IEC:", nio platser
        JP   INTE
        JP   INTE
        JP   INTE
        JP   INTE
        JP   INTE
        JP   INTE
        JP   INTE
        JP   INTE

; IEC$ och enheten finns inte i den här kretsen: fel 8, som när
; kretsen saknas.
INTE:   LD   A,$88
        JP   ERRA

; Kompileringen. HL pekar i programtexten efter CMD och DE på nästa
; plats i den kompilerade koden. CMPSTX kompilerar ett stränguttryck,
; flyttar fram båda och ger CY och en felkod i A om något är fel.
CMDKMP: JP   CMPSTX

; Körningen. DE pekar i den kompilerade koden efter CMD. RST38 beräknar
; uttrycket, flyttar fram DE och lämnar strängen på stacken:
;   SP+0  S, antalet bytes med strängens tecken längre ned
;   SP+2  teckens adress
;   SP+4  längden
;   SP+6  S bytes
CMDKOR: CALL RST38
        POP  AF             ; S (i AF, bara för att flytta det)
        POP  HL             ; adressen
        POP  BC             ; längden
        PUSH AF
        PUSH DE             ; DE behövs när satsen är slut
        LD   A,B
        AND  A
        JR   NZ,K1          ; 256 tecken eller fler
        LD   A,C
        CP   40
        JR   C,K2
K1:     LD   BC,40          ; högst en rad
K2:     LD   DE,RAD0
        LD   A,B
        OR   C
        JR   Z,K3
        LDIR                ; tecknen
K3:     LD   A,E
        CP   40             ; RAD0 & $FF = 0
        JR   NC,K4
        LD   A,' '          ; och blanktecken till radens slut
        LD   (DE),A
        INC  DE
        JR   K3
K4:     POP  DE
        POP  HL             ; S
        ADD  HL,SP          ; ta bort strängens tecken från stacken
        LD   SP,HL
        RET                 ; DE pekar på nästa sats
