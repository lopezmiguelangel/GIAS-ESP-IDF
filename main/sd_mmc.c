#include "sd_mmc.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rtc_updater.h"

static const char* TAG = "SD";
static sdmmc_card_t* card = NULL;
static const char* base_path = "/sdcard";

// Board-specific pin definitions
#define MMC_CLK  7
#define MMC_CMD  6
#define MMC_D0   15
#define MMC_D1   16
#define MMC_D2   4
#define MMC_D3   5

/**
 * @brief Initialize and mount the SD/MMC card.
 *
 * Configures SDMMC pins, slot, clock speed, and mounts the card
 * using FAT filesystem under /sdcard. Logs errors if mounting fails.
 */
void sd_card_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card...");
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 1,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = MMC_CLK;
    slot_config.cmd = MMC_CMD;
    slot_config.d0 = MMC_D0;
    slot_config.d1 = MMC_D1;
    slot_config.d2 = MMC_D2;
    slot_config.d3 = MMC_D3;
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot_config,
                                   &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SD mounted at %s", base_path);
}

/**
 * @brief Unmount the SD card and free allocated resources.
 *
 * Logs unmount operation.
 */
void sd_card_deinit(void)
{
    if (card) {
        esp_vfs_fat_sdcard_unmount(base_path, card);
        card = NULL;
        ESP_LOGI(TAG, "SD unmounted");
    }
}

/**
 * @brief Check if a file exists on the SD card.
 *
 * @param path Relative path of the file (from SD root).
 * @return true if the file exists, false otherwise.
 */
bool sd_card_exists(const char* path)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", base_path, path);
    FILE* f = fopen(full_path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

/**
 * @brief Open a file on the SD card with specified mode.
 *
 * @param path Relative path of the file (from SD root).
 * @param mode fopen mode string ("r", "w", "a", etc.)
 * @return FILE* pointer if successful, NULL on failure.
 */
FILE* sd_card_open(const char* path, const char* mode)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", base_path, path);
    return fopen(full_path, mode);
}

/**
 * @brief Close a previously opened file on the SD card.
 *
 * @param file FILE* pointer returned by sd_card_open().
 */
void sd_card_close(FILE* file)
{
    if (file) { fclose(file); }
}

/**
 * @brief Create a default configuration file on the SD card.
 *
 * Writes default WiFi SSID, password, and GMT offset.
 *
 * @param path Relative path for the config file.
 * @return true if creation was successful, false on error.
 */
bool create_config_file(const char* path)
{
    ESP_LOGW(TAG, "%s not found. Creating default configuration...", path);

    FILE* file = sd_card_open(path, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create %s", path);
        return false;
    }

    ESP_LOGI(TAG, "Writing default configuration to %s", path);
    fprintf(file, "ssid\n");
    fprintf(file, "YOUR_SSID\n");
    fprintf(file, "password\n");
    fprintf(file, "YOUR_SSID_PASSWORD\n");
    fprintf(file, "GMT\n");
    fprintf(file, "-3\n");

    fclose(file);
    return true;
}

/**
 * @brief Read WiFi credentials and GMT offset from configuration file.
 *
 * Parses "config.txt" and fills wifi_config_t struct.
 *
 * @param path Relative path to the config file.
 * @param config Pointer to wifi_config_t struct to fill.
 * @return true if read was successful, false on error.
 */
bool read_config_file(const char* path, wifi_config_t* config)
{
    memset(config, 0, sizeof(wifi_config_t));

    FILE* file = sd_card_open(path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open config file %s", path);
        return false;
    }

    char line[128];
    char current_key[32] = "";

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (strcmp(line, "ssid") == 0 ||
            strcmp(line, "password") == 0 ||
            strcmp(line, "GMT") == 0) {
            strcpy(current_key, line);
            continue;
        }

        if (strlen(current_key) > 0) {
            if (strcmp(current_key, "ssid") == 0) {
                strncpy(config->ssid, line, sizeof(config->ssid) - 1);
            } else if (strcmp(current_key, "password") == 0) {
                strncpy(config->password, line, sizeof(config->password) - 1);
            } else if (strcmp(current_key, "GMT") == 0) {
                config->gmt_offset_hours = atoi(line);
            }
            current_key[0] = '\0';
        }
    }

    fclose(file);
    ESP_LOGI(TAG, "Config file %s read successfully", path);
    return true;
}
