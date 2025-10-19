/**
 * @file stats_listener.c
 * @brief MIDI event statistics listener implementation
 */

#include "stats_listener.h"
#include "zbus_channels.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(stats_listener, LOG_LEVEL_INF);

/* Total MIDI events counter */
static atomic_t total_midi_events = ATOMIC_INIT(0);

/**
 * @brief Stats listener callback - counts MIDI events
 *
 * This is a synchronous listener that simply increments a counter.
 * It's very fast (just an atomic increment) and doesn't block.
 */
static void stats_listener_callback(const struct zbus_channel *chan) {
  ARG_UNUSED(chan);

  /* Just count - no processing */
  atomic_inc(&total_midi_events);
}

/* Define the zbus listener */
ZBUS_LISTENER_DEFINE(stats_listener, stats_listener_callback);

int stats_listener_init(void) {
  /* Add stats listener to MIDI event channel */
  int ret = zbus_chan_add_obs(&midi_event_chan, &stats_listener, K_MSEC(100));
  if (ret != 0) {
    LOG_ERR("Failed to add stats listener to midi_event_chan: %d", ret);
    return ret;
  }

  atomic_clear(&total_midi_events);
  LOG_INF("Stats listener initialized and subscribed to zbus");
  return 0;
}

uint32_t stats_get_total_events(void) {
  return (uint32_t)atomic_get(&total_midi_events);
}
