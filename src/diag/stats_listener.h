#pragma once

#include <zephyr/kernel.h>

/**
 * @file stats_listener.h
 * @brief MIDI event statistics listener
 *
 * This module implements a zbus listener that counts all MIDI events
 * published to the midi_event_chan. It provides a global view of
 * system activity.
 */

/**
 * @brief Initialize the stats listener
 *
 * Subscribes the stats listener to the MIDI event channel.
 *
 * @return 0 on success, negative error code on failure
 */
int stats_listener_init(void);

/**
 * @brief Get total number of MIDI events published
 *
 * @return Total count of MIDI events since initialization
 */
uint32_t stats_get_total_events(void);
