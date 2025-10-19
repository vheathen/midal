list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/lib/zephyr-ble-midi
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(midal)

# Allow including headers relative to the src/ tree, e.g. "midi/midi_types.h"
target_include_directories(app PRIVATE
  src
)

target_sources(app PRIVATE
  src/main.c
  src/usbd/usbd.c
)

if(CONFIG_MIDAL_ACQ_SELFTEST)

  target_sources(app PRIVATE
    src/diag/saadc_selftest.c
  )

else()

  target_sources(app PRIVATE
    src/diag/heartbeat.c
    src/diag/stats.c
    src/diag/stats_listener.c
    src/usbd/midi.c
    src/zbus_channels.c
    src/pedal/pedal.c
    src/pedal/pedal_reader.c
    src/pedal/pedal_sampler.c
    src/pedal/pedal_filter.c
    src/midi/midi_codec.c
    src/transports/transport_usb_midi.c
    src/transports/transport_ble_midi.c
    # src/transports/transport_uart_midi.c
  )

endif()
