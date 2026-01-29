#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "soc/rtc.h"
#include "led_control.h"
#include "nvs_flash.h"
#include "sd_mmc.h"
#include "calendar.h"
#include "rtc_updater.h"
#include "esp_log.h"

static const char *TAG = "GIAS";  // Log tag

/**
 * @brief Print CPU information via ESP log.
 */
static void print_cpu_info(void)
{
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    ESP_LOGI(TAG, "CPU Frequency: %lu MHz", conf.freq_mhz);
}

/**
 * @brief Initialize NVS (Non-Volatile Storage) for WiFi and RTC data.
 */
static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");
}

/**
 * @brief Ensure configuration file exists, create default if missing.
 * @param path Path to the config file
 * @return true if file exists, false if it was created
 */
static bool ensure_config_file(const char *path)
{
    if (!sd_card_exists(path)) {
        create_config_file(path);
        return false;
    }
    return true;
}

/**
 * @brief Check SD card configuration and update RTC via WiFi if available.
 */
void check_configuration(void)
{
    sd_card_init();

    // Ensure configuration file exists
    if (!ensure_config_file("/config.txt")) {
        sd_card_deinit();
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    wifi_config_t config;
    if (!read_config_file("/config.txt", &config)) {
        ESP_LOGE(TAG, "Failed to read /config.txt");
        sd_card_deinit();
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // Check for default/invalid credentials
    if (strcmp(config.ssid, "YOUR_SSID") == 0 || strcmp(config.password, "YOUR_SSID_PASSWORD") == 0) {
        ESP_LOGE(TAG, "SSID or password not set. Halting...");
        sd_card_deinit();
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // Prepare WiFi credentials and update RTC
    wifi_credentials_t creds = {.gmt_hours = config.gmt_offset_hours};
    strcpy(creds.ssid, config.ssid);
    strcpy(creds.password, config.password);
    update_rtc_via_wifi(&creds);

    sd_card_deinit();
}

/**
 * @brief Main function for the GIAS application.
 */
void gias(void)
{
    print_cpu_info();  // Debug CPU frequency
    led_init();        // Initialize LEDs
    init_nvs();        // Initialize NVS (WiFi and RTC)

    check_configuration(); // Update RTC via WiFi if needed
    check_calendar();      // Load and verify recording schedule
}
