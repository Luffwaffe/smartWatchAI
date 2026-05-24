#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_CENTER_TITLE_MAX_LEN 32
#define DATA_CENTER_MESSAGE_MAX_LEN 96
#define DATA_CENTER_SOURCE_MAX_LEN 24
#define DATA_CENTER_MAX_ITEMS 12
#define DATA_CENTER_TARGET_MAX_LEN 24
#define DATA_CENTER_TOPIC_MAX_LEN 32
#define DATA_CENTER_PAYLOAD_MAX_LEN 256
#define DATA_CENTER_MESSAGE_QUEUE_LEN 8
#define DATA_CENTER_EVENT_TYPE_ANY 0
#define DATA_CENTER_TARGET_BROADCAST "*"

#define DATA_CENTER_FLAG_BROADCAST (1U << 0)
#define DATA_CENTER_FLAG_RETAIN_LATEST (1U << 1)
#define DATA_CENTER_FLAG_NO_SELF_ECHO (1U << 2)

typedef enum {
    DATA_CENTER_ITEM_INFO = 0,
    DATA_CENTER_ITEM_WARNING,
    DATA_CENTER_ITEM_ACTION,
} data_center_item_type_t;

typedef struct {
    char source_app_id[DATA_CENTER_SOURCE_MAX_LEN];
    char title[DATA_CENTER_TITLE_MAX_LEN];
    char message[DATA_CENTER_MESSAGE_MAX_LEN];
    data_center_item_type_t type;
    uint32_t timestamp_ms;
    uint8_t priority;
} data_center_item_t;

typedef enum {
    DATA_CENTER_MESSAGE_KIND_INFO = 0,
    DATA_CENTER_MESSAGE_KIND_NOTIFICATION,
    DATA_CENTER_MESSAGE_KIND_COMMAND,
    DATA_CENTER_MESSAGE_KIND_TIME_SYNC,
    DATA_CENTER_MESSAGE_KIND_DEVICE_INFO,
    DATA_CENTER_MESSAGE_KIND_AI_TEXT,
    DATA_CENTER_MESSAGE_KIND_AI_AUDIO_CHUNK,
} data_center_message_kind_t;

typedef enum {
    DATA_CENTER_EVENT_GENERIC = 1,
    DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED,
    DATA_CENTER_EVENT_BLUETOOTH_COMMAND,
    DATA_CENTER_EVENT_PHONE_MESSAGE,
    DATA_CENTER_EVENT_TIME_SYNC,
    DATA_CENTER_EVENT_AI_TEXT,
    DATA_CENTER_EVENT_AI_AUDIO_CHUNK,
} data_center_event_type_t;

typedef enum {
    DATA_CENTER_BLUETOOTH_CMD_SET_ENABLED = 1,
} data_center_bluetooth_cmd_type_t;

typedef struct {
    data_center_bluetooth_cmd_type_t command;
    bool enabled;
} data_center_bluetooth_command_t;

typedef struct {
    char source[DATA_CENTER_SOURCE_MAX_LEN];
    char target[DATA_CENTER_TARGET_MAX_LEN];
    char topic[DATA_CENTER_TOPIC_MAX_LEN];
    uint16_t type;
    uint32_t flags;
    uint8_t payload[DATA_CENTER_PAYLOAD_MAX_LEN];
    size_t payload_len;
    uint32_t timestamp_ms;
    uint8_t priority;
} data_center_event_t;

typedef struct {
    char owner[DATA_CENTER_TARGET_MAX_LEN];
    char source[DATA_CENTER_SOURCE_MAX_LEN];
    char target[DATA_CENTER_TARGET_MAX_LEN];
    char topic[DATA_CENTER_TOPIC_MAX_LEN];
    uint16_t type;
    uint32_t flags;
} data_center_filter_t;

typedef struct {
    char source[DATA_CENTER_SOURCE_MAX_LEN];
    char target[DATA_CENTER_TARGET_MAX_LEN];
    data_center_message_kind_t kind;
    uint8_t payload[DATA_CENTER_PAYLOAD_MAX_LEN];
    size_t payload_len;
    uint32_t timestamp_ms;
    uint8_t priority;
} data_center_message_t;

typedef void (*data_center_event_cb_t)(const data_center_event_t *event, void *user_ctx);
typedef void (*data_center_message_cb_t)(const data_center_message_t *message, void *user_ctx);

esp_err_t data_center_init(void);
esp_err_t data_center_start(void);
esp_err_t data_center_publish(const data_center_item_t *item);
esp_err_t data_center_publish_event(const data_center_event_t *event);
esp_err_t data_center_publish_message(const data_center_message_t *message);
esp_err_t data_center_subscribe_event(const data_center_filter_t *filter, data_center_event_cb_t cb, void *user_ctx);
esp_err_t data_center_unsubscribe_event(data_center_event_cb_t cb, void *user_ctx);
esp_err_t data_center_subscribe(const char *target, data_center_message_cb_t cb, void *user_ctx);
esp_err_t data_center_unsubscribe(data_center_message_cb_t cb, void *user_ctx);
esp_err_t data_center_get_latest_event(const char *topic, uint16_t type, data_center_event_t *event);
size_t data_center_get_snapshot(data_center_item_t *items, size_t max_items);
void data_center_clear(void);
size_t data_center_count(void);

#ifdef __cplusplus
}
#endif
