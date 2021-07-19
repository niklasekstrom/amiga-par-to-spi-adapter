/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>
#include <hardware/cia.h>
#include <resources/cia.h>
#include <resources/misc.h>

#include <proto/exec.h>
#include "cia_protos.h"
#include "misc_protos.h"

#include "spi.h"

#define REQ_BIT		CIAB_PRTRSEL
#define CLK_BIT		CIAB_PRTRPOUT
#define ACT_BIT		CIAB_PRTRBUSY

#define REQ_MASK	(1 << REQ_BIT)
#define CLK_MASK	(1 << CLK_BIT)
#define ACT_MASK	(1 << ACT_BIT)

extern void spi_read_fast(__reg("a0") UBYTE *buf, __reg("d0") ULONG size);
extern void spi_write_fast(__reg("a0") const UBYTE *buf, __reg("d0") ULONG size);

static volatile UBYTE *cia_a_prb = (volatile UBYTE *)0xbfe101;
static volatile UBYTE *cia_a_ddrb = (volatile UBYTE *)0xbfe301;

static volatile UBYTE *cia_b_pra = (volatile UBYTE *)0xbfd000;
static volatile UBYTE *cia_b_ddra = (volatile UBYTE *)0xbfd200;

static long current_speed = SPI_SPEED_SLOW;

static const char spi_lib_name[] = "spi-lib";

static struct Library *miscbase;
static struct Library *ciaabase;

static struct Interrupt flag_interrupt;

static int wait_until_active()
{
	int count = 32;
	UBYTE ctrl = *cia_b_pra;
	while (count > 0 && (ctrl & ACT_MASK))
	{
		count--;
		ctrl = *cia_b_pra;
	}
	return count;
}

void spi_select()
{
	*cia_a_prb = 0xc1;

	UBYTE prev = *cia_b_pra;
	*cia_b_pra = prev & ~REQ_MASK;

	wait_until_active();

	*cia_b_pra = prev;
}

void spi_deselect()
{
	*cia_a_prb = 0xc0;

	UBYTE prev = *cia_b_pra;
	*cia_b_pra = prev & ~REQ_MASK;

	wait_until_active();

	*cia_b_pra = prev;
}

int spi_get_card_present()
{
	*cia_a_prb = 0xc2;

	UBYTE ctrl = *cia_b_pra;
	ctrl &= ~REQ_MASK;
	*cia_b_pra = ctrl;

	if (!wait_until_active())
	{
		ctrl |= REQ_MASK;
		*cia_b_pra = ctrl;
		return -1;
	}

	*cia_a_ddrb = 0x00;

	ctrl ^= CLK_MASK;
	*cia_b_pra = ctrl;

	int present = *cia_a_prb & 1;

	ctrl |= REQ_MASK;
	*cia_b_pra = ctrl;

	*cia_a_ddrb = 0xff;

	return present;
}

void spi_set_speed(long speed)
{
	*cia_a_prb = speed == SPI_SPEED_FAST ? 0xc5 : 0xc4;

	UBYTE prev = *cia_b_pra;
	*cia_b_pra = prev & ~REQ_MASK;

	wait_until_active();

	*cia_b_pra = prev;

	current_speed = speed;
}

// A slow SPI transfer takes 32 us (8 bits times 4us (250kHz)).
// An E-cycle is 1.4 us.
static void wait_40_us()
{
	UBYTE tmp;
	for (int i = 0; i < 32; i++)
		tmp = *cia_b_pra;
}

static void spi_write_slow(__reg("a0") const UBYTE *buf, __reg("d0") ULONG size)
{
	UBYTE ctrl = *cia_b_pra;

	if (size <= 64) // WRITE1: 00xxxxxx
	{
		*cia_a_prb = (size - 1) & 0x3f;

		ctrl &= ~REQ_MASK;
		*cia_b_pra = ctrl;

		wait_until_active();
	}
	else // WRITE2: 10xxxxxx 0xxxxxxx
	{
		*cia_a_prb = 0x80 | (((size - 1) >> 7) & 0x3f);

		ctrl &= ~REQ_MASK;
		*cia_b_pra = ctrl;

		wait_until_active();

		*cia_a_prb = (size - 1) & 0x7f;

		ctrl ^= CLK_MASK;
		*cia_b_pra = ctrl;
	}

	for (int i = 0; i < size; i++)
	{
		*cia_a_prb = *buf++;

		ctrl ^= CLK_MASK;
		*cia_b_pra = ctrl;

		wait_40_us();
	}

	ctrl |= REQ_MASK;
	*cia_b_pra = ctrl;
}

static void spi_read_slow(__reg("a0") UBYTE *buf, __reg("d0") ULONG size)
{
	UBYTE ctrl = *cia_b_pra;

	if (size <= 64) // READ1: 01xxxxxx
	{
		*cia_a_prb = 0x40 | ((size - 1) & 0x3f);

		ctrl &= ~REQ_MASK;
		*cia_b_pra = ctrl;

		wait_until_active();
	}
	else // READ2: 10xxxxxx 1xxxxxxx
	{
		*cia_a_prb = 0x80 | (((size - 1) >> 7) & 0x3f);

		ctrl &= ~REQ_MASK;
		*cia_b_pra = ctrl;

		wait_until_active();

		*cia_a_prb = 0x80 | ((size - 1) & 0x7f);

		ctrl ^= CLK_MASK;
		*cia_b_pra = ctrl;
	}

	*cia_a_ddrb = 0;

	for (int i = 0; i < size; i++)
	{
		wait_40_us();

		ctrl ^= CLK_MASK;
		*cia_b_pra = ctrl;

		*buf++ = *cia_a_prb;
	}

	ctrl |= REQ_MASK;
	*cia_b_pra = ctrl;

	*cia_a_ddrb = 0xff;
}

void spi_read(__reg("a0") UBYTE *buf, __reg("d0") ULONG size)
{
	if (current_speed == SPI_SPEED_FAST)
		spi_read_fast(buf, size);
	else
		spi_read_slow(buf, size);
}

void spi_write(__reg("a0") const UBYTE *buf, __reg("d0") ULONG size)
{
	if (current_speed == SPI_SPEED_FAST)
		spi_write_fast(buf, size);
	else
		spi_write_slow(buf, size);
}

int spi_initialize(void (*change_isr)())
{
	int success = 0;

	miscbase = (struct Library *)OpenResource(MISCNAME);
	if (!miscbase)
	{
		success = -1;
		goto fail_out1;
	}

	ciaabase = (struct Library *)OpenResource(CIAANAME);
	if (!ciaabase)
	{
		success = -2;
		goto fail_out1;
	}

	if (AllocMiscResource(miscbase, MR_PARALLELPORT, spi_lib_name))
	{
		success = -3;
		goto fail_out1;
	}

	if (AllocMiscResource(miscbase, MR_PARALLELBITS, spi_lib_name))
	{
		success = -4;
		goto fail_out2;
	}

	flag_interrupt.is_Node.ln_Name = (char *)spi_lib_name;
	flag_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	flag_interrupt.is_Code = change_isr;

	Disable();
	if (AddICRVector(ciaabase, CIAICRB_FLG, &flag_interrupt))
	{
		Enable();
		success = -5;
		goto fail_out3;
	}

	AbleICR(ciaabase, CIAICRF_FLG);
	SetICR(ciaabase, CIAICRF_FLG);
	Enable();

	*cia_b_pra = (*cia_b_pra & ~ACT_MASK) | (REQ_MASK | CLK_MASK);
	*cia_b_ddra = (*cia_b_ddra & ~ACT_MASK) | (REQ_MASK | CLK_MASK);

	*cia_a_prb = 0xff;
	*cia_a_ddrb = 0xff;

	int card_present = spi_get_card_present();
	if (card_present < 0)
	{
		success = -6;
		goto fail_out4;
	}

	AbleICR(ciaabase, CIAICRF_SETCLR | CIAICRF_FLG);

	return card_present;

fail_out4:
	*cia_b_ddra &= ~(ACT_MASK | REQ_MASK | CLK_MASK);
	*cia_a_ddrb = 0;

	RemICRVector(ciaabase, CIAICRB_FLG, &flag_interrupt);

fail_out3:
	FreeMiscResource(miscbase, MR_PARALLELBITS);

fail_out2:
	FreeMiscResource(miscbase, MR_PARALLELPORT);

fail_out1:
	return success;
}

void spi_shutdown()
{
	AbleICR(ciaabase, CIAICRF_FLG);

	*cia_b_ddra &= ~(ACT_MASK | REQ_MASK | CLK_MASK);
	*cia_a_ddrb = 0;

	RemICRVector(ciaabase, CIAICRB_FLG, &flag_interrupt);

	FreeMiscResource(miscbase, MR_PARALLELBITS);
	FreeMiscResource(miscbase, MR_PARALLELPORT);
}
