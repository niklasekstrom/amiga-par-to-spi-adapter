# RP2040 version

The code for the AVR microcontroller has been ported to the RP2040 microcontroller.
RP2040 is the microcontroller used on the Raspberry Pi Pico board.

## Build instructions

The [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) must be installed.
Then, standing in the rp2040 subdirectory, execute:

```bash
mkdir build
cd build
export PICO_SDK_PATH=~/pico/pico-sdk
cmake ..
make
```

Copy the generated file `build/par_spi.uf2` to the microcontroller's flash.
