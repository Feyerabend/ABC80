; This code is inherited and modified from samples at
; https://8bitworkshop.com/ and the emulator for Atari 2600
; by Steven Hugg.
;
; It displays a partially implemented "AIR-FIGHT".
; The code might have to be changed heavily due to
; the very odd the properties of the Atari 2600/VCS,
; if fully implemented.


	processor 6502
        include "vcs.h"
        include "macro.h"
        include "xmacro.h"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; We're going to set the player's coarse and fine position
; at the same time using a clever method.
; We divide the X coordinate by 15, in a loop that itself
; is 15 cycles long. When the loop exits, we are at
; the correct coarse position, and we set RESP0.
; The accumulator holds the remainder, which we convert
; into the fine position for the HMP0 register.
; This logic is in a subroutine called SetHorizPos.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SpriteHeight	equ #8
CounterStart	equ #16
DirectionStart	equ #4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Variables segment

        seg.u Variables
	org $80

XPos		.byte
YPos		.byte
Steering	.byte
Direction	.byte
Counter		.byte
FramePtr	.word


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Code segment

	seg Code
        org $f000

Start
	CLEAN_START
        
        lda #80
        sta YPos
        sta XPos
        
        lda #0
        sta Steering
        lda DirectionStart
        sta Direction
        lda CounterStart
        sta Counter

NextFrame
        lsr SWCHB	; test Game Reset switch
        bcc Start	; reset?
; 1 + 3 lines of VSYNC
	VERTICAL_SYNC
; 37 lines of underscan
	TIMER_SETUP 37
; move X and Y coordinates w/ joystick
	jsr MoveJoystick
        jsr CheckDirection
        jsr CheckXYPosLimits
; the next two scanlines
; position the player horizontally
	lda XPos	; get X coordinate
        ldx #0		; player 0
        jsr SetHorizPos	; set coarse offset
        sta WSYNC	; sync w/ scanline
        sta HMOVE	; apply fine offsets
; it's ok if we took an extra scanline because
; the PIA timer will always count 37 lines
; wait for end of underscan
        TIMER_WAIT
; 192 lines of frame
	ldx #192	; X = 192 scanlines
LVScan
	txa		; X -> A
        sec		; set carry for subtract
        sbc YPos	; local coordinate
        cmp #SpriteHeight ; in sprite?
        bcc InSprite	; yes, skip over next
        lda #0		; not in sprite, load 0
InSprite
	tay		; local coord -> Y
        lda (FramePtr),y	; lookup color
	sta WSYNC	; sync w/ scanline
        sta GRP0	; store bitmap
        lda ColorFrame,y ; lookup color
        sta COLUP0	; store color
        dex		; decrement X
        bne LVScan	; repeat until 192 lines

; 29 lines of overscan
	TIMER_SETUP 29
        TIMER_WAIT
; total = 262 lines, go to next frame
        jmp NextFrame

; SetHorizPos routine
; A = X coordinate
; X = player number (0 or 1)
SetHorizPos
	sta WSYNC	; start a new line
	sec		; set carry flag
DivideLoop
	sbc #15		; subtract 15
	bcs DivideLoop	; branch until negative
	eor #7		; calculate fine offset
	asl
	asl
	asl
	asl
	sta RESP0,x	; fix coarse position
	sta HMP0,x	; set fine offset
	rts		; return to caller

; Read joystick movement and apply to object 0
MoveJoystick

        lda Steering    ; get previous (frame) status for joystick
        bit SWCHA       ; check for current status
        beq NewTurn     ; if not the same as previous, get a new turn

	lda #15		; just a number to delay move
        adc Counter	; add to counter
	sta Counter	; store it
        bne EndSteering	; check for end of delay, goto end if not zero (roll over)

Steer			; input from joystick, steering left or right
	ldx Direction	; get direction

LeftTurn
        lda #%01000000
        bit SWCHA	; check for LEFT from joystick
        bne RightTurn	; if not left, could it be right?
        jmp LeftDirection

RightTurn
        lda #%10000000
        bit SWCHA	; check for RIGHT from joystick
        bne ZeroTurn	; no recognized input from joystick
        jmp RightDirection

			; SUBROUTINE
NewTurn                 ; the steering started and we initilize
			; we got a new start of a turn
        lda CounterStart 
        sta Counter
        lda SWCHA
        sta Steering	; load the current status from joystick
        rts

			; SUBROUTINES LEF AND RIGHT
                        ; change direction accoring to steering (and delay)
LeftDirection           ; check for "rollover" where 0 goes to 7 and 7 goes to 0,
                        ; otherwise just increase or decrease
	ldx Direction	;
        cpx #7
        bcc RollOverL
        ldx #0
        jmp StoreDirection

RollOverL
        inx
        jmp StoreDirection

RightDirection
        lda #0
        cmp Direction
        bne RollOverR
        ldx #7
        jmp StoreDirection

RollOverR
        dex

StoreDirection
	stx Direction

EndSteering
        lda SWCHA	; store current joystick
        sta Steering	; in steering
	rts

ZeroTurn
	lda #0		; reset
        sta Counter	; counter
        sta Steering	; and steering
	rts

			; SUBROUTINE
CheckDirection		; check for which direction is taken (0-7)
			; move to position in that direction
                        ; also select sprite for each direction

        ldx Direction
Zero
        cpx #0
        bne NotZero  
        inc XPos
        lda #<Frame0
	sta FramePtr
	lda #>Frame0
	sta FramePtr+1
        rts
NotZero  
        cpx #1
        bne NotOne
        inc XPos
        inc YPos
	lda #<Frame1
	sta FramePtr
	lda #>Frame1
	sta FramePtr+1
	rts
NotOne
        cpx #2
        bne NotTwo
        inc YPos
        lda #<Frame2
        sta FramePtr
        lda #>Frame2
        sta FramePtr+1
	rts
NotTwo
        cpx #3
        bne NotThree
        inc YPos
        dec XPos
        lda #<Frame3
        sta FramePtr
        lda #>Frame3
        sta FramePtr+1
        rts
NotThree
        cpx #4
        bne NotFour
        dec XPos
        lda #<Frame4
        sta FramePtr
        lda #>Frame4
        sta FramePtr+1
	rts
NotFour
        cpx #5
        bne NotFive
        dec XPos
        dec YPos
        lda #<Frame5
        sta FramePtr
        lda #>Frame5
        sta FramePtr+1
	rts
NotFive
        cpx #6
        bne NotSix
        dec YPos
        lda #<Frame6
        sta FramePtr
        lda #>Frame6
        sta FramePtr+1
	rts
NotSix
        cpx #7
        bne EndDirections
        dec YPos
        inc XPos
        lda #<Frame7
        sta FramePtr
        lda #>Frame7
        sta FramePtr+1

EndDirections
	rts


			; SUBROUTINE
CheckXYPosLimits	; check for min and max of XPos and YPos,
			; flip, if on boundary

CheckYPos
        ldx YPos
        cpx #2
        bne NotMINY
        ldx #183
        stx YPos
        jmp CheckXPos
NotMINY
        cpx #183
        bne CheckXPos
        ldx #2
        stx YPos

CheckXPos 
        ldx XPos
        cpx #16
        bne NotMINX
        ldx #153
        stx XPos
        jmp EndCheck
NotMINX
        cpx #153
        bne EndCheck
        ldx #16
        stx XPos

EndCheck
	rts


				; graphics data of aeroplane
Frame0				; right
        .byte #0
        .byte #%01100000
        .byte #%01110000
        .byte #%01111000
        .byte #%11111111
        .byte #%01111000
        .byte #%01110000
        .byte #%01100000
        .byte #%00000000

Frame1				; right up
        .byte #0
        .byte #%00011000
        .byte #%00011000
        .byte #%01111000
        .byte #%01111000
        .byte #%11111000
        .byte #%11111100
        .byte #%00000110
        .byte #%00000010

Frame2				; up
        .byte #0
        .byte #%00010000
        .byte #%11111110 
        .byte #%11111110
        .byte #%01111100
        .byte #%00111000
        .byte #%00010000 
        .byte #%00010000 
        .byte #%00010000

Frame3				; left up
        .byte #0
        .byte #%00011000
        .byte #%00011000
        .byte #%00011110
        .byte #%00011110
        .byte #%00011111
        .byte #%00111111
        .byte #%01100000
        .byte #%01000000

Frame4				; left
        .byte #0
        .byte #%00000110
        .byte #%00001110
        .byte #%00011110  
        .byte #%11111111
        .byte #%00011110
        .byte #%00001110  
        .byte #%00000110   
        .byte #%00000000

Frame5				; left down
        .byte #0
        .byte #%00000000
        .byte #%01100000
        .byte #%00111111
        .byte #%00011111
        .byte #%00011110
        .byte #%00011110
        .byte #%00011000
        .byte #%00011000

Frame6				; down
        .byte #0
        .byte #%00010000
        .byte #%00010000
        .byte #%00010000
        .byte #%00111000
        .byte #%01111100     
        .byte #%11111110      
        .byte #%11111110       
        .byte #%00010000
				; right down
Frame7
        .byte #0
        .byte #%00000110
        .byte #%11111100
        .byte #%11111000
        .byte #%01111000 
        .byte #%01111000
        .byte #%00011000
        .byte #%00011000
        .byte #%00000000

; color data
ColorFrame
        .byte #0
        .byte #$99
        .byte #$99
        .byte #$99
        .byte #$99
        .byte #$99
        .byte #$99
        .byte #$99
        .byte #$99
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Epilogue

	org $fffc
        .word Start	; reset vector
        .word Start	; BRK vector
