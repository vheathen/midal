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
 * This channel carries MIDI events from the pedal sampler to the MIDI router.
 * The MIDI router subscribes to this channel and dispatches events to all
 * registered outputs (USB MIDI, BLE MIDI, DIN MIDI).
 *
 * Message Type: midi_event_t
 * Publisher: Pedal Sampler
 * Subscribers: MIDI Router
 */
ZBUS_CHAN_DECLARE(midi_event_chan);
