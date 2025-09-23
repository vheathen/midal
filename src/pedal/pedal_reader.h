#pragma once

/**
 * @brief Initialize the complete pedal subsystem
 *
 * This function coordinates initialization of:
 * - Hardware interface (ADC channels, sensors)
 * - Threading infrastructure (sensor thread, timers)
 * - Starts sensor polling
 *
 * @return 0 on success, negative error code on failure
 */
int pedal_reader_init(void);