#include "pedal_sampler.h"
#include "pedal_reader.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal, LOG_LEVEL_INF);
#include <zephyr/sys/util.h>

int pedal_reader_start(void) {
  LOG_INF("Initializing pedal reading subsystem...");

  /* Initialize hardware interface (ADC channels, filters) */
  int ret = pedal_sampler_init_sensors();
  if (ret != 0) {
    LOG_ERR("Failed to initialize pedal sampler: %d", ret);
    return ret;
  }

  /* Initialize and start sensor thread */
  pedal_reader_init();

  LOG_INF("Pedal subsystem initialized successfully");
  return 0;
}