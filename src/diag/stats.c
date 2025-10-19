/**
 * @file stats.c
 * @brief MIDAL statistics aggregator implementation
 */

#include "stats.h"
#include "stats_listener.h"
#include "transports/transport_ble_midi.h"
#include "transports/transport_usb_midi.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midal_stats, LOG_LEVEL_INF);

int midal_stats_init(void) {
  int ret = stats_listener_init();
  if (ret != 0) {
    LOG_ERR("Failed to initialize stats listener: %d", ret);
    return ret;
  }

  LOG_INF("MIDAL stats system initialized");
  return 0;
}

void midal_get_stats(struct midal_stats *stats) {
  if (stats == NULL) {
    return;
  }

  /* Get total events from stats listener */
  stats->total_events = stats_get_total_events();

  /* Get USB transport stats */
  transport_usb_get_stats(&stats->usb);

  /* Get BLE transport stats */
#if IS_ENABLED(CONFIG_BLE_MIDI)
  transport_ble_get_stats(&stats->ble);
#else
  stats->ble.sent = 0;
  stats->ble.dropped = 0;
#endif
}
