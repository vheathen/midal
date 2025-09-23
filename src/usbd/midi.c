#include <zephyr/device.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_midi, LOG_LEVEL_INF);

static const struct device *const usb_midi_dev =
    DEVICE_DT_GET(DT_NODELABEL(usb_midi));

static void on_midi_device_ready(const struct device *dev, const bool ready) {
  ARG_UNUSED(dev);
  ARG_UNUSED(ready);

  LOG_INF("USB-MIDI2.0 is %s\r\n", ready ? "enabled" : "disabled");

  /* Light up the LED (if any) when USB-MIDI2.0 is enabled */
  // if (led.port) {
  // 	gpio_pin_set_dt(&led, ready);
  // }
}

static const struct usbd_midi_ops ops = {
    // .rx_packet_cb = on_midi_packet,
    .ready_cb = on_midi_device_ready,
};

int usbd_midi_init(void) {
  if (!device_is_ready(usb_midi_dev)) {
    LOG_ERR("USB MIDI device not ready");
    return -ENODEV;
  }

  usbd_midi_set_ops(usb_midi_dev, &ops);

  return 0;
}