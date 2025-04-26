    XDEF    _InterruptServer
    CODE

;struct InterruptData
;{
;    volatile BYTE *clockport_address;
;    void (*change_isr)();
;};

    ; a1 points to InterruptData structure

_InterruptServer:
    move.l  (a1),a5

    move.b  48(a5),d0               ; INT_FIRED
    beq.s   .spurious

    moveq   #0,d0
    move.b  d0,52(a5)               ; clear INT_ARMED
    move.b  d0,48(a5)               ; clear INT_FIRED

    move.l  4(a1),a1
    jsr     (a1)

.spurious:
    moveq    #0,d0
    rts
