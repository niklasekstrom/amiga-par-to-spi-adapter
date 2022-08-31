; Written in the end of April 2020 by Niklas Ekström.
; Updated in July 2021 by Niklas Ekström to handle Card Present signal.

        XDEF        _spi_read_fast
        XDEF        _spi_write_fast
        CODE

CIAB_PRTRSEL	equ	(2)
CIAB_PRTRPOUT	equ	(1)
CIAB_PRTRBUSY	equ	(0)

CIAA_BASE       equ     $bfe001
CIAB_BASE       equ     $bfd000

CIAPRA	        equ	$0000
CIAPRB	        equ	$0100
CIADDRA	        equ	$0200
CIADDRB	        equ	$0300

REQ_BIT         equ     CIAB_PRTRSEL
CLK_BIT         equ     CIAB_PRTRPOUT
ACT_BIT         equ     CIAB_PRTRBUSY

                ; a0 = unsigned char *buf
                ; d0 = unsigned int size
                ; assert: 1 <= size < 2^13 (three top bits are zeros)

_spi_write_fast:
                and     #$1fff,d0
                bne.b   .not_zero
                rts
.not_zero:
                movem.l d2/a5,-(a7)

                lea.l   CIAA_BASE+CIAPRB,a1     ; Data
                lea.l   CIAB_BASE+CIAPRA,a5     ; Control pins

                move.b  (a5),d2

                subq    #1,d0                   ; d0 = size - 1

                cmp     #63,d0
                ble.b   .one_byte_cmd

                ; WRITE2 = 10xxxxxx 0xxxxxxx
                move    d0,d1
                lsr     #7,d1
                or.b    #$80,d1
                move.b  d1,(a1)
                bclr    #REQ_BIT,d2
                move.b  d2,(a5)

.act_wait2:     move.b  (a5),d2
                btst    #ACT_BIT,d2
                bne.b   .act_wait2

                move.b  d0,d1
                and.b   #$7f,d1
                move.b  d1,(a1)
                bchg    #CLK_BIT,d2
                move.b  d2,(a5)
                bra.b   .cmd_sent

.one_byte_cmd:  ; WRITE1 = 00xxxxxx
                move.b  d0,(a1)
                bclr    #REQ_BIT,d2
                move.b  d2,(a5)

.act_wait1:     move.b  (a5),d2
                btst    #ACT_BIT,d2
                bne.b   .act_wait1

.cmd_sent:      addq    #1,d0                   ; d0 = size

                btst    #0,d0
                beq.b   .even

                move.b  (a0)+,(a1)
                bchg    #CLK_BIT,d2
                move.b  d2,(a5)

.even:          lsr     #1,d0
                beq.b   .done
                subq    #1,d0

                move.b  d2,d1
                bchg    #CLK_BIT,d1

.loop:          move.b  (a0)+,(a1)
                move.b  d1,(a5)
                move.b  (a0)+,(a1)
                move.b  d2,(a5)
                dbra    d0,.loop

.done:          move.b	d2,(a5)                 ; Delay to allow write to complete
                bset    #REQ_BIT,d2
                move.b  d2,(a5)

                movem.l (a7)+,d2/a5
                rts

                ; a0 = unsigned char *buf
                ; d0 = unsigned int size
                ; assert: 1 <= size < 2^13 (three top bits are zeros)

_spi_read_fast:
                and     #$1fff,d0
                bne.b   .not_zero
                rts
.not_zero:
                movem.l d2/a5,-(a7)

                lea.l   CIAA_BASE+CIAPRB,a1      ; Data
                lea.l   CIAB_BASE+CIAPRA,a5      ; Control pins

                move.b  (a5),d2

                subq    #1,d0                   ; d0 = size - 1

                cmp     #63,d0
                ble.b   .one_byte_cmd

                ; READ2 = 10xxxxxx 1xxxxxxx
                move    d0,d1
                lsr     #7,d1
                or.b    #$80,d1
                move.b  d1,(a1)
                bclr    #REQ_BIT,d2
                move.b  d2,(a5)

.act_wait2:     move.b  (a5),d2
                btst    #ACT_BIT,d2
                bne.b   .act_wait2

                move.b  d0,d1
                or.b    #$80,d1
                move.b  d1,(a1)
                bchg    #CLK_BIT,d2
                move.b  d2,(a5)
                bra.b   .cmd_sent

.one_byte_cmd:  ; READ1 = 01xxxxxx
                move.b  d0,d1
                or.b    #$40,d1
                move.b  d1,(a1)
                bclr    #REQ_BIT,d2
                move.b  d2,(a5)

.act_wait1:     move.b  (a5),d2
                btst    #ACT_BIT,d2
                bne.b   .act_wait1

.cmd_sent:      move.b  #0,$200(a1)             ; Stop driving data pins

                addq    #1,d0                   ; d0 = size

                btst    #0,d0
                beq.b   .even

                bchg    #CLK_BIT,d2
                move.b  d2,(a5)
                move.b  (a1),(a0)+

.even:          lsr     #1,d0
                beq.b   .done
                subq    #1,d0

                move.b  d2,d1
                bchg    #CLK_BIT,d1

.loop:          move.b  d1,(a5)
                move.b  (a1),(a0)+
                move.b  d2,(a5)
                move.b  (a1),(a0)+
                dbra    d0,.loop

.done:          bset    #REQ_BIT,d2
                move.b  d2,(a5)

                move.b  #$ff,$200(a1)             ; Start driving data pins

                movem.l (a7)+,d2/a5
                rts
