# Assembly instructions

The assembly of the adapter is very simple.
You need a parallel port connector and an Arduino Nano.
Connect the parallel port to the Nano as follows:

| Par. pin # | Par. pin name | Nano pin # | Nano pin name |
|-----------:|--------------:|-----------:|--------------:|
|  2 |   D0 | 26 | A0 |
|  3 |   D1 | 25 | A1 |
|  4 |   D2 | 24 | A2 |
|  5 |   D3 | 23 | A3 |
|  6 |   D4 | 22 | A4 |
|  7 |   D5 | 21 | A5 |
|  8 |   D6 |  9 | D6 |
|  9 |   D7 | 10 | D7 |
| 11 | BUSY |  7 | D4 |
| 12 | POUT |  8 | D5 |
| 20 |  GND |  4 | GND |

Note that pins D0-D5 are connected to port C on the AVR (pins A0-A5 on the Nano) and pins D6-D7 are connected to port D on the AVR (pins D6-D7 on the Nano).
This is identical to how the [plipbox](https://github.com/cnvogelg/plipbox) connects the parallel port to the Arduino Nano, so it should be possible to use an existing plipbox PCB for this adapter (although this has not been tested).

If you are connecting an SD card module to the adapter then connect pin 13 on the parallel port connector (SEL) to the chip select (CS) pin on the SD card module, and connect the other SPI pins as you normally would connect an SPI module to the Arduino Nano.
