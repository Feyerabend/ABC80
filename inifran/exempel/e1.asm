; Exempel 1: ta över tangentbordets avbrott och anropa ROM:en i båda
; versionerna. Skrivet för boken, inte ur någon krets.
;
; Kretsen sitter på $4000. Från BASIC:
;   Z=CALL(16384)   koppla in (I = $40)
;   Z=CALL(16387)   koppla ur (I = 0)
; Inkopplad räknar den tangenttryckningarna och skriver antalet överst
; till höger på skärmen, och går sedan vidare till ROM:ens tangentrutin.

KEYINT: EQU  $031E          ; ROM:ens tangentrutin (båda versionerna)
CASINT: EQU  $0594          ; ROM:ens kassettrutin (båda versionerna)
ANTAL:  EQU  $7FF8          ; räknaren: två bytes i bildminnet som inte syns
HORN:   EQU  $7C23          ; rad 0, kolumn 35

        ORG  $4000

        JP   PA             ; $4000
        JP   AV             ; $4003

; Koppla in: nollställ räknaren och låt I peka på den här sidan.
PA:     LD   HL,0
        LD   (ANTAL),HL
        LD   A,$40
        LD   I,A
        RET

; Koppla ur: ROM:ens egen tabell på $0034 gäller igen.
AV:     XOR  A
        LD   I,A
        RET

        DEFS $4034-$        ; fyll ut till vektortabellen

; Vektortabellen, på samma plats i sidan som ROM:ens på $0034.
        DEFW TANGNT         ; $4034 tangentbordet (vektor $34)
        DEFW CASINT         ; $4036 kassetten (vektor $36), ROM:ens

; Tangentbordets avbrott. Alla register sparas; ROM:ens KEYINT sparar
; själv AF och slutar med EI och RETI.
TANGNT: PUSH AF
        PUSH BC
        PUSH DE
        PUSH HL
        LD   HL,(ANTAL)     ; räkna
        INC  HL
        LD   (ANTAL),HL
        LD   DE,HORN        ; sudda fem tecken i hörnet
        LD   B,5
        LD   A,' '
T1:     LD   (DE),A
        INC  DE
        DJNZ T1
        LD   DE,HORN
        CALL PRNUM          ; HL som decimaltal till (DE)
        POP  HL
        POP  DE
        POP  BC
        POP  AF
        JP   KEYINT         ; vidare till ROM:en

; ROM:ens PRNUM ligger på olika adresser i de två versionerna.
; Den skriver HL som ett decimaltal utan inledande nollor till (DE);
; A, BC, DE, HL och flaggorna ändras.
PRNUM:  CALL VERSEL
        DEFW $1862          ; 11273
        DEFW $1860          ; 9913

; VERSEL hoppar till den första adressen efter CALL i 11273 och till
; den andra i 9913. Byten på $0019 (i RST $18:s JP) skiljer: $BE i
; 11273, $BC i 9913. Inga register ändras, så rutinen får samma
; argument som om den hade anropats direkt, och dess RET går tillbaka
; till den som anropade PRNUM.
VERSEL: EX   (SP),HL        ; HL = tabellen, anroparens HL på stacken
        PUSH AF
        LD   A,($0019)
        CP   $BE            ; 11273?
        JR   Z,V1
        INC  HL             ; 9913: den andra adressen
        INC  HL
V1:     LD   A,(HL)
        INC  HL
        LD   H,(HL)
        LD   L,A            ; HL = rutinens adress
        POP  AF
        EX   (SP),HL        ; tillbaka HL, rutinens adress på stacken
        RET                 ; hoppa dit
