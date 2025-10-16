#pragma once

#include <zephyr/kernel.h>

/**
 * @file stats.h
 * @brief MIDAL statistics types and API
 */

/**
 * @brief Transport statistics
 */
struct transport_stats {
  uint32_t sent;    /* Successfully sent messages */
  uint32_t dropped; /* Dropped messages (errors, queue full, etc.) */
};

/**
 * @brief Global MIDAL statistics
 */
struct midal_stats {
  uint32_t total_events;      /* Total MIDI events published to zbus */
  struct transport_stats usb; /* USB MIDI transport stats */
  struct transport_stats ble; /* BLE MIDI transport stats */
};

/**
 * @brief Get global MIDAL statistics
 *
 * Collects statistics from all subsystems (stats listener, transports).
 *
 * @param stats Pointer to structure to fill with statistics
 */
void midal_get_stats(struct midal_stats *stats);

/**
 * @brief Initialize statistics subsystem
 *
 * Initializes the stats listener and subscribes to MIDI event channel.
 *
 * @return 0 on success, negative error code on failure
 */
int midal_stats_init(void);
