#pragma once

#include <zephyr/kernel.h>

typedef enum {
  MIDI_EV_CC,       // control change
  MIDI_EV_NOTE,     // if click/triggers would become an option
  MIDI_EV_PITCHBEND // for expression pedals (optional)
} midi_ev_type_t;

typedef struct {
  uint8_t ch;     // 0..15
  uint8_t cc;     // CC# (64 sustain, 66 sostenuto, 67 soft, or custom)
  uint16_t value; // 0..127 (or 0..16383 if 14-bit mode is enabled)
} midi_cc_t;

typedef struct {
  midi_ev_type_t type;
  union {
    midi_cc_t cc;
    // note/pb ... if needed
  };
  uint32_t timestamp_us; // for BLE MIDI timestamps
} midi_event_t;