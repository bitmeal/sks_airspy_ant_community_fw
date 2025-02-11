# Firmware Installation
- [Firmware Installation](#firmware-installation)
  - [SWD programmer recommendation](#swd-programmer-recommendation)
  - [Flashing example using GDB](#flashing-example-using-gdb)
  - [Reverting to stock firmware](#reverting-to-stock-firmware)

## SWD programmer recommendation
If you do not already own a compatible SWD programmer, a personal recommendation is to head over to [Black Magic](https://black-magic.org/hardware.html) and get hold of some compatible hardware, build the corresponding firmware with nRF5 series support from their [GitHub Repo](https://github.com/blackmagic-debug/blackmagic/tree/main) and install according to their instructions. The easiest and likely cheapest variant is to get hold of one or two ST-Link programmers; firmwares are provided in [`./doc/stlink_bmdb_firmware`](/doc/stlink_bmdb_firmware/), installation per [documentation](https://github.com/blackmagic-debug/blackmagic/tree/main/src/platforms/stlink#upload-bmp-firmware).


## Flashing example using GDB
When using anything providing a GDB server ([OpenOCD](https://openocd.org/pages/getting-openocd.html), [pyOCD](https://pyocd.io/), [Black Magic probe](https://black-magic.org/), etc.), get a copy of GDB for AArch32 bare-metal targets (`arm-none-eabi-gdb`) for your plattform. You can obtain it from [arm here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).

The example uses a Black Magic probe providing a GDB server we connect to on COM1. If using some OCD variant, GDB should pick up a local server in default configuration automatically. `(gdb)` is your prompt.


1. Start GDB
    ```console
    ## OCD flavor default config
    arm-none-eabi-gdb

    ## bmdb COM1
    arm-none-eabi-gdb -iex "target extended-remote \\.\COM1"


    #### expected GDB response (similar) and prompt
    GNU gdb (Arm GNU Toolchain 13.3.Rel1 (Build arm-13.24)) 14.2.90.20240526-git
    Copyright (C) 2023 Free Software Foundation, Inc.
    License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
    This is free software: you are free to change and redistribute it.
    There is NO WARRANTY, to the extent permitted by law.
    Type "show copying" and "show warranty" for details.
    This GDB was configured as "--host=i686-w64-mingw32 --target=arm-none-eabi".
    Type "show configuration" for configuration details.
    For bug reporting instructions, please see:
    <https://bugs.linaro.org/>.
    Find the GDB manual and other documentation resources online at:
        <http://www.gnu.org/software/gdb/documentation/>.

    For help, type "help".
    Type "apropos word" to search for commands related to "word".
    (gdb)
    ```

2. Scan for Devices, attach to the locked access port, and unlock and erase chip
    ```console
    ## ...
    ## scan devices
    (gdb) monitor swd_scan
    Target voltage: 2.99V
    Available Targets:
    No. Att Driver
    1      Nordic nRF52 Access Port (protected)

    ## attach
    (gdb) attach 1
    Attaching to Remote target

    ## unlock and erase
    (gdb) monitor erase_mass
    Erasing device Flash: done
    ```

3. **Power cycle the device!** It may work without for you, but it proved to be the most failsafe procedure.

4. Scan for Devices, attach to the unlocked device and flash new firmware.
    ```console
    ## ...
    ## scan devices
    (gdb) monitor swd_scan
    Target voltage: 2.99V
    Available Targets:
    No. Att Driver
    1      Nordic nRF52 M4
    2      Nordic nRF52 Access Port

    ## attach
    (gdb) attach 1
    Attaching to Remote target
    warning: No executable has been specified and target does not support
    determining executable automatically.  Try using the "file" command.
    <signal handler called>

    ## flash
    (gdb) load ./firmware_file_name.hex
    Loading section .sec1, size 0x5ea8 lma 0x0
    Loading section .sec2, size 0xa000 lma 0x6000
    Loading section .sec3, size 0x10000 lma 0x10000
    Loading section .sec4, size 0x10000 lma 0x20000
    Loading section .sec5, size 0xa60d lma 0x30000
    Start address 0x00000000, load size 238773
    Transfer rate: 42 KB/sec, 966 bytes/write.
    ```

## Reverting to stock firmware
> ⚠ To revert to stock firmware, you have to have access to a dump of the original firmware.

SKS embeds firmware update packages in their mobile apps. From these you can obtain the application firmware, and the information about the used Softdevice. To date, for all tested update packages, the Softdevice is S332, but none does include a bootloader. Thus, no assembly of a full firmware is possible from public sources. Dumping the firmware is disabled by access port protection on the nRF52832.

On a side note, while talking about APPROTECT: Parts of the nRF52 series are vulnerable to a glitching attack to circumvent this protection. As [demonstrated by limitedresults](https://limitedresults.com/2020/06/nrf52-debug-resurrection-approtect-bypass/), and [implemented by atc1441](https://github.com/atc1441/ESP32_nRF52_SWD) on an ESP32. Apart from the suggestions in both links, DEC4 with a delay of 29000 - 31000 µs is supposed to be a rewarding target as well.

To convert a binary dump to a loadable hex file use:
```bash
arm-none-eabi-objcopy -I binary -O elf32-littlearm --change-section-address=.data=0x0 -B arm -S flash_0x0.bin flash_0x0.elf
arm-none-eabi-objcopy -I binary -O elf32-littlearm --change-section-address=.data=0x10001000 -B arm -S uicr_0x10001000.bin uicr_0x10001000.elf
```

Load using GDB:
```bash
# gdb
load flash_0x0.elf 0x0
load uicr_0x10001000.elf 0x10001000
```
