#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(saadc_selftest, 3);

#define USER_NODE DT_PATH(zephyr_user)

/* Каналы из DT: <&adc 7>, <&adc 0>, <&adc 5> */
static const struct adc_dt_spec chans[] = {
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 1),
    ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 2),
};

static int read_once(const struct adc_dt_spec *sp, int16_t *out) {
  struct adc_sequence seq = {
      .channels = BIT(sp->channel_id),
      .buffer = out,
      .buffer_size = sizeof(*out),
      .resolution = 12,
      .oversampling = sp->oversampling, /* из DT */
  };
  return adc_read(sp->dev, &seq);
}

/* Вернуть изменённый cfg с нужным acquisition_time */
static void make_cfg(const struct adc_dt_spec *sp, struct adc_channel_cfg *out,
                     uint32_t acq_us) {
  *out = (struct adc_channel_cfg){
      .gain = sp->channel_cfg.gain,
      .reference = sp->channel_cfg.reference,
      .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, acq_us),
      .channel_id = sp->channel_cfg.channel_id,
      .differential = sp->channel_cfg.differential,
      .input_positive = sp->channel_cfg.input_positive,
      .input_negative = sp->channel_cfg.input_negative,
  };
}

static void stats_run(uint32_t acq_us) {
  static const int iters = 200;

  /* Настроить все каналы на данный acq_us */
  for (size_t i = 0; i < ARRAY_SIZE(chans); i++) {
    if (!device_is_ready(chans[i].dev)) {
      LOG_ERR("ADC dev not ready for ch%u", chans[i].channel_id);
      return;
    }
    struct adc_channel_cfg cfg;
    make_cfg(&chans[i], &cfg, acq_us);
    int err = adc_channel_setup(chans[i].dev, &cfg);
    if (err) {
      LOG_ERR("adc_channel_setup ch%u (acq=%uus) failed: %d",
              chans[i].channel_id, acq_us, err);
      return;
    }
  }

  /* Замер статистик по каждому каналу */
  for (size_t i = 0; i < ARRAY_SIZE(chans); i++) {
    int16_t s;
    int err;

    int32_t sum = 0;
    int16_t vmin = INT16_MAX;
    int16_t vmax = INT16_MIN;

    for (int k = 0; k < iters; k++) {
      err = read_once(&chans[i], &s);
      if (err) {
        LOG_ERR("read ch%u err=%d", chans[i].channel_id, err);
        break;
      }
      /* clamp к 0..4095 для отчётности */
      if (s < 0) {
        s = 0;
      }
      if (s > 4095) {
        s = 4095;
      }

      sum += s;
      if (s < vmin) {
        vmin = s;
      }
      if (s > vmax) {
        vmax = s;
      }
    }
    int32_t mean = sum / iters;
    int32_t p2p = (int32_t)vmax - (int32_t)vmin;

    LOG_INF("[acq=%2u us] ch%u: mean=%ld  min=%d  max=%d  p2p=%ld", acq_us,
            chans[i].channel_id, (long)mean, vmin, vmax, (long)p2p);
  }

  /* Тест A->B->A (берём первые два канала как A и B) */
  if (ARRAY_SIZE(chans) >= 2) {
    const struct adc_dt_spec *A = &chans[0];
    const struct adc_dt_spec *B = &chans[1];
    int16_t a1, b, a2;
    int32_t worst = 0;
    for (int k = 0; k < 50; k++) {
      (void)read_once(A, &a1);
      (void)read_once(B, &b);
      (void)read_once(A, &a2);
      if (a1 < 0) {
        a1 = 0;
      }
      if (a1 > 4095) {
        a1 = 4095;
      }
      if (a2 < 0) {
        a2 = 0;
      }
      if (a2 > 4095) {
        a2 = 4095;
      }
      int32_t delta = a2 - a1;
      if (delta < 0) {
        delta = -delta;
      }
      if (delta > worst) {
        worst = delta;
      }
    }
    LOG_INF("[acq=%2u us] A->B->A worst delta on ch%u = %ld LSB (B=ch%u)",
            acq_us, A->channel_id, (long)worst, B->channel_id);
  }
}

void saadc_selftest_run(void) {

  /* Wait for ADC to settle */
  k_sleep(K_MSEC(5000));

  LOG_INF("=== SAADC acquisition-time self-test start ===");
  const uint32_t acq_list[] = {10, 20, 40};
  for (size_t i = 0; i < ARRAY_SIZE(acq_list); i++) {
    stats_run(acq_list[i]);
  }
  LOG_INF("=== SAADC self-test done ===");
}