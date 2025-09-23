#include "pedal_filter.h"
#include <stdlib.h>
#include <zephyr/kernel.h>

static pedal_filter_cfg_t g_cfg;
static float s_state[3];
static int16_t s_last_out[3];

void pedal_filter_init(const pedal_filter_cfg_t *cfg) {
  g_cfg = *cfg;
  for (int i = 0; i < 3; i++) {
    s_state[i] = 0.F;
    s_last_out[i] = -999;
  }
}

uint16_t pedal_filter_apply(uint8_t id, uint16_t raw12) {
  if (id >= 3) {
    id = 0;
  }
  float v = (float)raw12 / 4095.F; // 0..1
  s_state[id] = (g_cfg.alpha * v) + ((1.F - g_cfg.alpha) * s_state[id]);
  uint16_t span = g_cfg.use14bit ? 16383 : 127;
  int32_t q = (int32_t)((s_state[id] * span) + 0.5F);
  if (s_last_out[id] != -999) {
    if (abs(q - s_last_out[id]) < g_cfg.hysteresis) {
      q = s_last_out[id];
    }
  }
  s_last_out[id] = (int16_t)q;
  return (uint16_t)q;
}