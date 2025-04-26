set -x
vc +aos68k -I$NDK32/Include_H romtag.c version.c device.c sd.c timer.c ../../spi-lib-spider/spi.c ../../spi-lib-spider/interrupt.asm -I../../spi-lib-spider -L${NDK32}/lib -O2 -nostdlib -lamiga -ldebug -o spisd.device
