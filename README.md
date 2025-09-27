# MIDAL MIDI Interface Firmware

Firmware for the **MIDAL** piano pedal interface built around the
ProMicro nRF52840. The device samples three optical pedals (damper,
sostenuto, soft) at high resolution and streams the data over USB MIDI 2.0,
classic MIDI 1.0 and Bluetooth LE MIDI.

## Features

- Three-channel pedal sampling using the SAADC with automatic filtering,
  calibration and asymmetric response for musical feel.
- USB MIDI 2.0 transport based on Zephyr's `device-next` stack with
  optional MIDI 2.0 UMP output.
- Bluetooth LE MIDI transport powered by the
  [`zephyr-ble-midi`](https://github.com/stuffmatic/zephyr-ble-midi)
  module (advertises BLE-MIDI service 03B80E5A-EDE8-4B33-A751-6CE34EC4C700).
- Modular architecture: `pedal_*` modules for sampling/filters,
  `midi_router` for dispatching, transport modules for USB/BLE.
- Heartbeat and logging infrastructure for runtime diagnostics.

## Hardware Summary

- Base board: ProMicro nRF52840.
- Pedal inputs:
  - Damper – P0.31 / NRF_SAADC_AIN7
  - Sostenuto – P0.02 / NRF_SAADC_AIN0
  - Soft – P0.29 / NRF_SAADC_AIN5
- Optical pedal module (e.g. Kawai GFP-3) powered at 3V3 with 1 kΩ series
  resistors and 4.7 kΩ shunt for biasing.
- Status LEDs (not implemented yet)
  - `LED_POWER`: on when awake, blink on data wipe
  - `LED_USB`: on when USB idle, blink on activity
  - `LED_BLE`: on when BLE idle, blink on activity
- User buttons (not implemented yet)
  - `BTN_POWER`: wake on short press
  - `BTN_BLE`: short = disconnect, long = forget pairings
  - `BTN_USB`: short = toggle auto-off, long = clear calibration

## Firmware Architecture

```
pedal_sampler (SAADC + filters) -> midi_router -> transports (USB / BLE)
```

- `src/pedal/pedal_sampler.c`: configures ADC channels, manages filtering and
  calibration and publishes `midi_event_t` values.
- `src/midi/midi_router.c`: queue-based router with per-transport worker
  threads, statistics, and drop counters.
- `src/transports/transport_usb_midi.c`: formats MIDI 1.0 (scaled 7-bit) and
  MIDI 2.0 UMP control changes. Handles host backpressure.
- `src/transports/transport_ble_midi.c`: thin wrapper around the external
  `ble_midi` module. Keeps advertising alive across reconnects and reports
  readiness to the heartbeat.
- `src/diag/heartbeat.c`: periodic health log (USB/BLE readiness, router
  queue depth, drop count).

## Prerequisites

- **nRF Connect SDK 3.1.1** with the matching Zephyr toolchain.
- `west` (installed with the SDK) and git submodules enabled.
- Repository layout (clone this repo, then initialize submodules):

  ```bash
  git clone <your fork>
  cd midal
  git submodule update --init --recursive
  ```

## Building

1. Export the SDK environment (via nRF Connect for VS Code or CLI):

   ```bash
   source ~/ncs/v3.1.1/zephyr/zephyr-env.sh
   ```

2. Build with west:

   ```bash
   west build -b promicro_nrf52840_nrf52840_uf2
   ```

3. Flash:

   ```bash
   west flash
   ```

   or copy the generated UF2 to the bootloader drive.

## Configuration Highlights

Key options in `prj.conf`:

- `CONFIG_MIDAL_POLL_HZ`: SAADC sampling frequency (default 1000 Hz).
- `CONFIG_MIDAL_USE_14BIT_CC`: emit high-resolution CC values.
- Bluetooth stack tuning:
  - `CONFIG_BT_*` buffer counts sized for the SoftDevice controller
  - `CONFIG_BLE_MIDI_*` options from the `zephyr-ble-midi` module.
- USB MIDI pipeline: optional UMP output via `CONFIG_MIDAL_USB_MIDI2_NATIVE`.

## Runtime Notes

- Heartbeat output logs USB/BLE readiness and router queue statistics once
  per second.
- The USB transport may log `Unable to allocate Tx net_buf` if the host pauses;
  this is normal and the driver retries automatically.
- BLE advertising restarts automatically after disconnects; the BLE transport
  schedules retries if the controller is temporarily out of buffers.
- Calibration runs continuously. When pedals are unplugged, the filters clamp
  output until valid motion is detected.

## Directory Overview

- `src/`
  - `pedal/`: sampling, filtering, calibration
  - `midi/`: router, codec and event types
  - `transports/`: USB and BLE transports
  - `diag/`: heartbeat and self-test utilities
- `modules/lib/zephyr-ble-midi`: external BLE MIDI service module (git
  submodule).

## Troubleshooting

- **ADC conversion timeouts**: occasional warnings can appear during heavy
  BLE/USB traffic; the reader aborts and retries automatically. Persistent
  timeouts usually indicate wiring or power issues.

## Contributing / Next Ideas

- Consider migrating the MIDI event bus to Zephyr Zbus for decoupling.
- Add pedal presence detection so unplugged pedals are muted automatically.
- Implement basic indication and button-based control.
- Add BLE configuration support.
- Add display support and visual configuration.

Pull requests are welcome! Please run `west build` for the promicro board and
ensure coding style follows the surrounding modules.
