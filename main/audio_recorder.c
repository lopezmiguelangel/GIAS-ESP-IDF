// audio_recorder.c
#include "audio_recorder.h"
#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "sd_mmc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

static const char* TAG = "AUDIO_RECORDER";   // <--- TAG para logging

// ==================== GLOBAL VARIABLES ====================
static i2s_chan_handle_t tx_handle = NULL;      /**< I2S TX handle */
static i2s_chan_handle_t rx_handle = NULL;      /**< I2S RX handle */
static uint16_t* psram_buffer = NULL;           /**< PSRAM buffer for audio samples */
static uint16_t rx_buf[I2S_BUFFERSIZE];        /**< Temporary I2S buffer */

static recorder_state_t current_state = RECORDER_STATE_IDLE; /**< Recorder state */
static TaskHandle_t sd_task_handle = NULL;                  /**< SD write task handle */
static FILE* audio_file = NULL;                             /**< Current audio file */
static char current_filename[128] = {0};                    /**< Current filename */

static unsigned long i2s_write_pos = 0; /**< Write position in PSRAM buffer */
static unsigned long sd_write_pos = 0;  /**< Position for SD write */
static unsigned long cycle_count = 0;   /**< I2S/SD cycle counter */
static uint64_t time_recording = 0;     /**< Recording start time in ms */

// ==================== I2S FUNCTIONS ====================
/**
 * @brief Initialize I2S interface for TX/RX.
 * @return true if successful, false otherwise
 */
static bool init_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = BUF_COUNT,
        .dma_frame_num = BUF_LEN,
        .auto_clear_after_cb = false,
        .auto_clear_before_cb = false,
        .intr_priority = 7,
    };

    if (i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle) != ESP_OK) {
        return false;
    }

    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = SAMPLERATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    };

    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PM_MCK,
            .bclk = PM_BCK,
            .ws   = PM_WS,
            .dout = PM_SDO,
            .din  = PM_SDIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    if (i2s_channel_init_std_mode(tx_handle, &std_cfg) != ESP_OK) return false;
    if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) return false;

    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);

    return true;
}

/**
 * @brief Deinitialize I2S interface and free resources
 */
static void deinit_i2s(void)
{
    if (tx_handle) i2s_channel_disable(tx_handle);
    if (rx_handle) i2s_channel_disable(rx_handle);
    if (tx_handle) i2s_del_channel(tx_handle);
    if (rx_handle) i2s_del_channel(rx_handle);
    tx_handle = rx_handle = NULL;
}

// ==================== PSRAM FUNCTIONS ====================
/**
 * @brief Allocate PSRAM buffer for recording
 * @return true if allocation succeeded
 */
static bool init_psram(void)
{
    psram_buffer = (uint16_t*)heap_caps_malloc(PSRAM_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    return psram_buffer != NULL;
}

/**
 * @brief Free PSRAM buffer
 */
static void deinit_psram(void)
{
    if (psram_buffer) {
        free(psram_buffer);
        psram_buffer = NULL;
    }
}

// ==================== WAV HEADER FUNCTIONS ====================
/**
 * @brief Create WAV file with standard header
 * @param filename Path of WAV file
 * @return true if header creation succeeded
 */
static bool create_wav_header(const char* filename)
{
    uint8_t header[44] = {
        'R','I','F','F', 0,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0x44,0xAC,0,0, 0x88,0x58,0x01,0, 2,0, 16,0,
        'd','a','t','a', 0,0,0,0
    };

    sd_card_init();
    FILE* file = sd_card_open(filename, "wb");
    if (!file) { sd_card_deinit(); return false; }

    fwrite(header, 1, 44, file);
    fclose(file);
    sd_card_deinit();
    return true;
}

/**
 * @brief Update WAV header with final file size
 * @param filename Path of WAV file
 * @return true if update succeeded
 */
static bool update_wav_header(const char* filename)
{
    sd_card_init();
    FILE* file = sd_card_open(filename, "rb+");
    if (!file) { sd_card_deinit(); return false; }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < 44) { fclose(file); sd_card_deinit(); return false; }

    uint32_t data_size = file_size - 44;
    uint32_t riff_size = data_size + 36;

    fseek(file, 4, SEEK_SET);  fwrite(&riff_size, 1, 4, file);
    fseek(file, 40, SEEK_SET); fwrite(&data_size, 1, 4, file);

    fclose(file);
    sd_card_deinit();
    return true;
}

// ==================== SD TASKS ====================
/**
 * @brief Task to initialize SD write
 */
static void sd_init_task(void* parameter)
{
    sd_card_init();
    audio_file = sd_card_open(current_filename, "ab");
    if (!audio_file) { sd_card_deinit(); vTaskDelete(NULL); return; }

    current_state = RECORDER_STATE_RECORDING;
    sd_write_pos = 0;
    time_recording = esp_timer_get_time() / 1000;

    vTaskDelete(NULL);
}

/**
 * @brief Task to write entire PSRAM buffer to SD card and measure elapsed time
 */
static void sd_write_task(void* parameter)
{
    #define BLOCK_SD_WRITE (1024 * 3)  // 3 KB blocks like Arduino

    size_t total_written = 0;

    ESP_LOGI(TAG, "Starting SD write of PSRAM buffer (%u bytes)...", PSRAM_BUFFER_SIZE);

    while (sd_write_pos < PSRAM_BUFFER_SIZE) {
        size_t available_bytes = PSRAM_BUFFER_SIZE - sd_write_pos;
        size_t bytes_to_write = (available_bytes < BLOCK_SD_WRITE) ? available_bytes : BLOCK_SD_WRITE;

        size_t written = fwrite((uint8_t*)psram_buffer + sd_write_pos, 1, bytes_to_write, audio_file);
        if (written != bytes_to_write) {
            ESP_LOGE(TAG, "SD write error: expected %u, wrote %u", (unsigned)bytes_to_write, (unsigned)written);
            break;
        }

        sd_write_pos += written;
        total_written += written;
    }

    fflush(audio_file);
    fclose(audio_file);
    sd_card_deinit();
    ESP_LOGI(TAG, "SD unmounted");

    // Reset state and positions (same as Arduino)
    current_state = RECORDER_STATE_IDLE;
    i2s_write_pos = 0;
    sd_write_pos = 0;
    cycle_count = 0;

    // Time taken to write PSRAM to SD
    uint64_t write_time = (esp_timer_get_time() / 1000) - time_recording;
    ESP_LOGI(TAG, "Finished SD write: %u bytes in %.2f seconds", (unsigned)total_written, write_time / 1000.0);

    vTaskDelete(NULL);
}

// ==================== AUDIO LOGIC ====================
/**
 * @brief Read samples from I2S into PSRAM
 */
static void I2S_read(void)
{
    size_t readsize = 0, written = 0;
    i2s_channel_read(rx_handle, rx_buf, sizeof(rx_buf), &readsize, 1000);
    i2s_channel_write(tx_handle, rx_buf, readsize, &written, 100);

    for (size_t i = 0; i < readsize / 2; i += 2) {
        psram_buffer[i2s_write_pos] = rx_buf[i];
        if (++i2s_write_pos == PSRAM_BUFFER_SIZE / sizeof(uint16_t)) i2s_write_pos = 0;
    }
}

/**
 * @brief Handle SD write based on recorder state
 */
static void SD_write(void)
{
    switch (current_state) {
        case RECORDER_STATE_IDLE:
            if (cycle_count == (MAX_CICLE_COUNT - 10)) {
                xTaskCreatePinnedToCore(sd_init_task, "sd_init_task", 10000, NULL, 1, &sd_task_handle, 1);
            }
            break;

        case RECORDER_STATE_RECORDING:
            xTaskCreatePinnedToCore(sd_write_task, "sd_write_task", 10000, NULL, 1, &sd_task_handle, 1);
            current_state = RECORDER_STATE_IDLE;
            break;

        default: break;
    }
}

// ==================== PUBLIC API ====================
/**
 * @brief Initialize audio recorder
 * @return true on success
 */
bool audio_recorder_init(void)
{
    if (!init_psram()) return false;
    if (!init_i2s()) { deinit_psram(); return false; }

    i2s_write_pos = sd_write_pos = cycle_count = 0;
    current_state = RECORDER_STATE_IDLE;
    current_filename[0] = '\0';
    return true;
}

/**
 * @brief Start recording audio to file
 * @param filename Output WAV filename
 * @param minutes Duration in minutes
 * @return true on success
 */
bool audio_recorder_start(const char* filename, uint64_t minutes)
{
    if (!filename || minutes == 0) return false;

    strncpy(current_filename, filename, sizeof(current_filename) - 1);
    current_filename[sizeof(current_filename)-1] = '\0';

    if (!create_wav_header(filename)) return false;

    current_state = RECORDER_STATE_IDLE;
    cycle_count = i2s_write_pos = sd_write_pos = 0;

    uint64_t start_time = esp_timer_get_time() / 1000;
    uint64_t duration_ms = minutes * 60 * 1000;

    while ((esp_timer_get_time() / 1000 - start_time) < duration_ms) {
        I2S_read();
        SD_write();
        cycle_count++;
    }

    if (i2s_write_pos > 0 && current_state == RECORDER_STATE_RECORDING) {
        sd_write_task(NULL);
    }

    update_wav_header(filename);
    return true;
}

/**
 * @brief Stop recording
 */
void audio_recorder_stop(void)
{
    current_state = RECORDER_STATE_IDLE;
    if (sd_task_handle) vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief Deinitialize recorder, free resources
 */
void audio_recorder_deinit(void)
{
    audio_recorder_stop();
    deinit_i2s();
    deinit_psram();
}
