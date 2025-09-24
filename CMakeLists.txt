cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(midal)

# Allow including headers relative to the src/ tree, e.g. "midi/midi_types.h"
target_include_directories(app PRIVATE
  src
)

target_sources(app PRIVATE
  src/diag/saadc_selftest.c
  src/main.c
  src/usbd/usbd.c
  src/usbd/midi.c
  src/pedal/pedal.c
  src/pedal/pedal_reader.c
  src/pedal/pedal_sampler.c
  src/pedal/pedal_filter.c
  src/midi/midi_codec.c
  src/midi/midi_router.c
  # src/transports/transport_usb_midi.c
  # src/transports/transport_ble_midi.c
  # src/transports/transport_uart_midi.c
)
