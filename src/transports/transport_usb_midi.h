#pragma once

#include "midi/midi_router.h"

int transport_usb_midi_init(midi_tx_fn *out_tx, void **out_ctx);

void transport_usb_notify_ready(bool ready);
bool transport_usb_ready(void);
