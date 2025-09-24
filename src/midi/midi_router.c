#include "midi_router.h"
#include "midi_types.h"

#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(midi_router, LOG_LEVEL_INF);
#include <zephyr/kernel.h>

#define MIDI_Q_CAP 64
K_MSGQ_DEFINE(midi_q, sizeof(midi_event_t), MIDI_Q_CAP, 4);

#define MAX_TX 4
static midi_tx_fn s_txs[MAX_TX];
static size_t s_txn;

#define ROUTER_THREAD_PRIORITY 4
#define ROUTER_THREAD_STACK_SIZE 2048
static struct k_thread router_thread_data;
K_THREAD_STACK_DEFINE(router_stack, ROUTER_THREAD_STACK_SIZE);
static void router_thread(void *, void *, void *);

void midi_router_init(void) { s_txn = 0; }

void midi_router_register_tx(midi_tx_fn tx) {
  if (s_txn < MAX_TX && tx) {
    s_txs[s_txn++] = tx;
  }
}

bool midi_router_submit(const midi_event_t *ev) {
  return k_msgq_put(&midi_q, ev, K_NO_WAIT) == 0;
}

void midi_router_start(void) {
  k_thread_create(&router_thread_data, router_stack,
                  K_THREAD_STACK_SIZEOF(router_stack), router_thread, NULL,
                  NULL, NULL, ROUTER_THREAD_PRIORITY, 0, K_NO_WAIT);
}

static void router_thread(void *, void *, void *) {
  midi_event_t ev;
  while (1) {
    if (k_msgq_get(&midi_q, &ev, K_FOREVER) == 0) {

      for (size_t i = 0; i < s_txn; i++) {
        if (s_txs[i]) {
          (void)s_txs[i](&ev);
        }
      }
    }
  }
}