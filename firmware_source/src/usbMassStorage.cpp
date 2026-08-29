#include "usbMassStorage.h"

#include <stdlib.h>
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"
#include "tinyusb_default_config.h"
#include "sdmmc_cmd.h"

static const char* TAG = "USB_MSC_SDMMC";

// -------------------------- Static state --------------------------
static sdmmc_card_t* s_card = nullptr;
static sdmmc_host_t s_host;
static bool s_host_inited = false;
static bool s_tinyusb_inited = false;
static tinyusb_msc_storage_handle_t s_msc_handle = nullptr;

// -------------------------- TinyUSB descriptors -------------------
enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

// Device descriptor
static tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,
    .idProduct          = 0x4002,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration descriptor
static const uint8_t s_fs_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

// Strings
static const char* s_string_desc[] = {
    (const char[]){0x09, 0x04}, // 0: English (0x0409)
    "Diptyx",                // 1: Manufacturer
    "Diptyx E-reader storage",         // 2: Product
    "123456",                   // 3: Serial
    "SD Card",                  // 4: MSC
};

// -------------------------- Internal helpers ----------------------
static esp_err_t init_sdmmc_card(gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0, int bus_width)
{
    if (bus_width != 1 && bus_width != 4) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    bool host_init_called = false;
    s_card = nullptr;

    s_host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = (bus_width == 4) ? 4 : 1;
    slot_config.clk = clk;
    slot_config.cmd = cmd;
    slot_config.d0  = d0;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    s_card = (sdmmc_card_t*)malloc(sizeof(sdmmc_card_t));
    if (!s_card) return ESP_ERR_NO_MEM;

    ret = (*s_host.init)();
    if (ret != ESP_OK) { free(s_card); s_card=nullptr; return ret; }
    host_init_called = true;
    s_host_inited = true;

    ret = sdmmc_host_init_slot(s_host.slot, &slot_config);
    if (ret != ESP_OK) goto fail;

    ret = sdmmc_card_init(&s_host, s_card);
    if (ret != ESP_OK) goto fail;

    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;

fail:
    if (host_init_called) { (*s_host.deinit)(); s_host_inited=false; }
    if (s_card) { free(s_card); s_card=nullptr; }
    return ret;
}

static void deinit_sdmmc_card(void)
{
    if (s_host_inited) { (*s_host.deinit)(); s_host_inited=false; }
    if (s_card) { free(s_card); s_card=nullptr; }
}

// -------------------------- Public API ----------------------------
esp_err_t usb_msc_sdmmc_start(gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0, int bus_width)
{
    if (s_msc_handle || s_tinyusb_inited) {
        ESP_LOGW(TAG, "Already started");
        return ESP_OK;
    }

    esp_err_t ret = init_sdmmc_card(clk, cmd, d0, bus_width);
    if (ret != ESP_OK) return ret;

    // TinyUSB descriptors
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device              = &s_device_desc;
    tusb_cfg.descriptor.full_speed_config   = s_fs_cfg_desc;
    tusb_cfg.descriptor.string              = s_string_desc;
    tusb_cfg.descriptor.string_count        = sizeof(s_string_desc) / sizeof(s_string_desc[0]);

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        deinit_sdmmc_card();
        return ret;
    }
    s_tinyusb_inited = true;

    // Configure SDMMC storage for MSC
    tinyusb_msc_storage_config_t storage_cfg = {};
    storage_cfg.medium.card = s_card;
    storage_cfg.mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB;

    ret = tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_msc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_storage_init_sdmmc failed: %s", esp_err_to_name(ret));
        tinyusb_driver_uninstall();
        s_tinyusb_inited = false;
        deinit_sdmmc_card();
        return ret;
    }

    ESP_LOGI(TAG, "USB MSC started (SDMMC, %d-bit)", bus_width);
    return ESP_OK;
}

void usb_msc_stop(void)
{
    if (s_msc_handle) {
        tinyusb_msc_delete_storage(s_msc_handle);
        s_msc_handle = nullptr;
    }
    if (s_tinyusb_inited) {
        tinyusb_driver_uninstall();
        s_tinyusb_inited = false;
    }
    deinit_sdmmc_card();
    ESP_LOGI(TAG, "USB MSC stopped");
}

bool usb_msc_in_use_by_host(void)
{
    if (!s_msc_handle) return false;
    tinyusb_msc_mount_point_t mount;
    tinyusb_msc_get_storage_mount_point(s_msc_handle, &mount);
    return (mount == TINYUSB_MSC_STORAGE_MOUNT_USB);
}
