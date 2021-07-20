# Assembly instructions

The assembly of the adapter is quite simple.
You need a parallel port connector and an Arduino Nano.
Connect the parallel port to the Nano as follows:

| Par. pin # | Par. pin | Nano pin | Atmega328P |
|---:|-----:|----:|----:|
|  2 |   D0 |  A0 | PC0 |
|  3 |   D1 |  A1 | PC1 |
|  4 |   D2 |  A2 | PC2 |
|  5 |   D3 |  A3 | PC3 |
|  6 |   D4 |  A4 | PC4 |
|  7 |   D5 |  A5 | PC5 |
|  8 |   D6 |  D6 | PD6 |
|  9 |   D7 |  D7 | PD7 |
| 10 |  ACK |  D9 | PB1 |
| 11 | BUSY |  D8 | PB0 |
| 12 | POUT |  D4 | PD4 |
| 13 |  SEL |  D2 | PD2 |
| 18..25 |  GND | GND | GND |

## Connecting SD card module to Nano

To use the adapter with the example SD card driver (spisd.device),
connect the SD card module to the Nano as follows:

| SD card | Nano pin | Atmega328P |
|-----:|----:|----:|
|   SS | D10 | PB2 |
| MOSI | D11 | PB3 |
| MISO | D12 | PB4 |
|  SCK | D13 | PB5 |
|   CD |  D3 | PD3 |
|  VCC | VCC |     |
|  GND | GND | GND |

CD = Card Detect. Not all modules has this pin available, but since the Micro SD card connectors typically has this pin it should be possible to solder a wire to the connector.

Note that the SD card module must have voltage level translation between the +5V used by the Amiga and the Nano, and the +3.3V used by the SD card.
