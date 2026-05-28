#include "audio_manager.h"

#include <string.h>

#include "bsp_board_extra.h"
#include "data_center.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define AUDIO_MANAGER_QUEUE_LEN 12
#define AUDIO_MANAGER_TASK_STACK_SIZE 4096
#define AUDIO_MANAGER_TASK_PRIORITY 5
#define AUDIO_MANAGER_PLAY_TIMEOUT_MS 100
#define AUDIO_MANAGER_RECORD_READ_MS 20

static const char *TAG = "audio_manager";

typedef enum {
    AUDIO_MANAGER_CMD_PLAY_CHUNK = 1,
    AUDIO_MANAGER_CMD_START_RECORD,
    AUDIO_MANAGER_CMD_STOP_RECORD,
} audio_manager_cmd_type_t;

typedef struct {
    audio_manager_cmd_type_t type;
    union {
        struct {
            uint8_t data[DATA_CENTER_PAYLOAD_MAX_LEN];
            size_t len;
        } play_chunk;
        struct {
            audio_manager_record_cb_t cb;
            void *user_ctx;
        } record;
    } data;
} audio_manager_cmd_t;

static QueueHandle_t s_cmd_queue;
static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_recording;
static audio_manager_record_cb_t s_record_cb;
static void *s_record_user_ctx;
static audio_manager_pcm_config_t s_pcm_config = {
    .sample_rate = AUDIO_MANAGER_DEFAULT_SAMPLE_RATE,
    .bits_per_sample = AUDIO_MANAGER_DEFAULT_BITS_PER_SAMPLE,
    .channels = AUDIO_MANAGER_DEFAULT_CHANNELS,
};

static i2s_slot_mode_t audio_manager_slot_mode_from_channels(uint8_t channels)
{
    return channels == 2 ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
}

static size_t audio_manager_frame_size(const audio_manager_pcm_config_t *config)
{
    return ((size_t)config->bits_per_sample / 8U) * (size_t)config->channels;
}

static bool audio_manager_config_valid(const audio_manager_pcm_config_t *config)
{
    return config &&
           (config->bits_per_sample == 16 || config->bits_per_sample == 24 || config->bits_per_sample == 32) &&
           (config->channels == 1 || config->channels == 2) &&
           config->sample_rate > 0;
}

static esp_err_t audio_manager_apply_pcm_config_locked(void)
{
    return bsp_extra_codec_set_fs(s_pcm_config.sample_rate,
                                  s_pcm_config.bits_per_sample,
                                  audio_manager_slot_mode_from_channels(s_pcm_config.channels));
}

static esp_err_t audio_manager_write_pcm_locked(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t frame_size = audio_manager_frame_size(&s_pcm_config);
    if (frame_size == 0 || (len % frame_size) != 0) {
        ESP_LOGW(TAG, "Drop PCM chunk: len=%u frame_size=%u", (unsigned)len, (unsigned)frame_size);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t written = 0;
    esp_err_t ret = bsp_extra_i2s_write((void *)data, len, &written, timeout_ms);
    if (ret != ESP_OK || written != len) {
        ESP_LOGW(TAG, "PCM write incomplete ret=%s written=%u/%u", esp_err_to_name(ret), (unsigned)written, (unsigned)len);
    }
    return ret;
}

static void audio_manager_data_center_event_cb(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event || event->type != DATA_CENTER_EVENT_AI_AUDIO_CHUNK || event->payload_len == 0) {
        return;
    }

    audio_manager_cmd_t cmd = {0};
    cmd.type = AUDIO_MANAGER_CMD_PLAY_CHUNK;
    cmd.data.play_chunk.len = event->payload_len < sizeof(cmd.data.play_chunk.data)
                              ? event->payload_len
                              : sizeof(cmd.data.play_chunk.data);
    memcpy(cmd.data.play_chunk.data, event->payload, cmd.data.play_chunk.len);

    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Audio queue full, drop AI audio chunk");
    }
}

static void audio_manager_process_recording(void)
{
    uint8_t buffer[AUDIO_MANAGER_RECORD_CHUNK_MAX_LEN];
    size_t bytes_read = 0;

    esp_err_t ret = bsp_extra_i2s_read(buffer, sizeof(buffer), &bytes_read, AUDIO_MANAGER_RECORD_READ_MS);
    if (ret == ESP_OK && bytes_read > 0 && s_record_cb) {
        s_record_cb(buffer, bytes_read, s_record_user_ctx);
    }
}

static void audio_manager_task(void *arg)
{
    (void)arg;
    audio_manager_cmd_t cmd;

    while (true) {
        TickType_t wait_ticks = s_recording ? pdMS_TO_TICKS(AUDIO_MANAGER_RECORD_READ_MS) : portMAX_DELAY;
        if (xQueueReceive(s_cmd_queue, &cmd, wait_ticks) == pdTRUE) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            switch (cmd.type) {
            case AUDIO_MANAGER_CMD_PLAY_CHUNK:
                audio_manager_write_pcm_locked(cmd.data.play_chunk.data, cmd.data.play_chunk.len, AUDIO_MANAGER_PLAY_TIMEOUT_MS);
                break;
            case AUDIO_MANAGER_CMD_START_RECORD:
                s_record_cb = cmd.data.record.cb;
                s_record_user_ctx = cmd.data.record.user_ctx;
                s_recording = true;
                break;
            case AUDIO_MANAGER_CMD_STOP_RECORD:
                s_recording = false;
                s_record_cb = NULL;
                s_record_user_ctx = NULL;
                break;
            default:
                break;
            }
            xSemaphoreGive(s_lock);
        }

        if (s_recording) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            audio_manager_process_recording();
            xSemaphoreGive(s_lock);
        }
    }
}

esp_err_t audio_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "Failed to create lock");

    s_cmd_queue = xQueueCreate(AUDIO_MANAGER_QUEUE_LEN, sizeof(audio_manager_cmd_t));
    if (!s_cmd_queue) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "Failed to init codec");
    ESP_RETURN_ON_ERROR(audio_manager_apply_pcm_config_locked(), TAG, "Failed to configure codec PCM format");
    ESP_RETURN_ON_ERROR(data_center_subscribe_event(&(data_center_filter_t){
                            .owner = "audio_manager",
                            .target = DATA_CENTER_TARGET_BROADCAST,
                            .type = DATA_CENTER_EVENT_AI_AUDIO_CHUNK,
                        }, audio_manager_data_center_event_cb, NULL),
                        TAG, "Failed to subscribe AI audio chunks");

    s_initialized = true;
    ESP_LOGI(TAG, "Audio manager initialized: %lu Hz, %u-bit, %u ch",
             (unsigned long)s_pcm_config.sample_rate, s_pcm_config.bits_per_sample, s_pcm_config.channels);
    return ESP_OK;
}

esp_err_t audio_manager_start(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "audio_manager_init first");
    if (s_task_handle) {
        return ESP_OK;
    }
    BaseType_t ok = xTaskCreate(audio_manager_task, "audio_mgr", AUDIO_MANAGER_TASK_STACK_SIZE, NULL,
                                AUDIO_MANAGER_TASK_PRIORITY, &s_task_handle);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t audio_manager_set_pcm_config(const audio_manager_pcm_config_t *config)
{
    ESP_RETURN_ON_FALSE(audio_manager_config_valid(config), ESP_ERR_INVALID_ARG, TAG, "Invalid PCM config");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "audio_manager_init first");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pcm_config = *config;
    esp_err_t ret = audio_manager_apply_pcm_config_locked();
    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t audio_manager_play_pcm_chunk(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "audio_manager_init first");
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t ret = audio_manager_write_pcm_locked(data, len, timeout_ms);
    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t audio_manager_record_start(audio_manager_record_cb_t cb, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(s_initialized && s_cmd_queue, ESP_ERR_INVALID_STATE, TAG, "audio_manager_init first");
    ESP_RETURN_ON_FALSE(cb, ESP_ERR_INVALID_ARG, TAG, "record callback is required");

    audio_manager_cmd_t cmd = {
        .type = AUDIO_MANAGER_CMD_START_RECORD,
        .data.record = {
            .cb = cb,
            .user_ctx = user_ctx,
        },
    };
    return xQueueSend(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t audio_manager_record_stop(void)
{
    ESP_RETURN_ON_FALSE(s_initialized && s_cmd_queue, ESP_ERR_INVALID_STATE, TAG, "audio_manager_init first");
    audio_manager_cmd_t cmd = {.type = AUDIO_MANAGER_CMD_STOP_RECORD};
    return xQueueSend(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

bool audio_manager_is_recording(void)
{
    return s_recording;
}