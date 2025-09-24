#pragma once

#include <zephyr/kernel.h>

typedef struct {
    uint16_t min_adc;
    uint16_t max_adc;
    bool initialized;
} pedal_calibration_t;

void pedal_filter_init(void);
uint16_t pedal_filter_apply(uint8_t pedal_id,
                            uint16_t raw12bit); // -> 0..127/16383

void pedal_filter_reset_calibration(uint8_t pedal_id);
void pedal_filter_get_calibration(uint8_t pedal_id, pedal_calibration_t *cal);