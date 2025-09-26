#pragma once

#include "pedal_sampler.h"

/*
 * Initialize the pedals reading thread
 */
int pedal_reader_init(const pedal_sampler_hw_t *hw);
