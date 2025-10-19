#include "diag/heartbeat.h"
#include "diag/stats.h"
#include "pedal/pedal.h"
#include "transports/transport_ble_midi.h"
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

// For testing
#include <zephyr/drivers/gpio.h>
/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

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

  /* Initialize stats system - listener + aggregator */
  ret = midal_stats_init();
  if (ret != 0) {
    LOG_ERR("Stats init failed: %d", ret);
    return -ENODEV;
  }

  /* Initialize USB MIDI transport - subscribes to zbus directly */
  ret = transport_usb_midi_init();
  if (ret != 0) {
    LOG_ERR("USB MIDI transport init failed: %d", ret);
    return -ENODEV;
  }

#if IS_ENABLED(CONFIG_BLE_MIDI)
  /* Initialize BLE MIDI transport - subscribes to zbus directly */
  ret = transport_ble_midi_init();
  if (ret != 0) {
    LOG_WRN("BLE MIDI transport init failed: %d", ret);
  }
#endif

  heartbeat_start();

  ret = pedal_reader_start();
  if (ret != 0) {
    LOG_ERR("Failed to initialize pedal subsystem: %d", ret);
    return ret;
  }

#endif

  if (!gpio_is_ready_dt(&led)) {
    return 0;
  }

  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    return 0;
  }

  while (1) {
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      return 0;
    }

    k_sleep(K_MSEC(500));
  }

  return 0;
}
