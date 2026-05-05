; Demo of ABC80 ROM calls
; INLINE (0005h) and OUTSTR (000Bh)
;
; * OUTSTR  call 000Bh  HL=text ptr  BC=length  
; String may start with ESC,"=",X+32,Y+32 to position cursor first. 
;
; * INLINE  call 0005h  HL=buffer BC=max chars  
; Reads one keyboard line (terminated by RETURN) into (HL).
; Echoes the line.  Returns with BC = actual length read.  

        INLINE  EQU 0005H  
        OUTSTR  EQU 000BH  

        BUFLEN  EQU 40          ; max input length

        ORG 8000H  

  START:
; ---- Print prompt at column 2, row 5 ----
; OUTSTR with ESC,"=",X+32,Y+32 prefix positions the cursor.  
; X=column, Y=row  (both +32 to avoid control-code clashes)
        LD HL, PROMPT 
        LD BC, PROMPTLEN 
        CALL OUTSTR

; ---- Read one line from keyboard into BUFFER ----  
        LD HL, BUFFER
        LD BC, BUFLEN 
        CALL INLINE
 ; BC now holds the number of characters actually read

 ; ---- Print the response header at column 2, row 7 ----
        LD HL, RESP
        LD DE, RESPLEN
        LD B, D 
        LD C, E
        CALL OUTSTR

 ; ---- Echo the input back at column 10, row 7 ----  
 ; We reuse BC (char count) from INLINE  it is still valid.
 ; First prepend a position prefix so the text lands right. 
        LD HL, ECHOPOS          ; "ESC = X+32 Y+32" header 
        PUSH BC                 ; save input length  
        LD BC, 4
        CALL OUTSTR
        POP BC                  ; restore input length
        LD HL, BUFFER 
        CALL OUTSTR

        RET

  ; ---------------------------------------------------------------------------
  ; Prompt string  ESC,"=",col+32,row+32 positions cursor, then text follows.
  ; Cursor to col=2 (2+32=34), row=5 (5+32=37). 
  PROMPT:
        DB 1BH, "=", 34, 37     ; ESC "=" X+32 Y+32  (col 2, row 5)
        DB "Namn: "
  PROMPTLEN EQU $ - PROMPT

  ; Response label at col 2, row 7
  RESP:  
        DB 1BH, "=", 34, 39     ; col 2, row 7
        DB "Du skrev: "
  RESPLEN EQU $ - RESP

  ; Position prefix for the echo: col 12, row 7 
  ECHOPOS:
        DB 1BH, "=", 44, 39     ; col 12, row 7  
  ; (OUTSTR is called with BC=4 for just this 4-byte prefix)

  ; ---------------------------------------------------------------------------  

  BUFFER: DS BUFLEN             ; input buffer
