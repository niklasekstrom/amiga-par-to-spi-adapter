/*
 * Written in October 2022 by Niklas Ekström.
 *
 * Runs on RP2040 microcontroller instead of AVR as before,
 * but uses the same protocol and Amiga software.
 */
#include "hardware/gpio.h"
#include "hardware/spi.h"

//      Pin name    GPIO    Direction   Comment     Description
#define PIN_D(x)    (0+x)   // In/out
#define PIN_IRQ     8       // Output   Active low
#define PIN_ACT     9       // Output   Active low
#define PIN_CLK     10      // Input
#define PIN_REQ     11      // Input    Active low
#define PIN_MISO    16      // Input    Pull-up
#define PIN_SS      17      // Output   Active low
#define PIN_SCK     18      // Output
#define PIN_MOSI    19      // Output
#define PIN_CDET    20      // Input    Pull-up     Card Detect

#define SPI_SLOW_FREQUENCY (400*1000)
#define SPI_FAST_FREQUENCY (16*1000*1000)

static uint32_t prev_cdet;

static void handle_request() {
    uint32_t pins;

    while (1) {
        pins = gpio_get_all();
        if (!(pins & (1 << PIN_REQ)))
            break;

        if ((pins & (1 << PIN_CDET)) != prev_cdet) {
            gpio_put(PIN_IRQ, false);
            gpio_set_dir(PIN_IRQ, true);
            prev_cdet = pins & (1 << PIN_CDET);
        }
    }

    uint32_t prev_clk = pins & (1 << PIN_CLK);

    if ((pins & 0xc0) != 0xc0) {
        uint32_t byte_count = 0;
        bool read = false;

        if (!(pins & 0x80)) { // READ1 or WRITE1
            read = !!(pins & 0x40);
            byte_count = pins & 0x3f;

            gpio_put(PIN_ACT, 0);
        } else { // READ2 or WRITE2
            byte_count = (pins & 0x3f) << 7;

            gpio_put(PIN_ACT, 0);

            while (1) {
                pins = gpio_get_all();
                if ((pins & (1 << PIN_CLK)) != prev_clk)
                    break;

                if (pins & (1 << PIN_REQ))
                    return;
            }

            read = !!(pins & 0x80);
            byte_count |= pins & 0x7f;
            prev_clk = pins & (1 << PIN_CLK);
        }

        if (read) {
            spi_get_hw(spi0)->dr = 0xff;

            uint32_t prev_ss = pins & (1 << PIN_SS);

            while (1) {
                while (!spi_is_readable(spi0))
                    tight_loop_contents();

                uint32_t value = spi_get_hw(spi0)->dr;

                while (1) {
                    pins = gpio_get_all();
                    if ((pins & (1 << PIN_CLK)) != prev_clk)
                        break;

                    if (pins & (1 << PIN_REQ))
                        return;
                }

                gpio_put_all(prev_ss | value);
                gpio_set_dir_out_masked(0xff);

                if (!byte_count)
                    break;

                spi_get_hw(spi0)->dr = 0xff;
                prev_clk = pins & (1 << PIN_CLK);
                byte_count--;
            }
        } else {
            while (1) {
                while (1) {
                    pins = gpio_get_all();
                    if ((pins & (1 << PIN_CLK)) != prev_clk)
                        break;

                    if (pins & (1 << PIN_REQ))
                        return;
                }

                spi_get_hw(spi0)->dr = pins & 0xff;

                while (!spi_is_readable(spi0))
                    tight_loop_contents();

                (void)spi_get_hw(spi0)->dr;

                if (!byte_count)
                    break;

                prev_clk = pins & (1 << PIN_CLK);
                byte_count--;
            }
        }
    } else {
        switch ((pins & 0x3e) >> 1) {
            case 0: { // SPI_SELECT
                gpio_put(PIN_SS, !(pins & 1));
                gpio_put(PIN_ACT, 0);
                break;
            }
            case 1: { // CARD_PRESENT
                gpio_set_dir(PIN_IRQ, false);
                gpio_put(PIN_ACT, 0);

                while (1) {
                    pins = gpio_get_all();
                    if ((pins & (1 << PIN_CLK)) != prev_clk)
                        break;

                    if (pins & (1 << PIN_REQ))
                        return;
                }

                gpio_put(PIN_D(0), !gpio_get(PIN_CDET));
                gpio_set_dir_out_masked(0xff);
                break;
            }
            case 2: { // SPEED
                spi_set_baudrate(spi0, pins & 1 ?
                        SPI_FAST_FREQUENCY :
                        SPI_SLOW_FREQUENCY);

                gpio_put(PIN_ACT, 0);
                break;
            }
        }
    }

    while (1) {
        pins = gpio_get_all();
        if (pins & (1 << PIN_REQ))
            break;
    }
}

int main() {
    spi_init(spi0, SPI_SLOW_FREQUENCY);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(PIN_MISO);

    gpio_init(PIN_SS);
    gpio_put(PIN_SS, 1);
    gpio_set_dir(PIN_SS, GPIO_OUT);

    gpio_init(PIN_CDET);
    gpio_pull_up(PIN_CDET);

    for (int i = 0; i < 12; i++)
        gpio_init(i);

    gpio_put(PIN_ACT, 1);
    gpio_set_dir(PIN_ACT, GPIO_OUT);

    prev_cdet = gpio_get_all() & (1 << PIN_CDET);

    while (1) {
        handle_request();

        gpio_set_dir_in_masked(0xff);
        gpio_clr_mask(0xff);

        gpio_put(PIN_ACT, 1);

        while (spi_is_busy(spi0))
            tight_loop_contents();

        if (spi_is_readable(spi0))
            (void)spi_get_hw(spi0)->dr;
    }
}
