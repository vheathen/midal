#include "transport_ble_midi.h"

#include "midi/midi_types.h"
#include "zbus_channels.h"

#include <ble_midi/ble_midi.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(transport_ble_midi, LOG_LEVEL_INF);

/* Zbus message subscriber for MIDI events */
ZBUS_MSG_SUBSCRIBER_DEFINE(ble_midi_sub);

struct transport_ble_ctx {
  atomic_t ready;
  atomic_t sent;
  atomic_t dropped;
};

static struct transport_ble_ctx ble_ctx = {
    .ready = ATOMIC_INIT(0),
};

#define BLE_MIDI_THREAD_PRIORITY 5
#define BLE_MIDI_THREAD_STACK_SIZE 1024
static struct k_thread ble_midi_thread_data;
K_THREAD_STACK_DEFINE(ble_midi_stack, BLE_MIDI_THREAD_STACK_SIZE);
static void ble_midi_thread(void *, void *, void *);

static const struct bt_data adv_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BLE_MIDI_SERVICE_UUID),
};

static const struct bt_data scan_resp[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void);
static void start_advertising_work(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(adv_retry_work, start_advertising_work);

static void start_advertising_work(struct k_work *work) {
  ARG_UNUSED(work);

  const struct bt_le_adv_param *param = BT_LE_ADV_CONN_FAST_2;
  int err =
      bt_le_adv_start(param, adv_data, ARRAY_SIZE(adv_data), scan_resp, ARRAY_SIZE(scan_resp));
  if (err == -EALREADY) {
    return;
  }

  if (err == -ENOMEM) {
    LOG_WRN("BLE advertising buffer full; retrying");
    k_work_reschedule(&adv_retry_work, K_MSEC(500));
    return;
  }

  if (err) {
    LOG_ERR("Failed to start BLE advertising (%d)", err);
  } else {
    LOG_INF("BLE MIDI advertising started");
  }
}

static void start_advertising(void) {
  k_work_cancel_delayable(&adv_retry_work);
  k_work_reschedule(&adv_retry_work, K_NO_WAIT);
}

static void ble_ready_handler(ble_midi_ready_state_t state) {
  atomic_set(&ble_ctx.ready, state == BLE_MIDI_STATE_READY ? 1 : 0);

  if (state == BLE_MIDI_STATE_NOT_CONNECTED) {
    start_advertising();
  }
}

static void ble_tx_done_handler(void) {
  /* No-op for now, but hook retained for future telemetry. */
}

static struct ble_midi_callbacks callbacks = {
    .ready_cb = ble_ready_handler,
    .tx_done_cb = ble_tx_done_handler,
    .midi_message_cb = NULL,
    .sysex_start_cb = NULL,
    .sysex_data_cb = NULL,
    .sysex_end_cb = NULL,
};

static int ble_midi_tx(void *ctx_ptr, const midi_event_t *ev) {
  ARG_UNUSED(ctx_ptr);

  if (ev == NULL || ev->type != MIDI_EV_CC) {
    return 0;
  }

  if (!transport_ble_midi_ready()) {
    return -EAGAIN;
  }

  uint16_t value = ev->cc.value;

  uint8_t status = 0xB0 | (ev->cc.ch & 0x0F);
  uint8_t controller = ev->cc.cc & 0x7F;

  if (IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC)) {
    if (value > 16383U) {
      value = 16383U;
    }
  } else if (value > 127U) {
    value = 127U;
  }

  uint8_t msb = IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC)
                    ? (uint8_t)((value + 0x40U) >> 7) /* rounded */
                    : (uint8_t)value;

  uint8_t msg[3] = {status, controller, msb & 0x7F};

  enum ble_midi_error_t rc = ble_midi_tx_msg(msg);
  if (rc != BLE_MIDI_SUCCESS) {
    return -EIO;
  }

  if (IS_ENABLED(CONFIG_MIDAL_USE_14BIT_CC) && controller < 32U) {
    uint8_t lsb_msg[3] = {status, (uint8_t)(controller + 32U),
                          (uint8_t)(value & 0x7F)};
    rc = ble_midi_tx_msg(lsb_msg);
    if (rc != BLE_MIDI_SUCCESS) {
      return -EIO;
    }
  }

  return 0;
}

int transport_ble_midi_init(void) {
  int err = bt_enable(NULL);
  if (err && err != -EALREADY) {
    LOG_ERR("bt_enable failed (%d)", err);
    return err;
  }

  enum ble_midi_error_t rc = ble_midi_init(&callbacks);
  if (rc != BLE_MIDI_SUCCESS && rc != BLE_MIDI_ALREADY_INITIALIZED) {
    LOG_ERR("ble_midi_init failed (%d)", rc);
    return -EIO;
  }

  atomic_clear(&ble_ctx.sent);
  atomic_clear(&ble_ctx.dropped);

  /* Subscribe to MIDI event channel */
  int ret = zbus_chan_add_obs(&midi_event_chan, &ble_midi_sub, K_MSEC(100));
  if (ret != 0) {
    LOG_ERR("Failed to subscribe BLE MIDI to midi_event_chan: %d", ret);
    return ret;
  }

  /* Start transport thread */
  k_thread_create(&ble_midi_thread_data, ble_midi_stack,
                  K_THREAD_STACK_SIZEOF(ble_midi_stack), ble_midi_thread,
                  NULL, NULL, NULL, BLE_MIDI_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&ble_midi_thread_data, "ble-midi");

  start_advertising();

  LOG_INF("BLE MIDI transport initialized and subscribed to zbus");
  return 0;
}

bool transport_ble_midi_ready(void) { return atomic_get(&ble_ctx.ready) != 0; }

static void ble_midi_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  const struct zbus_channel *chan;
  midi_event_t ev;

  LOG_INF("BLE MIDI transport thread started, waiting for events...");

  while (true) {
    /* Wait for MIDI event from zbus channel */
    int ret = zbus_sub_wait_msg(&ble_midi_sub, &chan, &ev, K_FOREVER);
    if (ret != 0) {
      LOG_ERR("BLE MIDI zbus_sub_wait_msg failed: %d", ret);
      continue;
    }

    /* Verify correct channel */
    if (chan != &midi_event_chan) {
      LOG_WRN("BLE MIDI received event from unexpected channel: %p", chan);
      continue;
    }

    /* Process the event */
    ret = ble_midi_tx(&ble_ctx, &ev);
    if (ret == 0) {
      atomic_inc(&ble_ctx.sent);
    } else {
      atomic_inc(&ble_ctx.dropped);
    }
  }
}
