#include "diag/heartbeat.h"
#include "midi/midi_router.h"
#include "pedal/pedal.h"
#include "transports/transport_usb_midi.h"
#include "usbd/midi.h"
#include "usbd/usbd.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if IS_ENABLED(CONFIG_MIDAL_ACQ_SELFTEST)
#include "diag/saadc_selftest.h"
#endif

int main(void) {

  int ret = 0;

  ret = usbd_enable_device();
  if (ret != 0) {
    LOG_ERR("Failed to enable USB: %d", ret);
    return 0;
  }

  LOG_INF("USB up\r\n");

#if IS_ENABLED(CONFIG_MIDAL_ACQ_SELFTEST)
  saadc_selftest_run();
#else

  ret = usbd_midi_init();

  if (ret != 0) {
    LOG_ERR("Failed to init USB MIDI: %d", ret);
    return 0;
  }

  midi_router_init();

  /* Initialize USB MIDI transport (device_next) and register with router */
  midi_tx_fn usb_tx = NULL;
  void *usb_ctx = NULL;

  ret = transport_usb_midi_init(&usb_tx, &usb_ctx);
  if (ret != 0) {
    LOG_ERR("USB MIDI transport init failed: %d", ret);
    return -ENODEV;
  }

  if (usb_tx) {
    ret = midi_router_register_tx(usb_tx, usb_ctx, "usb");
    if (ret != 0) {
      LOG_ERR("Failed to register USB MIDI route: %d", ret);
      return ret;
    }
  } else {
    LOG_ERR("USB MIDI transport returned NULL tx function");
    return -ENODEV;
  }

  midi_router_start();

  heartbeat_start();

  ret = pedal_reader_start();
  if (ret != 0) {
    LOG_ERR("Failed to initialize pedal subsystem: %d", ret);
    return ret;
  }

#endif

  return 0;
}
