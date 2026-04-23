
## AIRFIGHT — Structured Decomposition for Z80 Assembly

Original BASIC: Kristian Lidberg & Set Lonnert, 1981
Target: Z80 assembly, ABC80 or compatible hardware



### Hardware Abstraction Constants

```
PORT_SOUND      equ  6
PORT_JOYSTICK   equ  58
ADDR_TIMER      equ  65008

SCREEN_ROWS     equ  24
SCREEN_COLS     equ  40

CHAR_CLEAR      equ  12    ; clears screen (form feed)
CHAR_BLOCK      equ  23    ; 151 = 128 +23 (not solid block char, but sets graph mode)
CHAR_BELL       equ   7
CHAR_ATTR_INV   equ 151    ; mosaic or graphic characters / attribute on  (CHR$(151))
CHAR_ATTR_NRM   equ 135    ; normal (mosaic attribute off)                (CHR$(135))

SOUND_OFF       equ   0
SOUND_INTRO     equ 135
SOUND_TITLE     equ 157
SOUND_READY     equ 131
SOUND_SHOOT_1   equ 155    ; player 1 shoots  (153 + 1*2)
SOUND_SHOOT_2   equ 157    ; player 2 shoots  (153 + 2*2)
SOUND_HIT       equ   9
SOUND_WINNER    equ 199
SOUND_LOOP      equ 137
SOUND_TUNE_NOTE equ 121    ; used in the victory tune

JOYSTICK_MASK   equ   7    ; lower 3 bits
JOY_LEFT        equ   1    ; bit 0
JOY_RIGHT       equ   2    ; bit 1
JOY_FIRE        equ   4    ; bit 2
JOY_DIR_MASK    equ   3    ; bits 0-1 used for direction change tracking

TIMER_INIT      equ 255
TIMER_THRESH    equ 207    ; PEEK < this means one tick elapsed

SCORE_LIMIT     equ  10
TIME_START_MIN  equ   2
DIR_COUNT       equ   8    ; 8 compass directions, 1..8
BULLET_STEPS    equ   3    ; bullet moves this many cells per shot cycle
```



### Module Overview

```
screen.asm      cursor_set, print_str, print_char, clear_screen, draw_border
input.asm       get_char, input_line, str_left1
joystick.asm    joystick_read
sound.asm       sound_off, sound_set, delay_loop, play_tune
strings.asm     all string literals, indexed by ID
gamedata.asm    sprite table, music data, instruction text data
gamevars.asm    RAM layout for all game state
intro.asm       logo, player name entry
instructions.asm  rule screens and mode selection
game.asm        main loop
shooting.asm    bullet movement and collision
endgame.asm     winner screen, play-again prompt
```



### Screen Module

```asm
; cursor_set(row: B, col: C)
;   Positions the hardware cursor at the given row and column.
;   All print_str / print_char calls go to current cursor position.

; print_str(hl: pointer to length-prefixed or null-terminated string)
;   Outputs characters until terminator.

; print_char(a: character code)
;   Outputs a single character at current cursor position.

; clear_screen()
;   Sends CHAR_CLEAR to output; resets cursor to 0,0.

; draw_border(first_row: B, last_row: C)
;   Fills each row from first_row to last_row with set graphics mode
;   here called: CHAR_BLOCK.
;   Original: FOR G%=first TO last : CUR(G%,0) : CHR$(23) : NEXT
```

PROM:

```
* О00BH OUTSTR
Prints text to where the cursor is currently positioned.
When calling HL should be pointing to address of the text
that will be printed, and BC should contain its length.
Position the cursor by starting with <ESC> and "=".
<ESC> is no. 27. After these two bytes with coordinates
x + 32, y + 32. Then the text.

* CURXY (≈ 0293H) First bytes should be E5H 2AH F3Н
Calculates the address in the memory for the screen.
Before calling X (row) should be in 65011, and Y
(column) in 65012. Returns the address in DE.
(Collision check.)

* CLEAR (≈ 0276H) First byte should be JEH 18H DDH
Clears screen. Destroys values in IX, D E and A.
```



### Input Module

```asm
; get_char(de: destination byte)
;   Waits for a single keypress, stores it, no echo.
;   Original: GET Ä$

; input_line(de: destination buffer, bc: max length)
;   Reads a line terminated by RETURN into buffer.
;   Original: INPUTLINE / INPUT

; str_left1(hl: string pointer)
;   Keeps only the first character of the string in-place.
;   Original: Ä$ = LEFT$(Ä$, 1)

; str_empty(hl: string pointer) -> Z flag
;   Returns Z set if string length is zero.
```

In PROM:

Address 65031 Bit 7=1 indicates that CTRL-C been pressed.
Ends program, return to BASIC.

```
* 0002H INCHAR
Read a character from keyboard into A. No echo.
DE and IX changes.

* 0005H INLINE
Read a line from keyboard. When calling HL must point
to the area in memory where the result is stored.
BC contains the maximum number of characters that are
accepted. The line is echoed.
```


### Joystick Module

```asm
; joystick_read(player: A) -> A: raw bits
;   OUT PORT_JOYSTICK, player * 8
;   IN  A, (PORT_JOYSTICK)
;   AND JOYSTICK_MASK
;   Returns the 3-bit value: bit0=left, bit1=right, bit2=fire
```

May be the hardest to replicate for ports, so we might use keyboard
instead.


### Sound Module

```asm
; sound_off()
;   OUT PORT_SOUND, SOUND_OFF

; sound_set(value: A)
;   OUT PORT_SOUND, SOUND_OFF
;   OUT PORT_SOUND, value

; delay_loop(count: HL)
;   Busy-wait loop; count is a 16-bit iteration count.
;   Original: FOR T=1 TO count : NEXT T

; play_tune()
;   Plays the 6-note victory melody using PORT_SOUND.
;   Note data: see TUNE_DATA in gamedata.asm.
;   Each entry is (count, period): OUT note, wait, repeat count times.
```



### String Table  (strings.asm)

All string literals are collected here, referenced by label.

```asm
; INTRO / LOGO STRINGS (displayed as overlapping segments to compose the "logo")
; Each entry: (row, col, string)
STR_LOGO_0      db  0, 10, "É&DJ#IH#)H#IH#I", 0
STR_LOGO_1      db  1, 10, "J#KJ#IJ ÉH#IJ J", 0
STR_LOGO_2      db  2, 10, "\" \"\"#! #! #! #!", 0
STR_LOGO_3      db  3,  1, "J#IJ#IJ##H#)J##J0J\"K#J##J#IÉ&DJ#I", 0
STR_LOGO_4      db  4,  1, "J#!J+1J#!É#IJ#!J\"N J J#!J+1J#KJ+1 !", 0
STR_LOGO_5      db  5,  1, "\"  \" \"\"### #!\"##\" \" \" \"##\" \"\" \"\" \" !", 0
STR_LOGO_6      db  9,  7, "É&D K!J#IJ### K!H#)J J\"K#", 0
STR_LOGO_7      db 10,  7, "J#K J J+1J#! J J LJ#K J", 0
STR_LOGO_8      db 11,  7, "\" \" #!\" \"\"   #! ##\" \" \"", 0
STR_LOGO_9      db 12,  4, "ÉPPÉPPÉPPÉPPÉPPÉPPÉPPÉPPÉPP", 0
STR_LOGO_10     db 13,  4, "(,,(,,(,,(,,(,,(,,(,,(,,(,,(,,", 0

; INTRO FLOW
STR_PLAYED_Q    db 20, 8,  "HAR NI SPELAT FÖRUT ", 0
STR_YES_NO_Q    db 22, 10, "SVARA MED (JA/NEJ)", 0
STR_PRESS_RET   db 23, 6,  "TRYCK DÄREFTER \"RETURN\"!", 0

; NAME ENTRY
STR_CHOOSE_JS   db  0, 0,  "VAR VÄNLIGA VÄLJ RESPEKTIVE JOYSTICK!", 0
STR_ENTER_NAMES db  5, 8,  "SLÅ IN DITT RESPEKTIVE", 0
STR_OPPONENT    db  7, 8,  "DIN MOTSPELARES NAMN ", 0
STR_PRESS_RET2  db 10, 5,  "TRYCK DÄREFTER PÅ \"RETURN\" KNAPPEN ", 0
STR_NAME_P2     db 15, 2,  "(BRUN JOYSTICK): ", 0
STR_NAME_P1     db 17, 2,  "(GRÅ JOYSTICK): ", 0

; TITLE ANIMATION
STR_TITLE       db "DETTA ÄR AIRFIGHT", 0   ; animated, col = J%+9, row=1

; INSTRUCTION SCREEN 1  (6 lines starting at row I%*2 + 7)
STR_INSTR_1_0   db "DETTA SPEL STYRS MED TVÅ ", 0
STR_INSTR_1_1   db "       JOYSTICKS : ", 0
STR_INSTR_1_2   db "EN GRÅ     OCH    EN BRUN", 0
STR_INSTR_1_3   db "", 0
STR_INSTR_1_4   db "DET GÄLLER ATT SKJUTA NER", 0
STR_INSTR_1_5   db "        VARANDRA .", 0

; INSTRUCTION SCREEN 2  (9 lines starting at row I%*2 + 3)
STR_INSTR_2_0   db " FÖR ATT SKJUTA TRYCKER NI", 0
STR_INSTR_2_1   db "PÅ DEN RÖDA KNAPPEN PÅ EDRA", 0
STR_INSTR_2_2   db "        JOYSTICKS.", 0
STR_INSTR_2_3   db "", 0
STR_INSTR_2_4   db "MED SPAKEN PÅ EDRA DOSOR", 0
STR_INSTR_2_5   db "KAN NI STYRA PLANEN TILL ATT", 0
STR_INSTR_2_6   db "SVÄNGA ÅT HÖGER RESP. VÄNSTER.", 0
STR_INSTR_2_7   db "   SPAKEN KAN EJ FÖRAS I", 0
STR_INSTR_2_8   db "   VERTIKAL RIKTNING !!!", 0

; MODE SELECTION SCREEN  (7 lines starting at row I%*2 + 3)
STR_MODE_0      db "VILL NI HA POÄNG BEGRÄNSNING (10P)", 0
STR_MODE_1      db "          ELLER", 0
STR_MODE_2      db "TIDS-BEGRÄNSNING (2 MIN.)?", 0
STR_MODE_3      db "SVARA MED : ", 0
STR_MODE_4      db "", 0
STR_MODE_5      db "P   FÖR      POÄNGBEGRÄNSNING", 0
STR_MODE_6      db "T   FÖR      TIDSBEGRÄNSNING", 0

; HUD  (row 23)
STR_HUD_P1      db  "GRÅ =", 0          ; preceded by CHAR_ATTR_INV
STR_HUD_P2      db  "BRUN B=", 0        ; preceded by CHAR_ATTR_INV
STR_HUD_TIME    db  "TID", 0            ; followed by T5%, ".", T6%
STR_HUD_SEP     db  " . ", 0

; GAME MESSAGES
STR_START       db  8, 10, "*** S-T-A-R-T ***", 0
STR_PRESS_RET3  db 23, 11, "TRYCK \"RETURN\"!  ", 0
STR_ANSWER_WITH db 20, 11, "SVARA MED", 0

; HIT ANIMATION  (relative to hit position)
STR_BOOM        db "F  !D", 0           ; at (H%(U), L%(U)-1)

; TIME-UP SCREEN
STR_TIMEOUT_0   db 11, 8, "*******************", 0
STR_TIMEOUT_1   db 12, 8, "*   TIDEN  SLUT   *", 0
STR_TIMEOUT_2   db 13, 8, "*******************", 0

; WINNER SCREEN
STR_WINNER      db  4, 12, "VINNARE BLEV...", 0
STR_TROPHY_A    db 10, 0,  "ÉDDD¤", 0   ; col = 8 + 6*W%
STR_TROPHY_B    db 11, 0,  "(9991", 0
STR_TROPHY_C    db 12, 0,  "\"&&&¤", 0
STR_DRAW_0      db  7, 10, "HEMSK OTUR DET BLEV", 0
STR_DRAW_1      db  9, 10, "O-A-V-G-J-O-R-T !!!", 0

; PLAY AGAIN
STR_PLAY_AGAIN  db 21, 7,  "SVARA MED (JA/NEJ)!", 0
STR_PLAY_Q      db 20, 7,  "SKALL VI SPELA EN GÅNG TILL  ", 0

; OUTRO
STR_OUTRO_0     db  2, 13, "INTE DET.", 0
STR_OUTRO_1     db  4,  3, "HOPPAS ATT NI I ALLA FALL HADT", 0
STR_OUTRO_2     db  5, 13, "ROLIGT!!!", 0
STR_OUTRO_3     db 12,  8, "LÄMNA NU ÖVER TILL NÄSTA", 0
STR_OUTRO_4     db 13,  8, "SPELARE I KÖN!!!!", 0
```



### Game Data  (gamedata.asm)

```asm
; SPRITE TABLE
; 8 sprites per plane, 2 planes.  Each sprite is a 1-2 char string.
; Index = direction (1..8): E, SE, S, SW, W, NW, N, NE

SPRITES_P1:
    db " =", 0    ; dir 1  east
    db "ÉF", 0    ; dir 2  SE
    db "(<", 0    ; dir 3  south
    db "BD", 0    ; dir 4  SW
    db "(5", 0    ; dir 5  west
    db "B&", 0    ; dir 6  NW
    db "(-", 0    ; dir 7  north
    db "\"F",0    ; dir 8  NE

SPRITES_P2:
    db "B=", 0    ; dir 1
    db "ÉF", 0    ; dir 2
    db "*>", 0    ; dir 3
    db "JT", 0    ; dir 4
    db "(W", 0    ; dir 5
    db "\"J\"",0  ; dir 6
    db "HM", 0    ; dir 7
    db "\"O",0    ; dir 8

; VICTORY TUNE DATA
; Each pair: (repeat_count, period)
; OUT PORT_SOUND, SOUND_TUNE_NOTE  then busy-wait period+10 times, repeat count times
TUNE_DATA:
    db 20, 45
    db 20, 45
    db 40, 45
    db 50, 32
    db 60, 23
    db 130, 13

; INSTRUCTION BLOCK TABLE
; Used by the instruction loop (I1% = 1, 2, 3)
; Entry: (line_count, start_row_offset, pointer to string list)
INSTR_BLOCKS:
    dw 6, 7, INSTR_STRINGS_1     ; screen 1
    dw 9, 3, INSTR_STRINGS_2     ; screen 2
    dw 7, 3, INSTR_STRINGS_MODE  ; mode selection screen

; LOGO POSITION TABLE
; (row, col, string_ptr)  — 11 entries for the animated logo
LOGO_TABLE:
    db  0, 10  \  dw STR_LOGO_0
    db  1, 10  \  dw STR_LOGO_1
    db  2, 10  \  dw STR_LOGO_2
    db  3,  1  \  dw STR_LOGO_3
    db  4,  1  \  dw STR_LOGO_4
    db  5,  1  \  dw STR_LOGO_5
    db  9,  7  \  dw STR_LOGO_6
    db 10,  7  \  dw STR_LOGO_7
    db 11,  7  \  dw STR_LOGO_8
    db 12,  4  \  dw STR_LOGO_9
    db 13,  4  \  dw STR_LOGO_10
```



### RAM Variables  (gamevars.asm)

```asm
; Two-player arrays are stored as pairs: index 0 = player 1, index 1 = player 2

var_player_dir:     dw 0, 0      ; F%(1..2)  current direction 1..8
var_player_row:     dw 0, 0      ; H%(1..2)  current screen row
var_player_col:     dw 0, 0      ; L%(1..2)  current screen col
var_player_row_prev:dw 0, 0      ; H1%(1..2) previous row (for erase)
var_player_col_prev:dw 0, 0      ; L1%(1..2) previous col (for erase)
var_player_score:   dw 0, 0      ; P%(1..2)
var_player_speed:   dw 0, 0      ; V%(1..2)  not well-defined in original; likely turn rate

var_bullet_active:  db 0, 0      ; Z%(1..2)  1 = bullet in flight
var_bullet_dir:     dw 0, 0      ; Q%(1..2)  direction when fired
var_bullet_row:     dw 0, 0      ; R%(1..2)  current bullet row
var_bullet_col:     dw 0, 0      ; K%(1..2)  current bullet col
var_bullet_row_prev:dw 0, 0      ; R1%(1..2)
var_bullet_col_prev:dw 0, 0      ; K1%(1..2)

var_joy_prev:       db 0, 0      ; B%(1..2)  previous joystick direction bits

var_mode_score_lim: dw 0         ; D1%  score limit (10) or -1 if time mode
var_mode_score_tgt: dw 0         ; D%   target score or 0
var_mode_time_dir:  dw 0         ; D3%  time delta (+1 countup or -1 countdown)
var_time_min:       dw 0         ; T5%  minutes display
var_time_sec:       dw 0         ; T6%  seconds display

var_winner:         dw 0         ; W%   1, 2, or 0 = draw
var_played_before:  db 0         ; Ä$ parsed:  'J'=yes 'N'=no

var_name_p1:        ds 32, 0     ; B$(1)  grey joystick player name
var_name_p2:        ds 32, 0     ; B$(2)  brown joystick player name
```



### Program Flow  (structured pseudocode)

#### Phase 1 — Cold Start and Logo

```
proc cold_start:
    clear_screen()
    delay_loop(800)
    draw_border(0, 19)

    for u = 0 to 10:
        if u == 6: delay_loop(2000)     ; pause mid-logo for effect
        row, col = LOGO_TABLE[u].row, LOGO_TABLE[u].col
        cursor_set(row, col)
        print_str(LOGO_TABLE[u].str)
    end for

    sound_set(SOUND_INTRO)              ; OUT 6,0 : OUT 6,135

    cursor_set(20, 29)
    print_str("                    ")
    delay_loop(600)
```

#### Phase 2 — "Played Before?" Prompt

```
proc ask_played_before:
loop:
    cursor_set(22, 10) : print_str(STR_YES_NO_Q)
    cursor_set(23,  6) : print_str(STR_PRESS_RET)
    cursor_set(20,  8) : print_str(STR_PLAYED_Q)
    input_line(var_played_before)
    if input_empty: goto loop
    str_left1(var_played_before)
    if var_played_before != 'N' and != 'J': goto loop
```

#### Phase 3 — Player Name Entry

```
proc get_player_names:
    clear_screen()
    cursor_set( 0, 0) : print_str(STR_CHOOSE_JS)
    cursor_set( 5, 8) : print_str(STR_ENTER_NAMES)
    cursor_set( 7, 8) : print_str(STR_OPPONENT)
    cursor_set(10, 5) : print_str(STR_PRESS_RET2)
    cursor_set(15, 2) : print_str(STR_NAME_P2) : input_line(var_name_p2)
    cursor_set(17, 2) : print_str(STR_NAME_P1) : input_line(var_name_p1)
```

#### Phase 4 — Title Animation and Instructions

```
proc show_title_anim:
    clear_screen()
    delay_loop(300)
    for j = 1 to len(STR_TITLE):
        cursor_set(1, j + 9)
        print_char(STR_TITLE[j])
        delay_loop(100)
    end for
    sound_set(SOUND_TITLE)
    cursor_set(3, 2) : print_str(STRING_OF(36, '*'))   ; decorative line

proc show_instructions:
    if var_played_before == 'J': skip to block 3 (mode select only)

    for i1 = 1 to 3:
        block = INSTR_BLOCKS[i1]
        for i = 1 to block.line_count:
            row = i * 2 + block.row_offset
            if i1 != 3:
                cursor_set(row, 7)
            else:
                cursor_set(row, 4)
            print_str(block.strings[i])
            delay_loop(1000)
        end for

        if i1 != 3:
            cursor_set(23, 11) : print_str(STR_PRESS_RET3)
            get_char()
        else:
            sound_set(SOUND_READY)
            cursor_set(11, 19)
            get_char(var_mode_choice)   ; waits for 'P' or 'T'

        delay_loop(400)
        sound_set(SOUND_READY)
        clear_screen()
    end for
```

#### Phase 5 — Mode Selection

```
proc apply_mode_choice:
loop:
    if var_mode_choice == 'P':
        var_mode_score_lim = 10
        var_mode_score_tgt = 100
        var_mode_time_dir  = 1
        var_time_min       = 0
        var_time_sec       = 0
        return

    if var_mode_choice == 'T':
        var_mode_score_lim = -1
        var_mode_score_tgt = 0
        var_mode_time_dir  = -1
        var_time_min       = 2
        var_time_sec       = 0
        return

    cursor_set(20, 11) : print_str(STR_ANSWER_WITH)
    poke(ADDR_TIMER+..., ...)     ; re-arm hardware timer
    sound_set(SOUND_READY)
    delay_loop(1000)
    sound_off()
    goto show_instructions (block 3 only)
```

#### Phase 6 — Game Init

```
proc game_init:
    delay_loop(500)
    cursor_set(12, 5) : print_char(CHAR_BLOCK) : print_str("(-                  HM")
    hud_draw()
    delay_loop(1000)
    cursor_set(8, 10) : print_str(STR_START)
    delay_loop(1500)
    cursor_set(8, 10) : print_str("                       ")
    sound_set(SOUND_READY)
    poke(ADDR_TIMER, TIMER_INIT)

    var_player_score[1] = 0
    var_player_score[2] = 0

    draw_border(0, 22)

    load_sprites()      ; fill P$(N,1..8) from SPRITE TABLE

    ; Starting positions
    var_player_col[2]  = 26 : var_player_row[2]  = 12 : var_player_dir[2] = 7
    var_player_col[1]  =  6 : var_player_row[1]  = 12 : var_player_dir[1] = 7

    for n = 1 to 2:
        var_bullet_row_prev[n] = var_player_row[n]
        var_bullet_col_prev[n] = var_player_col[n]
        var_bullet_active[n]   = 0
    end for

    hud_draw()

proc hud_draw:
    cursor_set(23, 0)
    print_char(CHAR_ATTR_INV) : print_str(STR_HUD_P1)
    print_char(CHAR_ATTR_NRM) : print_num(var_player_score[1]) : print_str("  ")
    cursor_set(23, 24)
    print_char(CHAR_ATTR_INV) : print_str(STR_HUD_P2)
    print_char(CHAR_ATTR_NRM) : print_num(var_player_score[2]) : print_str(" ")
    hud_draw_time()

proc hud_draw_time:
    cursor_set(23, 13)
    print_str(STR_HUD_TIME) : print_num(var_time_min)
    print_str(STR_HUD_SEP)  : print_num(var_time_sec) : print_str(" ")
```

#### Phase 7 — Main Game Loop

```
proc main_loop:
loop:
    for n = 1 to 2:
        plane_move(n)
        plane_draw(n)
        joystick_poll(n)
    end for

    if peek(ADDR_TIMER) < TIMER_THRESH:
        timer_tick()
        poke(ADDR_TIMER, TIMER_INIT)
    else:
        goto loop

    if var_time_min == 0 and var_time_sec == 0:
        goto time_up

    goto loop

proc plane_move(n):
    ; Jump table on var_player_dir[n], 8 directions
    ; Each case adjusts var_player_row[n] and var_player_col[n]
    ; Direction mapping: 1=E 2=SE 3=S 4=SW 5=W 6=NW 7=N 8=NE
    ; Row wraps 0..22, col wraps 1..37

    switch var_player_dir[n]:
        case 1: col += 1
        case 2: col += 1  :  row += 1
        case 3: row += 1
        case 4: col -= 1  :  row += 1
        case 5: col -= 1
        case 6: col -= 1  :  row -= 1
        case 7: row -= 1
        case 8: col += 1  :  row -= 1
    end switch

    if row < 0:  row = 22
    if row > 22: row = 0
    if col < 1:  col = 37
    if col > 37: col = 1

proc plane_draw(n):
    cursor_set(var_player_row_prev[n], var_player_col_prev[n])
    print_str("  ")                     ; erase old position
    cursor_set(var_player_row[n], var_player_col[n])
    print_str(SPRITES[n][var_player_dir[n]])

proc joystick_poll(n):
    raw = joystick_read(n)              ; OUT+IN PORT_JOYSTICK
    dir_bits = raw AND JOY_DIR_MASK

    if dir_bits != var_joy_prev[n]:
        if NOT (raw AND JOY_LEFT):  var_player_dir[n] -= 1
        if NOT (raw AND JOY_RIGHT): var_player_dir[n] += 1
        if var_player_dir[n] < 1: var_player_dir[n] = 8
        if var_player_dir[n] > 8: var_player_dir[n] = 1
        var_player_speed[n] = 3

    var_joy_prev[n] = dir_bits

    if var_bullet_active[n]: goto bullet_move(n)

    delay_loop(50)

    if NOT (raw AND JOY_FIRE):
        var_bullet_active[n] = 1
        goto bullet_init(n)

    if NOT (raw AND JOY_LEFT):  var_player_speed[n] -= 1
    if NOT (raw AND JOY_RIGHT): var_player_speed[n] += 1
    if var_player_speed[n] < 1: var_player_speed[n] = 3 : var_player_dir[n] -= 1
    if var_player_speed[n] > 5: var_player_speed[n] = 3 : var_player_dir[n] += 1
    if var_player_dir[n] < 1: var_player_dir[n] = 8
    if var_player_dir[n] > 8: var_player_dir[n] = 1

    var_player_row_prev[n] = var_player_row[n]
    var_player_col_prev[n] = var_player_col[n]

proc timer_tick:
    var_time_sec += var_mode_time_dir
    if var_time_sec < 0:  var_time_sec = 59 : var_time_min -= 1
    if var_time_sec > 59: var_time_sec = 0  : var_time_min += 1
    hud_draw_time()
```

#### Phase 8 — Bullet / Shooting

```
proc bullet_init(n):
    var_bullet_dir[n] = var_player_dir[n]
    var_bullet_col[n] = var_player_col[n]
    var_bullet_row[n] = var_player_row[n]
    sound_set(SOUND_SHOOT_1 + n * 2)    ; 155 or 157

proc bullet_move(n):
    cursor_set(var_bullet_row_prev[n], var_bullet_col_prev[n])
    print_str(" ")                      ; erase bullet

    for c = 1 to BULLET_STEPS:
        bullet_step(n)
        if bullet_out_of_bounds(n): goto bullet_off(n)
        opp = (n == 1) ? 2 : 1
        if hit_test(n, opp):            goto bullet_hit(n, opp)
    end for

    cursor_set(var_bullet_row[n], var_bullet_col[n])
    print_str("(")                      ; bullet glyph
    var_bullet_row_prev[n] = var_bullet_row[n]
    var_bullet_col_prev[n] = var_bullet_col[n]
    return

proc bullet_step(n):
    ; Same 8-direction jump table as plane_move, applied to bullet_row/col

proc bullet_out_of_bounds(n) -> bool:
    return row < 0 or row > 22 or col < 1 or col > 37

proc bullet_off(n):
    cursor_set(var_bullet_row_prev[n], var_bullet_col_prev[n])
    print_str(" ")
    var_bullet_active[n] = 0

proc hit_test(shooter, target) -> bool:
    ; Check both cells of the target sprite (sprite is 2 chars wide)
    return (var_bullet_row[shooter] == var_player_row[target]) and
           (var_bullet_col[shooter] == var_player_col[target] or
            var_bullet_col[shooter] == var_player_col[target] + 1)
```

#### Phase 9 — Hit and Scoring

```
proc bullet_hit(shooter, target):
    ; Guard: bullet cannot hit the shooter's own position
    if var_bullet_row[shooter] == var_player_row[shooter] and
       var_bullet_col[shooter] == var_player_col[shooter]:
        return                      ; ignore self-hit

    sound_set(SOUND_HIT)

    cursor_set(var_bullet_row_prev[shooter], var_bullet_col_prev[shooter])
    print_str(" ")
    cursor_set(var_bullet_row_prev[target],  var_bullet_col_prev[target])
    print_str(" ")

    var_player_score[shooter] += 1

    cursor_set(var_player_row[target], var_player_col[target] - 1)
    print_str(STR_BOOM)             ; "F  !D" explosion graphic

    hud_draw()
    delay_loop(700)

    cursor_set(var_player_row[target], var_player_col[target] - 1)
    print_str("       ")            ; clear explosion

    delay_loop(400)
    sound_set(SOUND_LOOP)
    delay_loop(2000)

    if var_player_score[shooter] == var_mode_score_lim:
        goto end_game(shooter)

    clear_screen()
    hud_draw()
    delay_loop(1000)
    draw_border(0, 22)
    goto game_init (restart round from line 118)
```

#### Phase 10 — End Game

```
proc time_up:
    sound_set(SOUND_WINNER)
    cursor_set(11, 8) : print_char(CHAR_BOLD_ON) : print_str(STR_TIMEOUT_0)
    cursor_set(12, 8) : print_char(CHAR_BOLD_ON) : print_str(STR_TIMEOUT_1)
    cursor_set(13, 8) : print_char(CHAR_BOLD_ON) : print_str(STR_TIMEOUT_2)
    delay_loop(1000)
    sound_off()
    sound_set(SOUND_INTRO)
    delay_loop(2500)
    ; fall through to end_game_screen

proc end_game_screen:
    clear_screen()
    delay_loop(800)

    if var_player_score[1] < var_player_score[2]: var_winner = 2
    else: var_winner = 1
    if var_player_score[1] == var_player_score[2]: var_winner = 0

    cursor_set(4, 12) : print_str(STR_WINNER)
    draw_border(10, 18)             ; decorative border around winner area

    ; Draw trophy glyphs (rows 13-15, columns 17-21)
    cursor_set(13, 12) : print_str("(S,P       P,S¤")
    cursor_set(14, 12) : print_str("\"(C¤#,P P,#(3¤!")
    cursor_set(15, 17) : print_str("_____")
    cursor_set(16, 17) : print_str("_____")

    delay_loop(1800)

    if var_winner == 0: goto show_draw

    play_tune()
    cursor_set(6, 16) : print_str(var_winner == 1 ? var_name_p1 : var_name_p2)
    col = (var_winner == 1) ? 8 : 14
    cursor_set(13, 4 + 8 * var_winner) : print_str("             ")
    cursor_set( 4, 4 + 8 * var_winner) : print_str("             ")

    g = 8 + 6 * var_winner
    cursor_set(10, g) : print_str(STR_TROPHY_A)
    cursor_set(11, g) : print_str(STR_TROPHY_B)
    cursor_set(12, g) : print_str(STR_TROPHY_C)

    k = (var_winner == 1) ? 37 : 40
    for r = 31 to 44: setdot(r, k)  ; graphics pixel line
    goto play_again_prompt

proc show_draw:
    delay_loop(500)
    cursor_set( 7, 10) : print_str(STR_DRAW_0)
    cursor_set( 9, 10) : print_str(STR_DRAW_1)
    delay_loop(1000)

proc play_again_prompt:
loop:
    cursor_set(21, 7) : print_str(STR_PLAY_AGAIN)
    cursor_set(20, 7) : print_str(STR_PLAY_Q)
    input_line(var_played_before)
    if input_empty: print_char(CHAR_BELL) : goto loop
    str_left1(var_played_before)
    if var_played_before != 'J' and != 'N': print_char(CHAR_BELL) : goto loop
    clear_screen() : delay_loop(500)
    if var_played_before == 'J': goto get_player_names   ; line 40
    goto outro

proc outro:
    clear_screen()
    cursor_set( 2, 13) : print_str(STR_OUTRO_0)
    cursor_set( 4,  3) : print_str(STR_OUTRO_1)
    cursor_set( 5, 13) : print_str(STR_OUTRO_2)
    cursor_set( 6,  0) : print_char(CHAR_BLOCK) : print_str(STRING_OF(39, ','))
    delay_loop(3000)
    cursor_set(12,  8) : print_str(STR_OUTRO_3)
    cursor_set(13,  8) : print_str(STR_OUTRO_4)
    delay_loop(4000)
    goto cold_start                 ; line 1, loop back to very beginning
```



### Notes for Z80 Translation

#### Cursor Positioning
The `CUR(row, col)` BASIC built-in issues an escape sequence to the ABC80
terminal controller. In assembly this becomes a small routine that outputs the
control bytes for the specific hardware. Isolate this entirely in `screen.asm`
so only that one routine needs changing if porting.

#### Time / Timer
The `PEEK(65008)` polling approach is a software timer driven by an interrupt or
a free-running counter at a fixed address. In assembly this maps to a simple
`LD A, (ADDR_TIMER)` followed by `CP TIMER_THRESH`. Wrap this in a
`timer_check` routine that returns carry set when a tick has elapsed.

#### Direction Jump Tables
All `ON F%(N%) GOTO ...` constructs map cleanly to a Z80 jump table:
```
    DEC  A              ; direction is 1-based, make 0-based
    ADD  A, A           ; multiply by 2 (each JP takes 3 bytes, so use JR or table of addresses)
    LD   HL, dir_table
    ADD  HL, A          ; not quite right; see note
    JP   (HL)
```
Use a table of 16-bit addresses (3-byte `JP` targets) for clarity, indexed by
`(direction - 1) * 2`.

#### Player/Bullet Arrays
In BASIC these are 1-indexed. In assembly, store them as zero-indexed pairs of
bytes or words in a fixed RAM block (see `gamevars.asm`). Access with:
```
    LD   HL, var_player_row
    LD   BC, player_index   ; 0 or 2 (word offset)
    ADD  HL, BC
    LD   (HL), A
```

#### Input Validation Loop
The `IF ... THEN line` pattern for input validation becomes a local loop label
with `JR NZ` / `JP NZ` back to the prompt. Keep each prompt + read + validate
sequence as a self-contained `call`-able procedure with a clear `RET` when valid
input is received.

#### Sound
All `OUT 6, 0 : OUT 6, x` pairs are already direct Z80 `OUT` instructions.
Wrap them in `sound_off` / `sound_set` to make intent readable.

#### String Layout Choice
Option A: null-terminated strings (simple, wastes no byte on length, easy
to print with a `CP 0 / RET Z` loop).
Option B: length-prefixed (one byte length then data), useful if you need
`LEN` frequently.
The original BASIC code rarely queries string length at runtime, so
null-terminated is the simpler choice here.
