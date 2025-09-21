/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/usb/usbd.h>

/*
 * This function configures and initialize a USB device via usbd_init_device()
 * and adds a callback to enable or disable device on VBUS detection.
 *
 * It returns 0 on success, otherwise it returns error code.
 */
int usbd_enable_device(void);

/*
 * This function is similar to usbd_init_device(), but does not
 * initialize the device. It allows the application to set additional features,
 * such as additional descriptors.
 */
struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb);

/*
 * This function uses Kconfig.usbd options to configure and initialize a
 * USB device. It configures the device context, default string
 * descriptors, USB device configuration, registers any available class
 * instances, and finally initializes USB device. It is limited to a single
 * device with a single configuration instantiated in usbd_init.c, which
 * should be enough for a simple USB device sample.
 *
 * It returns the configured and initialized USB device context on success,
 * otherwise it returns NULL.
 */
struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb);

/*
 * This function is similar to usbd_init_device(), but does not
 * initialize the device. It allows the application to set additional features,
 * such as additional descriptors.
 */
struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb);
