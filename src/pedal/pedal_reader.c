#include "pedal_sampler.h"
#include "pedal_sampler_thread.h"
#include "pedal_timer.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal_reader, LOG_LEVEL_INF);

static struct k_timer poll_tmr;

static void trigger_sensors_reading(struct k_timer *tmr) {
  ARG_UNUSED(tmr);

  LOG_INF("Sensor polling triggered");

  /* Signal sensor thread to perform ADC readings */
  k_sem_give(&sensor_sem);
}

static void pedal_sampler_start(void) {
  /* Initialize timer with thread trigger callback */
  k_timer_init(&poll_tmr, trigger_sensors_reading, NULL);

  uint32_t period_ms = 1000 / CONFIG_MIDAL_POLL_HZ;
  period_ms = period_ms == 0 ? 1 : period_ms;
  k_timer_start(&poll_tmr, K_MSEC(period_ms), K_MSEC(period_ms));

  LOG_INF("Sensor polling started at %d Hz", CONFIG_MIDAL_POLL_HZ);
}

int pedal_reader_init(void) {
  LOG_INF("Initializing pedal reading subsystem...");

  /* Initialize hardware interface (ADC channels, filters) */
  pedal_sampler_init_sensors();

  /* Initialize and start sensor thread */
  pedal_sampler_thread_init();

  /* Start sensor polling */
  pedal_sampler_start();

  LOG_INF("Pedal subsystem initialized successfully");
  return 0;
}