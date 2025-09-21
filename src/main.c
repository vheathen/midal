// #include "midi/midi_router.h"
// #include "pedal/pedal_sampler.h"
// #include "transports/transport_usb_midi.h"
#include "usbd/midi.h"
#include "usbd/usbd.h"

#include <stdbool.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {

  int ret;

  ret = usbd_enable_device();
  if (ret != 0) {
    LOG_ERR("Failed to enable USB");
    return 0;
  }

  LOG_INF("USB up\r\n");

  ret = usbd_midi_init();
  if (ret != 0) {
    LOG_ERR("Failed to init USB MIDI");
    return 0;
  }
  
  return 0;
}
