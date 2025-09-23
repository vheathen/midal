#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  float alpha;        // 0..1
  uint8_t hysteresis; // in CC steps
  bool use14bit;      // send CC+LSB
} pedal_filter_cfg_t;

void pedal_filter_init(const pedal_filter_cfg_t *cfg);
uint16_t pedal_filter_apply(uint8_t pedal_id,
                            uint16_t raw12bit); // -> 0..127/16383