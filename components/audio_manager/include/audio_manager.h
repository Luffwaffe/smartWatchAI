#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_MANAGER_DEFAULT_SAMPLE_RATE 16000
#define AUDIO_MANAGER_DEFAULT_BITS_PER_SAMPLE 16
#define AUDIO_MANAGER_DEFAULT_CHANNELS 1
#define AUDIO_MANAGER_RECORD_CHUNK_MAX_LEN 512

typedef struct {
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint8_t channels;
} audio_manager_pcm_config_t;

typedef void (*audio_manager_record_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

esp_err_t audio_manager_init(void);
esp_err_t audio_manager_start(void);
esp_err_t audio_manager_set_pcm_config(const audio_manager_pcm_config_t *config);
esp_err_t audio_manager_play_pcm_chunk(const uint8_t *data, size_t len, uint32_t timeout_ms);
esp_err_t audio_manager_record_start(audio_manager_record_cb_t cb, void *user_ctx);
esp_err_t audio_manager_record_stop(void);
bool audio_manager_is_recording(void);

#ifdef __cplusplus
}
#endif