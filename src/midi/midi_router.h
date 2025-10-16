#pragma once

#include "midi_types.h"

#include <zephyr/kernel.h>

#define MIDI_ROUTER_MAX_ROUTES 4

typedef int (*midi_tx_fn)(void *ctx, const midi_event_t *ev);

void midi_router_init(void);
void midi_router_start(void);

int midi_router_register_tx(midi_tx_fn tx, void *ctx, const char *name);
int midi_router_get_dropped(void);

struct midi_router_stats {
  uint32_t total_enqueued;
  uint32_t total_dispatched;
  uint32_t total_dropped;
  uint16_t queue_high_water;
  uint8_t route_count;
  struct {
    const char *name;
    uint32_t sent;
    uint32_t dropped;
    uint16_t queue_high_water;
  } routes[MIDI_ROUTER_MAX_ROUTES];
};

void midi_router_get_stats(struct midi_router_stats *stats);
