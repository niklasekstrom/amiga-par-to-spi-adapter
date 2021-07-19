/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 */
#include <avr/interrupt.h>
#include <avr/io.h>

// Par  P-name  Arduino AVR     SD      Name    Description
// 2    D0      A0      PC0                     Parallel port data lines
// 3    D1      A1      PC1
// 4    D2      A2      PC2
// 5    D3      A3      PC3
// 6    D4      A4      PC4
// 7    D5      A5      PC5
// 8    D6      D6      PD6
// 9    D7      D7      PD7
// 10   ACK     D9      PB1             IRQ     Interrupt request to Amiga
// 11   BUSY    D8      PB0             ACT     Indicate command running
// 12   POUT    D4      PD4             CLK     Clock to advance command
// 13   SEL     D2      PD2             REQ     Amiga wants to execute command
//              D3      PD3     CD      CP      Card Present
//              D10     PB2     SS
//              D11     PB3     MOSI
//              D12     PB4     MISO
//              D13     PB5     SCK

// Pins in port B.
#define SCK_BIT         5 // Output.
#define MISO_BIT        4 // Input.
#define MOSI_BIT        3 // Output.
#define SS_BIT_n        2 // Output, active low.
#define IRQ_BIT_n       1 // Output, active low, open collector.
#define ACT_BIT_n       0 // Output, active low.

// Pins in port D.
#define CLK_BIT         4 // Input.
#define CP_BIT_n        3 // Input, active low, internal pull-up enabled.
#define REQ_BIT_n       2 // Input, active low, internal pull-up enabled.

void start_command()
{
    uint8_t dval;
    uint8_t cval;
    uint8_t next_port_d;
    uint8_t next_port_c;
    uint16_t byte_count;

    dval = PIND;
    cval = PINC;

    if (!(dval & 0x80)) // READ1 or WRITE1
    {
        byte_count = cval;

        PORTB &= ~(1 << ACT_BIT_n);

        if (dval & 0x40)
            goto do_read;
        else
            goto do_write;
    }
    else if (!(dval & 0x40)) // READ2 or WRITE2
    {
        byte_count = cval << 7;

        PORTB &= ~(1 << ACT_BIT_n);

        if (dval & (1 << CLK_BIT))
        {
            while (PIND & (1 << CLK_BIT))
                ;
        }
        else
        {
            while (!(PIND & (1 << CLK_BIT)))
                ;
        }

        dval = PIND;
        cval = PINC;

        byte_count |= (dval & 0x40) | cval;
        if (dval & 0x80)
            goto do_read;
        else
            goto do_write;
    }
    else
    {
        uint8_t cmd = (cval & 0x3e) >> 1;
        if (cmd == 0) // SPI_SELECT
        {
            if (cval & 1) // Select
                PORTB &= ~(1 << SS_BIT_n);
            else // Deselect
                PORTB |= (1 << SS_BIT_n);

            PORTB &= ~(1 << ACT_BIT_n);
        }
        else if (cmd == 1) // CARD_PRESENT
        {
            DDRB &= ~(1 << IRQ_BIT_n);
            PORTB &= ~(1 << ACT_BIT_n);

            if (dval & (1 << CLK_BIT))
            {
                while (PIND & (1 << CLK_BIT))
                    ;
            }
            else
            {
                while (!(PIND & (1 << CLK_BIT)))
                    ;
            }

            DDRD = 0xc0;
            DDRC = 0x3f;

            if (!(PIND & (1 << CP_BIT_n)))
                PORTC = 1;
            else
                PORTC = 0;
        }
        else if (cmd == 2) // SPEED
        {
            if (cval & 1) // Fast
                SPCR = (1 << SPE) | (1 << MSTR);
            else // Slow
                SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);

            PORTB &= ~(1 << ACT_BIT_n);
        }

        while (1)
            ;
    }

do_read:
    SPDR = 0xff;

    PORTD = (dval & 0xc0) | (1 << CP_BIT_n) | (1 << REQ_BIT_n);
    DDRD = 0xc0;

    PORTC = cval;
    DDRC = 0x3f;

read_loop:
    while (!(SPSR & (1 << SPIF)))
        ;

    next_port_c = SPDR;
    next_port_d = (next_port_c & 0xc0) | (1 << CP_BIT_n) | (1 << REQ_BIT_n);

    if (dval & (1 << CLK_BIT))
    {
        while (PIND & (1 << CLK_BIT))
            ;
    }
    else
    {
        while (!(PIND & (1 << CLK_BIT)))
            ;
    }

    PORTD = next_port_d;
    PORTC = next_port_c;

    dval = PIND;

    if (byte_count)
    {
        byte_count--;
        SPDR = 0xff;
        goto read_loop;
    }

    while (1)
        ;

do_write:
write_loop:
    if (dval & (1 << CLK_BIT))
    {
        while (PIND & (1 << CLK_BIT))
            ;
    }
    else
    {
        while (!(PIND & (1 << CLK_BIT)))
            ;
    }

    dval = PIND;
    SPDR = (dval & 0xc0) | PINC;

    while (!(SPSR & (1 << SPIF)))
        ;

    (void) SPDR;

    if (byte_count)
    {
        byte_count--;
        goto write_loop;
    }

    while (1)
        ;
}

void busy_wait()
{
    while (1)
        ;
}

// Interrupt handler for REQ signal changes (INT0).
ISR(INT0_vect, ISR_NAKED)
{
    void (*next_fn)();

    if (PIND & (1 << REQ_BIT_n))
    {
        PORTB |= (1 << ACT_BIT_n);

        DDRD = 0;
        PORTD = (1 << CP_BIT_n) | (1 << REQ_BIT_n);

        DDRC = 0;
        PORTC = 0;

        EIMSK |= (1 << 1);

        next_fn = &busy_wait;
    }
    else
    {
        EIMSK &= ~(1 << 1);

        next_fn = &start_command;
    }

    uint16_t fn_int = (uint16_t)next_fn;
    uint16_t sp = ((SPH << 8) | SPL) + 1;
    uint8_t *p = (uint8_t *)sp;
    *p++ = fn_int >> 8;
    *p++ = fn_int & 0xff;

    reti();
}

// Interrupt handler for CP signal changes (INT1).
ISR(INT1_vect, ISR_NAKED)
{
    DDRB |= (1 << IRQ_BIT_n);

    reti();
}

void main()
{
    DDRB = (1 << SCK_BIT) | (1 << MOSI_BIT) | (1 << SS_BIT_n) | (1 << ACT_BIT_n);
    PORTB = (1 << SS_BIT_n) | (1 << ACT_BIT_n);

    // SPI enabled, master, fosc/64 = 250 kHz
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR |= (1 << SPI2X);

    DDRD = 0;
    PORTD = (1 << CP_BIT_n) | (1 << REQ_BIT_n);

    DDRC = 0;
    PORTC = 0;

    // Enable interrupts
    EICRA = (1 << 2) | (1 << 0);
    EIFR = (1 << 1) | (1 << 0);
    EIMSK = (1 << 1) | (1 << 0);

    sei();

    while (1)
        ;
}
