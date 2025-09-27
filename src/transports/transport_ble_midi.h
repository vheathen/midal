#pragma once

#include "midi/midi_router.h"

int transport_ble_midi_init(midi_tx_fn *out_tx, void **out_ctx);
bool transport_ble_midi_ready(void);
