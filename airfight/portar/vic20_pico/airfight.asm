; =============================================================================
; airfight.asm  -  AIRFIGHT for VIC-20 emulator on Pico 2W
; =============================================================================
;
; A one-player-vs-AI dogfight arcade game running as native 6502 machine code
; on a software VIC-20 emulator built for the Raspberry Pi Pico 2W.
;
; GAME OVERVIEW
; -------------
; The player (cyan) and an AI opponent (yellow) fly on a 22x23 character-cell
; grid.  A session is 10 rounds; each round ends when one plane is hit.
; Planes wrap around all four edges.  After 10 rounds the final score is shown
; and the player's kill count is entered into a persistent top-5 high score
; table (RAM, cleared on power-off).
;
; SESSION STRUCTURE
;   - Each round: one hit ends the round.
;   - Game speed increases after every round (delay $A0 -> $90 -> ... -> $20).
;   - After round 10: session over -> optional name entry -> high score table.
;   - Scores reset between sessions; high score table persists.
;
; CONTROLS (typed in any USB serial terminal at 115200 baud)
;   A     = rotate CW
;   D     = rotate CCW
;   SPACE = fire
;   (during name entry: A/D cycle the current letter, SPACE confirms it)
;
; TECHNICAL ARCHITECTURE
; ----------------------
; The program runs without the KERNAL or BASIC ROM.  The 6502 reset vector is
; patched at run-time (in memory.c) to point to $1000 so execution starts here.
; Interrupts are disabled (SEI) for the entire game; no IRQ handler is present.
;
; Keyboard input reaches the game via the KERNAL ring buffer at $C6/$0277.
; The Pico's USB-CDC keyboard driver (vic_kbd.c) injects ASCII bytes there
; using inject_petscii().  The game polls that buffer every frame in READ_KEYS.
;
; CUSTOM CHARACTER SET
; --------------------
; VIC register $9005 (char-base selector, lower nibble) is written with 7 to
; point the VIC chip at RAM block 7 ($1C00).  INIT_CHARS copies 88 bytes
; (11 chars x 8 rows) of bitmap data from CHAR_DATA to $1C00 at start-up.
;
;   Char  0  blank
;   Chars 1-8  plane shapes for the 8 directions (dir+1 = screen code)
;   Char  9  bullet (small dot)
;   Char 10  explosion (X pattern)
;
; Direction encoding:  0=up  1=up-left  2=left  3=down-left
;                      4=down  5=down-right  6=right  7=up-right
;
; Rotation: CW  = dir = (dir+1) & 7
;           CCW = dir = (dir-1) & 7
;
; DISPLAY
; -------
; Screen RAM: $1E00, colour RAM: $9400.
; Colour RAM was initialised to 1 (white) at start-up for the whole screen.
; Per-sprite colour is written directly to the colour RAM cell on each DRAW_SPR.
; During the score/high-score screens $9005 is set to 0 (ROM charset) and the
; screen is cleared with $20 (ROM space); $9005 is restored to 7 before the
; next round starts.
;
; MOVEMENT & WRAPPING
; --------------------
; DX/DY tables hold signed deltas (-1/0/+1) stored as two's-complement bytes.
; Wrapping uses BPL (sign-bit check) + add/subtract COLS/ROWS, matching the
; technique used for bullets, to avoid the unsigned-compare bug that would
; send planes to column 0 when moving left off the left edge.
; Planes move every 2nd frame (P0_MV/P1_MV counters); they rotate every 3rd
; frame that a turn key is held.
;
; BULLETS
; -------
; Each plane has one bullet (PB for P0, QB for P1).  A bullet advances 2 cells
; per frame along the firing direction, with a collision check after each step
; to prevent tunnelling through a single-cell target.  Bullet lifetime is
; 20 frames; when it expires the bullet simply disappears.
;
; AI OPPONENT
; -----------
; Each frame AI_MOVE computes the desired direction from a 3x3 sign table
; (AI_DIR_TBL, indexed by sign(dx) * 3 + sign(dy)) and rotates P1 one step
; toward it.  It fires when the angular error is <= 1 step AND the Manhattan
; distance to the player is < 12 cells.
;
; HIGH SCORE TABLE
; ----------------
; 5 entries stored at $0300 (page 3, low RAM).
; Each entry: [score_byte, name0, name1, name2] (4 bytes, score first).
; Entries are kept sorted highest-first.  Initialised to score=0, name="---".
; Only entries with score > 0 prompt name entry.
; The name-entry screen cycles A-Z with A/D keys (ROM screen codes 1-26).
;
; ZERO-PAGE MAP ($10..$68)
;   $10-$16  P0 state: COL, ROW, DIR, VEL, old-COL, old-ROW, old-DIR
;   $17-$1C  P0 bullet: COL, ROW, DIR, LIF, old-COL, old-ROW
;   $20-$26  P1 state  (same layout as P0)
;   $27-$2C  P1 bullet (same layout as P0 bullet)
;   $40-$45  key flags: P0 CCW, CW, fire; P1 CCW, CW, fire
;   $50-$51  GAMEON (0=game over), WINNER
;   $52-$5D  scratch: CUR_N, SCR ptr, TMP_C/R/CH/A/S0/S1, COL ptr, CUR_COL
;   $5E-$5F  P0_SCORE, P1_SCORE
;   $60-$61  P0_MV, P1_MV  (movement throttle counters)
;   $62-$63  GAME_CNT, SPEED_DEL
;   $65-$68  NAME_BUF0/1/2, NAME_POS
;
; BUILD
;   python3 asm6502/assembler.py airfight.asm -o build/airfight.prg
;   (then regenerate include/prg_airfight.h and run build_and_flash.sh)
; =============================================================================

; ---- Hardware addresses --------------------------------------------------

SCREEN  EQU $1E00
SCR2    EQU $1F00
CHRSET  EQU $1C00
COLRAM  EQU $9400
COL2    EQU $9500

VIC9005 EQU $9005
VIC900F EQU $900F

KBUF_N  EQU $00C6
KBUF    EQU $0277

; ---- Display constants ---------------------------------------------------

COLS    EQU 22
ROWS    EQU 23

SPR_C0  EQU 0
SPR_C1  EQU 21
SPR_R0  EQU 0
SPR_R1  EQU 22

CH_BL   EQU 0
CH_BUL  EQU 9
CH_EXP  EQU 10

COL_P0  EQU 3
COL_P1  EQU 7

; ---- Zero-page layout ---------------------------------------------------

P0_COL  EQU $10
P0_ROW  EQU $11
P0_DIR  EQU $12
P0_VEL  EQU $13
P0_OC   EQU $14
P0_OR   EQU $15
P0_OD   EQU $16
PB_COL  EQU $17
PB_ROW  EQU $18
PB_DIR  EQU $19
PB_LIF  EQU $1A
PB_OC   EQU $1B
PB_OR   EQU $1C

P1_COL  EQU $20
P1_ROW  EQU $21
P1_DIR  EQU $22
P1_VEL  EQU $23
P1_OC   EQU $24
P1_OR   EQU $25
P1_OD   EQU $26
QB_COL  EQU $27
QB_ROW  EQU $28
QB_DIR  EQU $29
QB_LIF  EQU $2A
QB_OC   EQU $2B
QB_OR   EQU $2C

KEY_CL  EQU $40
KEY_CR  EQU $41
KEY_CF  EQU $42
KEY_AL  EQU $43
KEY_AR  EQU $44
KEY_AF  EQU $45

GAMEON  EQU $50
WINNER  EQU $51
CUR_N   EQU $52
SCR_LO  EQU $53
SCR_HI  EQU $54
TMP_C   EQU $55
TMP_R   EQU $56
TMP_CH  EQU $57
TMP_A   EQU $58
TMP_S0  EQU $59
TMP_S1  EQU $5A
COL_LO  EQU $5B
COL_HI  EQU $5C
CUR_COL EQU $5D
P0_SCORE EQU $5E
P1_SCORE EQU $5F
P0_MV   EQU $60
P1_MV   EQU $61
GAME_CNT  EQU $62
SPEED_DEL EQU $63
NAME_BUF0 EQU $65
NAME_BUF1 EQU $66
NAME_BUF2 EQU $67
NAME_POS  EQU $68

HS_TABLE  EQU $0300   ; 5 entries x 4 bytes: [score, name0, name1, name2]

; =========================================================================
; PROGRAM START
; =========================================================================

    ORG $1000

START:
    SEI
    JSR INIT_CHARS
    JSR INIT_HS
    LDA #7
    STA VIC9005
    LDA #$00
    STA VIC900F

    LDX #0
    LDA #1
CFIL:
    STA COLRAM,X
    STA COL2,X
    INX
    BNE CFIL

    LDA #0
    STA P0_SCORE
    STA P1_SCORE
    STA GAME_CNT
    LDA #$A0
    STA SPEED_DEL

; ---- RESTART ------------------------------------------------------------

RESTART:
    JSR INIT_GAME
    JSR INIT_SCREEN
    LDA #0
    STA CUR_N
    JSR DRAW_SPR
    LDA #1
    STA CUR_N
    JSR DRAW_SPR
    LDA #10
    JSR LONG_DELAY

; ---- GAME LOOP ----------------------------------------------------------

GAME_LOOP:
    LDY SPEED_DEL
DELY:
    LDX #$FF
DELX:
    DEX
    BNE DELX
    DEY
    BNE DELY

    LDA GAMEON
    BEQ GAME_OVER_LOOP

    JSR READ_KEYS
    JSR AI_MOVE

    LDA #0
    STA CUR_N
    JSR UPDATE_PLAYER
    LDA #1
    STA CUR_N
    JSR UPDATE_PLAYER

    LDA PB_LIF
    BEQ SKIP_PB
    JSR UPDATE_PB
SKIP_PB:
    LDA QB_LIF
    BEQ SKIP_QB
    JSR UPDATE_QB
SKIP_QB:
    JMP GAME_LOOP

GAME_OVER_LOOP:
    JSR SHOW_SCORES
    INC GAME_CNT
    ; Speed up after every round, floor at $20
    LDA SPEED_DEL
    SEC
    SBC #$10
    CMP #$20
    BCS GE_SET_SPD
    LDA #$20
GE_SET_SPD:
    STA SPEED_DEL
    ; After 10 rounds end the session
    LDA GAME_CNT
    CMP #10
    BCC GE_CONTINUE
    JMP SESSION_OVER
GE_CONTINUE:
    JMP RESTART

; =========================================================================
; LONG_DELAY  -  A = number of ~0.3 s chunks
; =========================================================================

LONG_DELAY:
    STA TMP_A
LD_OUT:
    LDY #$FF
LD_DELY:
    LDX #$FF
LD_DELX:
    DEX
    BNE LD_DELX
    DEY
    BNE LD_DELY
    DEC TMP_A
    BNE LD_OUT
    RTS

; =========================================================================
; DRAW_EXP  -  draw CH_EXP at (TMP_C, TMP_R) using CUR_COL
; =========================================================================

DRAW_EXP:
    JSR XY_TO_SCR
    LDA SCR_LO
    STA COL_LO
    LDA SCR_HI
    CLC
    ADC #$76
    STA COL_HI
    LDA #CH_EXP
    LDY #0
    STA (SCR_LO),Y
    LDA CUR_COL
    STA (COL_LO),Y
    RTS

; =========================================================================
; INIT_CHARS  -  copy 88 bytes (11 chars x 8) to $1C00
; =========================================================================

INIT_CHARS:
    LDX #87
IC_LP:
    LDA CHAR_DATA,X
    STA CHRSET,X
    DEX
    BPL IC_LP
    RTS

; =========================================================================
; INIT_GAME
; =========================================================================

INIT_GAME:
    LDA #1
    STA P0_COL
    LDA #5
    STA P0_ROW
    LDA #6
    STA P0_DIR
    LDA #3
    STA P0_VEL
    LDA P0_COL
    STA P0_OC
    LDA P0_ROW
    STA P0_OR
    LDA P0_DIR
    STA P0_OD

    LDA #18
    STA P1_COL
    LDA #16
    STA P1_ROW
    LDA #2
    STA P1_DIR
    LDA #3
    STA P1_VEL
    LDA P1_COL
    STA P1_OC
    LDA P1_ROW
    STA P1_OR
    LDA P1_DIR
    STA P1_OD

    LDA #0
    STA PB_LIF
    STA QB_LIF
    LDA #2
    STA P0_MV
    STA P1_MV
    LDA #1
    STA GAMEON
    LDA #0
    STA WINNER
    RTS

; =========================================================================
; INIT_SCREEN  -  blank all cells
; =========================================================================

INIT_SCREEN:
    LDA #CH_BL
    LDX #0
IS_CLR:
    STA SCREEN,X
    STA SCR2,X
    INX
    BNE IS_CLR
    RTS

; =========================================================================
; ROW_TO_SCR  -  SCR_LO/HI = SCREEN + A*22
; =========================================================================

ROW_TO_SCR:
    TAX
    LDA ROW_LO,X
    STA SCR_LO
    LDA ROW_HI,X
    CLC
    ADC #$1E
    STA SCR_HI
    RTS

; =========================================================================
; XY_TO_SCR  -  SCR_LO/HI = SCREEN + TMP_R*22 + TMP_C
; =========================================================================

XY_TO_SCR:
    LDA TMP_R
    JSR ROW_TO_SCR
    LDA SCR_LO
    CLC
    ADC TMP_C
    STA SCR_LO
    BCC XY_OK
    INC SCR_HI
XY_OK:
    RTS

; =========================================================================
; DRAW_SPR  -  draw 1-cell directional plane char for CUR_N
; =========================================================================

DRAW_SPR:
    LDA CUR_N
    BEQ DS_P0
    LDA P1_COL
    STA TMP_C
    LDA P1_ROW
    STA TMP_R
    LDA P1_DIR
    CLC
    ADC #1
    STA TMP_CH
    LDA #COL_P1
    STA CUR_COL
    JMP DS_WRITE
DS_P0:
    LDA P0_COL
    STA TMP_C
    LDA P0_ROW
    STA TMP_R
    LDA P0_DIR
    CLC
    ADC #1
    STA TMP_CH
    LDA #COL_P0
    STA CUR_COL
DS_WRITE:
    JSR XY_TO_SCR
    LDA SCR_LO
    STA COL_LO
    LDA SCR_HI
    CLC
    ADC #$76
    STA COL_HI
    LDA TMP_CH
    LDY #0
    STA (SCR_LO),Y
    LDA CUR_COL
    STA (COL_LO),Y
    RTS

; =========================================================================
; ERASE_SPR  -  blank previous cell of CUR_N
; =========================================================================

ERASE_SPR:
    LDA CUR_N
    BEQ ER_P0
    LDA P1_OC
    STA TMP_C
    LDA P1_OR
    STA TMP_R
    JMP ER_WRITE
ER_P0:
    LDA P0_OC
    STA TMP_C
    LDA P0_OR
    STA TMP_R
ER_WRITE:
    JSR XY_TO_SCR
    LDA #CH_BL
    LDY #0
    STA (SCR_LO),Y
    RTS

; =========================================================================
; UPDATE_PLAYER
; =========================================================================

UPDATE_PLAYER:
    LDA CUR_N
    BNE UPD_P1
    JMP UPD_P0

; ---- Player 1 -----------------------------------------------------------
UPD_P1:
    LDA P1_COL
    STA P1_OC
    LDA P1_ROW
    STA P1_OR
    LDA P1_DIR
    STA P1_OD
    ; CCW
    LDA KEY_AL
    BEQ UP1_NCCW
    DEC P1_VEL
    BNE UP1_NCCW
    LDA #3
    STA P1_VEL
    LDA P1_DIR
    SEC
    SBC #1
    AND #$07
    STA P1_DIR
UP1_NCCW:
    ; CW
    LDA KEY_AR
    BEQ UP1_NCW
    DEC P1_VEL
    BNE UP1_NCW
    LDA #3
    STA P1_VEL
    LDA P1_DIR
    CLC
    ADC #1
    AND #$07
    STA P1_DIR
UP1_NCW:
    ; Move throttle: move every 2 frames
    DEC P1_MV
    BNE UP1_NOMOVE
    LDA #2
    STA P1_MV
    ; Move
    LDA P1_DIR
    TAY
    LDA DX_TBL,Y
    CLC
    ADC P1_COL
    BPL UP1_CHI
    CLC
    ADC #COLS
    JMP UP1_CSET
UP1_CHI:
    CMP #COLS
    BCC UP1_CSET
    SBC #COLS
UP1_CSET:
    STA P1_COL
    LDA DY_TBL,Y
    CLC
    ADC P1_ROW
    BPL UP1_RHI
    CLC
    ADC #ROWS
    JMP UP1_RSET
UP1_RHI:
    CMP #ROWS
    BCC UP1_RSET
    SBC #ROWS
UP1_RSET:
    STA P1_ROW
UP1_NOMOVE:
    ; Fire
    LDA KEY_AF
    BEQ UP1_NFIRE
    LDA QB_LIF
    BNE UP1_NFIRE
    LDA P1_COL
    STA QB_COL
    STA QB_OC
    LDA P1_ROW
    STA QB_ROW
    STA QB_OR
    LDA P1_DIR
    STA QB_DIR
    LDA #20
    STA QB_LIF
UP1_NFIRE:
    JSR ERASE_SPR
    JMP DRAW_SPR

; ---- Player 0 -----------------------------------------------------------
UPD_P0:
    LDA P0_COL
    STA P0_OC
    LDA P0_ROW
    STA P0_OR
    LDA P0_DIR
    STA P0_OD
    ; CCW
    LDA KEY_CL
    BEQ UP0_NCCW
    DEC P0_VEL
    BNE UP0_NCCW
    LDA #3
    STA P0_VEL
    LDA P0_DIR
    SEC
    SBC #1
    AND #$07
    STA P0_DIR
UP0_NCCW:
    ; CW
    LDA KEY_CR
    BEQ UP0_NCW
    DEC P0_VEL
    BNE UP0_NCW
    LDA #3
    STA P0_VEL
    LDA P0_DIR
    CLC
    ADC #1
    AND #$07
    STA P0_DIR
UP0_NCW:
    ; Move throttle: move every 2 frames
    DEC P0_MV
    BNE UP0_NOMOVE
    LDA #2
    STA P0_MV
    ; Move
    LDA P0_DIR
    TAY
    LDA DX_TBL,Y
    CLC
    ADC P0_COL
    BPL UP0_CHI
    CLC
    ADC #COLS
    JMP UP0_CSET
UP0_CHI:
    CMP #COLS
    BCC UP0_CSET
    SBC #COLS
UP0_CSET:
    STA P0_COL
    LDA DY_TBL,Y
    CLC
    ADC P0_ROW
    BPL UP0_RHI
    CLC
    ADC #ROWS
    JMP UP0_RSET
UP0_RHI:
    CMP #ROWS
    BCC UP0_RSET
    SBC #ROWS
UP0_RSET:
    STA P0_ROW
UP0_NOMOVE:
    ; Fire
    LDA KEY_CF
    BEQ UP0_NFIRE
    LDA PB_LIF
    BNE UP0_NFIRE
    LDA P0_COL
    STA PB_COL
    STA PB_OC
    LDA P0_ROW
    STA PB_ROW
    STA PB_OR
    LDA P0_DIR
    STA PB_DIR
    LDA #20
    STA PB_LIF
UP0_NFIRE:
    JSR ERASE_SPR
    JMP DRAW_SPR

; =========================================================================
; UPDATE_PB  -  advance P0 bullet 2 steps, check collision with P1
; =========================================================================

UPDATE_PB:
    LDA PB_OC
    STA TMP_C
    LDA PB_OR
    STA TMP_R
    JSR XY_TO_SCR
    LDA #CH_BL
    LDY #0
    STA (SCR_LO),Y

    DEC PB_LIF
    BNE UPB_MOVE
    JMP UPB_EXP
UPB_MOVE:
    LDA PB_DIR
    TAY
    ; Step 1
    LDA DX_TBL,Y
    CLC
    ADC PB_COL
    BPL UPB_CW1
    CLC
    ADC #COLS
    JMP UPB_CP1
UPB_CW1:
    CMP #COLS
    BCC UPB_CP1
    SEC
    SBC #COLS
UPB_CP1:
    STA PB_COL
    LDA DY_TBL,Y
    CLC
    ADC PB_ROW
    BPL UPB_RW1
    CLC
    ADC #ROWS
    JMP UPB_RP1
UPB_RW1:
    CMP #ROWS
    BCC UPB_RP1
    SEC
    SBC #ROWS
UPB_RP1:
    STA PB_ROW
    ; Check hit at step 1
    LDA PB_COL
    CMP P1_COL
    BNE UPB_STEP2
    LDA PB_ROW
    CMP P1_ROW
    BEQ UPB_HIT
UPB_STEP2:
    ; Step 2
    LDA DX_TBL,Y
    CLC
    ADC PB_COL
    BPL UPB_CW2
    CLC
    ADC #COLS
    JMP UPB_CP2
UPB_CW2:
    CMP #COLS
    BCC UPB_CP2
    SEC
    SBC #COLS
UPB_CP2:
    STA PB_COL
    LDA DY_TBL,Y
    CLC
    ADC PB_ROW
    BPL UPB_RW2
    CLC
    ADC #ROWS
    JMP UPB_RP2
UPB_RW2:
    CMP #ROWS
    BCC UPB_RP2
    SEC
    SBC #ROWS
UPB_RP2:
    STA PB_ROW
    ; Check hit at step 2
    LDA PB_COL
    CMP P1_COL
    BNE UPB_NOHIT
    LDA PB_ROW
    CMP P1_ROW
    BNE UPB_NOHIT
UPB_HIT:
    INC P0_SCORE
    LDA P1_COL
    STA TMP_C
    LDA P1_ROW
    STA TMP_R
    LDA #COL_P0
    STA CUR_COL
    JSR DRAW_EXP
    LDA #0
    STA PB_LIF
    STA GAMEON
    STA WINNER
    RTS

UPB_NOHIT:
    LDA PB_COL
    STA PB_OC
    STA TMP_C
    LDA PB_ROW
    STA PB_OR
    STA TMP_R
    JSR XY_TO_SCR
    LDA #CH_BUL
    LDY #0
    STA (SCR_LO),Y
    RTS

UPB_EXP:
    LDA #0
    STA PB_LIF
    RTS

; =========================================================================
; UPDATE_QB  -  advance P1 bullet 2 steps, check collision with P0
; =========================================================================

UPDATE_QB:
    LDA QB_OC
    STA TMP_C
    LDA QB_OR
    STA TMP_R
    JSR XY_TO_SCR
    LDA #CH_BL
    LDY #0
    STA (SCR_LO),Y

    DEC QB_LIF
    BNE UQB_MOVE
    JMP UQB_EXP
UQB_MOVE:
    LDA QB_DIR
    TAY
    ; Step 1
    LDA DX_TBL,Y
    CLC
    ADC QB_COL
    BPL UQB_CW1
    CLC
    ADC #COLS
    JMP UQB_CP1
UQB_CW1:
    CMP #COLS
    BCC UQB_CP1
    SEC
    SBC #COLS
UQB_CP1:
    STA QB_COL
    LDA DY_TBL,Y
    CLC
    ADC QB_ROW
    BPL UQB_RW1
    CLC
    ADC #ROWS
    JMP UQB_RP1
UQB_RW1:
    CMP #ROWS
    BCC UQB_RP1
    SEC
    SBC #ROWS
UQB_RP1:
    STA QB_ROW
    ; Check hit at step 1
    LDA QB_COL
    CMP P0_COL
    BNE UQB_STEP2
    LDA QB_ROW
    CMP P0_ROW
    BEQ UQB_HIT
UQB_STEP2:
    ; Step 2
    LDA DX_TBL,Y
    CLC
    ADC QB_COL
    BPL UQB_CW2
    CLC
    ADC #COLS
    JMP UQB_CP2
UQB_CW2:
    CMP #COLS
    BCC UQB_CP2
    SEC
    SBC #COLS
UQB_CP2:
    STA QB_COL
    LDA DY_TBL,Y
    CLC
    ADC QB_ROW
    BPL UQB_RW2
    CLC
    ADC #ROWS
    JMP UQB_RP2
UQB_RW2:
    CMP #ROWS
    BCC UQB_RP2
    SEC
    SBC #ROWS
UQB_RP2:
    STA QB_ROW
    ; Check hit at step 2
    LDA QB_COL
    CMP P0_COL
    BNE UQB_NOHIT
    LDA QB_ROW
    CMP P0_ROW
    BNE UQB_NOHIT
UQB_HIT:
    INC P1_SCORE
    LDA P0_COL
    STA TMP_C
    LDA P0_ROW
    STA TMP_R
    LDA #COL_P1
    STA CUR_COL
    JSR DRAW_EXP
    LDA #0
    STA QB_LIF
    STA GAMEON
    LDA #1
    STA WINNER
    RTS

UQB_NOHIT:
    LDA QB_COL
    STA QB_OC
    STA TMP_C
    LDA QB_ROW
    STA QB_OR
    STA TMP_R
    JSR XY_TO_SCR
    LDA #CH_BUL
    LDY #0
    STA (SCR_LO),Y
    RTS

UQB_EXP:
    LDA #0
    STA QB_LIF
    RTS

; =========================================================================
; READ_KEYS  -  poll KERNAL buffer, set P0 key flags
; PETSCII: A=$41 (CCW)  D=$44 (CW)  SPACE=$20 (fire)
; =========================================================================

READ_KEYS:
    LDA #0
    STA KEY_CL
    STA KEY_CR
    STA KEY_CF
    LDA KBUF_N
    BEQ RK_DONE
    TAX
RK_LOOP:
    DEX
    LDA KBUF,X
    CMP #$41
    BNE RK_D
    LDA #1
    STA KEY_CR
RK_D:
    CMP #$44
    BNE RK_SP
    LDA #1
    STA KEY_CL
RK_SP:
    CMP #$20
    BNE RK_NXT
    LDA #1
    STA KEY_CF
RK_NXT:
    TXA
    BNE RK_LOOP
    LDA #0
    STA KBUF_N
RK_DONE:
    RTS

; =========================================================================
; DATA TABLES
; =========================================================================

; Dir: 0=up 1=up-left 2=left 3=down-left 4=down 5=down-right 6=right 7=up-right
DX_TBL:
    DB $00,$FF,$FF,$FF,$00,$01,$01,$01
DY_TBL:
    DB $FF,$FF,$00,$01,$01,$01,$00,$FF

; row*22 offsets for ROW_TO_SCR (rows 0-22)
ROW_LO:
    DB $00,$16,$2C,$42,$58,$6E,$84,$9A
    DB $B0,$C6,$DC,$F2,$08,$1E,$34,$4A
    DB $60,$76,$8C,$A2,$B8,$CE,$E4
ROW_HI:
    DB $00,$00,$00,$00,$00,$00,$00,$00
    DB $00,$00,$00,$00,$01,$01,$01,$01
    DB $01,$01,$01,$01,$01,$01,$01

; ---- Custom character bitmaps (11 chars x 8 bytes = 88 bytes) ----------

CHAR_DATA:
    ; Char 0: blank
    DB $00,$00,$00,$00,$00,$00,$00,$00
    ; Char 1: dir 0  UP
    DB $18,$3C,$7E,$FF,$18,$18,$18,$00
    ; Char 2: dir 1  UP-LEFT
    DB $FE,$FC,$F8,$E0,$00,$00,$00,$00
    ; Char 3: dir 2  LEFT
    DB $18,$38,$78,$FF,$78,$38,$18,$00
    ; Char 4: dir 3  DOWN-LEFT
    DB $00,$00,$00,$00,$E0,$F8,$FC,$FE
    ; Char 5: dir 4  DOWN
    DB $00,$18,$18,$18,$FF,$7E,$3C,$18
    ; Char 6: dir 5  DOWN-RIGHT
    DB $00,$00,$00,$00,$07,$1F,$3F,$7F
    ; Char 7: dir 6  RIGHT
    DB $18,$1C,$1E,$FF,$1E,$1C,$18,$00
    ; Char 8: dir 7  UP-RIGHT
    DB $7F,$3F,$1F,$07,$00,$00,$00,$00
    ; Char 9: bullet
    DB $00,$00,$00,$18,$18,$00,$00,$00
    ; Char 10: explosion
    DB $81,$42,$24,$18,$18,$24,$42,$81

; =========================================================================
; SHOW_SCORES  -  clear screen, switch to ROM charset, display
;   "PLAYER: X"  (cyan, row 9)
;   "AI: X"      (yellow, row 13)
;   then restore custom charset
; =========================================================================

SHOW_SCORES:
    ; Switch to ROM charset first, then clear with $20 (ROM space).
    ; Clearing with $00 first then switching gives a screen full of '@'
    ; because $00 = '@' in the ROM charset.
    LDA #0
    STA VIC9005
    LDA #$20
    LDX #0
SS_CLR:
    STA SCREEN,X
    STA SCR2,X
    INX
    BNE SS_CLR

    ; "PLAYER: " row 9, col 6, cyan
    LDA #9
    STA TMP_R
    LDA #6
    STA TMP_C
    LDA #COL_P0
    STA CUR_COL
    LDX #0
SS_PL_LP:
    LDA SS_STR_PL,X     ; load before STX so X is still the index
    STX TMP_S1          ; save X: ROW_TO_SCR inside SS_WCHAR does TAX
    JSR SS_WCHAR
    LDX TMP_S1
    INX
    CPX #8
    BNE SS_PL_LP
    LDA P0_SCORE
    JSR WRITE_NUM

    ; "AI: " row 13, col 9, yellow
    LDA #13
    STA TMP_R
    LDA #9
    STA TMP_C
    LDA #COL_P1
    STA CUR_COL
    LDX #0
SS_AI_LP:
    LDA SS_STR_AI,X
    STX TMP_S1
    JSR SS_WCHAR
    LDX TMP_S1
    INX
    CPX #4
    BNE SS_AI_LP
    LDA P1_SCORE
    JSR WRITE_NUM

    LDA #8
    JSR LONG_DELAY

    ; Restore custom charset
    LDA #7
    STA VIC9005
    RTS

; SS_WCHAR: write char A at (TMP_C, TMP_R) with CUR_COL, advance TMP_C
SS_WCHAR:
    STA TMP_CH
    JSR XY_TO_SCR
    LDA SCR_LO
    STA COL_LO
    LDA SCR_HI
    CLC
    ADC #$76
    STA COL_HI
    LDA TMP_CH
    LDY #0
    STA (SCR_LO),Y
    LDA CUR_COL
    STA (COL_LO),Y
    INC TMP_C
    RTS

; WRITE_NUM: write score A as decimal digits using SS_WCHAR
WRITE_NUM:
    STA TMP_A
    LDA #0
    STA TMP_S0
WN_DIV:
    LDA TMP_A
    CMP #10
    BCC WN_DONE_DIV
    SEC
    SBC #10
    STA TMP_A
    INC TMP_S0
    JMP WN_DIV
WN_DONE_DIV:
    LDA TMP_S0
    BEQ WN_ONES
    CLC
    ADC #$30
    JSR SS_WCHAR
WN_ONES:
    LDA TMP_A
    CLC
    ADC #$30
    JSR SS_WCHAR
    RTS

; Screen codes: VIC-20 uppercase A=1..Z=26, digits 0-9 = $30-$39, space=$20, colon=$3A
SS_STR_PL: DB $10,$0C,$01,$19,$05,$12,$3A,$20   ; PLAYER: (space)
SS_STR_AI: DB $01,$09,$3A,$20                    ; AI: (space)

; =========================================================================
; AI_MOVE  -  steer P1 toward P0, fire when aligned
; =========================================================================

AI_DIR_TBL:
    DB 7,6,5,0,0,4,1,2,3

AI_MOVE:
    LDA #0
    STA KEY_AL
    STA KEY_AR
    STA KEY_AF

    ; sign_x: 0=P0 right of P1  1=same col  2=P0 left of P1
    LDA P0_COL
    CMP P1_COL
    BCC AI_SX2
    BEQ AI_SX1
    LDA #0
    JMP AI_SX_DONE
AI_SX2:
    LDA #2
    JMP AI_SX_DONE
AI_SX1:
    LDA #1
AI_SX_DONE:
    STA TMP_S0
    ASL
    CLC
    ADC TMP_S0
    STA TMP_S1

    ; sign_y: 0=P0 above P1  1=same row  2=P0 below P1
    LDA P0_ROW
    CMP P1_ROW
    BCC AI_SY0
    BEQ AI_SY1
    LDA #2
    JMP AI_SY_DONE
AI_SY0:
    LDA #0
    JMP AI_SY_DONE
AI_SY1:
    LDA #1
AI_SY_DONE:
    CLC
    ADC TMP_S1
    TAX
    LDA AI_DIR_TBL,X
    STA TMP_S1

    ; Rotate toward desired dir
    SEC
    SBC P1_DIR
    AND #$07
    BEQ AI_FIRE
    CMP #5
    BCS AI_CCW
    LDA #1
    STA KEY_AR
    JMP AI_FIRE
AI_CCW:
    LDA #1
    STA KEY_AL

AI_FIRE:
    ; Fire when nearly aligned and within range
    LDA TMP_S1
    SEC
    SBC P1_DIR
    AND #$07
    CMP #2
    BCS AI_DONE
    ; Manhattan distance
    LDA P0_COL
    SEC
    SBC P1_COL
    BPL AI_DX_POS
    EOR #$FF
    CLC
    ADC #1
AI_DX_POS:
    STA TMP_S0
    LDA P0_ROW
    SEC
    SBC P1_ROW
    BPL AI_DY_POS
    EOR #$FF
    CLC
    ADC #1
AI_DY_POS:
    CLC
    ADC TMP_S0
    CMP #12
    BCS AI_DONE
    LDA #1
    STA KEY_AF
AI_DONE:
    RTS

; =========================================================================
; INIT_HS  -  fill high score table with score=0, name="---"
; =========================================================================

INIT_HS:
    LDA #0
    LDX #19
IH_CLR:
    STA HS_TABLE,X
    DEX
    BPL IH_CLR
    LDX #4
IH_NAMES:
    TXA
    ASL
    ASL
    TAY
    INY
    LDA #$2D
    STA HS_TABLE,Y
    INY
    STA HS_TABLE,Y
    INY
    STA HS_TABLE,Y
    DEX
    BPL IH_NAMES
    RTS

; =========================================================================
; SESSION_OVER  -  called after 10 rounds; enter name if qualified
; =========================================================================

SESSION_OVER:
    LDA #0
    STA VIC9005
    ; Skip name entry if player scored nothing
    LDA P0_SCORE
    BEQ SO_NO_ENTRY
    ; Find if P0_SCORE qualifies for top 5
    LDA #0
    STA TMP_S1
SO_FIND:
    LDA TMP_S1
    ASL
    ASL
    TAY
    LDA P0_SCORE
    CMP HS_TABLE,Y
    BCS SO_QUAL
    INC TMP_S1
    LDA TMP_S1
    CMP #5
    BCC SO_FIND
    JMP SO_NO_ENTRY
SO_QUAL:
    JSR ENTER_NAME
    JSR INSERT_HISCORE
SO_NO_ENTRY:
    JSR SHOW_HISCORE
    ; Reset session state
    LDA #0
    STA P0_SCORE
    STA P1_SCORE
    STA GAME_CNT
    LDA #$A0
    STA SPEED_DEL
    LDA #7
    STA VIC9005
    JMP RESTART

; =========================================================================
; ENTER_NAME  -  3-char name entry; A=prev, D=next, SPACE=confirm
; Result in NAME_BUF0/1/2 as ROM screen codes (1-26=A-Z)
; =========================================================================

ENTER_NAME:
    LDA #1
    STA NAME_BUF0
    STA NAME_BUF1
    STA NAME_BUF2
    LDA #0
    STA NAME_POS

EN_REDRAW:
    LDA #$20
    LDX #0
EN_CLR:
    STA SCREEN,X
    STA SCR2,X
    INX
    BNE EN_CLR

    ; "ENTER NAME" row 7, col 6, white
    LDA #7
    STA TMP_R
    LDA #6
    STA TMP_C
    LDA #1
    STA CUR_COL
    LDA #$05
    JSR SS_WCHAR
    LDA #$0E
    JSR SS_WCHAR
    LDA #$14
    JSR SS_WCHAR
    LDA #$05
    JSR SS_WCHAR
    LDA #$12
    JSR SS_WCHAR
    LDA #$20
    JSR SS_WCHAR
    LDA #$0E
    JSR SS_WCHAR
    LDA #$01
    JSR SS_WCHAR
    LDA #$13
    JSR SS_WCHAR
    LDA #$05
    JSR SS_WCHAR

    ; "A/D=CYCLE SPACE=OK" row 17, col 2
    LDA #17
    STA TMP_R
    LDA #2
    STA TMP_C
    LDA #1
    STA CUR_COL
    LDA #$01
    JSR SS_WCHAR
    LDA #$2F
    JSR SS_WCHAR
    LDA #$04
    JSR SS_WCHAR
    LDA #$3D
    JSR SS_WCHAR
    LDA #$03
    JSR SS_WCHAR
    LDA #$19
    JSR SS_WCHAR
    LDA #$03
    JSR SS_WCHAR
    LDA #$0C
    JSR SS_WCHAR
    LDA #$05
    JSR SS_WCHAR
    LDA #$20
    JSR SS_WCHAR
    LDA #$13
    JSR SS_WCHAR
    LDA #$10
    JSR SS_WCHAR
    LDA #$01
    JSR SS_WCHAR
    LDA #$03
    JSR SS_WCHAR
    LDA #$05
    JSR SS_WCHAR
    LDA #$3D
    JSR SS_WCHAR
    LDA #$0F
    JSR SS_WCHAR
    LDA #$0B
    JSR SS_WCHAR

    ; Letter 0 at row 11, col 8
    LDA #11
    STA TMP_R
    LDA #8
    STA TMP_C
    LDA NAME_POS
    BNE EN_L0_DIM
    LDA #COL_P0
    JMP EN_L0_COL
EN_L0_DIM:
    LDA #1
EN_L0_COL:
    STA CUR_COL
    LDA NAME_BUF0
    JSR SS_WCHAR

    ; Letter 1 at row 11, col 11
    LDA #11
    STA TMP_R
    LDA #11
    STA TMP_C
    LDA NAME_POS
    CMP #1
    BNE EN_L1_DIM
    LDA #COL_P0
    JMP EN_L1_COL
EN_L1_DIM:
    LDA #1
EN_L1_COL:
    STA CUR_COL
    LDA NAME_BUF1
    JSR SS_WCHAR

    ; Letter 2 at row 11, col 14
    LDA #11
    STA TMP_R
    LDA #14
    STA TMP_C
    LDA NAME_POS
    CMP #2
    BNE EN_L2_DIM
    LDA #COL_P0
    JMP EN_L2_COL
EN_L2_DIM:
    LDA #1
EN_L2_COL:
    STA CUR_COL
    LDA NAME_BUF2
    JSR SS_WCHAR

    ; Wait for key
EN_WAIT:
    LDA KBUF_N
    BEQ EN_WAIT
    LDA KBUF
    LDX #0
    STX KBUF_N

    CMP #$44
    BNE EN_CHK_A
    JSR EN_NEXT
    JMP EN_REDRAW
EN_CHK_A:
    CMP #$41
    BNE EN_CHK_SP
    JSR EN_PREV
    JMP EN_REDRAW
EN_CHK_SP:
    CMP #$20
    BNE EN_WAIT
    INC NAME_POS
    LDA NAME_POS
    CMP #3
    BCS EN_DONE
    JMP EN_REDRAW
EN_DONE:
    RTS

EN_NEXT:
    LDX NAME_POS
    LDA NAME_BUF0,X
    CLC
    ADC #1
    CMP #27
    BCC EN_NX_OK
    LDA #1
EN_NX_OK:
    STA NAME_BUF0,X
    RTS

EN_PREV:
    LDX NAME_POS
    LDA NAME_BUF0,X
    SEC
    SBC #1
    BNE EN_PV_OK
    LDA #26
EN_PV_OK:
    STA NAME_BUF0,X
    RTS

; =========================================================================
; INSERT_HISCORE  -  insert NAME_BUF+P0_SCORE at correct position
; =========================================================================

INSERT_HISCORE:
    LDA #0
    STA TMP_S1
IHS_FIND:
    LDA TMP_S1
    ASL
    ASL
    TAY
    LDA P0_SCORE
    CMP HS_TABLE,Y
    BCS IHS_FOUND
    INC TMP_S1
    LDA TMP_S1
    CMP #5
    BCC IHS_FIND
    RTS
IHS_FOUND:
    ; Shift entries TMP_S1..3 down one slot
    LDA #3
    STA TMP_A
IHS_SHIFT:
    LDA TMP_A
    ASL
    ASL
    TAY
    LDA HS_TABLE,Y
    STA HS_TABLE+4,Y
    INY
    LDA HS_TABLE,Y
    STA HS_TABLE+4,Y
    INY
    LDA HS_TABLE,Y
    STA HS_TABLE+4,Y
    INY
    LDA HS_TABLE,Y
    STA HS_TABLE+4,Y
    LDA TMP_A
    CMP TMP_S1
    BEQ IHS_INSERT
    DEC TMP_A
    JMP IHS_SHIFT
IHS_INSERT:
    LDA TMP_S1
    ASL
    ASL
    TAY
    LDA P0_SCORE
    STA HS_TABLE,Y
    INY
    LDA NAME_BUF0
    STA HS_TABLE,Y
    INY
    LDA NAME_BUF1
    STA HS_TABLE,Y
    INY
    LDA NAME_BUF2
    STA HS_TABLE,Y
    RTS

; =========================================================================
; SHOW_HISCORE  -  display top 5 table (ROM charset active)
; =========================================================================

SHOW_HISCORE:
    LDA #$20
    LDX #0
SHR_CLR:
    STA SCREEN,X
    STA SCR2,X
    INX
    BNE SHR_CLR

    ; "HIGH SCORES" row 3, col 5, cyan
    LDA #3
    STA TMP_R
    LDA #5
    STA TMP_C
    LDA #COL_P0
    STA CUR_COL
    LDA #$08
    JSR SS_WCHAR
    LDA #$09
    JSR SS_WCHAR
    LDA #$07
    JSR SS_WCHAR
    LDA #$08
    JSR SS_WCHAR
    LDA #$20
    JSR SS_WCHAR
    LDA #$13
    JSR SS_WCHAR
    LDA #$03
    JSR SS_WCHAR
    LDA #$0F
    JSR SS_WCHAR
    LDA #$12
    JSR SS_WCHAR
    LDA #$05
    JSR SS_WCHAR
    LDA #$13
    JSR SS_WCHAR

    LDA #0
    STA TMP_S1

SHR_LOOP:
    ; Row = 6 + TMP_S1*2, col 4, white
    LDA TMP_S1
    ASL
    CLC
    ADC #6
    STA TMP_R
    LDA #4
    STA TMP_C
    LDA #1
    STA CUR_COL

    ; Rank digit '1'..'5'
    LDA TMP_S1
    CLC
    ADC #$31
    JSR SS_WCHAR

    LDA #$2E
    JSR SS_WCHAR
    LDA #$20
    JSR SS_WCHAR

    ; Name byte 0
    LDA TMP_S1
    ASL
    ASL
    TAY
    INY
    LDA HS_TABLE,Y
    JSR SS_WCHAR

    ; Name byte 1
    LDA TMP_S1
    ASL
    ASL
    TAY
    INY
    INY
    LDA HS_TABLE,Y
    JSR SS_WCHAR

    ; Name byte 2
    LDA TMP_S1
    ASL
    ASL
    TAY
    INY
    INY
    INY
    LDA HS_TABLE,Y
    JSR SS_WCHAR

    LDA #$20
    JSR SS_WCHAR

    ; Score
    LDA TMP_S1
    ASL
    ASL
    TAY
    LDA HS_TABLE,Y
    JSR WRITE_NUM

    INC TMP_S1
    LDA TMP_S1
    CMP #5
    BCC SHR_LOOP

    LDA #20
    JSR LONG_DELAY
    RTS
