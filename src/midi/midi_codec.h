// midi/midi_codec.h
#pragma once

#include "midi_types.h"

#include <zephyr/kernel.h>

/* Encodes an event into MIDI 1.0 bytes (7-bit or 14-bit CC). Returns length. */
size_t midi_codec_encode_cc(uint8_t *out, size_t cap, const midi_event_t *ev,
                            bool use14bit);
