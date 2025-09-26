#include "midi_router.h"
#include "midi_types.h"

#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(midi_router, LOG_LEVEL_INF);
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#define MIDI_Q_CAP 256
K_MSGQ_DEFINE(midi_q, sizeof(midi_event_t), MIDI_Q_CAP, 4);

#define ROUTER_THREAD_PRIORITY 4
#define ROUTER_THREAD_STACK_SIZE 2048
static struct k_thread router_thread_data;
K_THREAD_STACK_DEFINE(router_stack, ROUTER_THREAD_STACK_SIZE);
static void router_thread(void *, void *, void *);

#define ROUTE_THREAD_PRIORITY 5
#define ROUTE_THREAD_STACK_SIZE 1024
#define ROUTE_QUEUE_DEPTH 32

struct midi_route_slot {
  midi_tx_fn tx;
  void *ctx;
  const char *name;
  struct k_msgq queue;
  midi_event_t queue_storage[ROUTE_QUEUE_DEPTH];
  struct k_thread thread;
  atomic_t dropped;
  atomic_t sent;
  uint16_t queue_high_water;
  bool active;
};

static struct midi_route_slot s_routes[MIDI_ROUTER_MAX_ROUTES];

static K_THREAD_STACK_DEFINE(route_stack0, ROUTE_THREAD_STACK_SIZE);
static K_THREAD_STACK_DEFINE(route_stack1, ROUTE_THREAD_STACK_SIZE);
static K_THREAD_STACK_DEFINE(route_stack2, ROUTE_THREAD_STACK_SIZE);
static K_THREAD_STACK_DEFINE(route_stack3, ROUTE_THREAD_STACK_SIZE);

static k_thread_stack_t *const s_route_stacks[MIDI_ROUTER_MAX_ROUTES] = {
    route_stack0,
    route_stack1,
    route_stack2,
    route_stack3,
};

static const size_t s_route_stack_sizes[MIDI_ROUTER_MAX_ROUTES] = {
    K_THREAD_STACK_SIZEOF(route_stack0),
    K_THREAD_STACK_SIZEOF(route_stack1),
    K_THREAD_STACK_SIZEOF(route_stack2),
    K_THREAD_STACK_SIZEOF(route_stack3),
};

static atomic_t s_total_enqueued = ATOMIC_INIT(0);
static atomic_t s_total_dispatched = ATOMIC_INIT(0);
static atomic_t s_ev_dropped = ATOMIC_INIT(0);
static uint16_t s_router_queue_high_water;

int midi_router_get_dropped(void) { return (int)atomic_get(&s_ev_dropped); }

static void route_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);
  struct midi_route_slot *slot = p1;
  midi_event_t ev;

  while (true) {
    if (k_msgq_get(&slot->queue, &ev, K_FOREVER) == 0) {
      int rc = slot->tx(slot->ctx, &ev);
      if (rc == 0) {
        atomic_inc(&slot->sent);
        atomic_inc(&s_total_dispatched);
      } else {
        atomic_inc(&slot->dropped);
        atomic_inc(&s_ev_dropped);
        LOG_DBG("Route %s tx error %d", slot->name ? slot->name : "?", rc);
      }
    }
  }
}

void midi_router_init(void) {
  memset(s_routes, 0, sizeof(s_routes));
  s_router_queue_high_water = 0U;
  atomic_clear(&s_total_enqueued);
  atomic_clear(&s_total_dispatched);
  atomic_clear(&s_ev_dropped);
}

static void update_high_water(uint16_t *high_water, struct k_msgq *q) {
  uint32_t used = k_msgq_num_used_get(q);
  if (used > *high_water) {
    *high_water = (uint16_t)used;
  }
}

int midi_router_register_tx(midi_tx_fn tx, void *ctx, const char *name) {
  if (tx == NULL) {
    return -EINVAL;
  }

  for (size_t i = 0; i < MIDI_ROUTER_MAX_ROUTES; i++) {
    struct midi_route_slot *slot = &s_routes[i];
    if (!slot->active) {
      k_msgq_init(&slot->queue, (char *)slot->queue_storage,
                  sizeof(midi_event_t), ROUTE_QUEUE_DEPTH);

      slot->tx = tx;
      slot->ctx = ctx;
      slot->name = name;
      slot->queue_high_water = 0U;
      slot->active = true;
      atomic_clear(&slot->dropped);
      atomic_clear(&slot->sent);

      k_thread_create(&slot->thread, s_route_stacks[i], s_route_stack_sizes[i],
                      route_thread, slot, NULL, NULL, ROUTE_THREAD_PRIORITY, 0,
                      K_NO_WAIT);
      if (slot->name) {
        k_thread_name_set(&slot->thread, slot->name);
      }

      LOG_INF("MIDI route %zu registered (%s)", i, name ? name : "unnamed");
      return 0;
    }
  }

  LOG_ERR("No free MIDI router slots");
  return -ENOMEM;
}

bool midi_router_submit(const midi_event_t *ev) {
  if (ev == NULL) {
    return false;
  }

  int rc = k_msgq_put(&midi_q, ev, K_MSEC(1));
  if (rc != 0) {
    atomic_inc(&s_ev_dropped);
    return false;
  }

  atomic_inc(&s_total_enqueued);
  update_high_water(&s_router_queue_high_water, &midi_q);
  return true;
}

void midi_router_start(void) {
  k_thread_create(&router_thread_data, router_stack,
                  K_THREAD_STACK_SIZEOF(router_stack), router_thread, NULL,
                  NULL, NULL, ROUTER_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&router_thread_data, "midi-router");
}

static void dispatch_to_routes(const midi_event_t *ev) {
  for (size_t i = 0; i < MIDI_ROUTER_MAX_ROUTES; i++) {
    struct midi_route_slot *slot = &s_routes[i];
    if (!slot->active) {
      continue;
    }

    int rc = k_msgq_put(&slot->queue, ev, K_NO_WAIT);
    if (rc != 0) {
      atomic_inc(&slot->dropped);
      atomic_inc(&s_ev_dropped);
      LOG_DBG("Route %s queue full", slot->name ? slot->name : "?");
    } else {
      update_high_water(&slot->queue_high_water, &slot->queue);
    }
  }
}

static void router_thread(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  midi_event_t ev;
  uint32_t now = 0;
  uint32_t last_pong_time = 0;

  while (true) {
    if (k_msgq_get(&midi_q, &ev, K_FOREVER) == 0) {
      dispatch_to_routes(&ev);

      now = k_uptime_get_32();
      if ((now - last_pong_time) >= 1000U) {
        last_pong_time = now;
        LOG_DBG("MIDI router thread heartbeat");
      }
    }
  }
}

void midi_router_get_stats(struct midi_router_stats *stats) {
  if (stats == NULL) {
    return;
  }

  memset(stats, 0, sizeof(*stats));

  stats->total_enqueued = (uint32_t)atomic_get(&s_total_enqueued);
  stats->total_dispatched = (uint32_t)atomic_get(&s_total_dispatched);
  stats->total_dropped = (uint32_t)atomic_get(&s_ev_dropped);
  stats->queue_high_water = s_router_queue_high_water;

  size_t count = 0;
  for (size_t i = 0;
       i < MIDI_ROUTER_MAX_ROUTES && count < MIDI_ROUTER_MAX_ROUTES; i++) {
    struct midi_route_slot *slot = &s_routes[i];
    if (!slot->active) {
      continue;
    }

    stats->routes[count].name = slot->name;
    stats->routes[count].sent = (uint32_t)atomic_get(&slot->sent);
    stats->routes[count].dropped = (uint32_t)atomic_get(&slot->dropped);
    stats->routes[count].queue_high_water = slot->queue_high_water;
    count++;
  }
  stats->route_count = (uint8_t)count;
}
