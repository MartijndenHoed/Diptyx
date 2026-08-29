#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

#include "SDCard.h"

static const char *TAG = "SDC";

SDCard::SDCard(const char *mount_point,
               gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0)
{
    m_mount_point = mount_point;
    esp_err_t ret;

    // Mount options
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    ESP_LOGI(TAG, "Initializing SD card using SDMMC 1-bit mode");

    // Use SDMMC host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;   // Force 1-bit mode

    // Slot config
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1-bit mode
    slot_config.clk = clk;
    slot_config.cmd = cmd;
    slot_config.d0  = d0;
    slot_config.d1  = GPIO_NUM_NC; // not used
    slot_config.d2  = GPIO_NUM_NC; // not used
    slot_config.d3  = GPIO_NUM_NC; // not used
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;

    // Mount filesystem
    ret = esp_vfs_fat_sdmmc_mount(m_mount_point.c_str(),
                                  &host,
                                  &slot_config,
                                  &mount_config,
                                  &m_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "Set format_if_mount_failed = true to format.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Check pull-ups on CMD, CLK, D0.",
                     esp_err_to_name(ret));
        }
        return;
    }

    // Success — print card info
    sdmmc_card_print_info(stdout, m_card);
    mountedSuccesfully = true;
}

SDCard::~SDCard()
{
    // Unmount partition and release SDMMC
    esp_vfs_fat_sdcard_unmount(m_mount_point.c_str(), m_card);
    sdmmc_host_deinit();
    ESP_LOGI(TAG, "Card unmounted");
    mountedSuccesfully = false;
}
