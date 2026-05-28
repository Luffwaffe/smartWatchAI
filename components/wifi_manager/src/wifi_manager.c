#include "wifi_manager.h"

#include "data_center.h"

#include <stddef.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define WIFI_MANAGER_QUEUE_LEN 8
#define WIFI_MANAGER_TASK_STACK_SIZE 8192
#define WIFI_MANAGER_TASK_PRIORITY 5
#define WIFI_MANAGER_MAX_CALLBACKS 8
#define WIFI_MANAGER_SETTING_TARGET "setting"

static const char *TAG = "wifi_manager";

typedef enum {
    WIFI_MANAGER_CMD_ENABLE = 1,
    WIFI_MANAGER_CMD_SCAN,
    WIFI_MANAGER_CMD_CONNECT,
    WIFI_MANAGER_CMD_DISCONNECT,
    WIFI_MANAGER_CMD_DISABLE,
    WIFI_MANAGER_CMD_STOP_TASK,
} wifi_manager_cmd_type_t;

typedef struct {
    wifi_manager_cmd_type_t type;
    union {
        struct {
            char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
            char password[WIFI_MANAGER_PASSWORD_MAX_LEN + 1];
        } connect;
    } data;
} wifi_manager_cmd_t;

typedef struct {
    bool used;
    wifi_manager_event_cb_t cb;
    void *user_ctx;
} wifi_manager_callback_entry_t;

typedef enum {
    WIFI_MANAGER_DC_CMD_ENABLE = 1,
    WIFI_MANAGER_DC_CMD_SCAN,
    WIFI_MANAGER_DC_CMD_CONNECT,
    WIFI_MANAGER_DC_CMD_DISCONNECT,
    WIFI_MANAGER_DC_CMD_DISABLE,
    WIFI_MANAGER_DC_CMD_GET_STATUS,
} wifi_manager_dc_cmd_type_t;

typedef struct {
    wifi_manager_dc_cmd_type_t command;
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    char password[WIFI_MANAGER_PASSWORD_MAX_LEN + 1];
} wifi_manager_dc_command_t;

typedef struct {
    size_t count;
    wifi_manager_ap_record_t aps[WIFI_MANAGER_SCAN_MAX_APS];
} wifi_manager_scan_payload_t;

static QueueHandle_t s_cmd_queue;
static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_started;
static bool s_netif_initialized;
static esp_netif_t *s_sta_netif;
static wifi_manager_status_t s_status;
static wifi_manager_callback_entry_t s_callbacks[WIFI_MANAGER_MAX_CALLBACKS];

static void wifi_manager_task(void *arg);
static void wifi_manager_handle_data_event(const data_center_event_t *event, void *user_ctx);

static void wifi_manager_copy_string(char *dst, size_t dst_size, const char *src)
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

static void wifi_manager_notify(const wifi_manager_event_t *event)
{
    if (!event || !s_lock) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    wifi_manager_callback_entry_t callbacks[WIFI_MANAGER_MAX_CALLBACKS];
    memcpy(callbacks, s_callbacks, sizeof(callbacks));
    xSemaphoreGive(s_lock);

    for (size_t i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; ++i) {
        if (callbacks[i].used && callbacks[i].cb) {
            callbacks[i].cb(event, callbacks[i].user_ctx);
        }
    }
}

static void wifi_manager_publish_event(const char *topic, const char *target, uint16_t type, const void *payload, size_t payload_len, uint8_t priority)
{
    data_center_event_t event = {0};
    wifi_manager_copy_string(event.source, sizeof(event.source), "wifi");
    wifi_manager_copy_string(event.target, sizeof(event.target), target ? target : "Setting");
    wifi_manager_copy_string(event.topic, sizeof(event.topic), topic);
    event.type = type;
    event.flags = DATA_CENTER_FLAG_RETAIN_LATEST;
    if (!target || strcmp(target, "Setting") == 0) {
        event.flags |= DATA_CENTER_FLAG_BROADCAST;
    }
    event.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    event.priority = priority;

    if (payload && payload_len > 0) {
        event.payload_len = payload_len < sizeof(event.payload) ? payload_len : sizeof(event.payload);
        memcpy(event.payload, payload, event.payload_len);
    }

    esp_err_t ret = data_center_publish_event(&event);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish %s: %s", topic, esp_err_to_name(ret));
    }
}

static void wifi_manager_set_status(wifi_manager_state_t state, bool enabled, bool connected, esp_err_t last_error)
{
    wifi_manager_status_t snapshot;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    s_status.enabled = enabled;
    s_status.connected = connected;
    s_status.last_error = last_error;
    snapshot = s_status;
    xSemaphoreGive(s_lock);

    wifi_manager_event_t event = {
        .type = WIFI_MANAGER_EVT_STATUS_CHANGED,
        .data.status = snapshot,
    };
    wifi_manager_notify(&event);
    wifi_manager_publish_event("wifi.status", WIFI_MANAGER_SETTING_TARGET, DATA_CENTER_EVENT_WIFI_STATUS_CHANGED, &snapshot, sizeof(snapshot), 1);
}

static void wifi_manager_handle_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_manager_set_status(WIFI_MANAGER_STATE_DISCONNECTED, true, false, ESP_ERR_WIFI_NOT_CONNECT);
        wifi_manager_event_t event = {.type = WIFI_MANAGER_EVT_DISCONNECTED};
        wifi_manager_notify(&event);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *got_ip = (const ip_event_got_ip_t *)event_data;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_status.connected = true;
        s_status.state = WIFI_MANAGER_STATE_CONNECTED;
        s_status.ip[0] = esp_ip4_addr1_16(&got_ip->ip_info.ip);
        s_status.ip[1] = esp_ip4_addr2_16(&got_ip->ip_info.ip);
        s_status.ip[2] = esp_ip4_addr3_16(&got_ip->ip_info.ip);
        s_status.ip[3] = esp_ip4_addr4_16(&got_ip->ip_info.ip);
        wifi_manager_status_t snapshot = s_status;
        xSemaphoreGive(s_lock);

        wifi_manager_event_t event = {
            .type = WIFI_MANAGER_EVT_CONNECTED,
            .data.status = snapshot,
        };
        wifi_manager_notify(&event);
        wifi_manager_publish_event("wifi.status", DATA_CENTER_TARGET_BROADCAST, DATA_CENTER_EVENT_WIFI_STATUS_CHANGED, &snapshot, sizeof(snapshot), 2);
    }
}

static esp_err_t wifi_manager_backend_init(void)
{
    if (s_netif_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_manager_handle_event, NULL), TAG, "wifi event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_manager_handle_event, NULL), TAG, "ip event handler failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA mode failed");

    s_netif_initialized = true;
    return ESP_OK;
}

static esp_err_t wifi_manager_do_enable(void)
{
    if (s_status.enabled) {
        wifi_manager_set_status(s_status.state, true, s_status.connected, ESP_OK);
        return ESP_OK;
    }

    esp_err_t ret = wifi_manager_backend_init();
    if (ret != ESP_OK) {
        wifi_manager_set_status(WIFI_MANAGER_STATE_ERROR, false, false, ret);
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        wifi_manager_set_status(WIFI_MANAGER_STATE_ERROR, false, false, ret);
        return ret;
    }

    wifi_manager_set_status(WIFI_MANAGER_STATE_IDLE, true, false, ESP_OK);
    return ESP_OK;
}

static void wifi_manager_publish_scan(const wifi_manager_event_t *event)
{
    wifi_manager_scan_payload_t payload = {0};
    const size_t max_payload_aps = (DATA_CENTER_PAYLOAD_MAX_LEN - offsetof(wifi_manager_scan_payload_t, aps)) / sizeof(payload.aps[0]);
    size_t count = event->data.scan.count;
    if (count > WIFI_MANAGER_SCAN_MAX_APS) {
        count = WIFI_MANAGER_SCAN_MAX_APS;
    }
    if (count > max_payload_aps) {
        count = max_payload_aps;
    }

    payload.count = count;
    memcpy(payload.aps, event->data.scan.aps, count * sizeof(payload.aps[0]));
    size_t payload_len = offsetof(wifi_manager_scan_payload_t, aps) + count * sizeof(payload.aps[0]);
    wifi_manager_publish_event("wifi.scan", WIFI_MANAGER_SETTING_TARGET, DATA_CENTER_EVENT_WIFI_SCAN_DONE, &payload, payload_len, 1);
}

static void wifi_manager_publish_status(void)
{
    wifi_manager_status_t snapshot = {0};
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snapshot = s_status;
    xSemaphoreGive(s_lock);

    wifi_manager_publish_event("wifi.status", WIFI_MANAGER_SETTING_TARGET, DATA_CENTER_EVENT_WIFI_STATUS_CHANGED, &snapshot, sizeof(snapshot), 2);
}

static void wifi_manager_do_scan(void)
{
    if (!s_status.enabled) {
        esp_err_t ret = wifi_manager_do_enable();
        if (ret != ESP_OK) {
            return;
        }
    }

    wifi_manager_status_t status_before_scan = {0};
    xSemaphoreTake(s_lock, portMAX_DELAY);
    status_before_scan = s_status;
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "WiFi scan requested, stack high-water mark before scan: %u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    wifi_manager_set_status(WIFI_MANAGER_STATE_SCANNING, true, status_before_scan.connected, ESP_OK);

    wifi_scan_config_t scan_config = {0};
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        wifi_manager_state_t next_state = status_before_scan.connected ? WIFI_MANAGER_STATE_CONNECTED : WIFI_MANAGER_STATE_IDLE;
        wifi_manager_set_status(next_state, true, status_before_scan.connected, ret);
        wifi_manager_event_t event = {.type = WIFI_MANAGER_EVT_ERROR, .data.error_code = ret};
        wifi_manager_notify(&event);
        return;
    }

    uint16_t ap_count = WIFI_MANAGER_SCAN_MAX_APS;
    wifi_ap_record_t ap_records[WIFI_MANAGER_SCAN_MAX_APS] = {0};
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        wifi_manager_state_t next_state = status_before_scan.connected ? WIFI_MANAGER_STATE_CONNECTED : WIFI_MANAGER_STATE_IDLE;
        wifi_manager_set_status(next_state, true, status_before_scan.connected, ret);
        wifi_manager_event_t event = {.type = WIFI_MANAGER_EVT_ERROR, .data.error_code = ret};
        wifi_manager_notify(&event);
        return;
    }

#ifdef QDEBUG
    ESP_LOGI(TAG, "WiFi scan found %u AP(s)", (unsigned)ap_count);
#endif

    wifi_manager_event_t event = {.type = WIFI_MANAGER_EVT_SCAN_DONE};
    event.data.scan.count = ap_count;
    for (size_t i = 0; i < ap_count; ++i) {
        wifi_manager_copy_string(event.data.scan.aps[i].ssid, sizeof(event.data.scan.aps[i].ssid), (const char *)ap_records[i].ssid);
        event.data.scan.aps[i].rssi = ap_records[i].rssi;
        event.data.scan.aps[i].authmode = ap_records[i].authmode;
        event.data.scan.aps[i].primary = ap_records[i].primary;
#ifdef QDEBUG
        ESP_LOGI(TAG, "WiFi AP[%u]: SSID=\"%s\" RSSI=%d auth=%d channel=%u",
                 (unsigned)i,
                 event.data.scan.aps[i].ssid[0] ? event.data.scan.aps[i].ssid : "<hidden>",
                 event.data.scan.aps[i].rssi,
                 event.data.scan.aps[i].authmode,
                 event.data.scan.aps[i].primary);
#endif
    }

    wifi_manager_state_t next_state = status_before_scan.connected ? WIFI_MANAGER_STATE_CONNECTED : WIFI_MANAGER_STATE_IDLE;
    wifi_manager_set_status(next_state, true, status_before_scan.connected, ESP_OK);
    wifi_manager_notify(&event);
    wifi_manager_publish_scan(&event);
}

static void wifi_manager_do_connect(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0') {
        wifi_manager_set_status(WIFI_MANAGER_STATE_ERROR, true, false, ESP_ERR_INVALID_ARG);
        return;
    }

    wifi_config_t config = {0};
    wifi_manager_copy_string((char *)config.sta.ssid, sizeof(config.sta.ssid), ssid);
    wifi_manager_copy_string((char *)config.sta.password, sizeof(config.sta.password), password);
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    wifi_manager_copy_string(s_status.ssid, sizeof(s_status.ssid), ssid);
    memset(s_status.ip, 0, sizeof(s_status.ip));
    xSemaphoreGive(s_lock);

    wifi_manager_set_status(WIFI_MANAGER_STATE_CONNECTING, true, false, ESP_OK);
    esp_wifi_disconnect();
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (ret == ESP_OK) {
        ret = esp_wifi_connect();
    }
    if (ret != ESP_OK) {
        wifi_manager_set_status(WIFI_MANAGER_STATE_ERROR, true, false, ret);
        wifi_manager_event_t event = {.type = WIFI_MANAGER_EVT_ERROR, .data.error_code = ret};
        wifi_manager_notify(&event);
    }
}

static void wifi_manager_do_disconnect(void)
{
    esp_err_t ret = esp_wifi_disconnect();
    wifi_manager_set_status(WIFI_MANAGER_STATE_DISCONNECTED, true, false, ret);
}

static void wifi_manager_do_disable(void)
{
    esp_err_t ret = ESP_OK;
    if (s_status.enabled) {
        esp_err_t disconnect_ret = esp_wifi_disconnect();
        esp_err_t stop_ret = esp_wifi_stop();
        ret = stop_ret != ESP_OK ? stop_ret : disconnect_ret;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(s_status.ssid, 0, sizeof(s_status.ssid));
    memset(s_status.ip, 0, sizeof(s_status.ip));
    xSemaphoreGive(s_lock);

    wifi_manager_set_status(WIFI_MANAGER_STATE_OFF, false, false, ret);
}

static esp_err_t wifi_manager_enqueue(const wifi_manager_cmd_t *cmd)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    return xQueueSend(s_cmd_queue, cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void wifi_manager_handle_data_event(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event || event->payload_len < sizeof(wifi_manager_dc_command_t)) {
        return;
    }

    wifi_manager_dc_command_t dc_cmd = {0};
    memcpy(&dc_cmd, event->payload, sizeof(dc_cmd));

    switch (dc_cmd.command) {
    case WIFI_MANAGER_DC_CMD_ENABLE:
        wifi_manager_enable();
        break;
    case WIFI_MANAGER_DC_CMD_SCAN:
        wifi_manager_scan();
        break;
    case WIFI_MANAGER_DC_CMD_CONNECT:
        wifi_manager_connect(dc_cmd.ssid, dc_cmd.password);
        break;
    case WIFI_MANAGER_DC_CMD_DISCONNECT:
        wifi_manager_disconnect();
        break;
    case WIFI_MANAGER_DC_CMD_DISABLE:
        wifi_manager_disable();
        break;
    case WIFI_MANAGER_DC_CMD_GET_STATUS:
        wifi_manager_publish_status();
        break;
    default:
        ESP_LOGW(TAG, "Unknown wifi data-center command: %d", dc_cmd.command);
        break;
    }
}

static void wifi_manager_task(void *arg)
{
    (void)arg;
    wifi_manager_cmd_t cmd;

    while (true) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (cmd.type) {
        case WIFI_MANAGER_CMD_ENABLE:
            wifi_manager_do_enable();
            break;
        case WIFI_MANAGER_CMD_SCAN:
            wifi_manager_do_scan();
            break;
        case WIFI_MANAGER_CMD_CONNECT:
            wifi_manager_do_connect(cmd.data.connect.ssid, cmd.data.connect.password);
            break;
        case WIFI_MANAGER_CMD_DISCONNECT:
            wifi_manager_do_disconnect();
            break;
        case WIFI_MANAGER_CMD_DISABLE:
            wifi_manager_do_disable();
            break;
        case WIFI_MANAGER_CMD_STOP_TASK:
            s_task_handle = NULL;
            vTaskDelete(NULL);
            return;
        default:
            break;
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    s_cmd_queue = xQueueCreate(WIFI_MANAGER_QUEUE_LEN, sizeof(wifi_manager_cmd_t));
    if (!s_cmd_queue) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = WIFI_MANAGER_STATE_OFF;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (!s_initialized) {
        ESP_RETURN_ON_ERROR(wifi_manager_init(), TAG, "wifi manager init failed");
    }
    if (s_started) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(wifi_manager_task,
                                 "wifi_manager",
                                 WIFI_MANAGER_TASK_STACK_SIZE,
                                 NULL,
                                 WIFI_MANAGER_TASK_PRIORITY,
                                 &s_task_handle);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    data_center_filter_t filter = {0};
    wifi_manager_copy_string(filter.owner, sizeof(filter.owner), "wifi");
    wifi_manager_copy_string(filter.target, sizeof(filter.target), "wifi");
    wifi_manager_copy_string(filter.topic, sizeof(filter.topic), "wifi.command");
    filter.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    esp_err_t sub_ret = data_center_subscribe_event(&filter, wifi_manager_handle_data_event, NULL);
    if (sub_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe wifi commands: %s", esp_err_to_name(sub_ret));
    }

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_OK;
    }

    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_STOP_TASK};
    xQueueSend(s_cmd_queue, &cmd, portMAX_DELAY);
    data_center_unsubscribe_event(wifi_manager_handle_data_event, NULL);
    s_started = false;
    return ESP_OK;
}

esp_err_t wifi_manager_scan(void)
{
    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_SCAN};
    return wifi_manager_enqueue(&cmd);
}

esp_err_t wifi_manager_enable(void)
{
    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_ENABLE};
    return wifi_manager_enqueue(&cmd);
}

esp_err_t wifi_manager_disable(void)
{
    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_DISABLE};
    return wifi_manager_enqueue(&cmd);
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_CONNECT};
    wifi_manager_copy_string(cmd.data.connect.ssid, sizeof(cmd.data.connect.ssid), ssid);
    wifi_manager_copy_string(cmd.data.connect.password, sizeof(cmd.data.connect.password), password);
    return wifi_manager_enqueue(&cmd);
}

esp_err_t wifi_manager_disconnect(void)
{
    wifi_manager_cmd_t cmd = {.type = WIFI_MANAGER_CMD_DISCONNECT};
    return wifi_manager_enqueue(&cmd);
}

esp_err_t wifi_manager_get_status(wifi_manager_status_t *status)
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

esp_err_t wifi_manager_register_callback(wifi_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    for (size_t i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; ++i) {
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

esp_err_t wifi_manager_unregister_callback(wifi_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            memset(&s_callbacks[i], 0, sizeof(s_callbacks[i]));
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NOT_FOUND;
}
