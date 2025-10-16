/**
 * @file zbus_channels.c
 * @brief Zbus channel definitions for MIDAL firmware
 */

#include "zbus_channels.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zbus_channels, LOG_LEVEL_INF);

/**
 * @brief MIDI Event Channel Definition
 *
 * This channel carries MIDI events (CC messages, notes, etc.) from the
 * pedal sampler to the MIDI router for distribution to all outputs.
 *
 * Initial value: Zero-initialized MIDI CC event
 */
ZBUS_CHAN_DEFINE(midi_event_chan,           /* Channel name */
		 midi_event_t,              /* Message type */
		 NULL,                      /* No validator */
		 NULL,                      /* No user data */
		 ZBUS_OBSERVERS_EMPTY,      /* Observers added at runtime */
		 ZBUS_MSG_INIT(             /* Initial value */
			 .type = MIDI_EV_CC,
			 .cc = {
				 .ch = 0,
				 .cc = 0,
				 .value = 0
			 },
			 .timestamp_us = 0
		 )
);
