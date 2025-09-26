#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "midi/midi_router.h"
#include "transports/transport_usb_midi.h"

static void hb_timer_cb(struct k_timer *timer) {
  ARG_UNUSED(timer);
  /* Use printk to bypass the logging backend entirely */
  uint32_t t = k_uptime_get_32();
  struct midi_router_stats stats = {0};
  midi_router_get_stats(&stats);

  bool usb_ready = transport_usb_ready();

  printk("[hb] t=%ums usb_ready=%d q_hw=%u dropped=%lu dispatched=%lu\n", t,
         usb_ready ? 1 : 0, stats.queue_high_water,
         (unsigned long)stats.total_dropped,
         (unsigned long)stats.total_dispatched);
}

K_TIMER_DEFINE(hb_timer, hb_timer_cb, NULL);

void heartbeat_start(void) {
  /* Fire every 1000 ms; first tick after 1000 ms */
  k_timer_start(&hb_timer, K_MSEC(1000), K_MSEC(1000));
}
