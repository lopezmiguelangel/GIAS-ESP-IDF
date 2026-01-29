#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== CONFIGURACIÓN ====================
// I2S
#define SAMPLERATE 44100
#define PM_MCK 14
#define PM_WS 13  
#define PM_BCK 12
#define PM_SDO 11
#define PM_SDIN 10

// Buffers
#define MAX_CICLE_COUNT 1000
#define BUF_COUNT 16
#define BUF_LEN 512
#define I2S_BUFFERSIZE ((BUF_COUNT - 1) * BUF_LEN)
#define PSRAM_BUFFER_SIZE (MAX_CICLE_COUNT * I2S_BUFFERSIZE)

// Estados
typedef enum {
    RECORDER_STATE_IDLE,
    RECORDER_STATE_INIT_SD,
    RECORDER_STATE_RECORDING,
    RECORDER_STATE_WRITING_SD
} recorder_state_t;

// ==================== API PÚBLICA ====================
bool audio_recorder_init(void);
bool audio_recorder_start(const char* filename, uint64_t minutes);
void audio_recorder_stop(void);
void audio_recorder_deinit(void);

// Opcional: funciones para debug/monitoreo
recorder_state_t audio_recorder_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_RECORDER_H
