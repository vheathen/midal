// midi_codec.c
#include "midi_codec.h"

#include <zephyr/kernel.h>

/* Encodes an event into MIDI 1.0 bytes (7-bit or 14-bit CC). Returns length. */
size_t midi_codec_encode_cc(uint8_t *out, size_t cap, const midi_event_t *ev,
                            bool use14bit) {
  if (cap < 3) {
    return 0;
  }

  uint8_t status = 0xB0 | (ev->cc.ch & 0x0F);

  out[0] = status;
  out[1] = ev->cc.cc;
  out[2] = (uint8_t)(ev->cc.value & 0x7F);

  size_t n = 3;

  if (use14bit) {
    if (cap < 6) {
      return n;
    }
    out[3] = status;
    out[4] = (uint8_t)(ev->cc.cc + 32);
    out[5] = (uint8_t)((ev->cc.value >> 7) & 0x7F);
    n = 6;
  }
  return n;
}