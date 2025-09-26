#include "midi/midi_router.h"
#include "midi/midi_types.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(transport_usb_midi, LOG_LEVEL_INF);

struct usb_midi_ctx {
  const struct device *dev;
  atomic_t ready;
  atomic_t fail_streak;
};

static struct usb_midi_ctx s_usb_ctx = {
    .dev = DEVICE_DT_GET(DT_NODELABEL(usb_midi)),
};

static int usb_midi_tx(void *ctx_ptr, const midi_event_t *ev);

int transport_usb_midi_init(midi_tx_fn *out_tx, void **out_ctx) {
  if (!device_is_ready(s_usb_ctx.dev)) {
    LOG_ERR("USBD MIDI device not ready");
    return -ENODEV;
  }

  atomic_clear(&s_usb_ctx.fail_streak);

  if (out_tx) {
    *out_tx = usb_midi_tx;
  }
  if (out_ctx) {
    *out_ctx = &s_usb_ctx;
  }
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

static inline uint16_t scale7_to16(uint8_t v7) {
  /* Map 0..127 -> 0..65535 with rounding */
  return (uint16_t)(((uint32_t)v7 * 65535u + 63u) / 127u);
}

static inline uint16_t scale14_to16(uint16_t v14) {
  /* Map 0..16383 -> 0..65535 with rounding */
  return (uint16_t)(((uint32_t)v14 * 65535u + 8191u) / 16383u);
}

static inline void send_cc16_midi2(struct usb_midi_ctx *ctx, uint8_t ch,
                                   uint8_t cc, uint16_t val16) {
#if defined(UMP_MIDI2_CHANNEL_VOICE) && defined(UMP_MIDI2_CONTROL_CHANGE)
  /* MIDI 2.0 Channel Voice Control Change over UMP.
   * For CC in MIDI 2.0, the value field is 32-bit; we place 16-bit in MSBs. */
  uint32_t data32 = ((uint32_t)val16) << 16;     /* 0x0000VVVV */
  struct midi_ump m = UMP_MIDI2_CHANNEL_VOICE(0, /* group 0 */
                                              UMP_MIDI2_CONTROL_CHANGE,
                                              (ch & 0x0F), (cc & 0x7F), data32);
  (void)safe_send(ctx, m);
#else
  /* If MIDI2 macro not available in this SDK, fall back to coarse 7-bit */
  send_cc7(ctx, ch, cc, (uint8_t)(val16 >> 9));
#endif
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

#if IS_ENABLED(CONFIG_MIDAL_USB_MIDI2_NATIVE)
  /* Send as native MIDI 2.0 CC (16-bit). */
  uint16_t v16 = (v <= 127) ? scale7_to16((uint8_t)v) : scale14_to16(v);
  LOG_DBG("USB MIDI2 CC ch=%u cc=%u val16=%u", ch, cc, v16);
  send_cc16_midi2(ctx, ch, cc, v16);
  return 0;
#else
  /* MIDI 1.0-compatible path */
  if (IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC) && cc < 32) {
    /* High‑resolution mode: always send MSB+LSB pair for CC 0..31
     * to keep host LSB in sync, even when value <= 127. */
    uint16_t v14 = (v > 16383U) ? 16383U : v;
    uint8_t msb = (uint8_t)((v14 >> 7) & 0x7F);
    uint8_t lsb = (uint8_t)(v14 & 0x7F);
    LOG_DBG("USB CC14 ch=%u cc=%u val=%u", ch, cc, v14);
    send_cc7(ctx, ch, cc, msb);
    send_cc7(ctx, ch, (uint8_t)(cc + 32), lsb);
  } else {
    /* Fallback: plain 7‑bit CC (also used when cc >= 32) */
    uint8_t v7 = (v > 127U) ? 127U : (uint8_t)v;
    LOG_DBG("USB CC7 ch=%u cc=%u val=%u", ch, cc, v7);
    send_cc7(ctx, ch, cc, v7);
  }
  return 0;
#endif
}
