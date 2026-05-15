# Changelog
* The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
* This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
* Only publicly released versions (and work in progress) will be tracked
* Meaningful documentation starts with release v1.3.2

## [Unreleased]

## [1.3.3] 2026-05-16
### Added
- Data page 1
  - Sensor role (front/rear)
  - Sensor-side high/low pressure alarm; if thresholds configured
- Data page 16; full page for bi-directional configuration exchange
  - Alarm thresholds and role are stored persistently
  - Ambient pressure offset is stored in RAM only to limit flash wear
- Common page 70 (request data pages) support [issue #8]
- Changes to page 1 and addition of data pages 16 and 70 support add Karoo support [issue #6]

### Changed
- Software version reporting on common page 82
- OTA firmware update resets boot counter to 0
- Bluetooth enabled first boot after OTA DFU
- Removed leading "SKS" from Bluetooth device name; now "AIRSPY Community" only

### Fixed
- OTA DFU image validation/confirmation; going forward, for future updates on top of this (and future) firmware version
- Uninitialized memory in data page 1 [issue #7]

## [1.2.3] 2026-05-16
### Changed
- Pressure calibration curve fitting

### Fixed
- NUS (Nordic UART Service) logging backend; was accidentally disabled in last release


## [1.1.0] 2025-02-22
### Added
- Storage Partition
- BLE Service to persistently change ANT device ID

### Changed
- Changed flash partition layout
- Versioning scheme bumped to major 1, to make versions distinguishable on ANT displays: ANT common page 82 does not have enough resolution for patch versions

### Removed
- Compatibility for OTA DFU from older versions than this one due to flash layout differences


## [0.1.39] 2025-02-18 [YANKED]

## [0.1.32] 2025-02-11 [YANKED]
