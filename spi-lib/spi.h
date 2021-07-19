/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 */
#ifndef SPI_H_
#define SPI_H_

#define SPI_SPEED_SLOW 0
#define SPI_SPEED_FAST 1

int spi_initialize(void (*change_isr)());
int spi_get_card_present();
void spi_shutdown();
void spi_set_speed(long speed);
void spi_select();
void spi_deselect();
void spi_read(__reg("a0") unsigned char *buf, __reg("d0") unsigned long size);
void spi_write(__reg("a0") const unsigned char *buf, __reg("d0") unsigned long size);

#endif
