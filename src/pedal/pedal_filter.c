#include "pedal_filter.h"
#include "midal_conf.h"

#include <math.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#ifndef CONFIG_MIDAL_CAL_MARGIN_LSB
#define CONFIG_MIDAL_CAL_MARGIN_LSB 4
#endif
#ifndef CONFIG_MIDAL_CAL_MIN_SPAN_LSB
#define CONFIG_MIDAL_CAL_MIN_SPAN_LSB 32
#endif
#define CAL_MARGIN ((uint16_t)CONFIG_MIDAL_CAL_MARGIN_LSB)
#define CAL_MIN_SPAN ((uint16_t)CONFIG_MIDAL_CAL_MIN_SPAN_LSB)

typedef struct {
  float alpha; // 0..1
  float s_alpha_up;
  float s_alpha_down;
  uint8_t hysteresis; // in CC steps
  bool use14bit;      // send CC+LSB
} pedal_filter_cfg_t;

static pedal_filter_cfg_t g_cfg;
static float s_state[MIDAL_NUM_PEDALS];
static int16_t s_last_out[MIDAL_NUM_PEDALS];
static pedal_calibration_t s_calibration[MIDAL_NUM_PEDALS];

void pedal_filter_init(void) {

  /* Initialize filter configuration from Kconfig/prj.conf */
  g_cfg.use14bit = IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC);
  g_cfg.hysteresis = CONFIG_MIDAL_FILTER_HYST;

  /* Base alpha: either manual from Kconfig or auto-computed below */
#if !defined(CONFIG_MIDAL_FILTER_ALPHA_AUTO)
  g_cfg.alpha = (float)CONFIG_MIDAL_FILTER_ALPHA_MILLIPCT / 100000.0F;
  if (g_cfg.alpha < 0.0001F)
    g_cfg.alpha = 0.0001F;
  if (g_cfg.alpha > 1.0F)
    g_cfg.alpha = 1.0F;
  float a = g_cfg.alpha;
#else
  float a = g_cfg.alpha; /* may be overwritten by AUTO block below */
#endif

#if defined(CONFIG_MIDAL_FILTER_ALPHA_AUTO)
  /* α = 1 - exp( -Ts / τ ) where Ts = 1/fs; τ in ms
   * (CONFIG_MIDAL_FILTER_TAU_MS) */
  const uint32_t fs = CONFIG_MIDAL_POLL_HZ;
  const uint32_t tau_ms =
#ifdef CONFIG_MIDAL_FILTER_TAU_MS
      CONFIG_MIDAL_FILTER_TAU_MS;
#else
      5; /* default 5 ms if not provided */
#endif
  if (fs > 0) {
    const float Ts = 1.0F / (float)fs;
    const float tau = (float)tau_ms / 1000.0F;
    float aa = 1.0F - expf(-Ts / tau);
    aa = aa < 0.0001F ? 0.0001F : aa;
    aa = aa > 1.0F ? 1.0F : aa;
    a = g_cfg.alpha = aa;
  } else {
    a = g_cfg.alpha = 1.0F; /* degenerate: no smoothing if fs is 0 */
  }
#else
  a = g_cfg.alpha;
#endif

  /* Asymmetric EMA: faster attack, softer release when enabled */
#if defined(CONFIG_MIDAL_FILTER_ASYM)
  /* Ensure attack is at least 0.40, release at most 0.20 by default */
  g_cfg.s_alpha_up =
#ifdef CONFIG_MIDAL_FILTER_ALPHA_UP_MIN_MILLIPCT
      fmaxf(a, (float)CONFIG_MIDAL_FILTER_ALPHA_UP_MIN_MILLIPCT / 100000.0F);
#else
      fmaxf(a, 0.40F);
#endif

  g_cfg.s_alpha_down =
#ifdef CONFIG_MIDAL_FILTER_ALPHA_DOWN_MAX_MILLIPCT
      fminf(a, (float)CONFIG_MIDAL_FILTER_ALPHA_DOWN_MAX_MILLIPCT / 100000.0F);
#else
      fminf(a, 0.20F);
#endif
#else
  /* Symmetric EMA */
  g_cfg.s_alpha_up = g_cfg.s_alpha_down = a;
#endif

  for (int i = 0; i < MIDAL_NUM_PEDALS; i++) {
    s_state[i] = 0.0F;
    s_last_out[i] = -999;
    s_calibration[i].min_adc = 248;  // 0.3V starting point
    s_calibration[i].max_adc = 4095; // Will be set on first reading
    s_calibration[i].initialized = false;
  }
}

uint16_t pedal_filter_apply(uint8_t id, uint16_t raw12) {
  if (id >= MIDAL_NUM_PEDALS) {
    id = 0;
  }
  if (raw12 > 4095U) {
    raw12 = 4095U;
  }
  /* raw12 is unsigned, so no need to clamp below 0 */

  pedal_calibration_t *cal = &s_calibration[id];

  /* --- Dynamic calibration with noise margin --- */
  if (!cal->initialized) {
    /* First sample defines provisional upper bound (pedal at rest/up) */
    cal->max_adc = raw12;
    cal->min_adc = raw12; /* will move down as we discover lower values */
    cal->initialized = true;
  } else {
    if ((uint16_t)(raw12 + CAL_MARGIN) < cal->min_adc) {
      cal->min_adc = raw12;
    }
    if (raw12 > (uint16_t)(cal->max_adc + CAL_MARGIN)) {
      cal->max_adc = raw12;
    }
  }

  /* Compute safe span */
  uint16_t span = (cal->max_adc > cal->min_adc)
                      ? (uint16_t)(cal->max_adc - cal->min_adc)
                      : 0U;
  if (span < CAL_MIN_SPAN) {
    span = CAL_MIN_SPAN;
  }

  /* Normalize to 0..1 with current [min..max] */
  int32_t num = (int32_t)raw12 - (int32_t)cal->min_adc;
  if (num < 0) {
    num = 0;
  }
  if (num > (int32_t)span) {
    num = (int32_t)span;
  }
  float v = (float)num / (float)span; /* 0..1 */

  /* Endpoint hold: snap very close values to exact 0/1 to avoid chatter */
  const float eps = g_cfg.use14bit ? (1.0F / 16383.0F) : (1.0F / 127.0F);
  if (v < eps) {
    v = 0.0F;
  } else if (v > 1.0F - eps) {
    v = 1.0F;
  }

#if IS_ENABLED(CONFIG_MIDAL_INVERT_POLARITY)
  v = 1.0F - v;
#endif

  const float alpha = (v > s_state[id]) ? g_cfg.s_alpha_up : g_cfg.s_alpha_down;
  s_state[id] = (alpha * v) + ((1.0F - alpha) * s_state[id]);
  uint16_t span_out = g_cfg.use14bit ? 16383 : 127;
  int32_t q = (int32_t)((s_state[id] * (float)span_out) + 0.5F);
  if (s_last_out[id] != -999) {
    if (abs(q - s_last_out[id]) < g_cfg.hysteresis) {
      q = s_last_out[id];
    }
  }
  s_last_out[id] = (int16_t)q;
  return (uint16_t)q;
}

void pedal_filter_reset_calibration(uint8_t pedal_id) {
  if (pedal_id >= MIDAL_NUM_PEDALS) {
    return;
  }

  pedal_calibration_t *cal = &s_calibration[pedal_id];
  cal->min_adc = 500;  // 0.5V starting point
  cal->max_adc = 4095; // Will be set on first reading
  cal->initialized = false;
}

void pedal_filter_get_calibration(uint8_t pedal_id, pedal_calibration_t *cal) {
  if (pedal_id >= MIDAL_NUM_PEDALS || cal == NULL) {
    return;
  }

  *cal = s_calibration[pedal_id];
}