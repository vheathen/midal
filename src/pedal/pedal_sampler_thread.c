#include "pedal_sampler.h"
#include "pedal_timer.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal_sampler_thread, LOG_LEVEL_INF);

K_SEM_DEFINE(sensor_sem, 0, 1);

/* Sensor thread infrastructure */
#define SENSOR_THREAD_STACK_SIZE 1024
#define SENSOR_THREAD_PRIORITY 6

static struct k_thread sensor_thread_data;
static K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);

static void sensor_thread_entry(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  LOG_INF("Sensor thread started");

  while (1) {
    /* Wait for timer signal */
    k_sem_take(&sensor_sem, K_FOREVER);

    LOG_INF("Sensor thread step");

    /* Perform ADC readings and processing in thread context */
    read_pedals();
  }
}

void pedal_sampler_thread_init(void) {
  /* Create and start sensor thread */
  k_thread_create(&sensor_thread_data, sensor_thread_stack,
                  K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                  sensor_thread_entry, NULL, NULL, NULL, SENSOR_THREAD_PRIORITY,
                  0, K_NO_WAIT);
  k_thread_name_set(&sensor_thread_data, "sensor");

  LOG_INF("Sensor thread initialized with priority %d", SENSOR_THREAD_PRIORITY);
}