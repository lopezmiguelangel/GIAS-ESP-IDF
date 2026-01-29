#include "rtc_updater.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "RTC";

/**
 * @brief Synchronize time via NTP.
 */
static bool sync_time_via_ntp(int gmt_offset_hours)
{
    ESP_LOGI(TAG, "Synchronizing time via NTP...");

    // Configure timezone
    char tz[32];
    if (gmt_offset_hours < 0) {
        snprintf(tz, sizeof(tz), "GMT+%d", -gmt_offset_hours);
    } else {
        snprintf(tz, sizeof(tz), "GMT-%d", gmt_offset_hours);
    }
    setenv("TZ", tz, 1);
    tzset();

    // Configure NTP servers
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.windows.com");
    esp_sntp_setservername(3, "time.nist.gov");
    esp_sntp_init();

    // Check WiFi IP
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info.ip));
        }
    }

    // Wait up to 15 seconds for time sync
    for (int i = 0; i < 30; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year > (2020 - 1900)) {
            ESP_LOGI(TAG, "Time synchronized: %02d:%02d:%02d %02d/%02d/%04d",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

            esp_sntp_stop();
            return true;
        }

        if (i % 4 == 0) {
            ESP_LOGI(TAG, "Waiting for NTP... (%d/%d seconds)", i / 2, 15);
        }
    }

    ESP_LOGE(TAG, "NTP sync timeout after 15 seconds");
    esp_sntp_stop();
    return false;
}

/**
 * @brief Disconnect and clean up WiFi.
 */
static void wifi_cleanup(void)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * @brief Connect to WiFi with retries.
 */
static bool wifi_connect_with_retry(const wifi_credentials_t *creds, int max_retries)
{
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password) - 1);

    for (int attempt = 1; attempt <= max_retries; attempt++) {
        ESP_LOGI(TAG, "WiFi: Attempt %d/%d connecting to %s...", attempt, max_retries, creds->ssid);

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_start();
        esp_wifi_connect();

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        for (int i = 0; i < 100; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));

            if (netif) {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                    ESP_LOGI(TAG, "WiFi connected (attempt %d)", attempt);
                    return true;
                }
            }

            if (i % 20 == 0 && i > 0) {
                ESP_LOGI(TAG, "WiFi: Waiting for connection... %d seconds", i / 10);
            }
        }

        ESP_LOGW(TAG, "WiFi: Attempt %d failed", attempt);
        wifi_cleanup();

        if (attempt < max_retries) {
            ESP_LOGI(TAG, "WiFi: Retrying in 2 seconds...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    return false;
}

/**
 * @brief Update RTC via WiFi and NTP.
 */
bool update_rtc_via_wifi(const wifi_credentials_t *creds)
{
    if (!creds || strlen(creds->ssid) == 0 || strlen(creds->password) == 0) return false;

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    bool connected = wifi_connect_with_retry(creds, 4);
    bool time_synced = false;

    if (connected) {
        time_synced = sync_time_via_ntp(creds->gmt_hours);
    } else {
        ESP_LOGE(TAG, "WiFi connection failed after 4 attempts");
    }

    wifi_cleanup();
    esp_netif_deinit();

    return time_synced;
}
