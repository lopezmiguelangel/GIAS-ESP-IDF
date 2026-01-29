// calendar.c
#include "calendar.h"
#include "sd_mmc.h"
#include "audio_recorder.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char* TAG = "CALENDAR";

/** Internal structure for calendar data */
typedef struct {
    int schedule[HOURS_IN_DAY][DAYS_IN_WEEK]; /**< Schedule matrix [hour][day] */
    bool file_exists;                         /**< True if Calendar.csv exists */
    bool all_ones;                            /**< True if all values are RECORD_MODE */
} calendar_internal_t;

static calendar_internal_t g_calendar;

/**
 * @brief Create a default calendar file on the SD card.
 *
 * @param filename Name of the CSV file to create
 * @return true on success, false on failure
 */
static bool create_default_calendar(const char* filename)
{
    ESP_LOGI(TAG, "Creating default Calendar.csv...");

    FILE* file = sd_card_open(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "Error creating %s", filename);
        return false;
    }

    fprintf(file, "hour;sunday;monday;tuesday;wednesday;thursday;friday;saturday\n");
    for (int hour = 0; hour < HOURS_IN_DAY; hour++) {
        fprintf(file, "%d", hour);
        for (int day = 0; day < DAYS_IN_WEEK; day++) {
            fprintf(file, ";0");
        }
        fprintf(file, "\n");
    }

    fclose(file);
    ESP_LOGI(TAG, "Calendar.csv created successfully");
    return true;
}

/**
 * @brief Determine the number of minutes until the next schedule change.
 *
 * @param current_hour Current hour
 * @param current_day Current day of the week
 * @param current_value Current schedule value
 * @return Minutes until next change (or 0 if change is immediate)
 */
static uint64_t get_next_change_time(int current_hour, int current_day, int current_value)
{
    int hour = current_hour;
    int day = current_day;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int minutes_passed = 60 - timeinfo.tm_min;

    if (!g_calendar.file_exists || g_calendar.schedule[hour][day] != current_value) {
        return 0;
    }

    hour++;
    if (hour == HOURS_IN_DAY) { hour = 0; day = (day + 1) % DAYS_IN_WEEK; }

    int hours_checked = 1;

    while (hours_checked < HOURS_IN_DAY * DAYS_IN_WEEK) {
        if (g_calendar.schedule[hour][day] != current_value) {
            return minutes_passed;
        }

        hour++;
        if (hour == HOURS_IN_DAY) { hour = 0; day = (day + 1) % DAYS_IN_WEEK; }
        minutes_passed += 60;
        hours_checked++;
    }

    if (current_value == RECORD_MODE && g_calendar.all_ones) {
        return 0;
    }

    ESP_LOGW(TAG, "No change found, defaulting to 60 minutes");
    return 60;
}

/**
 * @brief Enter deep sleep for a specified duration.
 *
 * @param minutes Minutes to sleep
 */
static void enter_deep_sleep(uint64_t minutes)
{
    ESP_LOGI(TAG, "Entering deep sleep for %llu minutes...", minutes);

    sd_card_deinit();
    audio_recorder_deinit();

    esp_sleep_enable_timer_wakeup(minutes * 60 * 1000000ULL);
    ESP_LOGI(TAG, "Sleeping now...");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}

/**
 * @brief Generate a timestamped filename for recordings.
 *
 * @param buffer Buffer to store the filename
 * @param size Buffer size
 */
static void generate_filename(char* buffer, size_t size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    snprintf(buffer, size, "/%04d%02d%02d_%02d-%02d-%02d.wav",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

/**
 * @brief Execute a recording session.
 *
 * @param filename Name of the output WAV file
 * @param minutes Duration in minutes
 * @param continuous_mode True for continuous recording
 * @return true on success, false on failure
 */
static bool execute_recording_session(const char* filename, uint64_t minutes, bool continuous_mode)
{
    ESP_LOGI(TAG, "\n=== STARTING RECORDING SESSION ===");
    ESP_LOGI(TAG, "File: %s", filename);
    ESP_LOGI(TAG, "Duration: %llu minutes", minutes);
    ESP_LOGI(TAG, "Mode: %s", continuous_mode ? "CONTINUOUS" : "NORMAL");

    if (!audio_recorder_init()) {
        ESP_LOGE(TAG, "Failed to initialize recorder");
        return false;
    }

    uint64_t session_start_time = esp_timer_get_time() / 1000;
    bool success = audio_recorder_start(filename, minutes);
    uint64_t session_end_time = esp_timer_get_time() / 1000;

    audio_recorder_deinit();

    ESP_LOGI(TAG, "=== SESSION STATISTICS ===");
    ESP_LOGI(TAG, "Scheduled duration: %llu minutes (%llu ms)", minutes, minutes * 60 * 1000);
    ESP_LOGI(TAG, "Actual session time: %llu ms (%.2f minutes)",
             session_end_time - session_start_time,
             (session_end_time - session_start_time) / 60000.0);

    return success;
}

/**
 * @brief Check the recording calendar and execute scheduled recordings.
 *
 * Loads Calendar.csv, determines current schedule, and either starts
 * a recording session or enters deep sleep until the next scheduled change.
 */
void check_calendar(void)
{
    const char* filename = "/Calendar.csv";
    sd_card_init();

    // ------------------- Create calendar if it does not exist -------------------
    if (!sd_card_exists(filename)) {
        ESP_LOGI(TAG, "Calendar.csv does not exist, creating default...");
        if (!create_default_calendar(filename)) {
            ESP_LOGE(TAG, "Failed to create Calendar.csv");
            sd_card_deinit();
            while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
        }
    }

    // ------------------- Initialize internal calendar structure -------------------
    memset(&g_calendar, 0, sizeof(g_calendar));
    g_calendar.all_ones = true;
    g_calendar.file_exists = false;

    FILE* file = sd_card_open(filename, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open %s", filename);
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    char line[256];
    fgets(line, sizeof(line), file); // Skip header line

    int hour = 0;
    while (hour < HOURS_IN_DAY && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        int values[8];
        if (sscanf(line, "%d;%d;%d;%d;%d;%d;%d;%d",
                   &values[0], &values[1], &values[2], &values[3],
                   &values[4], &values[5], &values[6], &values[7]) == 8 && values[0] == hour) {
            for (int day = 0; day < DAYS_IN_WEEK; day++) {
                g_calendar.schedule[hour][day] = values[day + 1];
                if (values[day + 1] != RECORD_MODE) g_calendar.all_ones = false;
            }
            hour++;
        }
    }
    fclose(file);
    g_calendar.file_exists = true;

    // ------------------- Get current time -------------------
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int current_hour = timeinfo.tm_hour;
    int current_day  = timeinfo.tm_wday;
    int current_value = (g_calendar.file_exists && current_hour < HOURS_IN_DAY && current_day < DAYS_IN_WEEK) ?
                        g_calendar.schedule[current_hour][current_day] : 0;

    // ------------------- Calculate minutes until next schedule change -------------------
    uint64_t next_change = get_next_change_time(current_hour, current_day, current_value);

    // ------------------- LOG: Current time and next scheduled change -------------------
    ESP_LOGI(TAG, "Current time: %02d:%02d:%02d %02d/%02d/%04d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    if (next_change > 0) {
        time_t next_time = now + next_change * 60; // convert minutes to seconds
        struct tm next_tm;
        localtime_r(&next_time, &next_tm);
        ESP_LOGI(TAG, "Next recording change in %llu minutes -> %02d:%02d:%02d %02d/%02d/%04d",
                 next_change,
                 next_tm.tm_hour, next_tm.tm_min, next_tm.tm_sec,
                 next_tm.tm_mday, next_tm.tm_mon + 1, next_tm.tm_year + 1900);
    } else {
        ESP_LOGI(TAG, "Next recording change is immediate");
    }

    sd_card_deinit();

    // ------------------- Execute recording or enter deep sleep -------------------
    if (current_value == RECORD_MODE) {
        char wav_filename[64];
        generate_filename(wav_filename, sizeof(wav_filename));
        if (next_change == 0) {
            // Continuous recording
            while (1) {
                if (!execute_recording_session(wav_filename, 60, true)) {
                    ESP_LOGE(TAG, "Continuous recording failed");
                    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
                }
                generate_filename(wav_filename, sizeof(wav_filename));
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            // Scheduled recording
            if (!execute_recording_session(wav_filename, next_change, false)) {
                ESP_LOGE(TAG, "Recording failed");
                while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
            }
            enter_deep_sleep(60); // Sleep 60 minutes
        }
    } else {
        // Not scheduled for recording, sleep until next change
        enter_deep_sleep(next_change);
    }
}
