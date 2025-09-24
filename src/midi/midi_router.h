#pragma once

#include "midi_types.h"

#include <zephyr/kernel.h>

typedef int (*midi_tx_fn)(const midi_event_t *ev);

void midi_router_init(void);
void midi_router_start(void);

void midi_router_register_tx(midi_tx_fn tx);
bool midi_router_submit(const midi_event_t *ev); // send to the queue