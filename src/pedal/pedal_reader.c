#include "pedal_sampler.h"

#include <nrfx_saadc.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(pedal_reader, LOG_LEVEL_INF);

K_SEM_DEFINE(pedal_reader_sem, 0, 1);

#define PEDAL_READER_THREAD_STACK_SIZE 1024
#define PEDAL_READER_THREAD_PRIORITY 6
#define ADC_TIMEOUT_MS 5

typedef struct {
  int16_t adc_raw[MIDAL_NUM_PEDALS];
  pedal_raw_sample_t sample;
} pedal_sample_slot_t;

static struct k_thread pedal_reader_thread_data;
static K_THREAD_STACK_DEFINE(pedal_reader_thread_stack,
                             PEDAL_READER_THREAD_STACK_SIZE);
static void pedal_reader_thread(void *p1, void *p2, void *p3);

static struct k_timer poll_tmr;
static struct k_poll_signal adc_signal;
static struct k_poll_event adc_event;

static pedal_sampler_hw_t sampler_hw;
static pedal_sample_slot_t sample_slots[2];
static uint8_t slot_index;

static void trigger_pedals_reading(struct k_timer *tmr) {
  ARG_UNUSED(tmr);
  k_sem_give(&pedal_reader_sem);
}

static void reader_timer_start(void) {
  k_timer_init(&poll_tmr, trigger_pedals_reading, NULL);

  uint32_t period_us = DIV_ROUND_UP(1000000U, CONFIG_MIDAL_POLL_HZ);
  k_timer_start(&poll_tmr, K_USEC(period_us), K_USEC(period_us));

  LOG_INF("Pedal sensors polling started at %d Hz", CONFIG_MIDAL_POLL_HZ);
}

static void pedal_reader_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  reader_timer_start();

  uint32_t now = 0;
  uint32_t last_pong_time = 0;

  while (true) {
    k_sem_take(&pedal_reader_sem, K_FOREVER);

    now = k_uptime_get_32();
    if (now - last_pong_time >= 1000U) {
      last_pong_time = now;
      LOG_DBG("Pedal reader thread heartbeat");
    }

    pedal_sample_slot_t *slot = &sample_slots[slot_index];
    slot_index ^= 1U;

    sampler_hw.sequence.buffer = slot->adc_raw;
    sampler_hw.sequence.buffer_size = sizeof(slot->adc_raw);

    k_poll_signal_reset(&adc_signal);
    adc_event.state = K_POLL_STATE_NOT_READY;

    int err =
        adc_read_async(sampler_hw.adc_dev, &sampler_hw.sequence, &adc_signal);
    if (err == -EBUSY) {
      LOG_WRN("ADC busy, skipping cycle");
      continue;
    } else if (err < 0) {
      LOG_ERR("ADC async read failed: %d", err);
      continue;
    }

    int rc = k_poll(&adc_event, 1, K_MSEC(ADC_TIMEOUT_MS));
    if (rc == -EAGAIN) {
      LOG_ERR("ADC conversion timeout");
      (void)nrfx_saadc_abort();
      continue;
    } else if (rc < 0) {
      LOG_ERR("ADC poll error: %d", rc);
      continue;
    }

    k_poll_signal_reset(&adc_signal);

    slot->sample.timestamp_us = k_ticks_to_us_floor32(k_uptime_ticks());
    for (size_t i = 0; i < MIDAL_NUM_PEDALS; i++) {
      slot->sample.values[i] = slot->adc_raw[sampler_hw.result_offsets[i]];
    }

    pedal_sampler_process_sample(&slot->sample);
  }
}

int pedal_reader_init(const pedal_sampler_hw_t *hw) {
  if (hw == NULL || hw->adc_dev == NULL) {
    return -EINVAL;
  }

  sampler_hw = *hw;
  sampler_hw.sequence.options = &sampler_hw.sequence_opts;
  sampler_hw.sequence.buffer = NULL;
  sampler_hw.sequence.buffer_size = 0;

  memset(sample_slots, 0, sizeof(sample_slots));
  slot_index = 0U;

  k_poll_signal_init(&adc_signal);
  k_poll_event_init(&adc_event, K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
                    &adc_signal);

  k_thread_create(&pedal_reader_thread_data, pedal_reader_thread_stack,
                  K_THREAD_STACK_SIZEOF(pedal_reader_thread_stack),
                  pedal_reader_thread, NULL, NULL, NULL,
                  PEDAL_READER_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&pedal_reader_thread_data, "pedal-reader");

  LOG_INF("Pedal reader thread initialized with priority %d",
          PEDAL_READER_THREAD_PRIORITY);

  return 0;
}
