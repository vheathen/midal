#include "midal_conf.h"

#include "pedal_filter.h"
#include "pedal_sampler.h"

// #include "midi/midi_router.h"  // TODO: Enable when MIDI router is ready
// #include "midi/midi_types.h"   // TODO: Enable when MIDI router is ready

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pedal_sampler, LOG_LEVEL_INF);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) ||                                   \
    !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

/* Pedal configuration structure combining ADC and MIDI settings */
typedef struct {
  struct adc_dt_spec adc_spec;
  uint8_t midi_cc;
  uint8_t midi_channel;
  const char *name;
} pedal_config_t;

/* Pedal configurations using named devicetree channels */
const pedal_config_t pedal_configs[] = {
    {.adc_spec = ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pedal_sustain),
     .midi_cc = MIDAL_CC_SUSTAIN,
     .midi_channel = MIDAL_CH_PEDAL_SUSTAIN,
     .name = "Sustain"},

    {.adc_spec = ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pedal_sostenuto),
     .midi_cc = MIDAL_CC_SOSTENUTO,
     .midi_channel = MIDAL_CH_PEDAL_SOSTENUTO,
     .name = "Sostenuto"},

    {.adc_spec = ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pedal_soft),
     .midi_cc = MIDAL_CC_SOFT,
     .midi_channel = MIDAL_CH_PEDAL_SOFT,
     .name = "Soft"}};

const size_t pedals_count = ARRAY_SIZE(pedal_configs);
int16_t adc_sample_buffer[ARRAY_SIZE(pedal_configs)];

static uint16_t read_adc(uint8_t pedal_idx) {
  if (pedal_idx >= pedals_count) {
    LOG_ERR("Invalid pedal index %d", pedal_idx);
    return 0;
  }

  struct adc_sequence sequence = {
      .buffer = &adc_sample_buffer[pedal_idx],
      .buffer_size = sizeof(adc_sample_buffer[pedal_idx]),
  };

  (void)adc_sequence_init_dt(&pedal_configs[pedal_idx].adc_spec, &sequence);

  int ret = adc_read_dt(&pedal_configs[pedal_idx].adc_spec, &sequence);
  if (ret < 0) {
    LOG_ERR("%s pedal: ADC read failed with error code %d",
            pedal_configs[pedal_idx].name, ret);
    return 0;
  }

  /* Convert to 12-bit positive value */
  uint16_t value = (uint16_t)MAX(0, adc_sample_buffer[pedal_idx]);
  return value & 0x0FFF;
}

#if IS_ENABLED(CONFIG_MIDAL_PEDAL_LOG)
static uint32_t last_log_time[MIDAL_NUM_PEDALS] = {0};
#endif

void read_pedals(void) {
  for (size_t i = 0; i < pedals_count; i++) {
    uint16_t raw = read_adc(i);
    uint16_t val = pedal_filter_apply(i, raw);

    /* Log pedal values with descriptive names - per
pedal rate limiting */

#if IS_ENABLED(CONFIG_MIDAL_PEDAL_LOG)
    uint32_t now = k_uptime_get_32();
    if (now - last_log_time[i] >= CONFIG_MIDAL_PEDAL_LOG_RATE) {
      pedal_calibration_t cal;
      pedal_filter_get_calibration(i, &cal);
      LOG_INF("%s pedal: raw=%d filtered=%d CC%d [cal: %d-%d %s]",
              pedal_configs[i].name, raw, val, pedal_configs[i].midi_cc,
              cal.min_adc, cal.max_adc, cal.initialized ? "ready" : "init");
      last_log_time[i] = now;
    }
#endif

    /* TODO: Enable when MIDI router is ready */
    // midi_event_t ev = {.type = MIDI_EV_CC,
    //                    .timestamp_us =
    //                    k_ticks_to_us_floor32(k_uptime_ticks()), .cc = {.ch
    //                    = pedal_configs[i].midi_channel,
    //                           .cc = pedal_configs[i].midi_cc,
    //                           .value = val}};
    // (void)midi_router_submit(&ev);
  }
}

int pedal_sampler_init_sensors(void) {
  /* Configure channels individually prior to sampling. */
  for (size_t i = 0U; i < pedals_count; i++) {

    if (!adc_is_ready_dt(&pedal_configs[i].adc_spec)) {
      LOG_ERR("%s pedal: ADC controller device %s not ready",
              pedal_configs[i].name, pedal_configs[i].adc_spec.dev->name);
      return -ENODEV;
    }

    int err = adc_channel_setup_dt(&pedal_configs[i].adc_spec);
    if (err < 0) {
      LOG_ERR("%s pedal: Could not setup ADC channel (%d)",
              pedal_configs[i].name, err);
      return -ENODEV;
    }
  }

  /* Wait for ADC to settle */
  k_sleep(K_MSEC(2000));

  pedal_filter_init();

  LOG_INF("Pedal sampler initialized with %d pedals:", (int)pedals_count);

  for (size_t i = 0U; i < pedals_count; i++) {
    LOG_INF("  %s: CC%d on channel %d", pedal_configs[i].name,
            pedal_configs[i].midi_cc, pedal_configs[i].midi_channel);
  }

  return 0;
}