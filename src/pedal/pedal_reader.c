#include "pedal_sampler.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal_reader, LOG_LEVEL_INF);

K_SEM_DEFINE(pedal_reader_sem, 0, 1);

/* Sensor thread infrastructure */
#define PEDAL_READER_THREAD_STACK_SIZE 1024
#define PEDAL_READER_THREAD_PRIORITY 6

static struct k_thread pedal_reader_thread_data;
static K_THREAD_STACK_DEFINE(pedal_reader_thread_stack,
                             PEDAL_READER_THREAD_STACK_SIZE);
static void pedal_reader_thread(void *p1, void *p2, void *p3);

static struct k_timer poll_tmr;

void pedal_reader_init(void) {
  /* Create and start sensor thread */
  k_thread_create(&pedal_reader_thread_data, pedal_reader_thread_stack,
                  K_THREAD_STACK_SIZEOF(pedal_reader_thread_stack),
                  pedal_reader_thread, NULL, NULL, NULL,
                  PEDAL_READER_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&pedal_reader_thread_data, "pedal-reader");

  LOG_INF("Pedal reader thread initialized with priority %d",
          PEDAL_READER_THREAD_PRIORITY);
}

static void trigger_pedals_reading(struct k_timer *tmr) {
  ARG_UNUSED(tmr);

  /* Signal sensor thread to perform ADC readings */
  k_sem_give(&pedal_reader_sem);
}

static void reader_timer_start(void) {
  /* Initialize timer with thread trigger callback */
  k_timer_init(&poll_tmr, trigger_pedals_reading, NULL);

  uint32_t period_ms = DIV_ROUND_UP(1000, CONFIG_MIDAL_POLL_HZ);
  k_timer_start(&poll_tmr, K_MSEC(period_ms), K_MSEC(period_ms));

  LOG_INF("Pedal sensors polling started at %d Hz", CONFIG_MIDAL_POLL_HZ);
}

static void pedal_reader_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  reader_timer_start();

  while (1) {
    /* Wait for timer signal */
    k_sem_take(&pedal_reader_sem, K_FOREVER);

    /* Perform ADC readings and processing in thread context */
    read_pedals();
  }
}
