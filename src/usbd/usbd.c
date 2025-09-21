/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usbd_config);

#define MIDAL_USB_VID 0x2fe3

/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
    "dfu_dfu",
    NULL,
};

/*
 * Instantiate a context named midal_usbd using the default USB device
 * controller, the MIDAL project vendor ID, and the MIDAL product ID.
 */
USBD_DEVICE_DEFINE(midal_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   MIDAL_USB_VID, CONFIG_MIDAL_USBD_PID);

USBD_DESC_LANG_DEFINE(midal_lang);
USBD_DESC_MANUFACTURER_DEFINE(midal_mfr, CONFIG_MIDAL_USBD_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(midal_product, CONFIG_MIDAL_USBD_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(midal_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

static const uint8_t attributes =
    (IS_ENABLED(CONFIG_MIDAL_USBD_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0) |
    (IS_ENABLED(CONFIG_MIDAL_USBD_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(midal_fs_config, attributes,
                          CONFIG_MIDAL_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(midal_hs_config, attributes,
                          CONFIG_MIDAL_USBD_MAX_POWER, &hs_cfg_desc);

#if CONFIG_MIDAL_USBD_20_EXTENSION_DESC
/*
 * This does not yet provide valuable information, but rather serves as an
 * example, and will be improved in the future.
 */
static const struct usb_bos_capability_lpm bos_cap_lpm = {
    .bLength = sizeof(struct usb_bos_capability_lpm),
    .bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
    .bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
    .bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(midal_usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);
#endif

static void root_msg_cb(struct usbd_context *const ctx,
                        const struct usbd_msg *msg) {
  LOG_DBG("USBD message: %s", usbd_msg_type_string(msg->type));

  if (usbd_can_detect_vbus(ctx)) {
    if (msg->type == USBD_MSG_VBUS_READY) {
      if (usbd_enable(ctx)) {
        LOG_ERR("Failed to enable device support");
      }
    }

    if (msg->type == USBD_MSG_VBUS_REMOVED) {
      if (usbd_disable(ctx)) {
        LOG_ERR("Failed to disable device support");
      }
    }
  }
}

static void fix_code_triple(struct usbd_context *uds_ctx,
                            const enum usbd_speed speed) {
  /* Always use class code information from Interface Descriptors */
  if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
      IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS)) {
    /*
     * Class with multiple interfaces have an Interface
     * Association Descriptor available, use an appropriate triple
     * to indicate it.
     */
    usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02,
                                0x01);
  } else {
    usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
  }
}

struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb) {
  int err;

  err = usbd_add_descriptor(&midal_usbd, &midal_lang);
  if (err) {
    LOG_ERR("Failed to initialize language descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&midal_usbd, &midal_mfr);
  if (err) {
    LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&midal_usbd, &midal_product);
  if (err) {
    LOG_ERR("Failed to initialize product descriptor (%d)", err);
    return NULL;
  }

  IF_ENABLED(CONFIG_HWINFO,
             (err = usbd_add_descriptor(&midal_usbd, &midal_sn);))
  if (err) {
    LOG_ERR("Failed to initialize SN descriptor (%d)", err);
    return NULL;
  }

  if (USBD_SUPPORTS_HIGH_SPEED &&
      usbd_caps_speed(&midal_usbd) == USBD_SPEED_HS) {
    err = usbd_add_configuration(&midal_usbd, USBD_SPEED_HS, &midal_hs_config);
    if (err) {
      LOG_ERR("Failed to add High-Speed configuration");
      return NULL;
    }

    err = usbd_register_all_classes(&midal_usbd, USBD_SPEED_HS, 1, blocklist);
    if (err) {
      LOG_ERR("Failed to add register classes");
      return NULL;
    }

    fix_code_triple(&midal_usbd, USBD_SPEED_HS);
  }

  err = usbd_add_configuration(&midal_usbd, USBD_SPEED_FS, &midal_fs_config);
  if (err) {
    LOG_ERR("Failed to add Full-Speed configuration");
    return NULL;
  }

  err = usbd_register_all_classes(&midal_usbd, USBD_SPEED_FS, 1, blocklist);
  if (err) {
    LOG_ERR("Failed to add register classes");
    return NULL;
  }

  fix_code_triple(&midal_usbd, USBD_SPEED_FS);
  usbd_self_powered(&midal_usbd, attributes & USB_SCD_SELF_POWERED);

  if (msg_cb != NULL) {
    err = usbd_msg_register_cb(&midal_usbd, msg_cb);
    if (err) {
      LOG_ERR("Failed to register message callback");
      return NULL;
    }
  }

#if CONFIG_MIDAL_USBD_20_EXTENSION_DESC
  (void)usbd_device_set_bcd_usb(&midal_usbd, USBD_SPEED_FS, 0x0201);
  (void)usbd_device_set_bcd_usb(&midal_usbd, USBD_SPEED_HS, 0x0201);

  err = usbd_add_descriptor(&midal_usbd, &midal_usbext);
  if (err) {
    LOG_ERR("Failed to add USB 2.0 Extension Descriptor");
    return NULL;
  }
#endif

  return &midal_usbd;
}

struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb) {
  int err;

  if (usbd_setup_device(msg_cb) == NULL) {
    return NULL;
  }

  err = usbd_init(&midal_usbd);
  if (err) {
    LOG_ERR("Failed to initialize device support");
    return NULL;
  }

  return &midal_usbd;
}

int usbd_enable_device(void) {
  static struct usbd_context *midal_usbd;

  midal_usbd = usbd_init_device(root_msg_cb);

  if (midal_usbd == NULL) {
    LOG_ERR("Failed to initialize USB device");
    return -ENODEV;
  }

  if (!usbd_can_detect_vbus(midal_usbd)) {
    int err = usbd_enable(midal_usbd);
    if (err) {
      LOG_ERR("Failed to enable device support");
      return err;
    }
  }

  LOG_INF("USB device support enabled");

  return 0;
}