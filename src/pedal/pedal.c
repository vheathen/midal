#include "pedal_reader.h"
#include "pedal_sampler.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal, LOG_LEVEL_INF);
#include <zephyr/sys/util.h>

static pedal_sampler_hw_t g_sampler_hw;

int pedal_reader_start(void) {
  LOG_INF("Initializing pedal reading subsystem...");

  /* Initialize hardware interface (ADC channels, filters) */
  int ret = pedal_sampler_prepare_hw(&g_sampler_hw);
  if (ret != 0) {
    LOG_ERR("Failed to initialize pedal sampler: %d", ret);
    return ret;
  }

  /* Initialize and start sensor thread */
  ret = pedal_reader_init(&g_sampler_hw);
  if (ret != 0) {
    LOG_ERR("Failed to start pedal reader thread: %d", ret);
    return ret;
  }

  LOG_INF("Pedal subsystem initialized successfully");
  return 0;
}
