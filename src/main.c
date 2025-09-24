
#include "diag/saadc_selftest.h"
#include "pedal/pedal_reader.h"
#include "usbd/midi.h"
#include "usbd/usbd.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {

  int ret = 0;

  ret = usbd_enable_device();
  if (ret != 0) {
    LOG_ERR("Failed to enable USB: %d", ret);
    return 0;
  }

  LOG_INF("USB up\r\n");

  ret = usbd_midi_init();

  if (ret != 0) {
    LOG_ERR("Failed to init USB MIDI: %d", ret);
    return 0;
  }

#if IS_ENABLED(CONFIG_MIDAL_ACQ_SELFTEST)
  saadc_selftest_run();
#endif

#if !IS_ENABLED(CONFIG_MIDAL_ACQ_SELFTEST)
  ret = pedal_reader_init();
  if (ret != 0) {
    LOG_ERR("Failed to initialize pedal subsystem: %d", ret);
    return 0;
  }
#endif

  return 0;
}
