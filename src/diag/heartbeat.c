#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "diag/stats.h"
#include "transports/transport_ble_midi.h"
#include "transports/transport_usb_midi.h"

static void hb_timer_cb(struct k_timer *timer) {
  ARG_UNUSED(timer);
  /* Use printk to bypass the logging backend entirely */
  uint32_t t = k_uptime_get_32();

  bool usb_ready = transport_usb_ready();
  bool ble_ready =
      IS_ENABLED(CONFIG_BLE_MIDI) ? transport_ble_midi_ready() : false;

  struct midal_stats stats;
  midal_get_stats(&stats);

  printk(
      "[hb] t=%ums usb=%d ble=%d | events=%lu usb_tx=%lu/%lu ble_tx=%lu/%lu\n",
      t, usb_ready ? 1 : 0, ble_ready ? 1 : 0,
      (unsigned long)stats.total_events, (unsigned long)stats.usb.sent,
      (unsigned long)stats.usb.dropped, (unsigned long)stats.ble.sent,
      (unsigned long)stats.ble.dropped);
}

K_TIMER_DEFINE(hb_timer, hb_timer_cb, NULL);

void heartbeat_start(void) {
  /* Fire every 1000 ms; first tick after 1000 ms */
  k_timer_start(&hb_timer, K_MSEC(1000), K_MSEC(1000));
}
