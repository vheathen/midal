#pragma once

#include <zephyr/kernel.h>

struct transport_stats;

int transport_usb_midi_init(void);

void transport_usb_notify_ready(bool ready);
bool transport_usb_ready(void);

/**
 * @brief Get USB MIDI transport statistics
 *
 * @param stats Pointer to structure to fill with statistics
 */
void transport_usb_get_stats(struct transport_stats *stats);
