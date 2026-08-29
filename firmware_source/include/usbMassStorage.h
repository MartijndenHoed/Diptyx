#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize SDMMC (1-bit or 4-bit) and start USB MSC device that exposes the SD card.
 * This does NOT mount the SD to the app (FATFS). The host PC gets exclusive access.
 *
 * @param clk   SDMMC CLK GPIO
 * @param cmd   SDMMC CMD GPIO
 * @param d0    SDMMC D0  GPIO
 * @param bus_width  Bus width (1 or 4). Pass 1 for your wiring.
 * @return ESP_OK on success
 */
esp_err_t usb_msc_sdmmc_start(gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0, int bus_width);

/**
 * Stop USB MSC, deinit SDMMC host and free resources.
 */
void usb_msc_stop(void);

/**
 * True if the storage is currently in use by the USB host (TinyUSB helper knows this).
 */
bool usb_msc_in_use_by_host(void);

/* Optional helpers (only if you later want to mount for ESP app use):
   esp_err_t usb_msc_app_mount(const char* base_path, int max_files);
   esp_err_t usb_msc_app_unmount(void);
*/

#ifdef __cplusplus
}
#endif