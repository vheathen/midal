#include "diag/stats.h"
#include "midi/midi_types.h"
#include "zbus_channels.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_midi2.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(transport_usb_midi, LOG_LEVEL_INF);

/* Zbus message subscriber for MIDI events */
ZBUS_MSG_SUBSCRIBER_DEFINE(usb_midi_sub);

struct usb_midi_ctx {
  const struct device *dev;
  atomic_t ready;
  atomic_t fail_streak;
  atomic_t sent;
  atomic_t dropped;
};

static struct usb_midi_ctx s_usb_ctx = {
    .dev = DEVICE_DT_GET(DT_NODELABEL(usb_midi)),
};

#define USB_MIDI_THREAD_PRIORITY 5
#define USB_MIDI_THREAD_STACK_SIZE 1024
static struct k_thread usb_midi_thread_data;
K_THREAD_STACK_DEFINE(usb_midi_stack, USB_MIDI_THREAD_STACK_SIZE);
static void usb_midi_thread(void *, void *, void *);

static int usb_midi_tx(void *ctx_ptr, const midi_event_t *ev);

int transport_usb_midi_init(void) {
  if (!device_is_ready(s_usb_ctx.dev)) {
    LOG_ERR("USBD MIDI device not ready");
    return -ENODEV;
  }

  atomic_clear(&s_usb_ctx.fail_streak);
  atomic_clear(&s_usb_ctx.ready);
  atomic_clear(&s_usb_ctx.sent);
  atomic_clear(&s_usb_ctx.dropped);

  /* Subscribe to MIDI event channel */
  int ret = zbus_chan_add_obs(&midi_event_chan, &usb_midi_sub, K_MSEC(100));
  if (ret != 0) {
    LOG_ERR("Failed to subscribe USB MIDI to midi_event_chan: %d", ret);
    return ret;
  }

  /* Start transport thread */
  k_thread_create(&usb_midi_thread_data, usb_midi_stack,
                  K_THREAD_STACK_SIZEOF(usb_midi_stack), usb_midi_thread, NULL,
                  NULL, NULL, USB_MIDI_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&usb_midi_thread_data, "usb-midi");

  LOG_INF("USB MIDI transport initialized and subscribed to zbus");
  return 0;
}

static inline int safe_send(struct usb_midi_ctx *ctx, struct midi_ump m);

static inline void send_cc7(struct usb_midi_ctx *ctx, uint8_t ch, uint8_t cc,
                            uint8_t val7) {
  /* MIDI 1.0 Channel Voice Control Change over UMP */
  struct midi_ump m = UMP_MIDI1_CHANNEL_VOICE(
      0, /* group 0 */
      UMP_MIDI_CONTROL_CHANGE, (ch & 0x0F), (cc & 0x7F), (val7 & 0x7F));
  (void)safe_send(ctx, m);
}

static inline uint16_t scale14_to16(uint16_t v14) {
  /* Map 0..16383 -> 0..65535 with rounding */
  return (uint16_t)(((uint32_t)v14 * 65535u + 8191u) / 16383u);
}

static struct midi_ump midi2_cc_packet(uint8_t group, uint8_t channel,
                                       uint8_t controller, uint16_t value16) {
  struct midi_ump m = {0};

  uint32_t word0 = ((uint32_t)UMP_MT_MIDI2_CHANNEL_VOICE << 28) |
                   (((uint32_t)group & 0x0F) << 24) |
                   (((uint32_t)UMP_MIDI_CONTROL_CHANGE & 0x0F) << 20) |
                   (((uint32_t)channel & 0x0F) << 16) |
                   (((uint32_t)controller & 0x7F) << 8);

  uint32_t word1 = ((uint32_t)value16) << 16; /* Data in the MSBs */

  m.data[0] = word0;
  m.data[1] = word1;
  return m;
}

bool transport_usb_ready(void) { return atomic_get(&s_usb_ctx.ready) != 0; }

void transport_usb_notify_ready(bool ready) {
  atomic_set(&s_usb_ctx.ready, ready ? 1 : 0);
  LOG_INF("USB-MIDI2.0 is %s", ready ? "enabled" : "disabled");
}

static inline int safe_send(struct usb_midi_ctx *ctx, struct midi_ump m) {
  for (int attempt = 0; attempt < 3; attempt++) {
    int r = usbd_midi_send(ctx->dev, m);
    if (r == 0) {
      atomic_clear(&ctx->fail_streak);
      return 0;
    }

    if (r == -EAGAIN || r == -ENOSPC) {
#if defined(CONFIG_KERNEL_DEBUG)
      if (attempt == 0) {
        LOG_DBG("USB MIDI buffer busy, retrying");
      }
#endif
      k_busy_wait(100);
      continue;
    }

    atomic_inc(&ctx->fail_streak);
    LOG_WRN("USB MIDI send failed: %d", r);
    return r;
  }

  atomic_inc(&ctx->fail_streak);
  LOG_WRN("USB MIDI send gave up after retries");
  return -EAGAIN;
}

static int usb_midi_tx(void *ctx_ptr, const midi_event_t *ev) {
  if (ctx_ptr == NULL || ev == NULL) {
    return -EINVAL;
  }

  struct usb_midi_ctx *ctx = ctx_ptr;

  if (!transport_usb_ready()) {
    return 0; /* Not enumerated yet */
  }

  if (ev->type != MIDI_EV_CC) {
    return 0; /* only CC for now */
  }

  const uint8_t ch = ev->cc.ch & 0x0F;
  const uint8_t cc = ev->cc.cc & 0x7F;
  const uint16_t v = ev->cc.value; /* 0..127 or 0..16383 */

  uint8_t v7_scaled = 0;
  if (IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC)) {
    uint16_t v_clamped = (v > 16383U) ? 16383U : v;
    v7_scaled = (uint8_t)((v_clamped + 0x40U) >> 7); /* rounding */
  } else {
    v7_scaled = (uint8_t)((v > 127U) ? 127U : v);
  }

  LOG_DBG("USB CC7 ch=%u cc=%u val=%u (scaled)", ch, cc, v7_scaled);
  send_cc7(ctx, ch, cc, v7_scaled);

#if IS_ENABLED(CONFIG_MIDAL_USB_MIDI2_NATIVE)
  uint16_t v16 = (v > 16383U) ? 65535U : scale14_to16(v);
  LOG_DBG("USB MIDI2 CC ch=%u cc=%u val16=%u", ch, cc, v16);
  struct midi_ump midi2 = midi2_cc_packet(0, ch, cc, v16);
  (void)safe_send(ctx, midi2);
#endif

  return 0;
}

static void usb_midi_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  const struct zbus_channel *chan;
  midi_event_t ev;

  LOG_INF("USB MIDI transport thread started, waiting for events...");

  while (true) {
    /* Wait for MIDI event from zbus channel */
    int ret = zbus_sub_wait_msg(&usb_midi_sub, &chan, &ev, K_FOREVER);
    if (ret != 0) {
      LOG_ERR("USB MIDI zbus_sub_wait_msg failed: %d", ret);
      continue;
    }

    /* Verify correct channel */
    if (chan != &midi_event_chan) {
      LOG_WRN("USB MIDI received event from unexpected channel: %p", chan);
      continue;
    }

    /* Process the event */
    ret = usb_midi_tx(&s_usb_ctx, &ev);
    if (ret == 0) {
      atomic_inc(&s_usb_ctx.sent);
    } else {
      atomic_inc(&s_usb_ctx.dropped);
    }
  }
}

void transport_usb_get_stats(struct transport_stats *stats) {
  if (stats == NULL) {
    return;
  }

  stats->sent = (uint32_t)atomic_get(&s_usb_ctx.sent);
  stats->dropped = (uint32_t)atomic_get(&s_usb_ctx.dropped);
}
