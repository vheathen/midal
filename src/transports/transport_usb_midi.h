#pragma once

#include "midi/midi_router.h"

int transport_uart_midi_init(midi_tx_fn *out_tx);
int transport_usb_midi_init(midi_tx_fn *out_tx);
int transport_ble_midi_init(midi_tx_fn *out_tx);