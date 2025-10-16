#pragma once

#include <zephyr/kernel.h>

int transport_usb_midi_init(void);

void transport_usb_notify_ready(bool ready);
bool transport_usb_ready(void);
