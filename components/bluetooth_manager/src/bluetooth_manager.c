#include "bluetooth_manager.h"

#include "data_center.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#if __has_include("esp_nimble_hci.h")
#include "esp_nimble_hci.h"
#define BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI 1
#else
#define BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI 0
#endif
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#endif

#define BLUETOOTH_MANAGER_QUEUE_LEN 8
#define BLUETOOTH_MANAGER_TASK_STACK_SIZE 4096
#define BLUETOOTH_MANAGER_TASK_PRIORITY 5
#define BLUETOOTH_MANAGER_MAX_CALLBACKS 8
#define BLUETOOTH_MANAGER_RX_BUFFER_SIZE 1024
#define BLUETOOTH_MANAGER_FRAME_HEADER_SIZE 6
#define BLUETOOTH_MANAGER_FRAME_MAGIC_0 0xA5
#define BLUETOOTH_MANAGER_FRAME_MAGIC_1 0x5A
#define BLUETOOTH_MANAGER_FRAME_VERSION 0x01

static const char *TAG = "bt_manager";

typedef enum {
    BLUETOOTH_MANAGER_CMD_ENABLE = 1,
    BLUETOOTH_MANAGER_CMD_DISABLE,
    BLUETOOTH_MANAGER_CMD_SEND_MESSAGE,
    BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES,
    BLUETOOTH_MANAGER_CMD_MOCK_CONNECT,
    BLUETOOTH_MANAGER_CMD_STOP_TASK,
} bluetooth_manager_cmd_type_t;

typedef struct {
    bluetooth_manager_cmd_type_t type;
    union {
        bluetooth_manager_message_t tx_message;
        struct {
            uint8_t data[BLUETOOTH_MANAGER_MESSAGE_MAX_LEN];
            size_t len;
        } rx_bytes;
        struct {
            char name[BLUETOOTH_MANAGER_DEVICE_NAME_MAX_LEN];
            char address[BLUETOOTH_MANAGER_DEVICE_ADDR_MAX_LEN];
        } mock_connect;
    } data;
} bluetooth_manager_cmd_t;

typedef struct {
    bool used;
    bluetooth_manager_event_cb_t cb;
    void *user_ctx;
} bluetooth_manager_callback_entry_t;

static QueueHandle_t s_cmd_queue;
static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_started;
static bool s_backend_enabled;
static bool s_nimble_port_initialized;
static bluetooth_manager_status_t s_status;
static bluetooth_manager_callback_entry_t s_callbacks[BLUETOOTH_MANAGER_MAX_CALLBACKS];
static uint8_t s_rx_buffer[BLUETOOTH_MANAGER_RX_BUFFER_SIZE];
static size_t s_rx_len;

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static void bluetooth_manager_nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#endif

static esp_err_t bluetooth_manager_enqueue_enabled_command(bool enabled)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = enabled ? BLUETOOTH_MANAGER_CMD_ENABLE : BLUETOOTH_MANAGER_CMD_DISABLE,
    };
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void bluetooth_manager_handle_data_event(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event || event->type != DATA_CENTER_EVENT_BLUETOOTH_COMMAND ||
        event->payload_len < sizeof(data_center_bluetooth_command_t)) {
        return;
    }

    data_center_bluetooth_command_t command;
    memcpy(&command, event->payload, sizeof(command));
    if (command.command != DATA_CENTER_BLUETOOTH_CMD_SET_ENABLED) {
        ESP_LOGW(TAG, "Unsupported bluetooth command: %d", command.command);
        return;
    }

    esp_err_t ret = bluetooth_manager_enqueue_enabled_command(command.enabled);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enqueue bluetooth command: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_copy_string(char *dst, size_t dst_size, const char *src)
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

static data_center_message_kind_t bluetooth_manager_to_data_center_kind(bluetooth_manager_message_type_t type)
{
    switch (type) {
    case BLUETOOTH_MANAGER_MESSAGE_NOTIFICATION:
        return DATA_CENTER_MESSAGE_KIND_NOTIFICATION;
    case BLUETOOTH_MANAGER_MESSAGE_COMMAND:
        return DATA_CENTER_MESSAGE_KIND_COMMAND;
    case BLUETOOTH_MANAGER_MESSAGE_TIME_SYNC:
        return DATA_CENTER_MESSAGE_KIND_TIME_SYNC;
    case BLUETOOTH_MANAGER_MESSAGE_DEVICE_INFO:
        return DATA_CENTER_MESSAGE_KIND_DEVICE_INFO;
    case BLUETOOTH_MANAGER_MESSAGE_AI_TEXT:
        return DATA_CENTER_MESSAGE_KIND_AI_TEXT;
    case BLUETOOTH_MANAGER_MESSAGE_AI_AUDIO_CHUNK:
        return DATA_CENTER_MESSAGE_KIND_AI_AUDIO_CHUNK;
    case BLUETOOTH_MANAGER_MESSAGE_UNKNOWN:
    default:
        return DATA_CENTER_MESSAGE_KIND_INFO;
    }
}

static void bluetooth_manager_notify(const bluetooth_manager_event_t *event)
{
    bluetooth_manager_callback_entry_t callbacks[BLUETOOTH_MANAGER_MAX_CALLBACKS];
    memset(callbacks, 0, sizeof(callbacks));

    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        memcpy(callbacks, s_callbacks, sizeof(callbacks));
        xSemaphoreGive(s_lock);
    }

    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (callbacks[i].used && callbacks[i].cb) {
            callbacks[i].cb(event, callbacks[i].user_ctx);
        }
    }
}

static void bluetooth_manager_set_status(bluetooth_manager_state_t state, bool enabled)
{
    bluetooth_manager_status_t status;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    s_status.enabled = enabled;
    status = s_status;
    xSemaphoreGive(s_lock);

    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_STATUS_CHANGED,
        .data.status = status,
    };
    bluetooth_manager_notify(&event);

    data_center_event_t data_event = {0};
    bluetooth_manager_copy_string(data_event.source, sizeof(data_event.source), "bluetooth");
    bluetooth_manager_copy_string(data_event.target, sizeof(data_event.target), "device");
    bluetooth_manager_copy_string(data_event.topic, sizeof(data_event.topic), "bluetooth.status");
    data_event.type = DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED;
    data_event.flags = DATA_CENTER_FLAG_RETAIN_LATEST;
    data_event.payload_len = sizeof(status) < sizeof(data_event.payload) ? sizeof(status) : sizeof(data_event.payload);
    memcpy(data_event.payload, &status, data_event.payload_len);
    data_event.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    data_event.priority = 1;
    esp_err_t ret = data_center_publish_event(&data_event);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish bluetooth status: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_set_last_error(esp_err_t error)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.last_error = (int)error;
    xSemaphoreGive(s_lock);
}

static esp_err_t bluetooth_manager_backend_init(void)
{
    if (s_backend_enabled) {
        return ESP_OK;
    }

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
    esp_err_t ret = ESP_OK;

#if BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI
    esp_bt_controller_status_t controller_status = esp_bt_controller_get_status();
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (controller_status == ESP_BT_CONTROLLER_STATUS_IDLE) {
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        controller_status = esp_bt_controller_get_status();
    }

    if (controller_status != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
    }

#if BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI
    ret = esp_nimble_hci_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
#endif
#endif

    if (!s_nimble_port_initialized) {
        ret = nimble_port_init();
        if (ret != ESP_OK) {
            return ret;
        }
        s_nimble_port_initialized = true;
    }

    ble_svc_gap_device_name_set("AIWatch");
    nimble_port_freertos_init(bluetooth_manager_nimble_host_task);
    s_backend_enabled = true;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Bluetooth backend is not enabled in sdkconfig. Enable CONFIG_BT_ENABLED and CONFIG_BT_NIMBLE_ENABLED.");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void bluetooth_manager_publish_message(const bluetooth_manager_message_t *message)
{
    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_MESSAGE_RECEIVED,
        .data.message = *message,
    };
    bluetooth_manager_notify(&event);

    data_center_message_t data_message = {0};
    bluetooth_manager_copy_string(data_message.source, sizeof(data_message.source), "phone");
    bluetooth_manager_copy_string(data_message.target, sizeof(data_message.target), message->target);
    data_message.kind = bluetooth_manager_to_data_center_kind(message->type);
    data_message.payload_len = message->data_len < sizeof(data_message.payload) ? message->data_len : sizeof(data_message.payload);
    memcpy(data_message.payload, message->data, data_message.payload_len);
    data_message.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    data_message.priority = message->type == BLUETOOTH_MANAGER_MESSAGE_COMMAND ? 2 : 1;
    esp_err_t ret = data_center_publish_message(&data_message);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue message for data center: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_parse_rx_buffer(void)
{
    while (s_rx_len >= BLUETOOTH_MANAGER_FRAME_HEADER_SIZE) {
        if (s_rx_buffer[0] != BLUETOOTH_MANAGER_FRAME_MAGIC_0 || s_rx_buffer[1] != BLUETOOTH_MANAGER_FRAME_MAGIC_1) {
            memmove(&s_rx_buffer[0], &s_rx_buffer[1], s_rx_len - 1);
            s_rx_len--;
            continue;
        }

        if (s_rx_buffer[2] != BLUETOOTH_MANAGER_FRAME_VERSION) {
            memmove(&s_rx_buffer[0], &s_rx_buffer[1], s_rx_len - 1);
            s_rx_len--;
            continue;
        }

        uint16_t payload_len = ((uint16_t)s_rx_buffer[4] << 8) | s_rx_buffer[5];
        size_t frame_len = BLUETOOTH_MANAGER_FRAME_HEADER_SIZE + payload_len;
        if (payload_len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
            ESP_LOGW(TAG, "Dropping oversized frame: %u", payload_len);
            s_rx_len = 0;
            return;
        }
        if (s_rx_len < frame_len) {
            return;
        }

        bluetooth_manager_message_t message = {0};
        message.type = (bluetooth_manager_message_type_t)s_rx_buffer[3];
        message.data_len = payload_len;
        memcpy(message.data, &s_rx_buffer[BLUETOOTH_MANAGER_FRAME_HEADER_SIZE], payload_len);
        bluetooth_manager_publish_message(&message);

        size_t remaining = s_rx_len - frame_len;
        memmove(s_rx_buffer, &s_rx_buffer[frame_len], remaining);
        s_rx_len = remaining;
    }
}

static void bluetooth_manager_handle_rx_bytes(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }

    if (len > sizeof(s_rx_buffer) - s_rx_len) {
        ESP_LOGW(TAG, "RX buffer overflow, clearing buffer");
        s_rx_len = 0;
    }

    size_t copy_len = len < sizeof(s_rx_buffer) - s_rx_len ? len : sizeof(s_rx_buffer) - s_rx_len;
    memcpy(&s_rx_buffer[s_rx_len], data, copy_len);
    s_rx_len += copy_len;
    bluetooth_manager_parse_rx_buffer();
}

static void bluetooth_manager_handle_send_message(const bluetooth_manager_message_t *message)
{
    (void)message;
    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_MESSAGE_SENT,
    };
    bluetooth_manager_notify(&event);
}

static void bluetooth_manager_task(void *arg)
{
    (void)arg;
    bluetooth_manager_cmd_t cmd;

    while (true) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (cmd.type) {
        case BLUETOOTH_MANAGER_CMD_ENABLE:
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_ON, true);
            esp_err_t enable_ret = s_backend_enabled ? ESP_OK : ESP_ERR_INVALID_STATE;
            if (enable_ret == ESP_OK) {
                bluetooth_manager_set_last_error(ESP_OK);
                bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
            } else {
                bluetooth_manager_set_last_error(enable_ret);
                bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ERROR, false);
            }
            break;
        case BLUETOOTH_MANAGER_CMD_DISABLE:
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_OFF, false);
            bluetooth_manager_set_last_error(ESP_OK);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_status.connected_name[0] = '\0';
            s_status.connected_address[0] = '\0';
            xSemaphoreGive(s_lock);
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_OFF, false);
            break;
        case BLUETOOTH_MANAGER_CMD_SEND_MESSAGE:
            bluetooth_manager_handle_send_message(&cmd.data.tx_message);
            break;
        case BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES:
            bluetooth_manager_handle_rx_bytes(cmd.data.rx_bytes.data, cmd.data.rx_bytes.len);
            break;
        case BLUETOOTH_MANAGER_CMD_MOCK_CONNECT:
            xSemaphoreTake(s_lock, portMAX_DELAY);
            bluetooth_manager_copy_string(s_status.connected_name, sizeof(s_status.connected_name), cmd.data.mock_connect.name);
            bluetooth_manager_copy_string(s_status.connected_address, sizeof(s_status.connected_address), cmd.data.mock_connect.address);
            xSemaphoreGive(s_lock);
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_CONNECTED, true);
            break;
        case BLUETOOTH_MANAGER_CMD_STOP_TASK:
            s_task_handle = NULL;
            vTaskDelete(NULL);
            return;
        default:
            break;
        }
    }
}

esp_err_t bluetooth_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    s_cmd_queue = xQueueCreate(BLUETOOTH_MANAGER_QUEUE_LEN, sizeof(bluetooth_manager_cmd_t));
    if (!s_cmd_queue) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = BLUETOOTH_MANAGER_STATE_OFF;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t bluetooth_manager_init_ble_stack(void)
{
    esp_err_t ret = bluetooth_manager_init();
    if (ret != ESP_OK) {
        return ret;
    }

    bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_ON, true);
    ret = bluetooth_manager_backend_init();
    bluetooth_manager_set_last_error(ret);
    if (ret != ESP_OK) {
        bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ERROR, false);
        return ret;
    }

    bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
    return ESP_OK;
}

esp_err_t bluetooth_manager_start(void)
{
    if (!s_initialized) {
        esp_err_t ret = bluetooth_manager_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    if (s_started) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(bluetooth_manager_task,
                                 "bluetooth_manager",
                                 BLUETOOTH_MANAGER_TASK_STACK_SIZE,
                                 NULL,
                                 BLUETOOTH_MANAGER_TASK_PRIORITY,
                                 &s_task_handle);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    data_center_filter_t filter = {0};
    bluetooth_manager_copy_string(filter.owner, sizeof(filter.owner), "bluetooth");
    bluetooth_manager_copy_string(filter.target, sizeof(filter.target), "bluetooth");
    bluetooth_manager_copy_string(filter.topic, sizeof(filter.topic), "bluetooth.command");
    filter.type = DATA_CENTER_EVENT_BLUETOOTH_COMMAND;
    esp_err_t sub_ret = data_center_subscribe_event(&filter, bluetooth_manager_handle_data_event, NULL);
    if (sub_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe bluetooth commands: %s", esp_err_to_name(sub_ret));
    }
    return ESP_OK;
}

esp_err_t bluetooth_manager_stop(void)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_OK;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_STOP_TASK,
    };
    xQueueSend(s_cmd_queue, &cmd, portMAX_DELAY);
    data_center_unsubscribe_event(bluetooth_manager_handle_data_event, NULL);
    s_started = false;
    return ESP_OK;
}

esp_err_t bluetooth_manager_set_enabled(bool enabled)
{
    return bluetooth_manager_enqueue_enabled_command(enabled);
}

esp_err_t bluetooth_manager_get_status(bluetooth_manager_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t bluetooth_manager_send_message(const bluetooth_manager_message_t *message)
{
    if (!message || message->data_len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_SEND_MESSAGE,
        .data.tx_message = *message,
    };
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_send_raw(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    bluetooth_manager_message_t message = {
        .type = BLUETOOTH_MANAGER_MESSAGE_UNKNOWN,
        .data_len = len < BLUETOOTH_MANAGER_MESSAGE_MAX_LEN ? len : BLUETOOTH_MANAGER_MESSAGE_MAX_LEN,
    };
    memcpy(message.data, data, message.data_len);
    return bluetooth_manager_send_message(&message);
}

esp_err_t bluetooth_manager_on_raw_received(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES,
    };
    cmd.data.rx_bytes.len = len;
    memcpy(cmd.data.rx_bytes.data, data, len);
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_mock_connect_phone(const char *name, const char *address)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_MOCK_CONNECT,
    };
    bluetooth_manager_copy_string(cmd.data.mock_connect.name, sizeof(cmd.data.mock_connect.name), name);
    bluetooth_manager_copy_string(cmd.data.mock_connect.address, sizeof(cmd.data.mock_connect.address), address);
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_register_callback(bluetooth_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb || !s_lock) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (!s_callbacks[i].used) {
            s_callbacks[i].used = true;
            s_callbacks[i].cb = cb;
            s_callbacks[i].user_ctx = user_ctx;
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t bluetooth_manager_unregister_callback(bluetooth_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb || !s_lock) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            memset(&s_callbacks[i], 0, sizeof(s_callbacks[i]));
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NOT_FOUND;
}