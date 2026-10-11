# TODO
## Testing
- [x] DFU from 1.3.3 and pre 1.3.3
- [x] flashing `merged.hex`

## Documentation
- [x] ~~building without ANT~~
- [x] ANT SDK is open
- [x] building with/without `mcuboot.pem` signing key
- [x] ~~building from PR~~
- [ ] ID configuration using SMP

## Internals
- [x] Resource manager for consumption by supervisor
- [x] ~~async zbus listeners~~
- [x] Shared state store: https://www.zephyrproject.org/common-multithreading-problems-and-their-fixes-part-4/
- [x] Update retention system to DT based backend
  - https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/boards/nordic/system_off
  - https://docs.zephyrproject.org/latest/services/storage/retention/index.html
- [x] BLE Settings configuration
  - [x] add SMP settings mangement group
  - [x] add SMP OS group
  - [x] add SMP enum group
  - [x] adapt settings
- [x] BT ESS Environmental Sensing Service for Tire pressure
- [x] BT BAS Battery Service
- [x] CI
  - [x] ANT SDK is open: remove login and secret
  - [x] inject SB config for signing with  `mcuboot.pem` in CI;
  - [x] revert building from PR to PR context without signing
- [x] discard: 00 00 00 00 00 00 SPI receive
- [x] publish invalid (0xFFFF) sensor reading marker on SPI timeout

## User-facing
- [ ] Configuration web application using Web Bluetooth
- [ ] DFU web application using Web Bluetooth
- [ ] CMSIS DAP flasher web application using Web USB

### Notes/Links
- https://github.com/zephyrproject-rtos/zephyr/blob/main/samples/bluetooth/peripheral_esp/src/main.c
- https://github.com/FreeOCD/freeocd-web
- https://github.com/boogie/mcumgr-web
- https://github.com/qarnet/web-bluetooth-dfu
- https://github.com/intercreate/smpmgr
- https://github.com/Finomnis/mcumgr-toolkit
- https://github.com/makerdiary/web-device-cli
- https://github.com/KevinJohnMulligan/neutral-nus-terminal
- https://melt-ui.com/
- https://svelte.dev/