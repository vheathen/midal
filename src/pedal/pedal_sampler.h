#pragma once

#include "midal_conf.h"

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>

typedef struct {
  uint32_t timestamp_us;
  int16_t values[MIDAL_NUM_PEDALS];
} pedal_raw_sample_t;

typedef struct {
  const struct device *adc_dev;
  struct adc_sequence sequence;
  struct adc_sequence_options sequence_opts;
  uint8_t result_offsets[MIDAL_NUM_PEDALS];
} pedal_sampler_hw_t;

int pedal_sampler_prepare_hw(pedal_sampler_hw_t *out);
void pedal_sampler_process_sample(const pedal_raw_sample_t *sample);
