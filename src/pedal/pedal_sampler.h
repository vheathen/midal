#pragma once

/* Public API functions */
int pedal_sampler_init_sensors(void);

/* Hardware interface functions (used by thread) */
void read_pedals(void);