#pragma once

#include <zephyr/kernel.h>

#define MIDAL_CH_PEDAL_SUSTAIN   0
#define MIDAL_CH_PEDAL_SOSTENUTO 1
#define MIDAL_CH_PEDAL_SOFT      2
#define MIDAL_NUM_PEDALS         3

/* MIDI CC номера по умолчанию */
#define MIDAL_CC_SUSTAIN   64
#define MIDAL_CC_SOSTENUTO 66
#define MIDAL_CC_SOFT      67