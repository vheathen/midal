#include "midi/midi_router.h"
#include "midi/midi_types.h"
#include <zephyr/sys/util.h>

#include <zephyr/device.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(transport_usb_midi, LOG_LEVEL_INF);

/* MIDI 2.0 (device_next) function instance from DTS */
static const struct device *const midi = DEVICE_DT_GET(DT_NODELABEL(usb_midi));

static volatile bool s_usbd_ready;

static void on_midi_ready(const struct device *dev, const bool ready) {
  ARG_UNUSED(dev);
  s_usbd_ready = ready;
  LOG_INF("USB-MIDI2.0 is %s", ready ? "enabled" : "disabled");
}

static const struct usbd_midi_ops s_ops = {
    .rx_packet_cb = NULL, /* RX not used yet */
    .ready_cb = on_midi_ready,
};

static int usb_midi_tx(const midi_event_t *ev);

int transport_usb_midi_init(midi_tx_fn *out_tx) {
  if (!device_is_ready(midi)) {
    LOG_ERR("USBD MIDI device not ready");
    return -ENODEV;
  }

  usbd_midi_set_ops(midi, &s_ops);

  if (out_tx) {
    *out_tx = usb_midi_tx;
  }
  return 0;
}

static inline void send_cc7(uint8_t ch, uint8_t cc, uint8_t val7) {
  /* MIDI 1.0 Channel Voice Control Change over UMP */
  struct midi_ump m = UMP_MIDI1_CHANNEL_VOICE(
      0, /* group 0 */
      UMP_MIDI_CONTROL_CHANGE, (ch & 0x0F), (cc & 0x7F), (val7 & 0x7F));
  (void)usbd_midi_send(midi, m);
}

static inline uint16_t scale7_to16(uint8_t v7) {
  /* Map 0..127 -> 0..65535 with rounding */
  return (uint16_t)(((uint32_t)v7 * 65535u + 63u) / 127u);
}

static inline uint16_t scale14_to16(uint16_t v14) {
  /* Map 0..16383 -> 0..65535 with rounding */
  return (uint16_t)(((uint32_t)v14 * 65535u + 8191u) / 16383u);
}

static inline void send_cc16_midi2(uint8_t ch, uint8_t cc, uint16_t val16) {
#if defined(UMP_MIDI2_CHANNEL_VOICE) && defined(UMP_MIDI2_CONTROL_CHANGE)
  /* MIDI 2.0 Channel Voice Control Change over UMP.
   * For CC in MIDI 2.0, the value field is 32-bit; we place 16-bit in MSBs. */
  uint32_t data32 = ((uint32_t)val16) << 16;     /* 0x0000VVVV */
  struct midi_ump m = UMP_MIDI2_CHANNEL_VOICE(0, /* group 0 */
                                              UMP_MIDI2_CONTROL_CHANGE,
                                              (ch & 0x0F), (cc & 0x7F), data32);
  (void)usbd_midi_send(midi, m);
#else
  /* If MIDI2 macro not available in this SDK, fall back to coarse 7-bit */
  send_cc7(ch, cc, (uint8_t)(val16 >> 9));
#endif
}

static int usb_midi_tx(const midi_event_t *ev) {
  if (!s_usbd_ready) {
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
  send_cc16_midi2(ch, cc, v16);
  return 0;
#else
  /* MIDI 1.0-compatible path */
  if (IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC) && cc < 32) {
    /* High‑resolution mode: always send MSB+LSB pair for CC 0..31
     * to keep host LSB in sync, even when value <= 127. */
    uint16_t v14 = (v > 16383U) ? 16383U : v;
    uint8_t msb = (uint8_t)((v14 >> 7) & 0x7F);
    uint8_t lsb = (uint8_t)(v14 & 0x7F);
    send_cc7(ch, cc, msb);
    send_cc7(ch, (uint8_t)(cc + 32), lsb);
  } else {
    /* Fallback: plain 7‑bit CC (also used when cc >= 32) */
    uint8_t v7 = (v > 127U) ? 127U : (uint8_t)v;
    send_cc7(ch, cc, v7);
  }
  return 0;
#endif
}