/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 * Adapted in April 2025 by Niklas Ekström to work with the SPIder expansion.
 */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>

#include <hardware/intbits.h>

#include <proto/exec.h>

#include "spi.h"
#include "config_file.h"

#define REG_RESERVED_0          0
#define REG_RESERVED_1          1
#define REG_UPPER_LENGTH        2   // WO, Upper byte for lengths
#define REG_CARD_DETECT         3   // RO, Read CD
#define REG_RX_HEAD             4   // RO
#define REG_RX_TAIL             5   // RO
#define REG_TX_HEAD             6   // RO
#define REG_TX_TAIL             7   // RO
#define REG_RX_DISCARD          8   // WO, Lower byte
#define REG_TX_FEED             9   // WO, Lower byte
#define REG_SPI_FREQ            10  // WO, Set SPI frequency
#define REG_SLAVE_SELECT        11  // WO, Write SS
#define REG_INT_FIRED           12  // RW
#define REG_INT_ARMED           13  // WO
#define REG_FIFO                14  // RW, Write to TX, Read from RX
#define REG_IDENT               15  // RO

#define IRQ_CD_CHANGED          1

#define TEN_KHZ                 10000
#define ONE_MHZ                 1000000

#define IDENT_SIZE              8

static const UBYTE ident_str[] = {0xff, 's', 'p', 'd', 'r'};

#define DEFAULT_CLOCKPORT_ADDRESS   0xD80001
#define DEFAULT_INTERRUPT_NUMBER    6

static struct ClockportConfig clockport_config = {
    .clockport_address = DEFAULT_CLOCKPORT_ADDRESS,
    .interrupt_number = DEFAULT_INTERRUPT_NUMBER,
};

static volatile BYTE *clockport_address;

#define CP_REG(reg)         ((volatile UBYTE *)(clockport_address + ((reg) << 2)))
#define CP_WR(reg, val)     (*CP_REG((reg)) = (val))
#define CP_RD(reg)          (*CP_REG((reg)))

static const char spi_lib_name[] = "spi-lib-spider";

struct InterruptData
{
    volatile UBYTE *clockport_address;
    void (*change_isr)();
};

extern void InterruptServer();

static struct InterruptData interrupt_data;
static struct Interrupt ports_interrupt;

extern void kprintf(const char *fmt, ...);

void spi_select()
{
    CP_WR(REG_SLAVE_SELECT, 1);
}

void spi_deselect()
{
    CP_WR(REG_SLAVE_SELECT, 0);
}

int spi_get_card_present()
{
    int present = CP_RD(REG_CARD_DETECT);

    kprintf("CP_RD(REG_CARD_DETECT) = %ld\n", present);

    // Re-enable the CD changed interrupt.
    CP_WR(REG_INT_ARMED, IRQ_CD_CHANGED);

    return present;
}

void spi_set_speed(long speed)
{
    UBYTE freq = speed == SPI_SPEED_FAST ? (128 + 16) : 40;
    CP_WR(REG_SPI_FREQ, freq);
    kprintf("CP_WR(REG_SPI_FREQ, freq=%ld)\n", (LONG)freq);
}

// These two assembly functions were contributed by Patrik Axelsson.
static void copy_from_reg(__reg("a0") UBYTE *dst, __reg("a1") volatile UBYTE *reg, __reg("d0") WORD length) =
"    move.w  d0,d1\n"
"    lsr.w   #4,d0   ; Number of loops given 16 rept\n"
"    andi.w  #$f,d1  ; Bytes to copy the first loop iteration\n"
"    add.w   d1,d1   ; Adjust to steps in instruction size of a rept\n"
"    neg.w   d1\n"
"    jmp     .after_repts(pc,d1.w)\n"
".loop:\n"
"    rept    16\n"
"    move.b  (a1),(a0)+\n"
"    endr\n"
".after_repts:\n"
"    dbra    d0,.loop\n";

static void copy_to_reg(__reg("a0") volatile UBYTE *reg, __reg("a1") const UBYTE *src, __reg("d0") WORD length) =
"    move.w  d0,d1\n"
"    lsr.w   #4,d0   ; Number of loops given 16 rept\n"
"    andi.w  #$f,d1  ; Bytes to copy the first loop iteration\n"
"    add.w   d1,d1   ; Adjust to steps in instruction size of a rept\n"
"    neg.w   d1\n"
"    jmp     .after_repts(pc,d1.w)\n"
".loop:\n"
"    rept    16\n"
"    move.b  (a1)+,(a0)\n"
"    endr\n"
".after_repts:\n"
"    dbra    d0,.loop\n";

void spi_read(__reg("a0") UBYTE *buf, __reg("d0") WORD size)
{
    CP_WR(REG_UPPER_LENGTH, size >> 8);
    CP_WR(REG_TX_FEED, size & 0xff);

    volatile BYTE *fifo = CP_REG(REG_FIFO);

    UBYTE rx_head = CP_RD(REG_RX_HEAD);
    UBYTE rx_tail;

    if (size == 1)
    {
        do
        {
            rx_tail = CP_RD(REG_RX_TAIL);
        }
        while (rx_head == rx_tail);

        *buf = *fifo;
    }
    else
    {
        do
        {
            rx_tail = CP_RD(REG_RX_TAIL);

            UBYTE bytes_in_rx = rx_tail - rx_head;

            if (bytes_in_rx)
            {
                copy_from_reg(buf, fifo, bytes_in_rx);
                buf += bytes_in_rx;
                rx_head += bytes_in_rx;
                size -= bytes_in_rx;
            }
        }
        while (size);
    }
}

void spi_write(__reg("a0") const UBYTE *buf, __reg("d0") WORD size)
{
    CP_WR(REG_UPPER_LENGTH, size >> 8);
    CP_WR(REG_RX_DISCARD, size & 0xff);

    volatile BYTE *fifo = CP_REG(REG_FIFO);

    UBYTE tx_head;
    UBYTE tx_tail = CP_RD(REG_TX_TAIL);

    if (size == 1)
    {
        UBYTE next_tx_tail = tx_tail + 1;
        do
        {
            tx_head = CP_RD(REG_TX_HEAD);
        }
        while (next_tx_tail == tx_head);

        *fifo = *buf;
    }
    else
    {
        do
        {
            tx_head = CP_RD(REG_TX_HEAD);

            UBYTE bytes_in_tx = tx_tail - tx_head;
            UBYTE free_space = 255 - bytes_in_tx;

            if (free_space)
            {
                if (free_space > size)
                    free_space = size;

                copy_to_reg(fifo, buf, free_space);
                buf += free_space;
                tx_tail += free_space;
                size -= free_space;
            }
        }
        while (size);
    }
}

static int probe_interface()
{
    UBYTE read_bytes[IDENT_SIZE];

    for (short i = 0; i < IDENT_SIZE; i++)
        read_bytes[i] = CP_RD(REG_IDENT);

    BOOL found = FALSE;
    short pos = 0;

    for (short start = 0; start < IDENT_SIZE && !found; start++)
    {
        found = TRUE;
        pos = start;

        for (short i = 0; i < sizeof(ident_str); i++)
        {
            if (read_bytes[pos] != ident_str[i])
            {
                found = FALSE;
                break;
            }
            pos = (pos + 1) & (IDENT_SIZE - 1);
        }
    }

    if (!found)
        return -1;

    UBYTE fw_major_ver = read_bytes[pos];

    // Only major version 1 is currently supported.
    if (fw_major_ver != 1)
        return -2;

    pos = (pos + 1) & (IDENT_SIZE - 1);
    UBYTE fw_minor_ver = read_bytes[pos];

    pos = (pos + 1) & (IDENT_SIZE - 1);
    UBYTE fw_patch_ver = read_bytes[pos];

    kprintf("SPIder firmware version: %ld.%ld.%ld\n", (ULONG)fw_major_ver, (ULONG)fw_minor_ver, (ULONG)fw_patch_ver);

    return 0;
}

int spi_initialize(void (*change_isr)())
{
    read_and_parse_config_file(&clockport_config);

    clockport_address = (volatile UBYTE *)clockport_config.clockport_address;

    if (probe_interface() < 0)
        return -1;

    CP_WR(REG_SPI_FREQ, 40);

    CP_WR(REG_INT_ARMED, 0);
    CP_WR(REG_INT_FIRED, 0);

    interrupt_data.clockport_address = clockport_address;
    interrupt_data.change_isr = change_isr;

    ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
    ports_interrupt.is_Node.ln_Pri = -60;
    ports_interrupt.is_Node.ln_Name = (char *)spi_lib_name;
    ports_interrupt.is_Data = (APTR)&interrupt_data;
    ports_interrupt.is_Code = InterruptServer;

    LONG int_num = clockport_config.interrupt_number == 2 ? INTB_PORTS : (clockport_config.interrupt_number == 3 ? INTB_VERTB : INTB_EXTER);
    AddIntServer(int_num, &ports_interrupt);

    return spi_get_card_present();
}

void spi_shutdown()
{
    CP_WR(REG_INT_ARMED, 0);
    CP_WR(REG_INT_FIRED, 0);

    LONG int_num = clockport_config.interrupt_number == 2 ? INTB_PORTS : (clockport_config.interrupt_number == 3 ? INTB_VERTB : INTB_EXTER);
    RemIntServer(int_num, &ports_interrupt);
}
