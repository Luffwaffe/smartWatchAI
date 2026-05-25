#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLUETOOTH_MANAGER_DEVICE_NAME_MAX_LEN 64
#define BLUETOOTH_MANAGER_DEVICE_ADDR_MAX_LEN 32
#define BLUETOOTH_MANAGER_MESSAGE_MAX_LEN 512

typedef enum {
    BLUETOOTH_MANAGER_STATE_OFF = 0,
    BLUETOOTH_MANAGER_STATE_TURNING_ON,
    BLUETOOTH_MANAGER_STATE_ON,
    BLUETOOTH_MANAGER_STATE_CONNECTED,
    BLUETOOTH_MANAGER_STATE_TURNING_OFF,
    BLUETOOTH_MANAGER_STATE_ERROR,
} bluetooth_manager_state_t;

typedef enum {
    BLUETOOTH_MANAGER_MESSAGE_UNKNOWN = 0,
    BLUETOOTH_MANAGER_MESSAGE_NOTIFICATION,
    BLUETOOTH_MANAGER_MESSAGE_COMMAND,
    BLUETOOTH_MANAGER_MESSAGE_TIME_SYNC,
    BLUETOOTH_MANAGER_MESSAGE_DEVICE_INFO,
    BLUETOOTH_MANAGER_MESSAGE_AI_TEXT,
    BLUETOOTH_MANAGER_MESSAGE_AI_AUDIO_CHUNK,
} bluetooth_manager_message_type_t;

typedef enum {
    BLUETOOTH_MANAGER_EVT_STATUS_CHANGED = 1,
    BLUETOOTH_MANAGER_EVT_MESSAGE_RECEIVED,
    BLUETOOTH_MANAGER_EVT_MESSAGE_SENT,
    BLUETOOTH_MANAGER_EVT_ERROR,
} bluetooth_manager_event_type_t;

typedef enum {
    BLUETOOTH_MANAGER_MEDIA_KEY_PLAY_PAUSE = 0,
    BLUETOOTH_MANAGER_MEDIA_KEY_NEXT_TRACK,
    BLUETOOTH_MANAGER_MEDIA_KEY_PREVIOUS_TRACK,
    BLUETOOTH_MANAGER_MEDIA_KEY_VOLUME_UP,
    BLUETOOTH_MANAGER_MEDIA_KEY_VOLUME_DOWN,
    BLUETOOTH_MANAGER_MEDIA_KEY_MUTE,
} bluetooth_manager_media_key_t;

typedef struct {
    bool enabled;
    bluetooth_manager_state_t state;
    char connected_name[BLUETOOTH_MANAGER_DEVICE_NAME_MAX_LEN];
    char connected_address[BLUETOOTH_MANAGER_DEVICE_ADDR_MAX_LEN];
    int last_error;
} bluetooth_manager_status_t;

typedef struct {
    bluetooth_manager_message_type_t type;
    char target[24];
    uint8_t data[BLUETOOTH_MANAGER_MESSAGE_MAX_LEN];
    size_t data_len;
} bluetooth_manager_message_t;

typedef struct {
    bluetooth_manager_event_type_t type;
    union {
        bluetooth_manager_status_t status;
        bluetooth_manager_message_t message;
        int error_code;
    } data;
} bluetooth_manager_event_t;

typedef void (*bluetooth_manager_event_cb_t)(const bluetooth_manager_event_t *event, void *user_ctx);

esp_err_t bluetooth_manager_init(void);
esp_err_t bluetooth_manager_init_ble_stack(void);
esp_err_t bluetooth_manager_start(void);
esp_err_t bluetooth_manager_stop(void);

esp_err_t bluetooth_manager_set_enabled(bool enabled);
esp_err_t bluetooth_manager_get_status(bluetooth_manager_status_t *status);

esp_err_t bluetooth_manager_send_message(const bluetooth_manager_message_t *message);
esp_err_t bluetooth_manager_send_raw(const uint8_t *data, size_t len);
esp_err_t bluetooth_manager_send_media_key(bluetooth_manager_media_key_t key);
esp_err_t bluetooth_manager_on_raw_received(const uint8_t *data, size_t len);
esp_err_t bluetooth_manager_mock_connect_phone(const char *name, const char *address);

esp_err_t bluetooth_manager_register_callback(bluetooth_manager_event_cb_t cb, void *user_ctx);
esp_err_t bluetooth_manager_unregister_callback(bluetooth_manager_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif