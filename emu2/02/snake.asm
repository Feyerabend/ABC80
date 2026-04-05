; =============================================================================
; SNAKE for ABC80  —  character-cell graphics version
; =============================================================================
;
; The ABC80 screen is 40 columns x 24 rows of character cells.
; In graphics mode (triggered by CHR$(151) = 97h in column 0) each cell
; is rendered as a 2x3 dot pattern.  Pattern bits 5:0 light the six dots;
; the firmware converts a screen byte to its pattern with:
;
;   pattern = (byte & 1Fh) | ((byte & 40h) >> 1)
;
; Relevant byte values:
;
;   7Fh  ->  pattern 3Fh  (111111)  full block  — snake body, walls
;   35h  ->  pattern 15h  (010101)  three dots  — food pellet
;   20h  ->  pattern 00h  (000000)  no dots     — empty cell / blank
;
; NOTE: characters in the range 40h-5Fh (A-Z etc.) are always displayed
; as text even inside a graphics-mode row; keep game bytes outside that
; range.
;
; SCREEN LAYOUT  (column 0 always holds the 97h graphics-mode marker)
;
;   Row  0        wall (full blocks, cols 1-39)
;   Rows 1-22     play field, 22 rows tall
;   Row  23       wall (full blocks, cols 1-39)
;   Col  1        left wall  (full blocks, rows 1-22)
;   Col  39       right wall (full blocks, rows 1-22)
;   Cols 2-38     play area  (37 columns wide)
;
; SNAKE MECHANICS
;   The snake body is stored as a FIFO ring buffer of (row, col) byte pairs
;   in RAM at SBUF (9000h), holding up to 64 body segments.  SHEAD indexes
;   the newest cell (the head); STAIL indexes the oldest (the tail).
;
;   Every move tick:
;     1. Fetch head position from SBUF[SHEAD].
;     2. Apply direction delta -> new (row, col).
;     3. Read the screen byte at the new cell.
;        BLOCK (7Fh)  -> wall or self hit -> GAME OVER
;        FOOD  (35h)  -> eat pellet: grow (skip tail erase), place new food.
;        BLANK (20h)  -> normal move: erase SBUF[STAIL] from screen, slide.
;     4. Write new head into SBUF[SHEAD], draw BLOCK there.
;
;   Because the screen itself acts as a spatial lookup, self-collision is
;   detected for free: every body segment is BLOCK, so running into one
;   triggers GAME OVER exactly like hitting a wall.
;
; TIMING
;   The main loop syncs to the ABC80 50 Hz hardware clock at FDF0h.
;   The snake moves every 4th tick (~12.5 Hz).
;
; CONTROLS
;   W = up     S = down     A = left     D = right   (upper or lower case)
;   Ctrl-C = quit back to the monitor
;
; HOW TO RUN FROM THE MONITOR
;   P        load this source into the ASM editor
;   AS 8000  assemble at address 8000h
;   G 8000   run  (Ctrl-C returns to monitor)
; =============================================================================

; ---------------------------------------------------------------------------
; Z80 RAM  —  game variables at 8F00h (well above the assembled code).
; Each variable is one byte.
; ---------------------------------------------------------------------------

SHEAD   EQU 08F00H  ; ring-buffer head index  (0-63, wraps with AND 3Fh)
STAIL   EQU 08F01H  ; ring-buffer tail index  (0-63)
DIR     EQU 08F02H  ; current direction: 0=right 1=down 2=left 3=up
NDIR    EQU 08F03H  ; next (pending) direction from keyboard
PCLK    EQU 08F04H  ; previous 50 Hz clock byte  (for edge detection)
SPEED   EQU 08F05H  ; frame counter — move tick every 4th frame
RNG     EQU 08F06H  ; Galois LFSR state for pseudo-random food placement
KEY     EQU 08F07H  ; ASCII code of key read this frame (bit 7 stripped)
PFROW   EQU 08F08H  ; food-placement scratch: candidate row
PFCOL   EQU 08F09H  ; food-placement scratch: candidate column

; ---------------------------------------------------------------------------
; Ring buffer  —  64 entries × 2 bytes = 128 bytes.
;   Entry i is at: SBUF + i*2 + 0 = row,  SBUF + i*2 + 1 = col.
;   With i in 0..63, i*2 is 0..126, so the high byte of SBUF + i*2 is always
;   90h — address = 90h:L where L = i*2.
; ---------------------------------------------------------------------------

SBUF    EQU 09000H  ; base address of body ring buffer
SMASK   EQU 03FH    ; wrap mask for 64 entries  (AND SMASK = mod 64)

; ---------------------------------------------------------------------------
; Hardware
; ---------------------------------------------------------------------------

CLKLO   EQU 0FDF0H  ; ABC80 50 Hz real-time clock — low byte

; ---------------------------------------------------------------------------
; Screen byte constants
; ---------------------------------------------------------------------------

BLOCK   EQU 07FH    ; full 2×3 block  (pattern 3Fh = all 6 dots lit)
BLANK   EQU 020H    ; empty cell      (pattern 00h = no dots)
GFXMK   EQU 097H   ; CHR$(151) in col 0 -> switches that row to graphics mode
FOOD    EQU 035H    ; food pellet     (pattern 15h = 010101, 3 alternating dots)

; ---------------------------------------------------------------------------
; Direction constants
; ---------------------------------------------------------------------------

DRIGHT  EQU 0       ; delta: dcol = +1, drow =  0
DDOWN   EQU 1       ; delta: drow = +1, dcol =  0
DLEFT   EQU 2       ; delta: dcol = -1 (FFh), drow =  0
DUP     EQU 3       ; delta: drow = -1 (FFh), dcol =  0

; ---------------------------------------------------------------------------
        ORG 8000H
; ---------------------------------------------------------------------------



; START — one-time initialisation

START:
        DI              ; disable interrupts — we poll the 50 Hz clock ourselves

        CALL INITSCR    ; draw border walls, clear play field

        ; ---- Seed the RNG by mixing the current clock into whatever is in RNG.
        ;     This gives a different starting point each restart based on timing.
        ;     The OR A / INC A guard prevents the invalid LFSR state of 0. ----
        LD HL, CLKLO
        LD A, (HL)      ; A = current 50 Hz clock byte (varies with timing)
        XOR (RNG)       ; mix with previous RNG value
        OR A
        JR NZ, RSEED_OK
        INC A           ; RNG must not be 0 (LFSR stops at 0)
RSEED_OK:
        LD (RNG), A

        ; ---- Set up the initial snake  (3 segments, heading right) ----
        ;
        ;   SBUF[0] = (11, 18)  <-- tail
        ;   SBUF[1] = (11, 19)  <-- middle body
        ;   SBUF[2] = (11, 20)  <-- head
        ;
        XOR A
        LD (STAIL), A   ; STAIL = 0
        LD A, 2
        LD (SHEAD), A   ; SHEAD = 2

        LD HL, SBUF     ; point to SBUF[0]
        LD (HL), 11     ; [0].row = 11
        INC HL
        LD (HL), 18     ; [0].col = 18
        INC HL
        LD (HL), 11     ; [1].row = 11
        INC HL
        LD (HL), 19     ; [1].col = 19
        INC HL
        LD (HL), 11     ; [2].row = 11
        INC HL
        LD (HL), 20     ; [2].col = 20

        ; ---- Draw initial snake on screen ----
        LD B, 11
        LD C, 18
        LD A, BLOCK
        CALL PUTCHR
        LD B, 11
        LD C, 19
        LD A, BLOCK
        CALL PUTCHR
        LD B, 11
        LD C, 20
        LD A, BLOCK
        CALL PUTCHR

        ; ---- Set initial direction = right ----
        LD A, DRIGHT
        LD (DIR), A
        LD (NDIR), A

        ; ---- Place the first food pellet ----
        CALL PLACEFOOD

        ; ---- Snapshot the 50 Hz clock for edge detection ----
        LD A, (CLKLO)
        LD (PCLK), A

        ; ---- Init frame counter ----
        XOR A
        LD (SPEED), A



; MAIN LOOP — executes once per 50 Hz tick (20 ms)

LOOP:
        ; ------------------------------------------------------------------
        ; 1.  WAIT FOR THE NEXT 50 Hz TICK
        ;     Spin until CLKLO differs from the value saved last iteration.
        ; ------------------------------------------------------------------
        LD HL, CLKLO
WCLK:   LD A, (PCLK)
        CP (HL)
        JR Z, WCLK      ; same value -> tick not yet arrived
        LD A, (HL)
        LD (PCLK), A    ; save new clock value

        ; ------------------------------------------------------------------
        ; 2.  TICK THE RNG  (Galois LFSR — 8-bit, XOR mask B8h)
        ;     Polynomial x^8+x^6+x^5+x^4 gives maximal period 255.
        ;     Run every frame so food placement varies with play rhythm.
        ; ------------------------------------------------------------------
        CALL RNGBYTE    ; advance LFSR one step; result stored in (RNG)

        ; ------------------------------------------------------------------
        ; 3.  READ KEYBOARD
        ;     Port 38h returns the last key with bit 7 set as "valid" flag.
        ;     Strip bit 7, check Ctrl-C, then convert case to uppercase so
        ;     both lower and upper wasd work.
        ; ------------------------------------------------------------------
        IN A, (038H)
        AND 07FH        ; strip bit 7 valid flag -> 7-bit ASCII
        CP 03H          ; Ctrl-C (ASCII 3) ?
        JP Z, EXIT      ; yes -> quit to monitor

        AND 05FH        ; fold lowercase a-z to uppercase A-Z
        LD (KEY), A     ; save for direction tests below

        ; ------------------------------------------------------------------
        ; 4.  UPDATE PENDING DIRECTION
        ;     Pressing the key opposite to the current direction is silently
        ;     ignored — the snake cannot reverse on itself.
        ;     Opposite-direction check: direction D and its reverse share
        ;     (D XOR 2) == reverse.  We test: if DIR == (new XOR 2) -> skip.
        ; ------------------------------------------------------------------

        ; ---- W -> UP (DUP=3) — blocked only when currently going DOWN ----
        LD A, (KEY)
        CP 57H          ; 'W'?
        JR NZ, NOK_U
        LD A, (DIR)
        CP DDOWN        ; going DOWN is the opposite of UP
        JR Z, NOK_U    ; can't turn 180 degrees -> ignore
        LD A, DUP
        LD (NDIR), A
NOK_U:
        ; ---- S -> DOWN (DDOWN=1) — blocked only when going UP ----
        LD A, (KEY)
        CP 53H          ; 'S'?
        JR NZ, NOK_D
        LD A, (DIR)
        CP DUP
        JR Z, NOK_D
        LD A, DDOWN
        LD (NDIR), A
NOK_D:
        ; ---- A -> LEFT (DLEFT=2) — blocked only when going RIGHT ----
        LD A, (KEY)
        CP 41H          ; 'A'?
        JR NZ, NOK_L
        LD A, (DIR)
        CP DRIGHT
        JR Z, NOK_L
        LD A, DLEFT
        LD (NDIR), A
NOK_L:
        ; ---- D -> RIGHT (DRIGHT=0) — blocked only when going LEFT ----
        LD A, (KEY)
        CP 44H          ; 'D'?
        JR NZ, NOK_R
        LD A, (DIR)
        CP DLEFT
        JR Z, NOK_R
        LD A, DRIGHT
        LD (NDIR), A
NOK_R:

        ; ------------------------------------------------------------------
        ; 5.  SPEED THROTTLE  — move every 4th tick (~12.5 Hz)
        ;     INC SPEED, test bits 1:0.  When both are zero (SPEED mod 4 == 0)
        ;     we take a move step; otherwise loop back for the next tick.
        ; ------------------------------------------------------------------
        LD A, (SPEED)
        INC A
        LD (SPEED), A
        AND 03H
        JP NZ, LOOP     ; not a move tick -> back to clock wait

        ; ------------------------------------------------------------------
        ; 6.  COMMIT DIRECTION
        ;     Apply the pending direction now that we are on a move tick.
        ; ------------------------------------------------------------------
        LD A, (NDIR)
        LD (DIR), A

        ; ------------------------------------------------------------------
        ; 7.  FETCH CURRENT HEAD POSITION FROM SBUF[SHEAD]
        ;     Address of SBUF[SHEAD].row  =  9000h + SHEAD*2
        ;     With SHEAD in 0..63,  SHEAD*2 is 0..126 (fits in one byte),
        ;     so the high byte of the address is always 90h.
        ; ------------------------------------------------------------------
        LD A, (SHEAD)
        ADD A, A        ; A = SHEAD * 2  (no overflow: max 63*2 = 126)
        LD L, A
        LD H, 090H      ; HL = 9000h + SHEAD*2  =  &SBUF[SHEAD].row
        LD B, (HL)      ; B = head row
        INC HL
        LD C, (HL)      ; C = head col

        ; ------------------------------------------------------------------
        ; 8.  LOOK UP DIRECTION DELTA  (drow, dcol) FROM DRTAB
        ;     DRTAB holds 4 two-byte entries in direction order.
        ; ------------------------------------------------------------------
        LD A, (DIR)
        ADD A, A        ; *2 (each entry is 2 bytes)
        LD E, A
        LD D, 0
        LD HL, DRTAB
        ADD HL, DE      ; HL = &DRTAB[DIR]
        LD D, (HL)      ; D = drow  (00h = 0, 01h = +1, FFh = -1)
        INC HL
        LD E, (HL)      ; E = dcol

        ; ------------------------------------------------------------------
        ; 9.  COMPUTE NEW HEAD POSITION
        ;     ADD A wraps mod 256; FFh (-1) + row works correctly because
        ;     rows 1-22 never wrap below 0 (they hit the wall first).
        ; ------------------------------------------------------------------
        LD A, B
        ADD A, D        ; new_row = head_row + drow
        LD B, A
        LD A, C
        ADD A, E        ; new_col = head_col + dcol
        LD C, A         ; B = new_row,  C = new_col

        ; ------------------------------------------------------------------
        ; 10. CHECK WHAT IS AT THE NEW CELL
        ;     GETCHR preserves B and C (SCRADDR does not modify them).
        ; ------------------------------------------------------------------
        CALL GETCHR     ; A = screen byte at (B, C)

        CP BLOCK
        JP Z, GAMEOVER  ; hit wall (7Fh) or body segment -> GAME OVER

        CP FOOD
        JR Z, EATFOOD   ; food pellet -> grow

        ; ------------------------------------------------------------------
        ; 11. NORMAL MOVE — slide the snake one cell
        ;     a) Erase the tail cell from the screen.
        ;     b) Advance STAIL (tail segment leaves the ring buffer).
        ;     c) Advance SHEAD, write new head position.
        ;     d) Draw new head on screen.
        ; ------------------------------------------------------------------

        ; a) Erase tail: fetch SBUF[STAIL], write BLANK to screen
        LD A, (STAIL)
        ADD A, A        ; A = STAIL * 2
        LD L, A
        LD H, 090H      ; HL = &SBUF[STAIL].row
        LD D, (HL)      ; D = tail row
        INC HL
        LD E, (HL)      ; E = tail col
        ;    D, E clobbered by PUTCHR — save new head position (B,C) first
        PUSH BC         ; preserve new head position across PUTCHR
        LD B, D
        LD C, E
        LD A, BLANK
        CALL PUTCHR     ; erase tail; B,C preserved by PUTCHR/SCRADDR
        POP BC          ; restore new head position

        ; b) Advance STAIL
        LD A, (STAIL)
        INC A
        AND SMASK       ; mod 64
        LD (STAIL), A

        ; c) Advance SHEAD, write new head into ring buffer
        LD A, (SHEAD)
        INC A
        AND SMASK
        LD (SHEAD), A   ; store new SHEAD (A still holds new value)
        ADD A, A        ; A = new SHEAD * 2
        LD L, A
        LD H, 090H      ; HL = &SBUF[new_SHEAD].row
        LD (HL), B      ; .row = new head row
        INC HL
        LD (HL), C      ; .col = new head col

        ; d) Draw new head
        LD A, BLOCK
        CALL PUTCHR     ; B = new_row,  C = new_col

        JP LOOP



; EATFOOD — snake eats the food pellet at (B, C)
;
; The tail is NOT erased — the snake grows by one segment.

EATFOOD:
        ; Advance SHEAD and write new head (the food cell) into ring buffer
        LD A, (SHEAD)
        INC A
        AND SMASK
        LD (SHEAD), A   ; A = new SHEAD index
        ADD A, A
        LD L, A
        LD H, 090H
        LD (HL), B      ; SBUF[new_SHEAD].row = new head row
        INC HL
        LD (HL), C      ; SBUF[new_SHEAD].col = new head col

        ; Draw new head (overwrites the food pellet character)
        LD A, BLOCK
        CALL PUTCHR

        ; Place a new food pellet somewhere in the play field
        CALL PLACEFOOD

        JP LOOP



; GAME OVER — display the frozen board for ~2 seconds, then return

GAMEOVER:
        ; Wait 100 ticks (~2 seconds at 50 Hz) so the player sees the
        ; frozen board, then restart from the beginning.
        LD D, 100
GOWAIT: LD HL, CLKLO
GOWLP:  LD A, (PCLK)
        CP (HL)
        JR Z, GOWLP
        LD A, (HL)
        LD (PCLK), A
        DEC D
        JR NZ, GOWAIT
        JP START        ; reinitialise and play again



; EXIT — Ctrl-C: re-enable interrupts and return to the monitor

EXIT:
        EI
        RET




; INITSCR — initialise the screen for a new game
;
; 1. Write GFXMK (97h) into column 0 of every row.
; 2. Clear columns 1-39 of every row to BLANK.
; 3. Fill top wall    (row 0,  cols 1-39) with BLOCK.
; 4. Fill bottom wall (row 23, cols 1-39) with BLOCK.
; 5. Fill left wall   (rows 1-22, col 1)  with BLOCK.
; 6. Fill right wall  (rows 1-22, col 39) with BLOCK.
;
; Registers: A, B, C, D, E, H, L all clobbered.

INITSCR:
        ; ---- Steps 1+2: write GFXMK to col 0, BLANK to cols 1-39 ----
        LD B, 24        ; row counter — 24 rows total
        LD HL, ROWTAB   ; pointer into the row-address table
ILUP:
        ; Load row start address from ROWTAB (little-endian 16-bit word)
        LD E, (HL)
        INC HL
        LD D, (HL)
        INC HL
        PUSH BC         ; save outer row counter (B)
        PUSH HL         ; save ROWTAB pointer
        EX DE, HL       ; HL = row start address in screen RAM
        LD (HL), GFXMK  ; col 0 = 97h (graphics mode marker)
        INC HL          ; advance to col 1
        LD B, 39        ; 39 remaining columns (1-39)
ICLR:   LD (HL), BLANK
        INC HL
        DJNZ ICLR
        POP HL          ; restore ROWTAB pointer
        POP BC          ; restore row counter
        DJNZ ILUP

        ; ---- Step 3: top wall (row 0, cols 1-39) ----
        LD B, 0
        LD C, 1
        LD D, 39        ; 39 cells
ITOP:   LD A, BLOCK
        PUSH DE         ; D clobbered by SCRADDR inside PUTCHR
        CALL PUTCHR     ; B, C preserved; D, E, H, L clobbered
        POP DE
        INC C
        DEC D
        JR NZ, ITOP

        ; ---- Step 4: bottom wall (row 23, cols 1-39) ----
        LD B, 23
        LD C, 1
        LD D, 39
IBOT:   LD A, BLOCK
        PUSH DE
        CALL PUTCHR
        POP DE
        INC C
        DEC D
        JR NZ, IBOT

        ; ---- Step 5: left wall (rows 1-22, col 1) ----
        LD B, 1
        LD C, 1
        LD D, 22        ; 22 cells
ILFT:   LD A, BLOCK
        PUSH DE
        CALL PUTCHR
        POP DE
        INC B
        DEC D
        JR NZ, ILFT

        ; ---- Step 6: right wall (rows 1-22, col 39) ----
        LD B, 1
        LD C, 39
        LD D, 22
IRGT:   LD A, BLOCK
        PUSH DE
        CALL PUTCHR
        POP DE
        INC B
        DEC D
        JR NZ, IRGT

        RET



; PLACEFOOD — place a FOOD pellet at a random empty cell in the play area
;
; Uses RNGBYTE to generate row and column independently (two fresh LFSR steps
; each) so they do not share carry-flag contamination.  Tries up to 64
; positions; gives up silently if all are occupied (play field nearly full).
;
; PFROW and PFCOL hold the candidate position across the GETCHR call
; (GETCHR clobbers D, E, H, L but not these memory locations).

PLACEFOOD:
        LD B, 64        ; attempt counter for DJNZ
PFLOOP:
        PUSH BC         ; save attempt counter

        ; ---- Two RNG steps for the row (fresh, independent value) ----
        CALL RNGBYTE    ; step 1
        CALL RNGBYTE    ; step 2 — A = new RNG value

        ; Map to row 1-22: use lower 5 bits (0-31), fold 22-31 -> 0-9, add 1
        AND 01FH
        CP 22
        JR C, PFROW_OK
        SUB 22
PFROW_OK:
        INC A
        LD (PFROW), A

        ; ---- Two more RNG steps for the column (separate from row) ----
        CALL RNGBYTE
        CALL RNGBYTE    ; A = new RNG value

        ; Map to col 2-38: use lower 6 bits (0-63), fold 37-63 -> 0-26, add 2
        AND 03FH
        CP 37
        JR C, PFCOL_OK
        SUB 37
PFCOL_OK:
        ADD A, 2
        LD (PFCOL), A

        ; ---- Check whether that screen cell is empty ----
        LD A, (PFROW)
        LD B, A
        LD A, (PFCOL)
        LD C, A
        CALL GETCHR     ; A = screen byte;  B, C preserved;  D, E, H, L clobbered
        POP BC          ; restore attempt counter

        CP BLANK
        JR NZ, PFNEXT  ; occupied -> try again

        ; ---- Empty: place food here ----
        LD A, (PFROW)
        LD B, A
        LD A, (PFCOL)
        LD C, A
        LD A, FOOD
        CALL PUTCHR
        RET

PFNEXT:
        DJNZ PFLOOP
        RET             ; all attempts failed — give up



; RNGBYTE — advance the Galois LFSR by one step and return the new value in A
;
; XOR mask: B8h = 10111000b  (polynomial x^8+x^6+x^5+x^4, period 255)
; The state 00h is invalid for a Galois LFSR and is avoided by the seed guard
; in START.

RNGBYTE:
        LD A, (RNG)
        RRCA            ; shift right; LSB -> carry
        JR NC, RNGB1    ; if LSB was 0, no feedback needed
        XOR 0B8H        ; LSB was 1: apply feedback taps
RNGB1:  LD (RNG), A
        RET


; GETCHR — read one character from screen RAM
;
; Entry:  B = row (0-23),  C = col (0-39)
; Exit:   A = screen byte at that position;  B, C preserved

GETCHR:
        CALL SCRADDR    ; HL = screen address;  A, D, E, H, L clobbered;  B, C preserved
        LD A, (HL)
        RET



; PUTCHR — write one character to screen RAM
;
; Entry:  B = row (0-23),  C = col (0-39),  A = byte to write
; Exit:   character written;  B, C preserved;  HL = screen address

PUTCHR:
        PUSH AF         ; save character and flags
        CALL SCRADDR    ; HL = screen address;  B, C preserved
        POP AF          ; restore character
        LD (HL), A      ; write it
        RET



; SCRADDR — convert (row, col) to a screen-RAM address
;
; Entry:  B = row (0-23),  C = col (0-39)
; Exit:   HL = screen-RAM address;  B, C preserved
;         A, D, E clobbered
;
; The ABC80 video RAM (7C00h-7FFFh) is interleaved across three 8-row banks
; with a stride of 80h (128) bytes between rows and 28h (40) bytes between
; banks.  ROWTAB at the bottom of this file holds the 24 row start addresses
; as little-endian 16-bit words; we index it as ROWTAB[row*2].
;
; NOTE: uses ADD A,A (not RLCA) so the result is independent of the carry flag.

SCRADDR:
        LD HL, ROWTAB   ; base of the row-address table
        LD A, B         ; A = row (0-23)
        ADD A, A        ; A = row * 2  (max 23*2 = 46, no overflow)
        LD D, 0
        LD E, A
        ADD HL, DE      ; HL -> ROWTAB entry for this row
        LD A, (HL)
        INC HL
        LD H, (HL)
        LD L, A         ; HL = row start address (loaded little-endian)
        LD D, 0
        LD E, C
        ADD HL, DE      ; HL = rowstart + col  =  screen address
        RET


; DRTAB — direction delta table
;
; Four entries in direction-code order (DRIGHT=0, DDOWN=1, DLEFT=2, DUP=3).
; Each entry is two bytes: (drow, dcol).
; FFh represents -1 using Z80 signed byte / modulo-256 arithmetic.

DRTAB:
        DEFB 0,    1    ; DRIGHT: drow=0,  dcol=+1
        DEFB 1,    0    ; DDOWN:  drow=+1, dcol=0
        DEFB 0,  0FFH   ; DLEFT:  drow=0,  dcol=-1
        DEFB 0FFH, 0    ; DUP:    drow=-1, dcol=0



; ROWTAB — screen-RAM start address for each of the 24 rows (little-endian)
;
; The ABC80 video RAM layout:
;   Bank 0 (rows  0- 7): 7C00h, 7C80h, 7D00h, 7D80h, 7E00h, 7E80h, 7F00h, 7F80h
;   Bank 1 (rows  8-15): 7C28h, 7CA8h, 7D28h, 7DA8h, 7E28h, 7EA8h, 7F28h, 7FA8h
;   Bank 2 (rows 16-23): 7C50h, 7CD0h, 7D50h, 7DD0h, 7E50h, 7ED0h, 7F50h, 7FD0h
;
; Row stride = 80h (128 bytes).  Bank offset = 28h (40 bytes = one row width).
; Actually this table already exists in ROM, but for completeness ..

ROWTAB:
        DW 07C00H       ; row  0
        DW 07C80H       ; row  1
        DW 07D00H       ; row  2
        DW 07D80H       ; row  3
        DW 07E00H       ; row  4
        DW 07E80H       ; row  5
        DW 07F00H       ; row  6
        DW 07F80H       ; row  7
        DW 07C28H       ; row  8
        DW 07CA8H       ; row  9
        DW 07D28H       ; row 10
        DW 07DA8H       ; row 11
        DW 07E28H       ; row 12
        DW 07EA8H       ; row 13
        DW 07F28H       ; row 14
        DW 07FA8H       ; row 15
        DW 07C50H       ; row 16
        DW 07CD0H       ; row 17
        DW 07D50H       ; row 18
        DW 07DD0H       ; row 19
        DW 07E50H       ; row 20
        DW 07ED0H       ; row 21
        DW 07F50H       ; row 22
        DW 07FD0H       ; row 23
