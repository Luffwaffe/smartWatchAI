#include "data_center.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

#define DATA_CENTER_MAX_SUBSCRIBERS 8
#define DATA_CENTER_MAX_RETAINED_EVENTS 12
#define DATA_CENTER_TASK_STACK_SIZE 4096
#define DATA_CENTER_TASK_PRIORITY 4

static const char *TAG = "data_center";

static data_center_item_t s_items[DATA_CENTER_MAX_ITEMS];
static size_t s_item_count = 0;
static QueueHandle_t s_event_queue;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task_handle;
static bool s_initialized;
static bool s_started;

typedef struct {
    bool used;
    data_center_filter_t filter;
    data_center_event_cb_t cb;
    void *user_ctx;
} data_center_subscriber_t;

static data_center_subscriber_t s_subscribers[DATA_CENTER_MAX_SUBSCRIBERS];
static data_center_event_t s_retained_events[DATA_CENTER_MAX_RETAINED_EVENTS];
static bool s_retained_used[DATA_CENTER_MAX_RETAINED_EVENTS];

static void data_center_copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static esp_err_t data_center_store_item(const data_center_item_t *item)
{
    if (!item) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_item_count == DATA_CENTER_MAX_ITEMS) {
        memmove(&s_items[0], &s_items[1], sizeof(s_items[0]) * (DATA_CENTER_MAX_ITEMS - 1));
        s_item_count--;
    }

    data_center_item_t *slot = &s_items[s_item_count++];
    memset(slot, 0, sizeof(*slot));
    data_center_copy_string(slot->source_app_id, sizeof(slot->source_app_id), item->source_app_id);
    data_center_copy_string(slot->title, sizeof(slot->title), item->title);
    data_center_copy_string(slot->message, sizeof(slot->message), item->message);
    slot->type = item->type;
    slot->timestamp_ms = item->timestamp_ms;
    slot->priority = item->priority;

    xSemaphoreGive(s_lock);
    return ESP_OK;
}

static bool data_center_string_matches(const char *filter, const char *value)
{
    return !filter || filter[0] == '\0' || strcmp(filter, value ? value : "") == 0;
}

static bool data_center_event_is_broadcast(const data_center_event_t *event)
{
    return (event->flags & DATA_CENTER_FLAG_BROADCAST) ||
           event->target[0] == '\0' ||
           strcmp(event->target, DATA_CENTER_TARGET_BROADCAST) == 0;
}

static bool data_center_filter_matches(const data_center_filter_t *filter, const data_center_event_t *event)
{
    if (!filter || !event) {
        return false;
    }

    if (!data_center_string_matches(filter->source, event->source)) {
        return false;
    }
    if (!data_center_string_matches(filter->topic, event->topic)) {
        return false;
    }
    if (filter->type != DATA_CENTER_EVENT_TYPE_ANY && filter->type != event->type) {
        return false;
    }
    if ((event->flags & DATA_CENTER_FLAG_NO_SELF_ECHO) && filter->owner[0] && strcmp(filter->owner, event->source) == 0) {
        return false;
    }
    if (data_center_event_is_broadcast(event)) {
        return true;
    }
    if (filter->owner[0] && strcmp(filter->owner, event->target) == 0) {
        return true;
    }
    if (filter->target[0] && strcmp(filter->target, event->target) == 0) {
        return true;
    }

    return false;
}

static void data_center_store_retained_event(const data_center_event_t *event)
{
    if (!event || !(event->flags & DATA_CENTER_FLAG_RETAIN_LATEST)) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    size_t slot = DATA_CENTER_MAX_RETAINED_EVENTS;
    for (size_t i = 0; i < DATA_CENTER_MAX_RETAINED_EVENTS; ++i) {
        if (s_retained_used[i] && strcmp(s_retained_events[i].topic, event->topic) == 0 && s_retained_events[i].type == event->type) {
            slot = i;
            break;
        }
        if (!s_retained_used[i] && slot == DATA_CENTER_MAX_RETAINED_EVENTS) {
            slot = i;
        }
    }

    if (slot == DATA_CENTER_MAX_RETAINED_EVENTS) {
        memmove(&s_retained_events[0], &s_retained_events[1], sizeof(s_retained_events[0]) * (DATA_CENTER_MAX_RETAINED_EVENTS - 1));
        memmove(&s_retained_used[0], &s_retained_used[1], sizeof(s_retained_used[0]) * (DATA_CENTER_MAX_RETAINED_EVENTS - 1));
        slot = DATA_CENTER_MAX_RETAINED_EVENTS - 1;
    }

    s_retained_events[slot] = *event;
    s_retained_used[slot] = true;

    xSemaphoreGive(s_lock);
}

static void data_center_store_event_snapshot(const data_center_event_t *event)
{
    data_center_item_t item = {0};
    data_center_copy_string(item.source_app_id, sizeof(item.source_app_id), event->source);
    data_center_copy_string(item.title, sizeof(item.title), event->topic[0] ? event->topic : event->source);
    size_t copy_len = event->payload_len < sizeof(item.message) - 1 ? event->payload_len : sizeof(item.message) - 1;
    memcpy(item.message, event->payload, copy_len);
    item.message[copy_len] = '\0';
    item.type = event->type == DATA_CENTER_EVENT_PHONE_MESSAGE ? DATA_CENTER_ITEM_ACTION : DATA_CENTER_ITEM_INFO;
    item.timestamp_ms = event->timestamp_ms;
    item.priority = event->priority;
    data_center_store_item(&item);
}

static void data_center_process_event(const data_center_event_t *event)
{
    if (!event) {
        return;
    }

    data_center_store_event_snapshot(event);
    data_center_store_retained_event(event);

    data_center_subscriber_t subscribers[DATA_CENTER_MAX_SUBSCRIBERS];
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(subscribers, s_subscribers, sizeof(subscribers));
    xSemaphoreGive(s_lock);

    for (size_t i = 0; i < DATA_CENTER_MAX_SUBSCRIBERS; ++i) {
        data_center_subscriber_t *subscriber = &subscribers[i];
        if (subscriber->used && subscriber->cb && data_center_filter_matches(&subscriber->filter, event)) {
#ifdef QDEBUG
            ESP_LOGI(TAG,
                     "event route: source=%s target=%s topic=%s type=%u -> owner=%s filter_target=%s",
                     event->source,
                     event->target,
                     event->topic,
                     event->type,
                     subscriber->filter.owner,
                     subscriber->filter.target);
#endif
            subscriber->cb(event, subscriber->user_ctx);
        }
    }
}

static uint16_t data_center_message_kind_to_event_type(data_center_message_kind_t kind)
{
    switch (kind) {
    case DATA_CENTER_MESSAGE_KIND_NOTIFICATION:
        return DATA_CENTER_EVENT_PHONE_MESSAGE;
    case DATA_CENTER_MESSAGE_KIND_TIME_SYNC:
        return DATA_CENTER_EVENT_TIME_SYNC;
    case DATA_CENTER_MESSAGE_KIND_AI_TEXT:
        return DATA_CENTER_EVENT_AI_TEXT;
    case DATA_CENTER_MESSAGE_KIND_AI_AUDIO_CHUNK:
        return DATA_CENTER_EVENT_AI_AUDIO_CHUNK;
    case DATA_CENTER_MESSAGE_KIND_COMMAND:
    case DATA_CENTER_MESSAGE_KIND_DEVICE_INFO:
    case DATA_CENTER_MESSAGE_KIND_INFO:
    default:
        return DATA_CENTER_EVENT_GENERIC;
    }
}

static void data_center_message_to_event(const data_center_message_t *message, data_center_event_t *event)
{
    memset(event, 0, sizeof(*event));
    data_center_copy_string(event->source, sizeof(event->source), message->source);
    data_center_copy_string(event->target, sizeof(event->target), message->target);
    event->type = data_center_message_kind_to_event_type(message->kind);
    switch (message->kind) {
    case DATA_CENTER_MESSAGE_KIND_NOTIFICATION:
        data_center_copy_string(event->topic, sizeof(event->topic), "phone.notification");
        break;
    case DATA_CENTER_MESSAGE_KIND_TIME_SYNC:
        data_center_copy_string(event->topic, sizeof(event->topic), "time.sync");
        event->flags = DATA_CENTER_FLAG_RETAIN_LATEST;
        break;
    case DATA_CENTER_MESSAGE_KIND_AI_TEXT:
        data_center_copy_string(event->topic, sizeof(event->topic), "ai.text");
        break;
    case DATA_CENTER_MESSAGE_KIND_AI_AUDIO_CHUNK:
        data_center_copy_string(event->topic, sizeof(event->topic), "ai.audio");
        break;
    default:
        data_center_copy_string(event->topic, sizeof(event->topic), "message.generic");
        break;
    }
    event->payload_len = message->payload_len < sizeof(event->payload) ? message->payload_len : sizeof(event->payload);
    memcpy(event->payload, message->payload, event->payload_len);
    event->timestamp_ms = message->timestamp_ms;
    event->priority = message->priority;
}

static void data_center_task(void *arg)
{
    (void)arg;
    data_center_event_t event;

    while (true) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            data_center_process_event(&event);
        }
    }
}

esp_err_t data_center_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    s_event_queue = xQueueCreate(DATA_CENTER_MESSAGE_QUEUE_LEN, sizeof(data_center_event_t));
    if (!s_event_queue) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t data_center_start(void)
{
    if (!s_initialized) {
        esp_err_t ret = data_center_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (s_started) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(data_center_task,
                                 "data_center",
                                 DATA_CENTER_TASK_STACK_SIZE,
                                 NULL,
                                 DATA_CENTER_TASK_PRIORITY,
                                 &s_task_handle);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    return ESP_OK;
}

esp_err_t data_center_publish(const data_center_item_t *item)
{
    if (!item) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        esp_err_t ret = data_center_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return data_center_store_item(item);
}

esp_err_t data_center_publish_event(const data_center_event_t *event)
{
    if (!event || event->payload_len > DATA_CENTER_PAYLOAD_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_started) {
        esp_err_t ret = data_center_start();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return xQueueSend(s_event_queue, event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t data_center_publish_message(const data_center_message_t *message)
{
    if (!message || message->payload_len > DATA_CENTER_PAYLOAD_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    data_center_event_t event;
    data_center_message_to_event(message, &event);
    return data_center_publish_event(&event);
}

esp_err_t data_center_subscribe_event(const data_center_filter_t *filter, data_center_event_cb_t cb, void *user_ctx)
{
    if (!filter || !cb) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        esp_err_t ret = data_center_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    for (size_t i = 0; i < DATA_CENTER_MAX_SUBSCRIBERS; ++i) {
        if (s_subscribers[i].used && s_subscribers[i].cb == cb && s_subscribers[i].user_ctx == user_ctx) {
            s_subscribers[i].filter = *filter;
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }

    for (size_t i = 0; i < DATA_CENTER_MAX_SUBSCRIBERS; ++i) {
        if (!s_subscribers[i].used) {
            s_subscribers[i].used = true;
            s_subscribers[i].filter = *filter;
            s_subscribers[i].cb = cb;
            s_subscribers[i].user_ctx = user_ctx;
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t data_center_unsubscribe_event(data_center_event_cb_t cb, void *user_ctx)
{
    if (!cb) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    for (size_t i = 0; i < DATA_CENTER_MAX_SUBSCRIBERS; ++i) {
        if (s_subscribers[i].used && s_subscribers[i].cb == cb && s_subscribers[i].user_ctx == user_ctx) {
            memset(&s_subscribers[i], 0, sizeof(s_subscribers[i]));
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_lock);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t data_center_subscribe(const char *target, data_center_message_cb_t cb, void *user_ctx)
{
    (void)target;
    (void)cb;
    (void)user_ctx;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t data_center_unsubscribe(data_center_message_cb_t cb, void *user_ctx)
{
    (void)cb;
    (void)user_ctx;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t data_center_get_latest_event(const char *topic, uint16_t type, data_center_event_t *event)
{
    if (!topic || !event) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < DATA_CENTER_MAX_RETAINED_EVENTS; ++i) {
        if (s_retained_used[i] && strcmp(s_retained_events[i].topic, topic) == 0 &&
            (type == DATA_CENTER_EVENT_TYPE_ANY || s_retained_events[i].type == type)) {
            *event = s_retained_events[i];
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NOT_FOUND;
}

size_t data_center_get_snapshot(data_center_item_t *items, size_t max_items)
{
    if (!items || max_items == 0) {
        return 0;
    }

    if (!s_initialized) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t count = s_item_count < max_items ? s_item_count : max_items;
    for (size_t i = 0; i < count; ++i) {
        items[i] = s_items[i];
    }
    xSemaphoreGive(s_lock);
    return count;
}

void data_center_clear(void)
{
    if (!s_initialized) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(s_items, 0, sizeof(s_items));
    s_item_count = 0;
    memset(s_retained_events, 0, sizeof(s_retained_events));
    memset(s_retained_used, 0, sizeof(s_retained_used));
    xSemaphoreGive(s_lock);
}

size_t data_center_count(void)
{
    if (!s_initialized) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t count = s_item_count;
    xSemaphoreGive(s_lock);
    return count;
}
