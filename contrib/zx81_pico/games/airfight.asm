; ============================================================================
; airfight.asm  —  AIRFIGHT for ZX81 / Pico emulator
;
; Single-player dogfight: player vs AI.
; Controls: Q = rotate left   W = rotate right   SPACE = fire
;
; Memory layout
;   0x8000  game code (this file, ORG 0x8000)
;   0x8200  display file (793 bytes: 0x76 + 24*(32 chars + 0x76))
;
; On entry: D_FILE sysvar at 0x400C is updated to 0x8200.
; The Pico C renderer reads D_FILE each frame and renders our display.
;
; ZX81 keyboard (IN A,(0xFE), A=row-select, active-low result):
;   Row 0xFB: Q(bit0) W(bit1) E(bit2) R(bit3) T(bit4)
;   Row 0x7F: SPACE(bit0)
; ============================================================================

; ---- Constants -------------------------------------------------------------

DFILE       EQU 0x8800      ; display file base (must be past end of game binary)
DFILE_ROW   EQU 33          ; bytes per display row (32 chars + 0x76 HALT)
DFILE_ROWS  EQU 24
DFILE_COLS  EQU 32
D_FILE_LO   EQU 0x400C      ; ZX81 sysvar: D_FILE pointer (lo byte)
D_FILE_HI   EQU 0x400D      ; ZX81 sysvar: D_FILE pointer (hi byte)

SPRITE_W    EQU 3
SPRITE_H    EQU 3
COL_MAX     EQU DFILE_COLS - SPRITE_W    ; 29
ROW_MAX     EQU DFILE_ROWS - SPRITE_H    ; 21

SOLID       EQU 0x80        ; ZX81 inverse-space = solid block (bullets)
SPACE       EQU 0x00        ; ZX81 space = empty
CHECK       EQU 0x08        ; ZX81 char 0x08 = full checkerboard (player body)

AI_FIRE_RATE EQU 60         ; AI fires every N game frames
FRAME_DELAY  EQU 0x6000    ; delay loop count: 0x6000≈6 fps  0x1800≈24 fps

; ---- Variables  (placed just after code; assembled by assembler) -----------
; (Defined as labels into DEFS blocks at end of file)

; ---- Entry point -----------------------------------------------------------

    ORG     0x8000

START:
    XOR     A
    OUT     (0xFD), A
    LD      HL, DFILE
    LD      (D_FILE_LO), HL
    XOR     A
    LD      (SCORE_P), A        ; scores reset on first launch only
    LD      (SCORE_A), A
    CALL    CLEAR_DISPLAY
    CALL    SHOW_TITLE

; ---- Per-round init (re-entered after each game over) ----------------------

GAME_INIT:
    CALL    CLEAR_DISPLAY

    LD      A, 3
    LD      (P_COL), A
    LD      A, 3
    LD      (P_ROW), A
    LD      A, 6
    LD      (P_DIR), A

    LD      A, 26
    LD      (A_COL), A
    LD      A, 18
    LD      (A_ROW), A
    LD      A, 2
    LD      (A_DIR), A
    LD      A, 3
    LD      (A_VEL), A
    LD      A, AI_FIRE_RATE / 2
    LD      (A_FTMR), A

    XOR     A
    LD      (PB_ACT), A
    LD      (AB_ACT), A
    LD      (WINNER), A
    LD      (WINNER_REAL), A
    LD      (KR_QW), A          ; clear key state so edge detection is clean
    LD      (CHAMPION), A

    LD      C, 3
    LD      B, 3
    LD      D, 0
    LD      A, 6
    CALL    DRAW_SPR

    LD      C, 26
    LD      B, 18
    LD      D, 1
    LD      A, 2
    CALL    DRAW_SPR

; ---- Main game loop --------------------------------------------------------

GAME_LOOP:
    ; Frame delay: FRAME_DELAY * 26 T ≈ 160K T per 0x1800 count
    LD      HL, FRAME_DELAY
DELAY:
    DEC     HL
    LD      A, H
    OR      L
    JR      NZ, DELAY

    ; --- Player ---
    ; Erase
    LD      A, (P_COL)
    LD      C, A
    LD      A, (P_ROW)
    LD      B, A
    CALL    ERASE_3X3

    ; Read keyboard → may update P_DIR, spawn player bullet
    CALL    READ_KEYS

    ; Move one step in current direction
    LD      A, (P_DIR)
    CALL    GET_DELTA           ; returns D=dcol E=drow
    LD      A, (P_COL)
    ADD     A, D
    LD      (P_COL), A
    LD      A, (P_ROW)
    ADD     A, E
    LD      (P_ROW), A

    ; Clamp to screen
    LD      A, (P_COL)
    LD      C, A
    LD      A, (P_ROW)
    LD      B, A
    CALL    CLAMP_POS
    LD      A, C
    LD      (P_COL), A
    LD      A, B
    LD      (P_ROW), A

    ; Draw player sprite
    LD      A, (P_COL)
    LD      C, A
    LD      A, (P_ROW)
    LD      B, A
    LD      D, 0
    LD      A, (P_DIR)
    CALL    DRAW_SPR

    ; --- AI ---
    ; Erase
    LD      A, (A_COL)
    LD      C, A
    LD      A, (A_ROW)
    LD      B, A
    CALL    ERASE_3X3

    ; AI logic: steer toward player, fire periodically
    CALL    AI_THINK

    ; Move AI
    LD      A, (A_DIR)
    CALL    GET_DELTA
    LD      A, (A_COL)
    ADD     A, D
    LD      (A_COL), A
    LD      A, (A_ROW)
    ADD     A, E
    LD      (A_ROW), A

    ; Clamp
    LD      A, (A_COL)
    LD      C, A
    LD      A, (A_ROW)
    LD      B, A
    CALL    CLAMP_POS
    LD      A, C
    LD      (A_COL), A
    LD      A, B
    LD      (A_ROW), A

    ; Draw AI sprite
    LD      A, (A_COL)
    LD      C, A
    LD      A, (A_ROW)
    LD      B, A
    LD      D, 1
    LD      A, (A_DIR)
    CALL    DRAW_SPR

    ; --- Bullets ---
    LD      A, (PB_ACT)
    OR      A
    CALL    NZ, UPDT_PB

    LD      A, (AB_ACT)
    OR      A
    CALL    NZ, UPDT_AB

    ; --- Check for game-over set by bullet routines ---
    LD      A, (WINNER)
    CP      0xFF
    JP      NZ, GAME_LOOP

GAME_OVER:
    ; Increment winner's score; set CHAMPION flag if 9 wins reached
    XOR     A
    LD      (CHAMPION), A
    LD      A, (WINNER_REAL)
    OR      A
    JR      Z, GO_INC_AI
    LD      A, (SCORE_P)
    INC     A
    CP      10
    JR      NC, GO_SET_CHAMP    ; was already at 9
    LD      (SCORE_P), A
    CP      9
    JR      Z, GO_SET_CHAMP     ; just reached 9
    JR      GO_CRASH
GO_INC_AI:
    LD      A, (SCORE_A)
    INC     A
    CP      10
    JR      NC, GO_SET_CHAMP
    LD      (SCORE_A), A
    CP      9
    JR      NZ, GO_CRASH
GO_SET_CHAMP:
    LD      A, 1
    LD      (CHAMPION), A

GO_CRASH:
    ; Point B/C at the loser's sprite position
    LD      A, (WINNER_REAL)
    OR      A
    JR      Z, GO_LOSER_P
    LD      A, (A_COL)          ; player won → AI crashed
    LD      C, A
    LD      A, (A_ROW)
    LD      B, A
    JR      GO_FLASH
GO_LOSER_P:
    LD      A, (P_COL)          ; AI won → player crashed
    LD      C, A
    LD      A, (P_ROW)
    LD      B, A

GO_FLASH:
    LD      D, 5                ; 5 flash cycles
GO_FLASH_LOOP:
    PUSH    DE                  ; preserve counter across calls
    LD      A, SOLID
    CALL    FILL_3X3            ; BC=position preserved by FILL_3X3
    LD      HL, 0x5000
GO_FD1:
    DEC     HL
    LD      A, H
    OR      L
    JR      NZ, GO_FD1
    CALL    ERASE_3X3           ; BC=position preserved by ERASE_3X3
    LD      HL, 0x3000
GO_FD2:
    DEC     HL
    LD      A, H
    OR      L
    JR      NZ, GO_FD2
    POP     DE
    DEC     D
    JR      NZ, GO_FLASH_LOOP

    LD      A, (CHAMPION)
    OR      A
    JR      NZ, DO_CHAMPION
    CALL    SHOW_SCORE
    JP      GAME_INIT

DO_CHAMPION:
    CALL    SHOW_CHAMPION
    JP      START

; ============================================================================
; CLEAR_DISPLAY: fill D_FILE with spaces (32 per row) and row HALTs
; Destroys: HL, B, C
; ============================================================================

CLEAR_DISPLAY:
    LD      HL, DFILE
    LD      (HL), 0x76          ; leading HALT
    INC     HL
    LD      B, DFILE_ROWS
CD_ROW:
    LD      C, DFILE_COLS
CD_COL:
    LD      (HL), SPACE
    INC     HL
    DEC     C
    JR      NZ, CD_COL
    LD      (HL), 0x76          ; row-end HALT
    INC     HL
    DJNZ    CD_ROW
    RET

; ============================================================================
; SHOW_TITLE: write controls screen and wait for SPACE key press
; Destroys: AF, BC, DE, HL
; ============================================================================

SHOW_TITLE:
    LD      B, 3
    LD      C, 12
    LD      HL, MSG_TITLE
    CALL    WRITE_MSG
    LD      B, 7
    LD      C, 9
    LD      HL, MSG_KEY_Q
    CALL    WRITE_MSG
    LD      B, 8
    LD      C, 9
    LD      HL, MSG_KEY_W
    CALL    WRITE_MSG
    LD      B, 9
    LD      C, 11
    LD      HL, MSG_KEY_SP
    CALL    WRITE_MSG
    LD      B, 12
    LD      C, 6
    LD      HL, MSG_START
    CALL    WRITE_MSG
    ; If SPACE is already held, wait for release first (debounce)
ST_WAIT_REL:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      NZ, ST_WAIT_REL
    ; Wait for SPACE to be pressed
ST_WAIT_PRESS:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      Z, ST_WAIT_PRESS
    RET

; ============================================================================
; SHOW_SCORE: clear screen, show win/loss + scores, wait for SPACE
; ============================================================================

SHOW_SCORE:
    CALL    CLEAR_DISPLAY

    ; Win/loss banner at row 3, col 12
    LD      A, (WINNER_REAL)
    OR      A
    JR      Z, SS_AI_WIN
    LD      B, 3
    LD      C, 12
    LD      HL, MSG_YOU_WIN
    CALL    WRITE_MSG
    JR      SS_DIGITS
SS_AI_WIN:
    LD      B, 3
    LD      C, 12
    LD      HL, MSG_AI_WINS
    CALL    WRITE_MSG

SS_DIGITS:
    ; "YOU:" at row 8, col 5
    LD      B, 8
    LD      C, 5
    LD      HL, MSG_YOU_LBL
    CALL    WRITE_MSG

    ; Player score digit (inverse) at row 8, col 9
    LD      A, (SCORE_P)
    ADD     A, 0x1C             ; ZX81 digit: '0'=0x1C .. '9'=0x25
    OR      0x80                ; inverse video
    LD      B, 8
    LD      C, 9
    CALL    DRAW_CELL

    ; "AI:" at row 8, col 20
    LD      B, 8
    LD      C, 20
    LD      HL, MSG_AI_LBL
    CALL    WRITE_MSG

    ; AI score digit (inverse) at row 8, col 23
    LD      A, (SCORE_A)
    ADD     A, 0x1C
    OR      0x80
    LD      B, 8
    LD      C, 23
    CALL    DRAW_CELL

    ; "PRESS SPACE" at row 14, col 10
    LD      B, 14
    LD      C, 10
    LD      HL, MSG_PRESS_SP
    CALL    WRITE_MSG

    ; Debounce then wait for SPACE
SS_WAIT_REL:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      NZ, SS_WAIT_REL
SS_WAIT_PRESS:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      Z, SS_WAIT_PRESS
    RET

; ============================================================================
; SHOW_CHAMPION: clear screen, show 9-win champion banner + scores, wait SPACE
; ============================================================================

SHOW_CHAMPION:
    CALL    CLEAR_DISPLAY

    LD      A, (WINNER_REAL)
    OR      A
    JR      Z, SC_AI_CHAMP
    LD      B, 3
    LD      C, 10
    LD      HL, MSG_CHAMP_YOU
    CALL    WRITE_MSG
    JR      SC_CHAMP_DIGITS
SC_AI_CHAMP:
    LD      B, 3
    LD      C, 10
    LD      HL, MSG_CHAMP_AI
    CALL    WRITE_MSG

SC_CHAMP_DIGITS:
    LD      B, 8
    LD      C, 5
    LD      HL, MSG_YOU_LBL
    CALL    WRITE_MSG

    LD      A, (SCORE_P)
    ADD     A, 0x1C
    OR      0x80
    LD      B, 8
    LD      C, 9
    CALL    DRAW_CELL

    LD      B, 8
    LD      C, 20
    LD      HL, MSG_AI_LBL
    CALL    WRITE_MSG

    LD      A, (SCORE_A)
    ADD     A, 0x1C
    OR      0x80
    LD      B, 8
    LD      C, 23
    CALL    DRAW_CELL

    LD      B, 14
    LD      C, 10
    LD      HL, MSG_PRESS_SP
    CALL    WRITE_MSG

SC_WAIT_REL:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      NZ, SC_WAIT_REL
SC_WAIT_PRESS:
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    JR      Z, SC_WAIT_PRESS
    RET

; ============================================================================
; WRITE_MSG: write ZX81-encoded string to display file
; In: B=row, C=col, HL=pointer to string (0xFF terminated)
; Destroys: A, HL, DE, BC
; ============================================================================

; WRITE_MSG: In: B=row, C=col, HL=msg ptr (ZX81 char codes, 0xFF terminated)
; Destroys: A, B, C, D, E, H, L
WRITE_MSG:
WM_LOOP:
    LD      A, (HL)         ; load next char
    CP      0xFF            ; end of string?
    RET     Z
    PUSH    AF              ; save char
    INC     HL
    PUSH    HL              ; save string ptr+1
    CALL    DFILE_ADDR      ; HL = screen cell address (uses B=row, C=col)
    EX      DE, HL          ; DE = screen addr
    POP     HL              ; HL = string ptr
    POP     AF              ; A = char
    LD      (DE), A         ; write char to screen
    INC     C               ; advance column
    JR      WM_LOOP

; ============================================================================
; DFILE_ADDR: compute display file address of cell(C=col, B=row)
; Returns: HL = DFILE+1 + B*33 + C
; Destroys: HL, DE, A  (preserves BC)
; ============================================================================

DFILE_ADDR:
    LD      HL, DFILE + 1
    LD      A, B
    OR      A
    JR      Z, DA_DONE
DA_LOOP:
    LD      DE, DFILE_ROW
    ADD     HL, DE
    DEC     A
    JR      NZ, DA_LOOP
DA_DONE:
    LD      D, 0
    LD      E, C
    ADD     HL, DE
    RET

; ============================================================================
; GET_DELTA: direction A (0-7) → D=dcol, E=drow
; Destroys: HL (preserves BC)
; ============================================================================

GET_DELTA:
    LD      D, 0
    LD      E, A            ; DE = 0x00 : direction
    LD      HL, TBL_DCOL
    ADD     HL, DE
    LD      C, (HL)         ; C = dcol  (keep D=0 so second lookup is correct)

    LD      HL, TBL_DROW
    ADD     HL, DE          ; HL = TBL_DROW + direction  (D still 0)
    LD      E, (HL)         ; E = drow
    LD      D, C            ; D = dcol
    RET

; ============================================================================
; DRAW_SPR: draw 3×3 sprite
; In: C=col, B=row, D=plane (0=player,1=AI), A=direction (0-7)
; Destroys: HL, DE, BC, IX, AF
; ============================================================================

DRAW_SPR:
    ; Compute sprite data pointer: base + dir*9
    PUSH    BC
    PUSH    DE              ; save plane

    LD      H, 0
    LD      L, A            ; L = dir
    ADD     HL, HL          ; *2
    ADD     HL, HL          ; *4
    ADD     HL, HL          ; *8
    LD      E, A
    LD      D, 0
    ADD     HL, DE          ; *9

    POP     DE
    LD      A, D
    OR      A
    JR      NZ, DS_AI
    LD      DE, SPR_PLAYER
    ADD     HL, DE
    JR      DS_GOT
DS_AI:
    LD      DE, SPR_AI
    ADD     HL, DE
DS_GOT:
    PUSH    HL
    POP     IX              ; IX = sprite data ptr (ADD IX,HL is invalid Z80)

    POP     BC
    CALL    DFILE_ADDR      ; HL = screen top-left
    ; Draw 3×3: row 0
    LD      A, (IX+0)
    LD      (HL), A
    INC     HL
    LD      A, (IX+1)
    LD      (HL), A
    INC     HL
    LD      A, (IX+2)
    LD      (HL), A
    ; Advance to col C of next row: +DFILE_ROW-2 = +31
    LD      DE, DFILE_ROW - 2
    ADD     HL, DE
    ; Row 1
    LD      A, (IX+3)
    LD      (HL), A
    INC     HL
    LD      A, (IX+4)
    LD      (HL), A
    INC     HL
    LD      A, (IX+5)
    LD      (HL), A
    ADD     HL, DE
    ; Row 2
    LD      A, (IX+6)
    LD      (HL), A
    INC     HL
    LD      A, (IX+7)
    LD      (HL), A
    INC     HL
    LD      A, (IX+8)
    LD      (HL), A
    RET

; ============================================================================
; ERASE_3X3: write SPACE to 3×3 area at (C=col, B=row)
; ============================================================================

ERASE_3X3:
    PUSH    BC
    CALL    DFILE_ADDR
    POP     BC
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    LD      DE, DFILE_ROW - 2
    ADD     HL, DE
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    ADD     HL, DE
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    INC     HL
    LD      (HL), SPACE
    RET

; ============================================================================
; FILL_3X3: write char A to 3×3 area at (C=col, B=row)
; Preserves BC. Destroys HL, DE, AF.
; ============================================================================

FILL_3X3:
    PUSH    AF
    PUSH    BC
    CALL    DFILE_ADDR
    POP     BC
    POP     AF
    LD      DE, DFILE_ROW - 2
    LD      (HL), A
    INC     HL
    LD      (HL), A
    INC     HL
    LD      (HL), A
    ADD     HL, DE
    LD      (HL), A
    INC     HL
    LD      (HL), A
    INC     HL
    LD      (HL), A
    ADD     HL, DE
    LD      (HL), A
    INC     HL
    LD      (HL), A
    INC     HL
    LD      (HL), A
    RET

; ============================================================================
; CLAMP_POS: wrap C=col within 0..COL_MAX, B=row within 0..ROW_MAX
; Underflow (bit 7 set after subtraction) wraps to max; overflow wraps to 0.
; ============================================================================

CLAMP_POS:
    LD      A, C
    BIT     7, A
    JR      Z, CP_COL_HI
    LD      C, COL_MAX      ; underflow → wrap to far edge
    JR      CP_ROW
CP_COL_HI:
    CP      COL_MAX + 1
    JR      C, CP_ROW
    LD      C, 0            ; overflow → wrap to near edge
CP_ROW:
    LD      A, B
    BIT     7, A
    JR      Z, CP_ROW_HI
    LD      B, ROW_MAX      ; underflow → wrap to far edge
    RET
CP_ROW_HI:
    CP      ROW_MAX + 1
    RET     C
    LD      B, 0            ; overflow → wrap to near edge
    RET

; ============================================================================
; READ_KEYS: read Q/W/SPACE, update P_DIR and spawn bullet
; ============================================================================

READ_KEYS:
    ; Read Q/W row, invert active-low, keep bits 0-1 only
    LD      A, 0xFB
    IN      A, (0xFE)
    CPL
    AND     0x03            ; bit0=Q, bit1=W

    ; Edge detection: only act on the frame a key is first pressed
    ; KR_QW holds the previous frame's state
    LD      C, A            ; C = current state
    LD      A, (KR_QW)      ; A = previous state
    CPL                     ; A = NOT previous
    AND     C               ; A = newly pressed bits (0→1 transitions)
    PUSH    AF              ; stash newly-pressed bits
    LD      A, C
    LD      (KR_QW), A      ; save current as next frame's previous (only A can write to (nn))
    POP     AF              ; restore newly-pressed bits into A

    ; Q (bit 0) = rotate left (counter-clockwise) → increment direction
    ; Direction encoding: 0=up 1=up-left 2=left 3=down-left 4=down 5=down-right 6=right 7=up-right
    ; Incrementing goes counter-clockwise (right→up-right→up→...) = left turn
    BIT     0, A
    JR      Z, RK_W
    LD      A, (P_DIR)
    INC     A
    AND     0x07
    LD      (P_DIR), A
    JR      RK_FIRE

RK_W:
    ; W (bit 1) = rotate right (clockwise) → decrement direction
    BIT     1, A
    JR      Z, RK_FIRE
    LD      A, (P_DIR)
    DEC     A
    AND     0x07
    LD      (P_DIR), A

RK_FIRE:
    ; SPACE = fire (only if no active player bullet)
    LD      A, 0x7F
    IN      A, (0xFE)
    CPL
    AND     0x01
    LD      (KR_SP), A
    BIT     0, A
    RET     Z               ; SPACE not pressed
    LD      A, (PB_ACT)
    OR      A
    RET     NZ              ; bullet already in flight
    ; Spawn bullet at sprite centre (col+1, row+1)
    LD      A, (P_COL)
    INC     A
    LD      (PB_COL), A
    LD      A, (P_ROW)
    INC     A
    LD      (PB_ROW), A
    LD      A, (P_DIR)
    LD      (PB_DIR), A
    LD      A, 15
    LD      (PB_LIFE), A
    LD      A, 1
    LD      (PB_ACT), A
    RET

; ============================================================================
; AI_THINK: steer AI toward player, fire when timer expires
; ============================================================================

AI_THINK:
    ; --- Steering ---
    ; Compute sign of (a_col - p_col) and (a_row - p_row)
    LD      A, (A_VEL)
    DEC     A
    LD      (A_VEL), A
    JP      NZ, AI_FIRE_CHK

    LD      A, 3
    LD      (A_VEL), A

    ; Desired column direction: negative dcol = go left, positive = go right
    LD      A, (P_COL)
    LD      B, A
    LD      A, (A_COL)
    SUB     B               ; A = ai_col - p_col
    ; If A=0 no horizontal adjustment; if negative player is right of AI (go right=+);
    ; if positive player is left (go left=-)
    LD      C, 0            ; C = col_want: 0=neutral 1=left 2=right
    OR      A
    JR      Z, AI_ROW_SGN
    BIT     7, A
    JR      NZ, AI_WANT_RIGHT
    LD      C, 1            ; positive diff → go left (dir 2)
    JR      AI_ROW_SGN
AI_WANT_RIGHT:
    LD      C, 2            ; negative diff → go right (dir 6)

AI_ROW_SGN:
    LD      A, (P_ROW)
    LD      B, A
    LD      A, (A_ROW)
    SUB     B               ; A = ai_row - p_row
    LD      E, 0            ; E = row_want: 0=neutral 1=up 2=down
    OR      A
    JR      Z, AI_TURN
    BIT     7, A
    JR      NZ, AI_WANT_DOWN
    LD      E, 1            ; positive diff → go up (dir 0)
    JR      AI_TURN
AI_WANT_DOWN:
    LD      E, 2            ; negative diff → go down (dir 4)

AI_TURN:
    ; Map (C=col_want, E=row_want) to desired direction
    ; Then rotate AI one step toward it
    LD      A, C
    OR      E
    JR      Z, AI_FIRE_CHK  ; no preference → keep current dir

    ; Simplified 8-way: derive desired dir from C,E
    ; C: 0=neutral 1=left(dir2) 2=right(dir6)
    ; E: 0=neutral 1=up(dir0)   2=down(dir4)
    LD      A, E
    OR      A
    JR      Z, AI_PURE_COL
    LD      A, C
    OR      A
    JR      Z, AI_PURE_ROW
    ; Diagonal: combine
    LD      A, E
    CP      1               ; up?
    JR      NZ, AI_DIAG_DN
    ; Up half
    LD      A, C
    CP      1
    JR      Z, AI_TARGET_1  ; up-left
    JP      AI_TARGET_7     ; up-right
AI_DIAG_DN:
    LD      A, C
    CP      1
    JR      Z, AI_TARGET_3  ; down-left
    JP      AI_TARGET_5     ; down-right
AI_PURE_COL:
    LD      A, C
    CP      1
    JR      Z, AI_TARGET_2  ; left
    JP      AI_TARGET_6     ; right
AI_PURE_ROW:
    LD      A, E
    CP      1
    JR      Z, AI_TARGET_0  ; up
    JP      AI_TARGET_4     ; down

AI_TARGET_0:
    LD      A, 0
    JR      AI_DO_TURN
AI_TARGET_1:
    LD      A, 1
    JR      AI_DO_TURN
AI_TARGET_2:
    LD      A, 2
    JR      AI_DO_TURN
AI_TARGET_3:
    LD      A, 3
    JR      AI_DO_TURN
AI_TARGET_4:
    LD      A, 4
    JR      AI_DO_TURN
AI_TARGET_5:
    LD      A, 5
    JR      AI_DO_TURN
AI_TARGET_6:
    LD      A, 6
    JR      AI_DO_TURN
AI_TARGET_7:
    LD      A, 7

AI_DO_TURN:
    ; A = desired direction; rotate A_DIR one step toward it
    LD      B, A            ; B = desired
    LD      A, (A_DIR)
    CP      B
    JR      Z, AI_FIRE_CHK  ; already aligned
    ; Turn right: (A_DIR+1)%8; if that equals desired, take it; else turn left
    LD      C, A
    INC     C
    LD      A, C
    AND     0x07
    CP      B
    JR      Z, AI_TURN_RIGHT
    ; Turn left
    LD      A, (A_DIR)
    DEC     A
    AND     0x07
    LD      (A_DIR), A
    JR      AI_FIRE_CHK
AI_TURN_RIGHT:
    LD      (A_DIR), A

AI_FIRE_CHK:
    ; Fire timer
    LD      A, (A_FTMR)
    INC     A
    LD      (A_FTMR), A
    CP      AI_FIRE_RATE
    RET     C               ; not time yet
    XOR     A
    LD      (A_FTMR), A
    LD      A, (AB_ACT)
    OR      A
    RET     NZ              ; bullet in flight
    ; Spawn AI bullet
    LD      A, (A_COL)
    INC     A
    LD      (AB_COL), A
    LD      A, (A_ROW)
    INC     A
    LD      (AB_ROW), A
    LD      A, (A_DIR)
    LD      (AB_DIR), A
    LD      A, 15
    LD      (AB_LIFE), A
    LD      A, 1
    LD      (AB_ACT), A
    RET

; ============================================================================
; UPDT_PB: update player bullet (called only when PB_ACT != 0)
; ============================================================================

UPDT_PB:
    ; Erase bullet at current position
    LD      A, (PB_COL)
    LD      C, A
    LD      A, (PB_ROW)
    LD      B, A
    CALL    ERASE_CELL

    ; Decrement life
    LD      A, (PB_LIFE)
    DEC     A
    LD      (PB_LIFE), A
    JR      Z, PB_EXPIRE

    ; Move 2 steps (faster than planes)
    LD      A, (PB_DIR)
    CALL    GET_DELTA       ; D=dcol, E=drow

    LD      A, (PB_COL)
    ADD     A, D
    ADD     A, D
    AND     0x1F            ; wrap col mod 32
    LD      (PB_COL), A

    LD      A, (PB_ROW)
    ADD     A, E
    ADD     A, E
    AND     0x1F
    CP      DFILE_ROWS
    JR      C, PB_ROW_OK
    SUB     DFILE_ROWS
PB_ROW_OK:
    LD      (PB_ROW), A

    ; Collision: |pb_col - a_col| < 3 AND |pb_row - a_row| < 3
    LD      A, (PB_COL)
    LD      C, A
    LD      A, (A_COL)
    SUB     C
    CALL    ABS_A
    CP      3
    JR      NC, PB_NO_HIT

    LD      A, (PB_ROW)
    LD      C, A
    LD      A, (A_ROW)
    SUB     C
    CALL    ABS_A
    CP      3
    JR      NC, PB_NO_HIT

    ; Hit! Player wins.
    XOR     A
    LD      (PB_ACT), A
    LD      A, 1
    LD      (WINNER_REAL), A
    LD      A, 0xFF
    LD      (WINNER), A     ; signal game over
    RET

PB_NO_HIT:
    ; Draw bullet
    LD      A, (PB_COL)
    LD      C, A
    LD      A, (PB_ROW)
    LD      B, A
    LD      A, SOLID
    CALL    DRAW_CELL
    RET

PB_EXPIRE:
    XOR     A
    LD      (PB_ACT), A
    RET

; ============================================================================
; UPDT_AB: update AI bullet
; ============================================================================

UPDT_AB:
    LD      A, (AB_COL)
    LD      C, A
    LD      A, (AB_ROW)
    LD      B, A
    CALL    ERASE_CELL

    LD      A, (AB_LIFE)
    DEC     A
    LD      (AB_LIFE), A
    JR      Z, AB_EXPIRE

    LD      A, (AB_DIR)
    CALL    GET_DELTA

    LD      A, (AB_COL)
    ADD     A, D
    ADD     A, D
    AND     0x1F
    LD      (AB_COL), A

    LD      A, (AB_ROW)
    ADD     A, E
    ADD     A, E
    AND     0x1F
    CP      DFILE_ROWS
    JR      C, AB_ROW_OK
    SUB     DFILE_ROWS
AB_ROW_OK:
    LD      (AB_ROW), A

    ; Collision with player
    LD      A, (AB_COL)
    LD      C, A
    LD      A, (P_COL)
    SUB     C
    CALL    ABS_A
    CP      3
    JR      NC, AB_NO_HIT

    LD      A, (AB_ROW)
    LD      C, A
    LD      A, (P_ROW)
    SUB     C
    CALL    ABS_A
    CP      3
    JR      NC, AB_NO_HIT

    ; AI wins.
    XOR     A
    LD      (AB_ACT), A
    LD      A, 0
    LD      (WINNER_REAL), A
    LD      A, 0xFF
    LD      (WINNER), A
    RET

AB_NO_HIT:
    LD      A, (AB_COL)
    LD      C, A
    LD      A, (AB_ROW)
    LD      B, A
    LD      A, SOLID
    CALL    DRAW_CELL
    RET

AB_EXPIRE:
    XOR     A
    LD      (AB_ACT), A
    RET

; ============================================================================
; ERASE_CELL / DRAW_CELL: single cell at (C=col, B=row)
; DRAW_CELL: A = char to write
; ============================================================================

ERASE_CELL:
    PUSH    BC
    CALL    DFILE_ADDR
    POP     BC
    LD      (HL), SPACE
    RET

DRAW_CELL:
    PUSH    AF
    PUSH    BC
    CALL    DFILE_ADDR
    POP     BC
    POP     AF
    LD      (HL), A
    RET

; ============================================================================
; ABS_A: A = |A|  (treats A as signed byte)
; ============================================================================

ABS_A:
    BIT     7, A
    RET     Z
    NEG
    RET

; ============================================================================
; Data tables
; ============================================================================

; Movement delta: dcol (signed), indexed by direction 0-7
TBL_DCOL:
    DEFB    0, 0xFF, 0xFF, 0xFF, 0, 1, 1, 1
;   dir:    0    1     2     3   4  5  6  7

; Movement delta: drow (signed), indexed by direction 0-7
TBL_DROW:
    DEFB    0xFF, 0xFF, 0, 1, 1, 1, 0, 0xFF
;   dir:      0     1  2  3  4  5  6     7

; ---- Sprite bitmaps --------------------------------------------------------
; Row-major, 3 rows × 3 cols = 9 bytes per direction.
; 0x00=space 0x80=solid block.
; 8 directions × 9 bytes = 72 bytes per sprite set.

SPR_PLAYER:
    ; dir 0 (up):        . C . / C C C / . . .
    DEFB SPACE, CHECK, SPACE, CHECK, CHECK, CHECK, SPACE, SPACE, SPACE
    ; dir 1 (up-left):   C . C / . C . / C . .
    DEFB CHECK, SPACE, CHECK, SPACE, CHECK, SPACE, CHECK, SPACE, SPACE
    ; dir 2 (left):      . C . / C C . / . C .
    DEFB SPACE, CHECK, SPACE, CHECK, CHECK, SPACE, SPACE, CHECK, SPACE
    ; dir 3 (down-left): C . . / . C . / C . C
    DEFB CHECK, SPACE, SPACE, SPACE, CHECK, SPACE, CHECK, SPACE, CHECK
    ; dir 4 (down):      . . . / C C C / . C .
    DEFB SPACE, SPACE, SPACE, CHECK, CHECK, CHECK, SPACE, CHECK, SPACE
    ; dir 5 (down-right): . . C / . C . / C . C
    DEFB SPACE, SPACE, CHECK, SPACE, CHECK, SPACE, CHECK, SPACE, CHECK
    ; dir 6 (right):     . C . / . C C / . C .
    DEFB SPACE, CHECK, SPACE, SPACE, CHECK, CHECK, SPACE, CHECK, SPACE
    ; dir 7 (up-right):  C . C / . C . / . . C
    DEFB CHECK, SPACE, CHECK, SPACE, CHECK, SPACE, SPACE, SPACE, CHECK

SPR_AI:
    ; Same shape as SPR_PLAYER but filled with SOLID instead of CHECK
    ; dir 0 (up):        . S . / S S S / . . .
    DEFB SPACE, SOLID, SPACE, SOLID, SOLID, SOLID, SPACE, SPACE, SPACE
    ; dir 1 (up-left):   S . S / . S . / S . .
    DEFB SOLID, SPACE, SOLID, SPACE, SOLID, SPACE, SOLID, SPACE, SPACE
    ; dir 2 (left):      . S . / S S . / . S .
    DEFB SPACE, SOLID, SPACE, SOLID, SOLID, SPACE, SPACE, SOLID, SPACE
    ; dir 3 (down-left): S . . / . S . / S . S
    DEFB SOLID, SPACE, SPACE, SPACE, SOLID, SPACE, SOLID, SPACE, SOLID
    ; dir 4 (down):      . . . / S S S / . S .
    DEFB SPACE, SPACE, SPACE, SOLID, SOLID, SOLID, SPACE, SOLID, SPACE
    ; dir 5 (down-right): . . S / . S . / S . S
    DEFB SPACE, SPACE, SOLID, SPACE, SOLID, SPACE, SOLID, SPACE, SOLID
    ; dir 6 (right):     . S . / . S S / . S .
    DEFB SPACE, SOLID, SPACE, SPACE, SOLID, SOLID, SPACE, SOLID, SPACE
    ; dir 7 (up-right):  S . S / . S . / . . S
    DEFB SOLID, SPACE, SOLID, SPACE, SOLID, SPACE, SPACE, SPACE, SOLID

; ---- Game-over messages (ZX81 char codes, 0xFF terminated) -----------------
; ZX81: space=0x00, A=0x26 B=0x27 ... Z=0x3F (A+25)
; Y=0x3E O=0x34 U=0x3A  W=0x3C I=0x2E N=0x33  A=0x26 I=0x2E  S=0x38

MSG_YOU_WIN:
    DEFB 0x3E, 0x34, 0x3A, 0x00, 0x3C, 0x2E, 0x33  ; Y O U   W I N
    DEFB 0xFF

MSG_AI_WINS:
    DEFB 0x26, 0x2E, 0x00, 0x3C, 0x2E, 0x33, 0x38  ; A I   W I N S
    DEFB 0xFF

; ZX81 codes: A=0x26..Z=0x3F, 0=0x1C..9=0x25, space=0x00, '='=0x14
; Inverse video: OR 0x80

MSG_TITLE:
    DEFB 0xA6, 0xAE, 0xB7, 0xAB, 0xAE, 0xAC, 0xAD, 0xB9  ; AIRFIGHT (inverse)
    DEFB 0xFF

MSG_KEY_Q:
    DEFB 0x36, 0x14, 0x37, 0x34, 0x39, 0x26, 0x39, 0x2A, 0x00, 0x31, 0x2A, 0x2B, 0x39  ; Q=ROTATE LEFT
    DEFB 0xFF

MSG_KEY_W:
    DEFB 0x3C, 0x14, 0x37, 0x34, 0x39, 0x26, 0x39, 0x2A, 0x00, 0x37, 0x2E, 0x2C, 0x2D, 0x39  ; W=ROTATE RIGHT
    DEFB 0xFF

MSG_KEY_SP:
    DEFB 0x38, 0x35, 0x26, 0x28, 0x2A, 0x14, 0x2B, 0x2E, 0x37, 0x2A  ; SPACE=FIRE
    DEFB 0xFF

MSG_START:
    DEFB 0x35, 0x37, 0x2A, 0x38, 0x38, 0x00, 0x38, 0x35, 0x26, 0x28, 0x2A, 0x00, 0x39, 0x34, 0x00, 0x38, 0x39, 0x26, 0x37, 0x39  ; PRESS SPACE TO START
    DEFB 0xFF

MSG_YOU_LBL:
    DEFB 0x3E, 0x34, 0x3A, 0x0E    ; Y O U :
    DEFB 0xFF

MSG_AI_LBL:
    DEFB 0x26, 0x2E, 0x0E           ; A I :
    DEFB 0xFF

MSG_PRESS_SP:
    DEFB 0x35, 0x37, 0x2A, 0x38, 0x38, 0x00, 0x38, 0x35, 0x26, 0x28, 0x2A  ; PRESS SPACE
    DEFB 0xFF

; ZX81 inverse: OR 0x80. Y=0xBE O=0xB4 U=0xBA C=0xA8 H=0xAD A=0xA6 M=0xB2 P=0xB5 I=0xAE N=0xB3

MSG_CHAMP_YOU:
    ; "YOU CHAMPION" (12 chars) — col 10 centres in 32
    DEFB 0xBE, 0xB4, 0xBA, 0x00, 0xA8, 0xAD, 0xA6, 0xB2, 0xB5, 0xAE, 0xB4, 0xB3
    DEFB 0xFF

MSG_CHAMP_AI:
    ; "AI CHAMPION" (11 chars) — col 10 centres in 32
    DEFB 0xA6, 0xAE, 0x00, 0xA8, 0xAD, 0xA6, 0xB2, 0xB5, 0xAE, 0xB4, 0xB3
    DEFB 0xFF

; ---- Game state variables --------------------------------------------------

P_COL:      DEFB 0
P_ROW:      DEFB 0
P_DIR:      DEFB 0

A_COL:      DEFB 0
A_ROW:      DEFB 0
A_DIR:      DEFB 0
A_VEL:      DEFB 0
A_FTMR:     DEFB 0

PB_ACT:     DEFB 0
PB_COL:     DEFB 0
PB_ROW:     DEFB 0
PB_DIR:     DEFB 0
PB_LIFE:    DEFB 0

AB_ACT:     DEFB 0
AB_COL:     DEFB 0
AB_ROW:     DEFB 0
AB_DIR:     DEFB 0
AB_LIFE:    DEFB 0

KR_QW:      DEFB 0
KR_SP:      DEFB 0

WINNER:     DEFB 0      ; 0xFF = game over sentinel
WINNER_REAL: DEFB 0     ; 1 = player won, 0 = AI won

SCORE_P:    DEFB 0      ; player win count (0-9)
SCORE_A:    DEFB 0      ; AI win count (0-9)

CHAMPION:   DEFB 0      ; 1 = 9-win tournament completed this round

    END
