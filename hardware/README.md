# Video link

Coming soon!

# MIDAL Optical Pedal MIDI Interface — Hardware Overview

## Project Introduction

MIDAL is a standalone MIDI interface for acoustic-style piano pedals, built around the ProMicro nRF52840 module. It reads three pedal sensors — damper, sostenuto, and soft — from assemblies such as the Kawai GFP-3, and streams their motion over USB MIDI 1.0/2.0, Bluetooth LE MIDI, and (soon) classic DIN-5 MIDI independently and simultaneously for each channel.

The project was originally created to connect my own Kawai GFP-3 pedal block to a computer (and/or Apple Vision Pro) running Pianoteq, but the design is intended to work with any pedal block that shares the same pinout and power requirements.

Development began on a solderless breadboard, moves to a perfboard pilot soon, and will progress to a dedicated PCB once the electronics and firmware stabilize.

## Hardware Features & Functions

- **Three-channel sensor front-end** using the nRF52840 SAADC with automatic calibration and asymmetric filtering handled in firmware.
- **Multi-transport MIDI outputs**:
  - USB MIDI 1.0 + MIDI 2.0 (UMP) already live;
  - BLE MIDI is active through the Nordic SoftDevice;
  - a DIN-5 driver stage is captured on the schematic for the next hardware revision.
- **User interaction board** providing three LEDs (Power, USB, BLE) and three buttons (Wake, BLE disconnect/forget, Auto-off/Calibration reset).
- **Robust power handling** on the pedal rail: resettable fuse, ferrite bead, bulk/decoupling capacitors, and per-line ESD clamps.
- **Extensive test access** through labelled pads on the core schematic for
  power rails, pedal channels, and MIDI TX debugging.

## Key Hardware Parameters

- **Controller**: ProMicro nRF52840 (64 MHz ARM Cortex-M4F, 1 MB flash). Key pins: P0.31 (damper), P0.29 (soft), P0.02 (sostenuto), P1.15/P1.13/P1.11 (LED cathodes), and dedicated lines for the three buttons.
- **Sensor bias network**: series resistors R8/R9/R10 = 1 kΩ feed each SAADC channel; pull-ups R5/R6/R7 = 4.7 kΩ reference the sensors to 3V3, matching the pedal optical module characterization captured in `resistance-diag.txt`.
- **Button conditioning**: each switch (BTN_WAKE, BTN_BLE_DISC, SW_IDLE BTN_CAL_RESET) uses 10 kΩ pull-ups (R1–R4) and 100 nF RC snubbers (C1–C3) to suppress contact bounce before reaching the MCU.
- **Power protection**: pedal rail protected by 1206L025/16 resettable fuse F1, ferrite bead BLM18AG601SN1D (L1), bulk capacitor C5 = 10 µF, and decoupler C6 = 100 nF. All external lines protected by ESD5Z5VU TVS diodes (D1–D4 on the pedal connector, D6–D7 on the MIDI harness).
- **DIN-5 MIDI stage**: DTC114EK digital NPN transistor (Q1) plus 220 Ω series resistors (R11/R12) provide the opto-less current-loop driver; Schottky diode D5 (1N5819WS) isolates VBUS_5V feed for that section.

## Hardware Architecture

### Core Board — MCU & System Glue (`SCH_Core board_1-MC`)

- Hosts the ProMicro nRF52840 and routes power, pedal signals, USB/BLE LEDs, and button inputs.
- Dedicated headers expose LED cathodes (`LED_PWR_K`, `LED_USB_K`, `LED_BLE_K`) and button nets to the daughtercard.
- Test points `TP1`/`TP2` provide easy access to 3V3 and MIDI_TX during bring-up.

### Core Board — Pedal Power & Protection (`SCH_Core board_2-PEDAL`)

- J1 mates to the pedal block, carrying 3V3, ground, and the three wiper signals.
- Fuse F1 and bead L1 create a filtered, self-resetting 3V3_PED rail; C5/C6 provide local energy storage.
- ESD diodes D1–D4 shunt transients from the pedal harness. TP4–TP6 offer probe points for each pedal channel during calibration or troubleshooting.

### Core Board — DIN-5 MIDI Stub (`SCH_Core board_3-MIDI OUT`)

- J2 breaks out five wires to a DIN-5 jack. The schematic reserves space for a future level shifter or opto-isolation if required.
- Q1 (DTC114EK) forms the MIDI current driver with R11/R12. D6/D7 protect the twisted pair from ESD; D5 provides reverse-current protection if an external MIDI-powered adapter is attached.
- Test pads TP7–TP9 expose MIDI_TX, VBUS_5V, and ground.

### Indication Board (`SCH_Indication_1-Indication`)

- Board-to-board connector (A1251WV-S-10P) mates to the core. Power (3V3) and signal lines are looped through for the LED cathodes and buttons.
- LED bill of materials:
  - LED1 `19-217/GHC-YR1S2/3T` (amber, Power) with R1 = 1.2 kΩ.
  - LED2 `NCD0603Y1` (yellow, USB) with R2 = 1.2 kΩ.
  - LED3 `19-217/BHC-ZL1M2RY/3T` (blue, BLE) with R3 = 470 Ω.
- Buttons sit on the same board, pulling the nets low; no additional hardware debouncing is required thanks to the RC network on the core board.

## Firmware & Resources

- Firmware source lives in the GitHub repository and already supports USB/BLE MIDI, SAADC self-tests, and asymmetric filtering.
- Toolchain: nRF Connect SDK 3.1.1 with Zephyr’s `device-next` USB stack for native MIDI 2.0 support.
- BLE MIDI functionality is provided by the [zephyr-ble-midi](https://github.com/stuffmatic/zephyr-ble-midi) library (submodule `modules/lib/zephyr-ble-midi`).

## Assembly & Bring-Up Notes

1. Populate the core board: solder the ProMicro, install R1–R12, C1–C6, F1, L1, and diodes. Verify 3V3_VCC_SW and the filtered 3V3_PED rail before attaching peripherals.
2. Mount J1 and confirm the ESD diodes are oriented toward the connector. Use TP4–TP6 to verify the idle voltage on each pedal channel after wiring the DIN-6/240 module (GFP-3 connector).
3. (Optional) Fit J2 and the DIN-5 harness if you plan to prototype classic MIDI out; firmware enablement is pending.
4. Assemble the indication board (LEDs, resistors, buttons) and mate it via the designated connector.
5. Flash firmware via UF2 or SWD, then confirm the serial console reports `Pedal sampler initialized` and the LEDs reflect USB/BLE readiness.

## Current Status — 2025-09-28

- Breadboard prototype validated with USB and BLE MIDI streaming; hardware tests exercised the pedal bias network defined in the schematic.
- Perfboard layout underway using the exact BOM (PTC, ferrite, ESD clamps).
- DIN-5 MIDI hardware present on the schematic; firmware support and connector footprint will be finalized before ordering the first PCB spin.

## Roadmap & Contributions

- ✅ Breadboard prototype validated with USB/BLE MIDI
- 🔜 Perfboard layout underway using the exact BOM (PTC, ferrite, ESD clamps).
- 🔜 First PCB revision with DIN‑5 hardware support
- 🚀 Planned features: pedal presence detection, OTA updates, OLED display
- 📦 Future: documented enclosure and assembly guide

Feedback, schematic reviews, and test data are very welcome: please open an issue or PR in the firmware repository to collaborate.

Please note: I come from the “fat software” side (applications, high-level systems), not embedded or electronics, so I appreciate any feedback on the hardware and software design.

Notes:
Project built end evolves with with high LLM support.
