; AIRFIGHT for ABC80 -- Z80 assembly
; Original BASIC: Kristian Lidberg & Set Lonnert, 1981
; Reinterpretation of the original game. Fixed with some
; other graphics, and animations at times. 
; Assemble: z80asm -o airfight.bin -g 0x8000 airfight.asm
; Run on device: P (loads binary), then G 8000 (or just G)


; HARDWARE CONSTANTS

PORT_SND    EQU 006H    ; SN76477 sound chip -- OUT (6), value
                        ; Bit layout of value written:
                        ;   bit0 = chip enable (0=on, 1=off)
                        ;   bit1 = VCO voltage select
                        ;   bit2 = VCO mode (0=external, 1=SLF)
                        ;   bits3-5 = mixer mode
                        ;   bits6-7 = envelope mode
PORT_KB     EQU 038H    ; Keyboard port -- IN A,(38H)
                        ; Returns current key code with bit7 set,
                        ; or 0 if no key pressed.  ABC80 generates
                        ; a Z80 interrupt (vector 34H) on each 20ms
                        ; strobe; keyboard is only valid just after
                        ; the strobe.
CLKLO       EQU 0FDF0H  ; System RAM: low byte of 50Hz tick counter.
                        ; Incremented by the ABC80 firmware ISR on
                        ; every 20ms strobe interrupt.  Bytes at
                        ; FDF1H/FDF2H are mid/high bytes.  DLYLOOP
                        ; watches only the low byte (wraps ~5s).


; SOUND VALUES  (SN76477 register bytes)
;
; Each value encodes chip-enable + VCO/mixer/envelope settings.
; The exact timbre depends on the RC network on the real board;
; values were chosen to match the original ABC80 BASIC sounds.
SOUND_OFF   EQU 0       ; Silence (chip disabled, all bits 0)
SOUND_INTRO EQU 135     ; Startup whoosh (noise dominant) time: "FOR T=1 TO 600 : NEXT T"
SOUND_TITLE EQU 157     ; Title screen tone
SOUND_READY EQU 131     ; "Ready" prompt tone
SOUND_SH1   EQU 155     ; Shoot sound variant 1
SOUND_SH2   EQU 157     ; Shoot sound variant 2
SOUND_HIT   EQU 9       ; Hit / explosion (short burst)
SOUND_WIN   EQU 199     ; Winner fanfare note
SOUND_LOOP  EQU 137     ; Loop tone (used in victory tune)
SOUND_TUNE  EQU 121     ; Tune note (used in victory tune)


; SCREEN BYTE CONSTANTS
;
; ABC80 uses teletext-style graphics (e.g. Prestel).  The FIRST byte of
; each screen row (col 0) acts as a mode setter for the entire row:
;   SCR_GFXMK in col 0   -> rest of row is mosaic/graphics mode
;   Normal char in col 0 -> entire row is text mode
; Within a graphics row:
;   Chars with bit5 set (0x20-0x3F, 0x60-0x7F) -> mosaic patterns
;   Chars 0x40-0x5F (uppercase)                -> normal font
;   SCR_GFXOFF (0x87) anywhere                 -> exits graphics for rest of row
; Mosaic pattern encoding (6 sub-pixels per cell, 2 wide x 3 tall):
;   pat = (cell & 0x1F) | ((cell & 0x40) >> 1)
;   bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit5=BR
SCR_BLANK   EQU 020H    ; Space char -- blank in both text/gfx modes
SCR_BLOCK   EQU 07FH    ; Full 6-pixel mosaic block (all bits set) -- not in use
SCR_GFXMK   EQU 097H    ; 151 decimal. Graphics mode marker for e.g. col 0
                        ; (bit7 set = invisible on screen; 0x97 & 0x7F = 0x17)
CHAR_GFX    EQU 017H    ; 23 decimal. Same marker without bit7 (legacy alias?)


; PLAYER KEY CODES  (ASCII, bit7 stripped)
;
; P1: left hand on QWERTY (A/D/Space)
; P2: right hand on QWERTY (J/L/M)
P1_LEFT     EQU 041H    ; 'A' -- Player 1 turn left
P1_RIGHT    EQU 044H    ; 'D' -- Player 1 turn right
P1_FIRE     EQU 058H    ; 'X' -- Player 1 fire  (original BASIC mapping)
P2_LEFT     EQU 04AH    ; 'J' -- Player 2 turn left
P2_RIGHT    EQU 04CH    ; 'L' -- Player 2 turn right
P2_FIRE     EQU 04DH    ; 'M' -- Player 2 fire


; GAME CONSTANTS
;
SCORE_LIM   EQU 10      ; Wins needed in score mode
TIME_MIN    EQU 2       ; Starting minutes in time mode
DIR_CNT     EQU 8       ; Number of compass directions (N..NW)
BUL_STEPS   EQU 3       ; Cells a bullet travels per game tick
TURN_DELAY  EQU 3       ; Frames player must wait after turning (blocks double-fire)
TUNE_SCALE  EQU 200     ; PLY_TUNE: float FOR-NEXT speed (FOR T=1 TO 10: NEXT T in BASIC line 270).
                        ; Calibrated: float loop ~1000 iters/s; 200x2x2.5us = 1ms per step.
INT_SCALE   EQU 62      ; PLY_TUNE: integer FOR-NEXT speed (FOR J%=1% TO A1%: NEXT J%, line 268).
                        ; PTLP_INNER loop body = 18 + 16*INT_SCALE T-states/step at 3 MHz.
                        ; 62 → 1010 T-states ≈ 337 µs/step ≈ 2970 steps/s ≈ BASIC ~3000 int-iters/s.
                        ; Raise INT_SCALE if pitch sounds too high; lower if too low.


; VARIABLE BLOCK  (256 bytes at 8F00H, above game code)
;
; All mutable game state lives here so it is easy to zero-init and
; so that addresses are known at assemble time.
; Arrays of 4 bytes serve both players (index 0=P1, index 1=P2)
; and both bullets (index 0=B1, index 1=B2).
VAR_BASE    EQU 08F00H

CUR_ROW     EQU VAR_BASE+0   ; Current cursor row  (used by PRTCHR)
CUR_COL     EQU VAR_BASE+1   ; Current cursor col  (used by PRTCHR)

P_DIR       EQU VAR_BASE+2   ; Player direction [0..1], 2 bytes each
                             ; (4 bytes reserved: P1 at +2, P2 at +4)
P_ROW       EQU VAR_BASE+6   ; Player row position [0..1]
P_COL       EQU VAR_BASE+10  ; Player col position [0..1]
P_PROW      EQU VAR_BASE+14  ; Player previous row (for erase) [0..1]
P_PCOL      EQU VAR_BASE+18  ; Player previous col (for erase) [0..1]
P_SCORE     EQU VAR_BASE+22  ; Player score [0..1]

B_ACTIVE    EQU VAR_BASE+26  ; Bullet active flag [0..1] (0=inactive)
B_DIR       EQU VAR_BASE+28  ; Bullet direction [0..1]
B_ROW       EQU VAR_BASE+32  ; Bullet row [0..1]
B_COL       EQU VAR_BASE+36  ; Bullet col [0..1]
B_PROW      EQU VAR_BASE+40  ; Bullet previous row (for erase) [0..1]
B_PCOL      EQU VAR_BASE+44  ; Bullet previous col (for erase) [0..1]

J_PREV      EQU VAR_BASE+48  ; TIMER_TICK frame-down counter: 10-0 = 1 second

MODE_SLIM   EQU VAR_BASE+50  ; Score-limit mode flag (1=active)
MODE_STGT   EQU VAR_BASE+52  ; Score target (copy of SCORE_LIM)
MODE_TDIR   EQU VAR_BASE+54  ; Time mode flag (1=active)
TMIN_V      EQU VAR_BASE+56  ; Remaining minutes
TSEC_V      EQU VAR_BASE+58  ; Remaining seconds (0..59)

WINNER_V    EQU VAR_BASE+60  ; Result: 0=P1 wins, 1=P2 wins, 0xFF=draw
PLAYED_V    EQU VAR_BASE+62  ; 1 if players have played before

NAME_P1     EQU VAR_BASE+64  ; Player 1 name string (32 bytes, null-term)
NAME_P2     EQU VAR_BASE+96  ; Player 2 name string (32 bytes, null-term)
INBUF       EQU VAR_BASE+128 ; General input line buffer (16 bytes)
P_TDELAY    EQU VAR_BASE+144 ; Turn cooldown per player [0..1], stride 2 (same as P_DIR)

        ORG 8000H


; Intro screen: border, logo, ask-played
;
; Entry point for the game.  PLAY_AGAIN will JP COLD_START
; to restart without going back to the monitor.
;
START:
COLD_START:
        DI               ; disable interrupts for clean startup
        CALL CLRSCR      ; blank all 24 rows

; --- Brief intro sound --- TODO: longer intro
        LD A, SOUND_INTRO
        CALL SNDON
        LD HL, 10        ; 10 ticks = 200ms
        CALL DLYLOOP
        CALL SNDOFF

; --- Border: set col 0 of rows 0-19 to SCR_GFXMK (graphics mode) ---
        LD B, 0
        LD C, 19
        CALL DRAWBDR

; --- Draw logo strings from LOGO_TABLE ---
        CALL DRAW_LOGO

; --- Ask whether players have played before ---
        CALL ASK_PLAYED

; --- Phase 8: names, instructions, mode ---
        CALL GET_NAMES
        CALL SHOW_INSTR
        CALL GET_MODE
; --- Phase 9: game setup ---
RESTART_GAME:            ; PLAY_AGAIN "J" re-enters here (same mode/names)
        CALL GAME_INIT
        CALL SHOW_READY      ; show names + planes, wait for keypress
; --- Phase 10: run game loop (DI: BASIC ISR must not steal key reads) ---
        DI
        JP MAIN_LOOP


; DRAW_LOGO  -- print strings listed in LOGO_TABLE onto the screen
; In:  nothing  (reads LOGO_TABLE)
; Out: logo strings written to screen RAM
; Clobbers: A, B, C, D, E, H, L, IX
; Table format: one entry = (row:1, col:1, ptr:2) little-endian.
; Table ends with 0FFH in the row byte.
; 
DRAW_LOGO:
        LD IX, LOGO_TABLE
DLGLP:
        LD A, (IX+0)     ; row byte (0FFH = end of table)
        CP 0FFH
        RET Z
        LD B, A          ; B = row
        LD A, (IX+1)
        LD C, A          ; C = col
        CALL CSRSET
        LD L, (IX+2)     ; string pointer low byte
        LD H, (IX+3)     ; string pointer high byte
        CALL PRTSTR      ; clobbers HL, A, B, C, D, E; IX is safe
        LD DE, 4
        ADD IX, DE       ; advance to next entry (4 bytes per entry)
        JR DLGLP


; ASK_PLAYED  -- ask "HAR NI SPELAT FÖRUT? (J/N)" and record answer
; In:  nothing
; Out: PLAYED_V = 1 if J (yes), 0 if N (no)
; Clobbers: A, B, C, D, E, H, L
; Accepts both upper and lower case (J/j, N/n).
; Loops until a valid answer is given.
;
ASK_PLAYED:
        LD B, 21         ; prompt on row 21 (below play area)
        LD C, 7
        CALL CSRSET
        LD HL, TXT_PLAYED
        CALL PRTSTR
ASKPL_LP:
        LD B, 22         ; input echo on row 22
        LD C, 7
        CALL CSRSET
        LD HL, INBUF
        LD B, 1          ; accept at most 1 character
        CALL INLINE_V
        LD A, (INBUF)    ; first (and only) char entered
        AND 0DFH         ; force uppercase (clears bit 5: 'j'->J, 'n'->N)
        CP 04AH          ; 'J'
        JR Z, ASKPL_YES
        CP 04EH          ; 'N'
        JR Z, ASKPL_NO
        JR ASKPL_LP      ; anything else -- ask again
ASKPL_YES:
        LD A, 1
        LD (PLAYED_V), A
        RET
ASKPL_NO:
        XOR A            ; A = 0
        LD (PLAYED_V), A
        RET



; GET_NAMES  -- collect player names into NAME_P1 and NAME_P2
; In:  nothing  (CUR_ROW/CUR_COL may be anything)
; Out: NAME_P1 and NAME_P2 hold null-terminated name strings (max 24 chars)
; Clobbers: A, B, C, D, E, H, L
; Clears rows 21-22 before each prompt so old text does not bleed through.
;
GET_NAMES:
        LD B, 21         ; ---- Player 1 ----
        CALL CLRROW
        LD B, 22
        CALL CLRROW
        LD B, 21
        LD C, 0
        CALL CSRSET
        LD HL, TXT_NAME_P1
        CALL PRTSTR
        LD B, 22
        LD C, 0
        CALL CSRSET
        LD HL, NAME_P1
        LD B, 24         ; accept up to 24 chars
        CALL INLINE_V

        LD B, 21         ; ---- Player 2 ----
        CALL CLRROW
        LD B, 22
        CALL CLRROW
        LD B, 21
        LD C, 0
        CALL CSRSET
        LD HL, TXT_NAME_P2
        CALL PRTSTR
        LD B, 22
        LD C, 0
        CALL CSRSET
        LD HL, NAME_P2
        LD B, 24
        CALL INLINE_V
        RET


; SHOW_INSTR  -- show instruction screen (skipped if PLAYED_V != 0)
; In:  PLAYED_V: 0 = first game (show instructions), else skip
; Out: instructions displayed; waits for any keypress before returning
; Clobbers: A, B, C, D, E, H, L, IX
; Calls CLRSCR so the border/logo is gone on return.  GET_MODE below
; just uses rows 21-22 so the cleared screen is not a problem.
;
SHOW_INSTR:
        LD A, (PLAYED_V)
        OR A
        RET NZ           ; already played -- skip instructions
        CALL CLRSCR
        LD IX, INSTR_TABLE
SINLP:
        LD A, (IX+0)     ; row (0FFH = end of table)
        CP 0FFH
        JR Z, SINWAIT
        LD B, A
        LD A, (IX+1)
        LD C, A
        CALL CSRSET
        LD L, (IX+2)     ; string pointer low byte
        LD H, (IX+3)     ; string pointer high byte
        CALL PRTSTR
        LD DE, 4
        ADD IX, DE
        JR SINLP
SINWAIT:
        CALL INKEY       ; any key to continue
        RET


; GET_MODE  -- ask player to choose score or time mode
; In:  nothing
; Out: score mode: MODE_TDIR=0, MODE_SLIM=1, MODE_STGT=SCORE_LIM
;      time mode:  MODE_SLIM=0, MODE_TDIR=1, TMIN_V=TIME_MIN, TSEC_V=0
; Clobbers: A, B, C, D, E, H, L
; Loops until 'P' (score) or 'T' (time) is entered (case-insensitive).
;
GET_MODE:
        LD B, 21
        CALL CLRROW
        LD B, 22
        CALL CLRROW
        LD B, 21
        LD C, 0
        CALL CSRSET
        LD HL, TXT_MODE_Q
        CALL PRTSTR
GET_MODE_LP:
        LD B, 22
        LD C, 0
        CALL CSRSET
        LD HL, INBUF
        LD B, 1          ; accept 1 character
        CALL INLINE_V
        LD A, (INBUF)
        AND 0DFH         ; force uppercase
        CP 050H          ; 'P' = score mode
        JR Z, GMODE_P
        CP 054H          ; 'T' = time mode
        JR Z, GMODE_T
        LD B, 22         ; invalid input -- clear and retry
        CALL CLRROW
        JR GET_MODE_LP
GMODE_P:
        XOR A
        LD (MODE_TDIR), A  ; time mode off
        LD A, 1
        LD (MODE_SLIM), A  ; score mode on
        LD A, SCORE_LIM
        LD (MODE_STGT), A  ; score target = 10
        LD B, 21
        LD C, 0
        CALL CSRSET
        LD HL, TXT_MODE_SCORE
        CALL PRTSTR        ; show "POÄNGLÄGE: VINNER VID 10" (wins at 10)
        RET
GMODE_T:
        XOR A
        LD (MODE_SLIM), A  ; score mode off
        LD A, 1
        LD (MODE_TDIR), A  ; time mode on
        LD A, TIME_MIN
        LD (TMIN_V), A     ; starting minutes
        XOR A
        LD (TSEC_V), A     ; starting seconds = 0
        LD B, 21
        LD C, 0
        CALL CSRSET
        LD HL, TXT_MODE_TIME
        CALL PRTSTR        ; show "TIDSLÄGE: 2 MINUTER" (max 2 min)
        RET



; GAME_INIT  -- reset all game state and draw starting screen
; In:  MODE_SLIM/MODE_TDIR/TMIN_V already set by GET_MODE
; Out: screen cleared and redrawn; planes at start positions; HUD shown
; Clobbers: A, B, C, D, E, H, L, IX
;
GAME_INIT:
        CALL CLRSCR
        LD B, 0
        LD C, 19
        CALL DRAWBDR         ; graphics (invisible) border on rows 0-19

        ; P1: row=5, col=5, direction=2 (East)
        LD A, 2
        LD (P_DIR), A
        LD A, 5
        LD (P_ROW), A
        LD (P_COL), A
        LD (P_PROW), A       ; prev = start pos so first erase is harmless
        LD (P_PCOL), A

        ; P2: row=14, col=34, direction=6 (West)
        LD A, 6
        LD (P_DIR+2), A
        LD A, 14
        LD (P_ROW+2), A
        LD (P_PROW+2), A
        LD A, 34
        LD (P_COL+2), A
        LD (P_PCOL+2), A

        ; Clear scores, bullets, timer, turn cooldowns
        XOR A
        LD (P_SCORE), A
        LD (P_SCORE+2), A
        LD (B_ACTIVE), A
        LD (B_ACTIVE+1), A
        LD (TSEC_V), A
        LD (TMIN_V), A       ; clear first so TIME_MIN load below is always safe
        LD (P_TDELAY), A     ; P1 turn cooldown = 0
        LD (P_TDELAY+2), A   ; P2 turn cooldown = 0
        LD A, (MODE_TDIR)    ; restore minutes only in time mode
        OR A
        JR Z, GI_NO_TMIN
        LD A, TIME_MIN
        LD (TMIN_V), A
GI_NO_TMIN:
        LD A, 10
        LD (J_PREV), A       ; frame counter: 10 frames x 100ms = 1 second

        CALL HUD_DRAW        ; draw score/timer row 20
        LD B, 0
        CALL PLN_DRAW        ; draw P1 at starting position
        LD B, 1
        CALL PLN_DRAW        ; draw P2 at starting position
        RET


; SHOW_READY -- pre-game name/plane presentation
; Planes are already at starting positions (drawn by GAME_INIT).
; Layout in game area (col 0 = 0x97 border marker, untouched throughout):
;   Row  6: 0x87 "A/D/X <P1 name>" 0x97
;   Row  9: 0x87 "REDO! TRYCK VALFRI TANGENT" 0x97   (centred, col 7)
;   Row 12: 0x87 "J/L/M <P2 name>" 0x97
; Two blank rows between each line for visual spacing.
; 0x87 = CHR(135) turns graphics OFF; 0x97 = CHR(151) turns graphics ON.
; Cleanup writes spaces to cols 1-39 (no need to restore col 0).
; Clobbers: A, B, C, D, E, H, L
;
SHOW_READY:
        ; --- 2.5 s pause: planes visible, no text yet ---
        LD HL, 125
        CALL DLYLOOP

        ; --- Row 7: P1 keys + name ---
        LD B, 7
        LD C, 1
        CALL CSRSET
        LD A, 087H           ; CHR135: open text window
        CALL PRTCHR
        LD HL, TXT_P1KEYS    ; "A/D/X "
        CALL PRTSTR
        LD HL, NAME_P1
        LD D, 24
        CALL PRTNAME
        LD A, SCR_GFXMK      ; CHR151: close text window
        CALL PRTCHR

        ; --- Row 10: REDO! centred (starts at col 7, 26 chars) ---
        LD B, 10
        LD C, 6
        CALL CSRSET
        LD A, 087H
        CALL PRTCHR
        LD HL, TXT_READY     ; "REDO! TRYCK VALFRI TANGENT"
        CALL PRTSTR
        LD A, SCR_GFXMK
        CALL PRTCHR

        ; --- Row 13: P2 keys + name ---
        LD B, 13
        LD C, 1
        CALL CSRSET
        LD A, 087H
        CALL PRTCHR
        LD HL, TXT_P2KEYS    ; "J/L/M "
        CALL PRTSTR
        LD HL, NAME_P2
        LD D, 24
        CALL PRTNAME
        LD A, SCR_GFXMK
        CALL PRTCHR

        ; --- Drain any held key, then wait for a fresh press ---
SR_DRAIN:
        IN A, (PORT_KB)
        AND 07FH
        JR NZ, SR_DRAIN
        CALL INKEY

        ; --- Cleanup: overwrite cols 1-39 with spaces; col 0 untouched ---
        LD B, 7
        CALL CLRGFXROW
        LD B, 10
        CALL CLRGFXROW
        LD B, 13
        CALL CLRGFXROW
        RET


; CLRGFXROW -- clear cols 1-39 of a graphics row with spaces
; Leaves col 0 (SCR_GFXMK border marker) untouched.
; In:  B = row (0..19)
; Clobbers: A, C, D, E, H, L  (B preserved by PUTCHR)
;
CLRGFXROW:
        LD C, 1
CLRGFX_LP:
        LD A, SCR_BLANK
        CALL PUTCHR
        INC C
        LD A, C
        CP 40
        JR NZ, CLRGFX_LP
        RET


; HUD_DRAW  -- redraw the score / timer line (row 20)
; In:  P_SCORE[0..1], TMIN_V, TSEC_V, MODE_TDIR
; Out: row 20 updated
; Clobbers: A, B, C, D, E, H, L
;
HUD_DRAW:
        LD B, 20
        CALL CLRROW

        ; P1 score at col 0
        LD B, 20
        LD C, 0
        CALL CSRSET
        LD HL, TXT_SCORE_P1  ; "P1:"
        CALL PRTSTR
        LD A, (P_SCORE)
        CALL PRNUM8

        ; P2 score at col 15
        LD B, 20
        LD C, 15
        CALL CSRSET
        LD HL, TXT_SCORE_P2  ; "P2:"
        CALL PRTSTR
        LD A, (P_SCORE+2)
        CALL PRNUM8

        ; Timer at col 30 only in time mode
        LD A, (MODE_TDIR)
        OR A
        RET Z
        LD B, 20
        LD C, 30
        CALL CSRSET
        LD HL, TXT_TIME      ; "TID:"
        CALL PRTSTR
        LD A, (TMIN_V)
        ADD A, '0'
        CALL PRTCHR          ; single-digit minutes
        LD A, ':'
        CALL PRTCHR
        LD A, (TSEC_V)
        CALL PRNUM8          ; 2-digit seconds
        RET


; PRNUM8  -- print 2-digit decimal value at cursor (00-99)
; In:  A = value (0-99)
; Out: two digits printed; CUR_COL advanced by 2
; Clobbers: A, B, C (PRTCHR clobbers B/C so PUSH BC protects tens/units)
;
PRNUM8:
        LD C, A              ; C = value
        LD B, 0              ; B = tens counter
PRNUM8_LP:
        LD A, C
        SUB 10
        JR C, PRNUM8_PR
        LD C, A
        INC B
        JR PRNUM8_LP
PRNUM8_PR:                   ; B = tens digit, C = units digit
        PUSH BC
        LD A, B
        ADD A, '0'
        CALL PRTCHR          ; PRTCHR clobbers B and C
        POP BC
        LD A, C
        ADD A, '0'
        CALL PRTCHR
        RET


; PLN_DRAW  -- erase old sprite, draw new sprite, update prev position
; In:  B = player index (0 or 1)
; Out: sprite drawn at (P_ROW[B], P_COL[B]); P_PROW/P_PCOL updated
; Clobbers: A, B, C, D, E, H, L
; Uses INBUF[0..1] as scratch for the two sprite char bytes.
; Player offset = N*2 (stride between player 0 and player 1 in each array).
; PUTCHR preserves B and C (row/col), so no PUSH BC needed there.
;
PLN_DRAW:
        LD A, B
        ADD A, A
        LD E, A
        LD D, 0              ; DE = player offset (0 or 2)

        ; --- Look up sprite chars for this player/direction ---
        LD HL, P_DIR
        ADD HL, DE
        LD A, (HL)           ; A = direction (0-7)
        ADD A, A             ; sprite byte index = dir * 2
        LD C, A
        LD B, 0              ; BC = sprite byte index
        LD HL, SPRITES_P1    ; default: P1 sprite table
        LD A, E
        OR A
        JR Z, PDRAW_SPR      ; E=0 means player 0 -> SPRITES_P1
        LD HL, SPRITES_P2
PDRAW_SPR:
        ADD HL, BC           ; HL -> sprite entry for this direction
        LD A, (HL)
        LD (INBUF), A        ; save char0 (left cell)
        INC HL
        LD A, (HL)
        LD (INBUF+1), A      ; save char1 (right cell)

        ; --- Erase old sprite (at P_PROW[N], P_PCOL[N]) ---
        LD HL, P_PROW
        ADD HL, DE
        LD B, (HL)           ; B = old row
        LD HL, P_PCOL
        ADD HL, DE
        LD C, (HL)           ; C = old col (left cell)
        LD A, SCR_BLANK
        PUSH DE
        CALL PUTCHR          ; erase left; PUTCHR preserves B (old row)
        POP DE
        LD HL, P_PCOL
        ADD HL, DE
        LD C, (HL)
        INC C                ; C = old col + 1 (right cell)
        LD A, SCR_BLANK
        PUSH DE
        CALL PUTCHR          ; erase right
        POP DE

        ; --- Draw new sprite (at P_ROW[N], P_COL[N]) ---
        LD HL, P_ROW
        ADD HL, DE
        LD B, (HL)           ; B = new row
        LD HL, P_COL
        ADD HL, DE
        LD C, (HL)           ; C = new col (left cell)
        LD A, (INBUF)        ; char0
        PUSH DE
        CALL PUTCHR          ; draw left; PUTCHR preserves B (new row)
        POP DE
        LD HL, P_COL
        ADD HL, DE
        LD C, (HL)
        INC C                ; C = new col + 1 (right cell)
        LD A, (INBUF+1)      ; char1
        PUSH DE
        CALL PUTCHR          ; draw right
        POP DE

        ; --- Update previous position ---
        LD HL, P_ROW
        ADD HL, DE
        LD A, (HL)
        LD HL, P_PROW
        ADD HL, DE
        LD (HL), A           ; P_PROW[N] = P_ROW[N]
        LD HL, P_COL
        ADD HL, DE
        LD A, (HL)
        LD HL, P_PCOL
        ADD HL, DE
        LD (HL), A           ; P_PCOL[N] = P_COL[N]
        RET



; MAIN_LOOP  -- game loop, runs forever (Ctrl-C -> monitor via C code)
; Frame rate: 2 CLKLO ticks = 40ms (~25fps).
; Order: delay > keys > move P1 > move P2 > draw P1 > draw P2 > bullets > timer
;
MAIN_LOOP:
        LD HL, 5
        CALL DLYLOOP         ; wait 5 x 20ms = 100ms
        CALL TRN_TICK        ; decrement turn cooldowns each frame
        CALL KEY_POLL
        LD B, 0
        CALL PLN_MOVE
        LD B, 1
        CALL PLN_MOVE
        LD B, 0
        CALL PLN_DRAW
        LD B, 1
        CALL PLN_DRAW
        CALL BUL_MOVE
        CALL TIMER_TICK
        JR MAIN_LOOP


; KEY_POLL  -- read keyboard port once and act on it (non-blocking)
; In:  nothing
; Out: P_DIR[0..1] updated on turn keys; fire handled by BUL_INIT (Phase 11)
; Clobbers: A
; The keyboard port returns the key with bit7 set, or 0 if no key.
; Direction wrap uses AND 07H: (0-1)&07 = 07 (N->NW), (7+1)&07 = 0 (NW->N).
;
KEY_POLL:
        IN A, (PORT_KB)
        AND 07FH             ; strip bit7 framing flag
        RET Z                ; no key pressed
        AND 0DFH             ; force uppercase (a->A, d->D, x->X, j->J, l->L, m->M)

        CP P1_LEFT           ; 'A'
        JR NZ, KP_P1R
        LD A, (P_TDELAY)     ; P1 cooldown active?
        OR A
        RET NZ               ; yes -- ignore turn key
        LD A, (P_DIR)
        DEC A
        AND 07H              ; wrap 0-1 -> 7
        LD (P_DIR), A
        LD A, TURN_DELAY
        LD (P_TDELAY), A     ; start cooldown
        RET
KP_P1R:
        CP P1_RIGHT          ; 'D'
        JR NZ, KP_P1F
        LD A, (P_TDELAY)
        OR A
        RET NZ
        LD A, (P_DIR)
        INC A
        AND 07H              ; wrap 7+1 -> 0
        LD (P_DIR), A
        LD A, TURN_DELAY
        LD (P_TDELAY), A
        RET
KP_P1F:
        CP P1_FIRE           ; 'X'
        JR NZ, KP_P2L
        LD B, 0
        CALL BUL_INIT
        RET
KP_P2L:
        CP P2_LEFT           ; 'J'
        JR NZ, KP_P2R
        LD A, (P_TDELAY+2)   ; P2 cooldown active?
        OR A
        RET NZ
        LD A, (P_DIR+2)
        DEC A
        AND 07H
        LD (P_DIR+2), A
        LD A, TURN_DELAY
        LD (P_TDELAY+2), A
        RET
KP_P2R:
        CP P2_RIGHT          ; 'L'
        JR NZ, KP_P2F
        LD A, (P_TDELAY+2)
        OR A
        RET NZ
        LD A, (P_DIR+2)
        INC A
        AND 07H
        LD (P_DIR+2), A
        LD A, TURN_DELAY
        LD (P_TDELAY+2), A
        RET
KP_P2F:
        CP P2_FIRE           ; 'M'
        RET NZ
        LD B, 1
        CALL BUL_INIT
        RET


; PLN_MOVE  -- advance one plane one step in its current direction
; In:  B = player index (0 or 1)
; Out: P_ROW[B], P_COL[B] updated; wrapped in play area rows 1-19, cols 1-38
; Clobbers: A, B, C, D, E, H, L
; Row/col wrap: if result = 0 (underflow past top/left) -> wrap to max;
;               if result >= limit (overflow past bottom/right) -> wrap to 1.
; Signed delta from DELTA_TAB: 0xFF = -1 (N/W), 0x00 = no movement,
; 0x01 = +1 (S/E).  8-bit addition handles sign automatically.
;
PLN_MOVE:
        LD A, B
        ADD A, A
        LD E, A
        LD D, 0              ; DE = player offset (0 or 2)

        ; Load delta from DELTA_TAB indexed by direction
        LD HL, P_DIR
        ADD HL, DE
        LD A, (HL)           ; A = direction (0-7)
        ADD A, A             ; A = dir * 2 (byte offset)
        LD C, A
        LD B, 0              ; BC = delta table byte offset
        LD HL, DELTA_TAB
        ADD HL, BC
        LD B, (HL)           ; B = row_delta (0xFF/-1, 0, or 0x01/+1)
        INC HL
        LD C, (HL)           ; C = col_delta

        ; Update row (play area: 1..19)
        LD HL, P_ROW
        ADD HL, DE
        LD A, (HL)
        ADD A, B             ; row + row_delta (8-bit, wraps through 0)
        OR A
        JR Z, PMOV_ROWLO     ; result 0 -> underflow -> wrap to 19
        CP 20
        JR C, PMOV_ROWOK     ; 1..19 -> valid
        LD A, 1              ; >19 -> overflow -> wrap to 1
        JR PMOV_ROWOK
PMOV_ROWLO:
        LD A, 19
PMOV_ROWOK:
        LD (HL), A           ; P_ROW[N] = new row

        ; Update col (play area: 2..37; right sprite cell stays in 3..38)
        LD HL, P_COL
        ADD HL, DE
        LD A, (HL)
        ADD A, C             ; col + col_delta
        OR A
        JR Z, PMOV_COLLO     ; result 0 -> underflow -> wrap to far side
        CP 38
        JR C, PMOV_COLDONE   ; 2..37 -> valid
        LD A, 2              ; >37 -> overflow -> wrap to col 2
        JR PMOV_COLDONE
PMOV_COLLO:
        LD A, 37             ; underflow -> wrap to col 37
PMOV_COLDONE:
        LD (HL), A           ; P_COL[N] = new col
        RET



; BUL_INIT  -- fire a bullet from player B's plane
; In:  B = player/bullet index (0 or 1)
; Out: bullet armed at plane's position/direction; no-op if already active
; Clobbers: A, D, E, H, L
; B_ACTIVE[B] stride 1; all other bullet arrays stride 2.
;
BUL_INIT:
        LD D, 0
        LD E, B
        LD HL, B_ACTIVE
        ADD HL, DE           ; HL -> B_ACTIVE[B] (stride 1)
        LD A, (HL)
        OR A
        RET NZ               ; bullet already in flight -- don't re-fire
        LD (HL), 1           ; mark active

        LD A, B
        ADD A, A
        LD E, A              ; DE = boff2 = B*2 (stride-2 offset)

        LD HL, P_DIR         ; copy direction
        ADD HL, DE
        LD A, (HL)
        LD HL, B_DIR
        ADD HL, DE
        LD (HL), A

        LD HL, P_ROW         ; copy row to B_ROW and B_PROW
        ADD HL, DE
        LD A, (HL)
        LD HL, B_ROW
        ADD HL, DE
        LD (HL), A
        LD HL, B_PROW
        ADD HL, DE
        LD (HL), A

        LD HL, P_COL         ; copy col to B_COL and B_PCOL
        ADD HL, DE
        LD A, (HL)
        LD HL, B_COL
        ADD HL, DE
        LD (HL), A
        LD HL, B_PCOL
        ADD HL, DE
        LD (HL), A

        ; brief fire sound: SH1 for P1, SH2 for P2
        LD A, B
        OR A
        LD A, SOUND_SH1
        JR Z, BI_SND
        LD A, SOUND_SH2
BI_SND:
        CALL SNDON
        LD HL, 1
        CALL DLYLOOP
        CALL SNDOFF
        RET


; BUL_MOVE  -- move both active bullets one frame
; Called each frame; processes bullet 0 then bullet 1.
; Clobbers: A, B, C, D, E, H, L
;
BUL_MOVE:
        LD B, 0
        CALL BUL_STEP        ; process bullet 0
        LD B, 1
        ; fall through to BUL_STEP for bullet 1


; BUL_STEP  -- move one active bullet BUL_STEPS cells, with bounds + hit test
; In:  B = bullet index (0 or 1)
; Clobbers: A, B, C, D, E, H, L
; D=0, E=boff2 held across the step loop; PUSH DE / POP DE around PUTCHR.
; Step counter kept in INBUF+2 (safe: PLN_DRAW uses INBUF[0..1] only).
;
BUL_STEP:
        LD D, 0
        LD E, B
        LD HL, B_ACTIVE
        ADD HL, DE           ; stride-1 active flag
        LD A, (HL)
        OR A
        RET Z                ; not active

        LD A, E
        ADD A, A
        LD E, A              ; E = boff2 = B*2 (D still 0)

        ; Erase last drawn position
        LD HL, B_PROW
        ADD HL, DE
        LD B, (HL)           ; B = old row
        LD HL, B_PCOL
        ADD HL, DE
        LD C, (HL)           ; C = old col
        LD A, SCR_BLANK
        PUSH DE
        CALL PUTCHR
        POP DE

        ; Run BUL_STEPS advance steps
        LD A, BUL_STEPS
        LD (INBUF+2), A

BSTEP_LP:
        ; Load movement delta for current bullet direction
        LD HL, B_DIR
        ADD HL, DE
        LD A, (HL)           ; A = direction 0-7
        ADD A, A             ; A = dir*2 (table offset)
        LD C, A
        LD B, 0
        LD HL, DELTA_TAB
        ADD HL, BC
        LD B, (HL)           ; B = row_delta
        INC HL
        LD C, (HL)           ; C = col_delta

        ; Advance row
        LD HL, B_ROW
        ADD HL, DE
        LD A, (HL)
        ADD A, B             ; new_row = old_row + row_delta
        OR A
        JR Z, BSTEP_OFF      ; row 0 --> out of play area
        CP 20
        JR NC, BSTEP_OFF     ; row > 19 --> out of play area
        LD (HL), A           ; store only when valid

        ; Advance col
        LD HL, B_COL
        ADD HL, DE
        LD A, (HL)
        ADD A, C             ; new_col = old_col + col_delta
        OR A
        JR Z, BSTEP_OFF      ; col 0 --> out of play area
        CP 39
        JR NC, BSTEP_OFF     ; col > 38 --> out of play area
        LD (HL), A           ; store only when valid

        ; Hit test against opponent sprite
        PUSH DE
        CALL HIT_TEST
        POP DE
        JR Z, BSTEP_HIT

        ; Decrement step counter and continue
        LD A, (INBUF+2)
        DEC A
        LD (INBUF+2), A
        JR NZ, BSTEP_LP

        ; Draw bullet at final position
        LD HL, B_ROW
        ADD HL, DE
        LD B, (HL)
        LD HL, B_COL
        ADD HL, DE
        LD C, (HL)
        LD A, '('            ; small mosaic dot -- bullet char
        PUSH DE
        CALL PUTCHR
        POP DE

        ; Update B_PROW / B_PCOL for next erase
        LD HL, B_ROW
        ADD HL, DE
        LD A, (HL)
        LD HL, B_PROW
        ADD HL, DE
        LD (HL), A
        LD HL, B_COL
        ADD HL, DE
        LD A, (HL)
        LD HL, B_PCOL
        ADD HL, DE
        LD (HL), A
        RET

BSTEP_HIT:
        LD A, E
        SRL A                ; bullet index = boff2 >> 1  (0 or 1)
        LD B, A
        CALL BUL_HIT
        RET
BSTEP_OFF:
        LD A, E
        SRL A
        LD B, A
        CALL BUL_OFF
        RET


; HIT_TEST  -- check if bullet (E=boff2) hit the opponent's 2-cell sprite
; In:  D=0, E=bullet boff2 (0 or 2); caller does PUSH/POP DE around call
; Out: Z=1 if bullet row/col overlaps opponent's left or right sprite cell
; Clobbers: A, B, C, D, E, H, L  (D/E restored by caller)
; 
HIT_TEST:
        LD B, E              ; B = bullet boff2 (saved across E changes)
        LD A, E
        XOR 002H
        LD C, A              ; C = opp_boff2  (0 XOR 2 = 2, 2 XOR 2 = 0)

        ; Row must match
        LD HL, B_ROW
        ADD HL, DE           ; D=0, E=bullet boff2
        LD A, (HL)           ; A = bullet_row
        LD HL, P_ROW
        LD E, C              ; DE = (0, opp_boff2)
        ADD HL, DE
        CP (HL)              ; bullet_row == opp_row?
        RET NZ

        ; Col must hit left or right sprite cell
        LD HL, B_COL
        LD E, B              ; DE = (0, bullet boff2)
        ADD HL, DE
        LD A, (HL)           ; A = bullet_col
        LD HL, P_COL
        LD E, C              ; DE = (0, opp_boff2)
        ADD HL, DE
        LD B, (HL)           ; B = opp_left_col
        CP B                 ; bullet on left sprite cell?
        RET Z
        INC B                ; B = opp_right_col
        CP B                 ; bullet on right sprite cell?
        RET                  ; Z=1 if hit, Z=0 if not


; BUL_OFF  -- deactivate bullet B and erase it from the screen
; In:  B = bullet index (0 or 1)
; Clobbers: A, B, C, D, E, H, L
;
BUL_OFF:
        LD D, 0
        LD E, B
        LD HL, B_ACTIVE
        ADD HL, DE           ; stride-1
        LD (HL), 0           ; B_ACTIVE[B] = 0

        LD A, B
        ADD A, A
        LD E, A              ; DE = boff2
        LD HL, B_PROW
        ADD HL, DE
        LD B, (HL)           ; B = last drawn row
        LD HL, B_PCOL
        ADD HL, DE
        LD C, (HL)           ; C = last drawn col
        LD A, SCR_BLANK
        CALL PUTCHR
        RET


; TIMER_TICK  -- count game frames, update TSEC_V/TMIN_V once per second
; In:  J_PREV = frame down-counter (10 --> 0 = 1 second at 100ms/frame)
; Out: seconds/minutes updated; HUD redrawn on each second boundary
; Clobbers: A, H, L
; Time mode only: TMIN_V stops at 0 (Phase 12 will call END_GAME there).
;
TIMER_TICK:
        LD HL, J_PREV
        DEC (HL)             ; decrement frame counter
        RET NZ               ; not a second boundary yet

        LD (HL), 10          ; reset: 10 frames x 100ms = 1 second

        LD A, (MODE_TDIR)    ; only count down in time mode
        OR A
        RET Z                ; score mode: nothing to do

        LD A, (TSEC_V)
        OR A
        JR Z, TTICK_MIN      ; seconds hit 0 -- handle minute boundary
        DEC A
        LD (TSEC_V), A
        JR TTICK_HUD

TTICK_MIN:
        LD A, (TMIN_V)
        OR A
        JR Z, TTICK_GAMEOVER ; already 0:00 -- time up
        DEC A
        LD (TMIN_V), A
        LD A, 59
        LD (TSEC_V), A       ; count down from :59 into the new minute
        JR TTICK_HUD

TTICK_GAMEOVER:
        XOR A
        LD (TSEC_V), A       ; freeze display at 0:00
        CALL HUD_DRAW        ; show 0:00
        JP END_GAME          ; time is up
TTICK_HUD:
        CALL HUD_DRAW
        RET


; TRN_TICK  -- decrement turn-cooldown counters for both players each frame
; In:  P_TDELAY[0], P_TDELAY[2] = remaining frames before player can turn
; Out: each counter decremented if > 0; both 0 when cooldown expired
; Clobbers: A
;
TRN_TICK:
        LD A, (P_TDELAY)
        OR A
        JR Z, TTK_P2
        DEC A
        LD (P_TDELAY), A
TTK_P2:
        LD A, (P_TDELAY+2)
        OR A
        RET Z
        DEC A
        LD (P_TDELAY+2), A
        RET



; BUL_HIT  -- bullet hit opponent: boom, score, win check
; In:  B = bullet index (0=P1 shot, 1=P2 shot)
; Clobbers: A, B, C, D, E, H, L
; 
BUL_HIT:
        LD A, B
        LD (INBUF+3), A      ; save shooter index (B clobbered by BUL_OFF)
        CALL BUL_OFF         ; erase bullet

        ; locate opponent's 2-cell sprite position
        LD A, (INBUF+3)
        XOR 001H             ; opp index = shooter XOR 1
        ADD A, A             ; opp_poff2 = opp_index * 2
        LD E, A
        LD D, 0
        LD HL, P_ROW
        ADD HL, DE
        LD B, (HL)           ; B = opp row
        LD HL, P_COL
        ADD HL, DE
        LD C, (HL)           ; C = opp left col
        LD A, B
        LD (INBUF+4), A      ; save opp row for frame 2
        LD A, C
        LD (INBUF+5), A      ; save opp col for frame 2

        ; start sound now so it plays through both animation frames
        LD A, SOUND_HIT
        CALL SNDON

        ; frame 1: solid impact flash ~160ms (0x7F = all 6 sub-pixels, bit5 set)
        LD A, 07FH
        CALL PUTCHR
        INC C
        LD A, 07FH
        CALL PUTCHR
        LD HL, 15            ; ~300ms
        CALL DLYLOOP

        ; frame 2: scattered debris ~300ms
        LD A, (INBUF+4)
        LD B, A
        LD A, (INBUF+5)
        LD C, A
        LD A, 'f'            ; 066H: TR+ML+BR scattered piece
        CALL PUTCHR
        INC C
        LD A, '!'            ; 021H: TL-only fragment
        CALL PUTCHR
        LD HL, 15            ; ~300ms
        CALL DLYLOOP
        CALL SNDOFF

        ; increment shooter's score
        LD A, (INBUF+3)
        ADD A, A             ; shooter_poff2
        LD E, A
        LD D, 0
        LD HL, P_SCORE
        ADD HL, DE
        INC (HL)

        CALL HUD_DRAW

        ; win check (score mode only; time mode is handled by TTICK_GAMEOVER)
        LD A, (MODE_SLIM)
        OR A
        JR Z, BH_RESET       ; time mode: no win check, just reset planes

        LD A, (INBUF+3)
        ADD A, A
        LD E, A
        LD D, 0
        LD HL, P_SCORE
        ADD HL, DE
        LD A, (HL)
        CP SCORE_LIM
        JP NC, END_GAME      ; reached limit: game over

BH_RESET:
        CALL PLN_RESET
        RET


; PLN_RESET  -- reset both planes to start positions after a hit
; Clears play area, redraws border/HUD/planes, pauses before continuing.
;
PLN_RESET:
        XOR A
        LD (B_ACTIVE), A     ; deactivate bullet 0
        LD (B_ACTIVE+1), A   ; deactivate bullet 1
        LD (P_TDELAY), A     ; clear turn cooldowns
        LD (P_TDELAY+2), A

        ; P1: row=5, col=5, dir=2 (East)
        LD A, 2
        LD (P_DIR), A
        LD A, 5
        LD (P_ROW), A
        LD (P_COL), A
        LD (P_PROW), A
        LD (P_PCOL), A

        ; P2: row=14, col=34, dir=6 (West)
        LD A, 6
        LD (P_DIR+2), A
        LD A, 14
        LD (P_ROW+2), A
        LD (P_PROW+2), A
        LD A, 34
        LD (P_COL+2), A
        LD (P_PCOL+2), A

        ; redraw play area cleanly
        CALL CLRSCR
        LD B, 0
        LD C, 19
        CALL DRAWBDR
        CALL HUD_DRAW
        LD B, 0
        CALL PLN_DRAW
        LD B, 1
        CALL PLN_DRAW

        ; pause before resuming so players can reorient
        LD HL, 75            ; 75 x 20ms = 1.5 seconds
        CALL DLYLOOP
        RET


; END_GAME  -- announce game over, show cup (winner) or draw, ask play again
; BASIC lines 222-240: sound + asterisk box, then winner cup or draw text.
; In:  P_SCORE[0], P_SCORE[2] set
;
END_GAME:
        EI

        ; --- Determine winner, store in WINNER_V before any display ---
        LD A, (P_SCORE)
        LD B, A
        LD A, (P_SCORE+2)
        CP B
        JR Z, EG_SET_DRAW    ; equal scores -> draw
        JR C, EG_SET_P1WIN   ; P2 < P1   -> P1 wins
        LD A, 1              ; P2 > P1   -> P2 wins
        LD (WINNER_V), A
        JR EG_ANNOUNCE
EG_SET_DRAW:
        LD A, 0FFH
        LD (WINNER_V), A
        JR EG_ANNOUNCE
EG_SET_P1WIN:
        XOR A
        LD (WINNER_V), A

        ; --- Lines 222-231: CLRSCR, box + SOUND_WIN ~1s, SOUND_INTRO ~2.5s ---
EG_ANNOUNCE:
        CALL SNDOFF
        CALL CLRSCR
        LD A, SOUND_WIN
        CALL SNDON
        LD B, 11
        LD C, 8
        CALL CSRSET
        LD HL, TXT_BOX_TOP
        CALL PRTSTR
        LD B, 12
        LD C, 8
        CALL CSRSET
        LD HL, TXT_BOX_MID
        CALL PRTSTR
        LD B, 13
        LD C, 8
        CALL CSRSET
        LD HL, TXT_BOX_TOP
        CALL PRTSTR
        LD HL, 50            ; ~1 s
        CALL DLYLOOP
        CALL SNDOFF
        LD A, SOUND_INTRO
        CALL SNDON
        LD HL, 125           ; ~2.5 s
        CALL DLYLOOP
        CALL SNDOFF
        CALL CLRSCR
        LD HL, 40            ; ~0.8 s
        CALL DLYLOOP

        ; --- Branch: winner gets cup, draw gets OAVGJORT ---
        LD A, (WINNER_V)
        CP 0FFH
        JR Z, EG_DRAW_SHOW

        ; --- Lines 234-240: "VINNARE BLEV..." + cup mosaic ---
        LD B, 4
        LD C, 12
        CALL CSRSET
        LD HL, TXT_WIN_HDR   ; "VINNARE BLEV..."
        CALL PRTSTR
        LD B, 10             ; rows 10-18 -> graphics mode (line 235)
        LD C, 18
        CALL DRAWBDR
        LD B, 13             ; cup top rim (line 236)
        LD C, 12
        CALL CSRSET
        LD HL, CUP_R0
        CALL PRTSTR
        LD B, 14             ; cup body   (line 237)
        LD C, 12
        CALL CSRSET
        LD HL, CUP_R1
        CALL PRTSTR
        LD B, 15             ; cup stem   (line 238)
        LD C, 17
        CALL CSRSET
        LD HL, CUP_R2
        CALL PRTSTR
        LD B, 16             ; cup base   (line 239)
        LD C, 17
        CALL CSRSET
        LD HL, CUP_R2
        CALL PRTSTR
        LD HL, 90            ; ~1.8 s     (line 240)
        CALL DLYLOOP

        ; --- Winner name + " VINNER!" at row 6 ---
        LD A, (WINNER_V)
        PUSH AF              ; save 0=P1 / 1=P2 across CSRSET (clobbers A)
        LD B, 6
        LD C, 4
        CALL CSRSET
        POP AF
        OR A
        JR NZ, EG_SHOW_P2
        LD HL, NAME_P1
        LD D, 20
        CALL PRTNAME
        JR EG_SHOW_SFX
EG_SHOW_P2:
        LD HL, NAME_P2
        LD D, 20
        CALL PRTNAME
EG_SHOW_SFX:
        CALL PLY_TUNE
        JP PLAY_AGAIN

EG_DRAW_SHOW:
        LD B, 11
        LD C, 16
        CALL CSRSET
        LD HL, TXT_DRAW      ; "OAVGJORT"
        CALL PRTSTR
        JP PLAY_AGAIN


; PLY_TUNE  -- victory tune from original 1981 BASIC (lines 263-274)
; BASIC structure (lines 265-270):
;   READ A0%,A1% : A1%=A1%+10%        ; outer count, raw delay+10 = actual delay
;   FOR I%=1 TO A0%                    ; outer repeat (integer I%)
;     OUT 6,121                        ; sound ON
;     FOR J%=1 TO A1% : NEXT J%       ; pitch delay -- INTEGER loop, ~3x faster than float
;     OUT 6,0 : NEXT I%               ; sound OFF + NEXT I% overhead (~1 int iter)
;   FOR T=1 TO 10 : NEXT T            ; inter-note silence -- FLOAT loop, slower
; J%/I% loops → INT_SCALE; T loop → TUNE_SCALE (float).  Adjust both EQUs if pitch wrong.
; Uses direct OUT rather than CALL SNDON (avoids chip reset mid-loop).
;
PLY_TUNE:
        LD IX, TUNE_DATA
PTLP_NOTE:
        LD B, (IX+0)         ; B = outer repeat count (0 = end)
        LD A, B
        OR A
        RET Z
        LD C, (IX+1)         ; C = inner delay (A1 value from original BASIC)
        LD DE, 2
        ADD IX, DE           ; advance tune pointer before DJNZ clobbers B
PTLP_OUTER:
        LD A, SOUND_TUNE
        OUT (PORT_SND), A    ; sound ON -- direct OUT, no chip reset
        LD E, C              ; E counts down inner delay
PTLP_INNER:
        LD A, INT_SCALE      ; integer J% speed: ~0.33ms per step (3x faster than float)
PTLP_WASTE:
        DEC A
        JR NZ, PTLP_WASTE
        DEC E
        JR NZ, PTLP_INNER
        XOR A
        OUT (PORT_SND), A    ; sound OFF
        LD A, INT_SCALE      ; OFF delay: ~0.33ms = 1 integer iter (BASIC "NEXT I%" overhead)
PTLP_OFF:
        DEC A
        JR NZ, PTLP_OFF
        DJNZ PTLP_OUTER
        LD E, 10             ; inter-note gap: 10 silent steps (BASIC line 270)
PTLP_GAP:
        LD A, TUNE_SCALE
PTLP_GAPW:
        DEC A
        JR NZ, PTLP_GAPW
        DEC E
        JR NZ, PTLP_GAP
        JR PTLP_NOTE


; PLAY_AGAIN  -- ask "SPELA IGEN? (J/N)" and restart or exit
; J = restart with same names/mode.  N = full intro (new names, new mode).
; Drains any keys still held at game-end before reading the answer.
;
PLAY_AGAIN:
        LD B, 22
        LD C, 0
        CALL CSRSET
        LD HL, TXT_AGAIN
        CALL PRTSTR
PA_DRAIN:
        IN A, (PORT_KB)      ; wait for all keys to be released
        AND 07FH
        JR NZ, PA_DRAIN
PA_WAIT:
        CALL INKEY
        AND 0DFH             ; force uppercase
        CP 'J'
        JP Z, RESTART_GAME
        CP 'N'
        JP Z, COLD_START
        JR PA_WAIT           ; ignore anything else, wait for J or N



; SCREEN PRIMITIVES

; CSRSET  -- set cursor position
; In:  B = row (0..23),  C = col (0..39)
; Out: CUR_ROW, CUR_COL updated
; Clobbers: A
;
CSRSET:
        LD A, B
        LD (CUR_ROW), A
        LD A, C
        LD (CUR_COL), A
        RET


; PRTCHR  -- write character at cursor, advance column
; In:  A = character to write; CUR_ROW/CUR_COL = position
; Out: character written to screen RAM; CUR_COL incremented
; Clobbers: A, B, C (loaded from CUR_ROW/CUR_COL), D, E, HL
; Note: loads B/C from RAM before calling PUTCHR, so caller's
;       B/C are NOT preserved -- wrap with PUSH BC/POP BC if needed.
;
PRTCHR:
        PUSH AF          ; save the character
        LD A, (CUR_ROW)
        LD B, A          ; B = row for PUTCHR
        LD A, (CUR_COL)
        LD C, A          ; C = col for PUTCHR
        POP AF           ; A = character again
        CALL PUTCHR      ; write to screen RAM
        LD A, (CUR_COL)
        INC A
        LD (CUR_COL), A  ; advance cursor right
        RET


; PRTSTR  -- print null-terminated string
; In:  HL = pointer to string (null-terminated, last byte = 0)
;      CUR_ROW/CUR_COL = starting position
; Out: string written to screen; CUR_COL advanced past last char
; Clobbers: A, B, C, D, E, HL
; Note: PUSH/POP HL required because PUTCHR -> SCRADDR clobbers HL.
;
PRTSTR:
        LD A, (HL)
        OR A             ; test for null terminator
        RET Z
        PUSH HL          ; protect string pointer across PRTCHR call
        CALL PRTCHR
        POP HL
        INC HL           ; next character
        JR PRTSTR


; PRTNAME -- print null-terminated string at cursor, up to D chars
; In:  HL = string address, D = max characters to print
; Out: cursor advanced past printed chars
; Clobbers: A, B, C, D, HL  (B/C clobbered by PRTCHR; use D not B for count)
;
PRTNAME:
        LD A, D
        OR A
        RET Z            ; limit reached
        LD A, (HL)
        OR A
        RET Z            ; null terminator
        PUSH HL
        CALL PRTCHR
        POP HL
        INC HL
        DEC D
        JR PRTNAME


; CLRSCR  -- clear all 24 screen rows
; Sets col 0 of each row to 0x00 (not a space -- ensures no accidental
; graphics-mode activation) and cols 1-39 to SCR_BLANK (0x20 = space).
; In:  nothing
; Out: entire screen blanked
; Clobbers: A, B, C, D, E, H, L
;
CLRSCR:
        LD B, 24         ; row counter
        LD HL, ROWTAB    ; HL walks through the row-address table
CLRLP:
        LD E, (HL)       ; fetch low byte of row start address
        INC HL
        LD D, (HL)       ; fetch high byte
        INC HL
        PUSH BC          ; save row counter and ROWTAB pointer
        PUSH HL
        EX DE, HL        ; HL = screen RAM address for this row
        LD (HL), 000H    ; col 0 = 0x00 (neutral, no graphics trigger)
        INC HL
        LD B, 39         ; 39 remaining columns
CLRL2:
        LD (HL), SCR_BLANK  ; write space to cols 1..39
        INC HL
        DJNZ CLRL2
        POP HL           ; restore ROWTAB pointer
        POP BC           ; restore row counter
        DJNZ CLRLP
        RET


; DRAWBDR  -- write SCR_GFXMK to col 0 of a range of rows
; In:  B = first row,  C = last row  (inclusive)
; Out: col 0 of rows B..C set to 0x97, enabling graphics mode for
;      those rows.  On the ABC80, writing 0x97 to col 0 means the
;      renderer sees (0x97 & 0x7F) = 0x17, which is the teletext
;      graphics-mode trigger byte.
; Clobbers: A, D, E  (B and C restored on return)
; Note: PUSH DE / POP DE required because PUTCHR -> SCRADDR
;       clobbers D and E (used as scratch for address arithmetic).
; 
DRAWBDR:
        PUSH BC          ; preserve caller's B/C
        LD D, B          ; D = current row (walking from B to C)
        LD E, C          ; E = last row
DBRLP:
        LD B, D          ; B = row argument for PUTCHR
        LD C, 0          ; C = col 0
        LD A, SCR_GFXMK  ; 0x97 -- graphics mode marker
        PUSH DE          ; SCRADDR clobbers D/E; protect row range
        CALL PUTCHR
        POP DE           ; restore D=current_row, E=last_row
        LD A, D
        CP E             ; reached the last row?
        JR Z, DBREND
        INC D            ; advance to next row
        JR DBRLP
DBREND:
        POP BC
        RET


; CLRROW  -- clear a single screen row to blank
; In:  B = row (0..23)
; Out: col 0 = 0x00, cols 1..39 = SCR_BLANK (space)
; Clobbers: A, C, D, E, HL  (B preserved -- PUTCHR preserves B and C,
;           so the column counter in C survives each PUTCHR call)
; Do NOT call on graphics rows 0-19 after DRAWBDR: it overwrites the
; graphics-mode marker at col 0.  Use only for text rows 20-23.
; 
CLRROW:
        LD C, 0
        LD A, 0          ; col 0: neutral (no graphics-mode trigger)
        CALL PUTCHR
        LD C, 1          ; fill cols 1..39 with spaces
CLRROW_LP:
        LD A, SCR_BLANK
        CALL PUTCHR
        INC C
        LD A, C
        CP 40            ; done when C reaches 40 (past last col)
        JR NZ, CLRROW_LP
        RET


; SOUND + DELAY

; SNDON  -- activate sound chip with given value
; In:  A = SN76477 register byte
; Out: sound starts
; Clobbers: A
; Protocol: first write 0 (disables chip), then write A (re-enables).
;           The chip triggers on the 0->1 transition of its enable bit.
; 
SNDON:
        PUSH AF          ; save desired value
        XOR A            ; A = 0 -- disable chip first (clears all bits)
        OUT (PORT_SND), A
        POP AF           ; A = desired value
        OUT (PORT_SND), A ; write value -- bit0=0 re-enables chip
        RET


; SNDOFF  -- silence the sound chip
; Out: sound stopped
; Clobbers: A
; 
SNDOFF:
        XOR A            ; A = 0 -- chip disabled, all bits clear
        OUT (PORT_SND), A
        RET


; DLYLOOP  -- delay for HL ticks of the 50Hz system clock
; In:  HL = number of 20ms ticks to wait  (HL=50 -> 1 second)
; Out: HL = 0
; Clobbers: A, B, D, E
; Uses CLKLO (0xFDF0), the low byte of the ABC80 50Hz counter.
; The counter is incremented by the firmware ISR on every strobe.
; Each iteration of the outer loop (DLYO) waits for the low byte to
; change from its current value, which takes exactly one 20ms tick.
; 
DLYLOOP:
        LD DE, CLKLO     ; DE = address of clock low byte
DLYO:
        LD A, (DE)       ; snapshot current clock value
        LD B, A          ; B = reference value to wait for change
DLYI:
        LD A, (DE)       ; re-read clock
        CP B             ; same as snapshot?
        JR Z, DLYI       ; yes -- keep polling (busy-wait)
                         ; no  -- one 20ms tick has elapsed
        DEC HL           ; count down remaining ticks
        LD A, H
        OR L
        JR NZ, DLYO      ; more ticks needed -- re-snapshot clock
        RET              ; HL reached zero -- done


; LOW-LEVEL SCREEN ACCESS

; PUTCHR  -- write character to screen RAM
; In:  B = row (0..23),  C = col (0..39),  A = character byte
; Out: character stored in ABC80 screen RAM
; Clobbers: A, D, E, HL  (B and C preserved)
; 
PUTCHR:
        PUSH AF          ; save character (SCRADDR clobbers A)
        CALL SCRADDR     ; HL = screen RAM address for (B, C)
        POP AF
        LD (HL), A       ; write character
        RET


; GETCHR  -- read character from screen RAM
; In:  B = row (0..23),  C = col (0..39)
; Out: A = character byte at that position
; Clobbers: A, D, E, HL  (B and C preserved)
; 
GETCHR:
        CALL SCRADDR
        LD A, (HL)
        RET


; SCRADDR  -- compute screen RAM address for a (row, col) position
; In:  B = row (0..23),  C = col (0..39)
; Out: HL = address in ABC80 screen RAM
; Clobbers: A, D, E  (B and C preserved)
; The ABC80 screen RAM is NOT laid out linearly by row.  The 24 rows
; are interleaved across four 40-byte blocks within the 0x7C00-0x7FFF
; range (a quirk of the ABC80 video hardware to save RAM).  ROWTAB
; holds the actual start address for each of the 24 rows in the
; correct order.
; 
SCRADDR:
        LD HL, ROWTAB    ; base of address table
        LD A, B          ; A = row index
        ADD A, A         ; A = row * 2  (each table entry is 2 bytes)
        LD D, 0
        LD E, A          ; DE = byte offset into ROWTAB
        ADD HL, DE       ; HL -> entry for this row
        LD A, (HL)       ; fetch low byte of row start address
        INC HL
        LD H, (HL)       ; fetch high byte
        LD L, A          ; HL = start of row in screen RAM
        LD D, 0
        LD E, C          ; DE = col offset
        ADD HL, DE       ; HL = final screen RAM address
        RET


; INPUT ROUTINES

; INKEY  -- wait for a keypress and return its ASCII code
; In:  nothing  (busy-waits)
; Out: A = ASCII code (bit 7 stripped)
; Clobbers: A
; The ABC80 keyboard port returns the current key with bit7 set, or
; 0 if no key is held.  Reading twice drains the second latch slot
; to prevent auto-repeat triggering immediately on the next call.
; 
INKEY:
        IN A, (PORT_KB)
        AND 07FH         ; strip bit7 (framing bit, not part of ASCII)
        JR Z, INKEY      ; 0 means no key -- keep polling
        PUSH AF          ; save the key code
        IN A, (PORT_KB)  ; drain second read slot (prevents auto-repeat)
        POP AF           ; A = key code (bit7 already stripped)
        RET


; INLINE_V  -- read a line of text into a buffer with echo and backspace
; In:  HL = destination buffer address
;      B  = maximum number of characters to accept (not counting null)
;      CUR_ROW/CUR_COL = echo position on screen
; Out: buffer contains null-terminated string of up to B characters
; Clobbers: A, B, C, D, HL
; Handles: CR (0x0D) = done/accept,  BS (0x08) = delete last character.
; Note: PUSH BC / POP BC required around PRTCHR calls because PRTCHR
;       loads B and C from CUR_ROW/CUR_COL, destroying the count in C
;       and the max-length in B.
; 
INLINE_V:
        LD C, 0          ; C = current character count
INLP:
        CALL INKEY
        CP 00DH          ; CR -- end of input
        JR Z, INLDONE
        CP 008H          ; Backspace
        JR Z, INLBS
        LD D, A          ; save character in D while we check length
        LD A, C
        CP B             ; at maximum length?
        JR NC, INLP      ; yes -- ignore this character
        LD A, D          ; restore character
        LD (HL), A       ; store in buffer
        PUSH BC          ; protect count/max across PRTCHR
        PUSH HL          ; protect buffer pointer
        CALL PRTCHR      ; echo to screen
        POP HL
        POP BC
        INC HL           ; advance buffer pointer
        INC C            ; increment count
        JR INLP

INLBS:
        LD A, C
        OR A             ; count == 0?  nothing to delete
        JR Z, INLP
        DEC HL           ; back up buffer pointer
        DEC C            ; decrement count
        LD A, (CUR_COL)
        DEC A
        LD (CUR_COL), A  ; move cursor left
        LD A, 020H       ; write a space to erase the last character
        PUSH BC
        PUSH HL
        CALL PRTCHR      ; overwrites character with space
        POP HL
        POP BC
        LD A, (CUR_COL)
        DEC A
        LD (CUR_COL), A  ; move cursor left again (PRTCHR advanced it)
        JR INLP

INLDONE:
        LD (HL), 0       ; null-terminate
        RET


; STRLFT1  -- truncate buffer to first character only
; In:  HL = buffer address
; Out: byte at (HL) unchanged; byte at (HL+1) set to 0
; Clobbers: HL (points to byte 1 on return, then restored)
; Used after INLINE_V when only a single-character answer is needed
; (e.g. J/N prompts).
; 
STRLFT1:
        INC HL
        LD (HL), 0       ; terminate after first char
        DEC HL
        RET


; GAME STRINGS  (null-terminated DEFB)
; ABC80 character set deviations from ASCII:
;   '[' (5BH) = Ä    '\' (5CH) = Ö    ']' (5DH) = Å
; Ö is encoded as 05CH hex because backslash is ambiguous in DEFB.
;
TXT_PLAYED:
        DEFB "HAR NI SPELAT F", 05CH, "RUT? (J/N)", 0  ; "HAR NI SPELAT FÖRUT?"
TXT_NAME_P1:
        DEFB "SPELARE 1 - ANGE NAMN: ", 0
TXT_NAME_P2:
        DEFB "SPELARE 2 - ANGE NAMN: ", 0
TXT_MODE_Q:
        DEFB "V[LJ SPELL[GE (P=PO[NG T=TID): ", 0       ; VÄLJ SPELLÄGE...
TXT_MODE_SCORE:
        DEFB "PO[NGL[GE: VINNER VID 10", 0              ; POÄNGLÄGE...
TXT_MODE_TIME:
        DEFB "TIDSL[GE: 2 MINUTER", 0                   ; TIDSLÄGE...
TXT_READY:
        DEFB "REDO! TRYCK VALFRI TANGENT", 0
TXT_FIRE:
        DEFB "SKJUT!", 0
TXT_WIN_SFX:
        DEFB " VINNER!", 0
TXT_DRAW:
        DEFB "OAVGJORT", 0
TXT_WIN_P1:
        DEFB "SPELARE 1 VINNER!", 0       ; fallback (kept for monitor reference)
TXT_WIN_P2:
        DEFB "SPELARE 2 VINNER!", 0       ; fallback (kept for monitor reference)
TXT_P1KEYS:
        DEFB "A/D/X ", 0
TXT_P2KEYS:
        DEFB "J/L/M ", 0
TXT_AGAIN:
        DEFB "SPELA IGEN? (J/N)", 0
TXT_SCORE_P1:
        DEFB "P1:", 0
TXT_SCORE_P2:
        DEFB "P2:", 0
TXT_TIME:
        DEFB "TID:", 0
TXT_YES:
        DEFB "J", 0      ; expected answer for J/N prompts
TXT_NO:
        DEFB "N", 0
TXT_BOX_TOP:
        DEFB "*******************", 0
TXT_BOX_MID:
        DEFB "*   SPELET SLUT   *", 0
TXT_WIN_HDR:
        DEFB "VINNARE BLEV...", 0

; Cup/trophy mosaic (END_GAME lines 236-239).
; Characters are ABC80 mosaic: char = 0x20|(P&0x1F)|((P&0x20)<<1)
; P bit0=TL bit1=TR bit2=ML bit3=MR bit4=BL bit5=BR
CUP_R0:                          ; row 13 col 12: top rim
        DEFB 028H, 073H, 02CH, 070H, "       ", 070H, 02CH, 073H, 024H, 0
CUP_R1:                          ; row 14 col 12: body
        DEFB 022H, 028H, 063H, 024H, 023H, 02CH, 070H, 020H
        DEFB 070H, 02CH, 023H, 028H, 033H, 024H, 021H, 0
CUP_R2:                          ; rows 15+16 col 17: stem / base
        DEFB "_____", 0           ; 0x5F = full block in graphics mode

; Instruction screen strings (used by SHOW_INSTR via INSTR_TABLE)
; Swedish: [ = Ä (0x5B)   ] = Å (0x5D)   05CH = Ö
TXT_INSTR_HEAD:
        DEFB "** AIRFIGHT - INSTRUKTIONER **", 0
TXT_INSTR_P1:
        DEFB "SPELARE 1:", 0
TXT_INSTR_P1K:
        DEFB "A=V[NSTER  D=H", 05CH, "GER  X=SKJUT", 0
TXT_INSTR_P2:
        DEFB "SPELARE 2:", 0
TXT_INSTR_P2K:
        DEFB "J=V[NSTER  L=H", 05CH, "GER  M=SKJUT", 0
TXT_INSTR_GOAL:
        DEFB "M[L: TR[FFA MOTS]NDARENS PLAN", 0
TXT_INSTR_WAIT:
        DEFB "TRYCK VALFRI TANGENT", 0

; --- Intro screen mosaic logo strings (ABC80 teletext graphics) ---
; é = 060H (ABC80 code 96, bit5=1 -> mosaic)
; " = 022H (quote must be encoded as hex to avoid terminating the literal)
LOGO_R0:
        DEFB 060H, "&dj#ih#)h#ih#i",0
LOGO_R1:
        DEFB "j#kj#ij ",060H,"h#ij j",0
LOGO_R2:
        DEFB 022H," ",022H,022H,"#! #! #! #!",0
LOGO_R3:
        DEFB "j#ij#ij##h#)j##j0j",022H,"k#j##j#i",060H,"&dj#i",0
LOGO_R4:
        DEFB "j#!j+1j#!",060H,"#ij#!j",022H,"n j j#!j+1j#kj+1 !",0
LOGO_R5:
        DEFB 022H,"  ",022H," ",022H,022H,"## #!",022H,"##",022H
        DEFB " ",022H," ",022H," ",022H,"##",022H," ",022H,022H
        DEFB " ",022H,022H," ",022H," !",0
LOGO_R9:
        DEFB 060H,"&d k!j#ij## k!h#)j j",022H,"k#",0
LOGO_R10:
        DEFB "j#k j j+1j#! j j lj#k j",0
LOGO_R11:
        DEFB 022H," ",022H," #!",022H," ",022H,022H
        DEFB "   #! ##",022H," ",022H," ",022H,0
LOGO_R12:
        DEFB 060H,"pp",060H,"pp",060H,"pp",060H,"pp",060H,"pp"
        DEFB 060H,"pp",060H,"pp",060H,"pp",060H,"pp",060H,"pp",0
LOGO_R13:
        DEFB "(,,(,,(,,(,,(,,(,,(,,(,,(,,(,,",0
TXT_WAIT:
        DEFB "CTRL-C TO EXIT", 0


; GAME DATA TABLES

; SPRITES_P1 / SPRITES_P2
; 8 entries x 2 bytes = (left_char, right_char) for each compass direction.
; Directions: 0=N  1=NE  2=E  3=SE  4=S  5=SW  6=W  7=NW
; Characters are mosaic codes (bit5 set) designed to look like a small
; aircraft silhouette pointing in the given direction.
; Mosaic encoding: char = 0x20|(P&0x1F)|((P&0x20)<<1)
;   where P = 6-bit pattern: bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit5=BR
; DO NOT CHANGE THESE VALUES -- hand-composed from original BASIC game.
; 
SPRITES_P1:
        DEFB 028H, 02DH   ; N  -- nose up
        DEFB 022H, 066H   ; NE -- banking up-right
        DEFB 020H, 03DH   ; E  -- nose right
        DEFB 060H, 066H   ; SE -- banking down-right
        DEFB 028H, 03CH   ; S  -- nose down
        DEFB 062H, 064H   ; SW -- banking down-left
        DEFB 028H, 035H   ; W  -- nose left
        DEFB 062H, 026H   ; NW -- banking up-left

SPRITES_P2:
        DEFB 068H, 06DH   ; N
        DEFB 022H, 06FH   ; NE
        DEFB 062H, 03DH   ; E
        DEFB 060H, 07EH   ; SE
        DEFB 02AH, 03EH   ; S
        DEFB 06AH, 074H   ; SW
        DEFB 028H, 077H   ; W
        DEFB 06AH, 027H   ; NW


; DELTA_TAB
; 8 entries x 2 signed bytes = (row_delta, col_delta) per direction.
; Used by PLN_MOVE and BUL_MOVE to advance position one step.
; 0xFF = -1 in two's complement (move up/left), 0x01 = +1 (down/right).
; 
DELTA_TAB:
        DEFB 0FFH, 000H   ; N  row-1  col+0
        DEFB 0FFH, 001H   ; NE row-1  col+1
        DEFB 000H, 001H   ; E  row+0  col+1
        DEFB 001H, 001H   ; SE row+1  col+1
        DEFB 001H, 000H   ; S  row+1  col+0
        DEFB 001H, 0FFH   ; SW row+1  col-1
        DEFB 000H, 0FFH   ; W  row+0  col-1
        DEFB 0FFH, 0FFH   ; NW row-1  col-1


; DIR_NAMES
; 8 entries x 2 chars (no null terminator -- exact 2 bytes per entry).
; Used in the Phase 6 sprite test to label each direction row.
; Indexed as: DIR_NAMES + direction_index * 2
; 
DIR_NAMES:
        DEFB "N "         ; 0 = North
        DEFB "NE"         ; 1 = North-East
        DEFB "E "         ; 2 = East
        DEFB "SE"         ; 3 = South-East
        DEFB "S "         ; 4 = South
        DEFB "SW"         ; 5 = South-West
        DEFB "W "         ; 6 = West
        DEFB "NW"         ; 7 = North-West


; TUNE_DATA
; Victory tune: pairs of (outer_count, inner_delay) played by PLY_TUNE.
; Terminated by 0,0.
; BASIC line 274 raw: DATA 20,45, 20,45, 40,45, 50,32, 60,23, 130,13
; BASIC line 265 adds 10: A1%=A1%+10%, so actual delays = raw+10.
; Values stored here are the adjusted (raw+10) delays, matching BASIC exactly.
TUNE_DATA:
        DEFB  20, 55         ; note 1: 20 reps, delay 45+10
        DEFB  20, 55         ; note 2: 20 reps, delay 45+10
        DEFB  40, 55         ; note 3: 40 reps, delay 45+10
        DEFB  50, 42         ; note 4: 50 reps, delay 32+10
        DEFB  60, 33         ; note 5: 60 reps, delay 23+10
        DEFB 130, 23         ; note 6: 130 reps, delay 13+10
        DEFB   0,  0         ; end


; ROWTAB -- screen RAM start address for each of the 24 display rows
;
; The ABC80 video hardware maps its 24x40 character display into the
; Z80 address range 0x7C00-0x7FFF (1 KB).  The rows are NOT stored
; consecutively; instead they are interleaved in three groups of 8:
;   rows  0- 7: addresses 0x7C00, 0x7C80, 0x7D00 ... 0x7F80  (stride 0x80)
;   rows  8-15: addresses 0x7C28, 0x7CA8, 0x7D28 ... 0x7FA8  (stride 0x80, +0x28)
;   rows 16-23: addresses 0x7C50, 0x7CD0, 0x7D50 ... 0x7FD0  (stride 0x80, +0x50)
; Each row occupies 40 bytes (cols 0..39).
; SCRADDR indexes this table using (row * 2) to get the 16-bit address.
; 
ROWTAB:
        DW 07C00H   ; row 0
        DW 07C80H   ; row 1
        DW 07D00H   ; row 2
        DW 07D80H   ; row 3
        DW 07E00H   ; row 4
        DW 07E80H   ; row 5
        DW 07F00H   ; row 6
        DW 07F80H   ; row 7
        DW 07C28H   ; row 8
        DW 07CA8H   ; row 9
        DW 07D28H   ; row 10
        DW 07DA8H   ; row 11
        DW 07E28H   ; row 12
        DW 07EA8H   ; row 13
        DW 07F28H   ; row 14
        DW 07FA8H   ; row 15
        DW 07C50H   ; row 16
        DW 07CD0H   ; row 17
        DW 07D50H   ; row 18
        DW 07DD0H   ; row 19
        DW 07E50H   ; row 20
        DW 07ED0H   ; row 21
        DW 07F50H   ; row 22
        DW 07FD0H   ; row 23


; LOGO_TABLE -- strings to draw on the intro screen
;
; Each entry is 4 bytes: row (1), col (1), string_ptr (2 LE).
; Table ends with 0FFH in the row byte.
; "AIRFIGHT" is 8 chars; (40-8)/2=16 -> col 16 centres it.
; Upper-case letters (0x40-0x5F) display as text even in graphics mode,
; so row 2 (inside the DRAWBDR range) works correctly.
; 
LOGO_TABLE:
        DEFB 0, 10           ; row 0, col 10
        DW   LOGO_R0
        DEFB 1, 10           ; row 1, col 10
        DW   LOGO_R1
        DEFB 2, 10           ; row 2, col 10
        DW   LOGO_R2
        DEFB 3, 1            ; row 3, col 1
        DW   LOGO_R3
        DEFB 4, 1            ; row 4, col 1
        DW   LOGO_R4
        DEFB 5, 1            ; row 5, col 1
        DW   LOGO_R5
        DEFB 9, 7            ; row 9, col 7
        DW   LOGO_R9
        DEFB 10, 7           ; row 10, col 7
        DW   LOGO_R10
        DEFB 11, 7           ; row 11, col 7
        DW   LOGO_R11
        DEFB 12, 4           ; row 12, col 4
        DW   LOGO_R12
        DEFB 13, 4           ; row 13, col 4
        DW   LOGO_R13
        DEFB 0FFH            ; end of table


; INSTR_TABLE -- strings printed by SHOW_INSTR on the instruction screen
; Same 4-byte entry format as LOGO_TABLE: (row, col, ptr_lo, ptr_hi).
; Terminated by 0FFH in the row byte.
; 
INSTR_TABLE:
        DEFB 2, 5            ; row 2, col 5 -- header centred (30 chars)
        DW   TXT_INSTR_HEAD
        DEFB 4, 0            ; row 4 -- Player 1 label
        DW   TXT_INSTR_P1
        DEFB 5, 2            ; row 5 -- Player 1 key bindings (indented)
        DW   TXT_INSTR_P1K
        DEFB 7, 0            ; row 7 -- Player 2 label
        DW   TXT_INSTR_P2
        DEFB 8, 2            ; row 8 -- Player 2 key bindings (indented)
        DW   TXT_INSTR_P2K
        DEFB 10, 0           ; row 10 -- game goal
        DW   TXT_INSTR_GOAL
        DEFB 22, 10          ; row 22 -- "press any key" prompt
        DW   TXT_INSTR_WAIT
        DEFB 0FFH            ; end of table
