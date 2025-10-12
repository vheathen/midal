# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

## [0.2.0] - 2025-10-12

### Added
- USB MIDI implementation via Zephyr's USB device-next stack (USBD)
- Bluetooth MIDI implementation via zephyr-ble-midi module
- MIDI Router with queue-based routing system
- Per-transport worker threads for USB and BLE MIDI
- Queue statistics and drop counters
- Heartbeat module for periodic health monitoring
- Advanced filtering system with automatic alpha calculation
- Asymmetric filtering (fast attack, slow release) for musical response
- Rate-limited logging (100ms throttle per pedal)
- Support for 14-bit MIDI CC messages
- Optional MIDI 2.0 UMP output via CONFIG_MIDAL_USB_MIDI2_NATIVE
- Configuration option CONFIG_MIDAL_FILTER_TAU_MS for filter time constant
- zephyr-ble-midi as git submodule
- .clang-format for code formatting

### Changed
- Refactored pedal system into modular architecture
- Split pedal_sampler.c into pedal.c, pedal_sampler.c, and pedal_sampler_thread.c
- Moved ADC reading from timer ISR to dedicated high-priority thread
- Changed threading architecture to Timer ISR → Semaphore → Sensor Thread
- Default polling rate set to 1000Hz
- Filter algorithm now uses frequency-independent time constants

### Fixed
- ADC read failures (-11/-22 errors) caused by calling blocking adc_read_dt() from timer ISR context
- Filter frequency scaling issue (filter was 1000x too slow at 1Hz test rate)
- Thread safety issues with semaphore-based coordination
- BLE discoverability issues after initial connect/disconnect
- SAADC self-test removed unsupported 80us acquisition-time

## [0.1.0] - 2024-09-23

### Added
- Initial firmware implementation
- SAADC pedal sampling for three optical sensors (damper, sostenuto, soft)
- USB CDC logger via Zephyr's logging system
- Basic MIDI message generation
- ProMicro nRF52840 board support
- CMake build system
- Kconfig configuration
- Apache 2.0 license
- README and documentation
