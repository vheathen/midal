#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "transports/transport_ble_midi.h"
#include "transports/transport_usb_midi.h"

static void hb_timer_cb(struct k_timer *timer) {
  ARG_UNUSED(timer);
  /* Use printk to bypass the logging backend entirely */
  uint32_t t = k_uptime_get_32();

  bool usb_ready = transport_usb_ready();
  bool ble_ready = IS_ENABLED(CONFIG_BLE_MIDI) ? transport_ble_midi_ready() : false;

  printk("[hb] t=%ums usb_ready=%d ble_ready=%d\n",
         t, usb_ready ? 1 : 0, ble_ready ? 1 : 0);
}

K_TIMER_DEFINE(hb_timer, hb_timer_cb, NULL);

void heartbeat_start(void) {
  /* Fire every 1000 ms; first tick after 1000 ms */
  k_timer_start(&hb_timer, K_MSEC(1000), K_MSEC(1000));
}
