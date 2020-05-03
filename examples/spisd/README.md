# SD card module

An SD card module can be used with the SPI adapter in order to mount the SD card as a volume in AmigaOS.
This driver uses the code written by Mike Sterling: https://github.com/mikestir/k1208-drivers/tree/master/sd. Thanks Mike!

The file `sd.c` does all the heavy lifiting and remains largely unchanged compared to Mike's original. The file device.c had to be partially changed in order to be compiled with VBCC. The only reason I'm using VBCC instead of using gcc is that I haven't been successful in installing gcc.

The `build.bat` Windows batch file contains the command line used to compile the driver with VBCC, producing the binary `spisd.device` which should go in the DEVS: directory.

Install the [fat95 file system handler](http://aminet.net/package/disk/misc/fat95) in L: and copy the mountfile (available [here](https://github.com/mikestir/k1208-drivers/tree/master/amiga)) to some suitable place where it can be used to mount the SD card (read more about how this works in other places, e.g. the fat95 documentation).
