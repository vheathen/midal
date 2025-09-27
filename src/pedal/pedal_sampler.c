#include "pedal_sampler.h"
#include "midal_conf.h"
#include "midi/midi_router.h"
#include "midi/midi_types.h"
#include "pedal_filter.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
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

/* Last sent CC value per pedal (uint16_t: 0..127 or 0..16383). 0xFFFF = unknown
 */
static uint16_t last_sent_cc[MIDAL_NUM_PEDALS];

#if IS_ENABLED(CONFIG_MIDAL_PEDAL_LOG)
static uint32_t last_log_time[MIDAL_NUM_PEDALS] = {0};
#endif

typedef struct {
  uint8_t pedal_idx;
  uint8_t channel_id;
} channel_map_entry_t;

static void log_pedal_state(size_t pedal_idx, uint16_t raw, uint16_t filtered) {
#if IS_ENABLED(CONFIG_MIDAL_PEDAL_LOG)
  uint32_t now = k_uptime_get_32();
  if (now - last_log_time[pedal_idx] >= CONFIG_MIDAL_PEDAL_LOG_RATE_MS) {
    pedal_calibration_t cal;
    pedal_filter_get_calibration(pedal_idx, &cal);
    LOG_INF("%s pedal: raw=%u filtered=%u CC%d [cal: %u-%u %s]",
            pedal_configs[pedal_idx].name, raw, filtered,
            pedal_configs[pedal_idx].midi_cc, cal.min_adc, cal.max_adc,
            cal.initialized ? "ready" : "init");
    last_log_time[pedal_idx] = now;
  }
#else
  ARG_UNUSED(pedal_idx);
  ARG_UNUSED(raw);
  ARG_UNUSED(filtered);
#endif
}

void pedal_sampler_process_sample(const pedal_raw_sample_t *sample) {
  if (sample == NULL) {
    return;
  }

  for (size_t i = 0; i < pedals_count; i++) {
    int32_t raw = sample->values[i];
    if (raw < 0) {
      raw = 0;
    }
    if (raw > 4095) {
      raw = 4095;
    }

    uint16_t filtered = pedal_filter_apply(i, (uint16_t)raw);
    log_pedal_state(i, (uint16_t)raw, filtered);

    if (last_sent_cc[i] != filtered) {
      last_sent_cc[i] = filtered;
      midi_event_t ev = {
          .type = MIDI_EV_CC,
          .timestamp_us = sample->timestamp_us,
          .cc = {.ch = pedal_configs[i].midi_channel,
                 .cc = pedal_configs[i].midi_cc,
                 .value = filtered},
      };
      (void)midi_router_submit(&ev);
    }
  }
}

int pedal_sampler_prepare_hw(pedal_sampler_hw_t *out) {
  if (out == NULL) {
    return -EINVAL;
  }

  if (pedals_count != MIDAL_NUM_PEDALS) {
    LOG_ERR("Pedal config count mismatch (expected %d, got %d)",
            MIDAL_NUM_PEDALS, (int)pedals_count);
    return -EINVAL;
  }

  memset(out, 0, sizeof(*out));

  const struct device *adc_dev = NULL;
  channel_map_entry_t map[MIDAL_NUM_PEDALS];
  uint32_t channels_mask = 0U;

  for (size_t i = 0; i < pedals_count; i++) {
    const struct adc_dt_spec *spec = &pedal_configs[i].adc_spec;

    if (!adc_is_ready_dt(spec)) {
      LOG_ERR("%s pedal: ADC controller device %s not ready",
              pedal_configs[i].name, spec->dev->name);
      return -ENODEV;
    }

    if (adc_dev == NULL) {
      adc_dev = spec->dev;
    } else if (adc_dev != spec->dev) {
      LOG_ERR("Pedal channels use different ADC devices (unsupported)");
      return -ENODEV;
    }

    int err = adc_channel_setup_dt(spec);
    if (err < 0) {
      LOG_ERR("%s pedal: Could not setup ADC channel (%d)",
              pedal_configs[i].name, err);
      return err;
    }

    map[i] = (channel_map_entry_t){.pedal_idx = (uint8_t)i,
                                   .channel_id = spec->channel_id};
    channels_mask |= BIT(spec->channel_id);
  }

  /* Wait for ADC to settle */
  k_sleep(K_MSEC(2000));

  pedal_filter_init();
  memset(last_sent_cc, 0xFF, sizeof(last_sent_cc));
#if IS_ENABLED(CONFIG_MIDAL_PEDAL_LOG)
  memset(last_log_time, 0, sizeof(last_log_time));
#endif

  /* Sort channel map by channel ID to match SAADC result ordering */
  for (size_t i = 0; i < pedals_count; i++) {
    for (size_t j = i + 1; j < pedals_count; j++) {
      if (map[j].channel_id < map[i].channel_id) {
        channel_map_entry_t tmp = map[i];
        map[i] = map[j];
        map[j] = tmp;
      }
    }
  }

  out->adc_dev = adc_dev;
  out->sequence = (struct adc_sequence){
      .channels = channels_mask,
      .buffer = NULL,
      .buffer_size = 0,
      .resolution = pedal_configs[0].adc_spec.resolution,
      .oversampling = pedal_configs[0].adc_spec.oversampling,
      .calibrate = 0,
      .options = NULL,
  };
  out->sequence_opts = (struct adc_sequence_options){0};
  out->sequence.options = &out->sequence_opts;

  for (size_t i = 0; i < pedals_count; i++) {
    out->result_offsets[map[i].pedal_idx] = (uint8_t)i;
  }

  LOG_INF("Pedal sampler initialized with %d pedals:", (int)pedals_count);
  for (size_t i = 0U; i < pedals_count; i++) {
    LOG_INF("  %s: CC%d on channel %d (offset %u)", pedal_configs[i].name,
            pedal_configs[i].midi_cc, pedal_configs[i].adc_spec.channel_id,
            out->result_offsets[i]);
  }

  return 0;
}
