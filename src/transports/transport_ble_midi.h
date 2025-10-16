#pragma once

#include <zephyr/kernel.h>

struct transport_stats;

int transport_ble_midi_init(void);
bool transport_ble_midi_ready(void);

/**
 * @brief Get BLE MIDI transport statistics
 *
 * @param stats Pointer to structure to fill with statistics
 */
void transport_ble_get_stats(struct transport_stats *stats);
