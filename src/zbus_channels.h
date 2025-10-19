#pragma once

/**
 * @file zbus_channels.h
 * @brief Zbus channel definitions for MIDAL firmware
 *
 * This file defines all zbus channels used for inter-module communication
 * in the MIDAL MIDI Interface firmware.
 */

#include <zephyr/zbus/zbus.h>
#include "midi/midi_types.h"

/**
 * @brief MIDI Event Channel
 *
 * This channel carries MIDI events from sources (pedal sampler, etc.) to
 * all MIDI transport outputs (USB MIDI, BLE MIDI, DIN MIDI).
 *
 * The channel carries 14-bit native format MIDI data. Each transport
 * subscriber is responsible for converting to its required format:
 * - USB MIDI2: Uses 14-bit native (scaled to 16-bit UMP)
 * - BLE/DIN MIDI1: Converts 14-bit to 7-bit (>> 7 with rounding)
 *
 * Message Type: midi_event_t (always 14-bit: cc.value = 0..16383)
 * Publishers: Pedal Sampler
 * Subscribers: USB MIDI Transport, BLE MIDI Transport, DIN MIDI Transport
 */
ZBUS_CHAN_DECLARE(midi_event_chan);
