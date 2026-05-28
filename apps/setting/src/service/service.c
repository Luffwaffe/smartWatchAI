#include "setting/service/service.h"

#include "app_manager.h"
#include "data_center.h"
#include "setting/contract.h"

#ifdef QDEBUG
#include "esp_log.h"
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SETTING_WIFI_COMMAND_TOPIC "wifi.command"
#define SETTING_WIFI_COMMAND_TARGET "wifi"
#define SETTING_WIFI_SCAN_TOPIC "wifi.scan"
#define SETTING_WIFI_STATUS_TOPIC "wifi.status"

#ifdef QDEBUG
static const char *TAG = "setting_service";
#define SETTING_SERVICE_LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define SETTING_SERVICE_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#else
#define SETTING_SERVICE_LOGI(...) ((void)0)
#define SETTING_SERVICE_LOGW(...) ((void)0)
#endif

typedef enum {
    SETTING_WIFI_CMD_ENABLE = 1,
    SETTING_WIFI_CMD_SCAN,
    SETTING_WIFI_CMD_CONNECT,
    SETTING_WIFI_CMD_DISCONNECT,
    SETTING_WIFI_CMD_DISABLE,
    SETTING_WIFI_CMD_GET_STATUS,
} setting_wifi_cmd_type_t;

typedef struct {
    setting_wifi_cmd_type_t command;
    char ssid[SETTING_WIFI_SSID_MAX_LEN + 1];
    char password[SETTING_WIFI_PASSWORD_MAX_LEN + 1];
} setting_wifi_command_t;

typedef struct {
    size_t count;
    wifi_manager_ap_record_t aps[WIFI_MANAGER_SCAN_MAX_APS];
} setting_wifi_scan_payload_t;

static setting_status_t s_status = {0};
static const uint8_t s_wifi_subscription = 1;

static void setting_service_request_ui_refresh(app_event_type_t type)
{
    SETTING_SERVICE_LOGI("request UI refresh: type=%d", type);

    QueueHandle_t queue = app_manager_get_event_queue();
    if (!queue) {
        SETTING_SERVICE_LOGW("request UI refresh skipped: app event queue is NULL");
        return;
    }

    app_event_t event = {
        .type = type,
        .value = 0,
    };
    snprintf(event.source_app_id, sizeof(event.source_app_id), "%s", SETTING_APP_ID);
    if (xQueueSend(queue, &event, 0) != pdTRUE) {
        SETTING_SERVICE_LOGW("request UI refresh failed: queue send timeout type=%d", type);
    }
}

static void setting_service_handle_wifi_scan(const data_center_event_t *event)
{
    SETTING_SERVICE_LOGI("handle WiFi scan event: event=%p", (const void *)event);

    if (!event || event->payload_len < offsetof(setting_wifi_scan_payload_t, aps)) {
        SETTING_SERVICE_LOGW("handle WiFi scan skipped: invalid payload_len=%u", event ? (unsigned)event->payload_len : 0U);
        return;
    }

    setting_wifi_scan_payload_t scan = {0};
    memcpy(&scan.count, event->payload, offsetof(setting_wifi_scan_payload_t, aps));

    size_t available_count = (event->payload_len - offsetof(setting_wifi_scan_payload_t, aps)) / sizeof(scan.aps[0]);
    if (scan.count > available_count) {
        SETTING_SERVICE_LOGW("WiFi scan count truncated: advertised=%u available=%u", (unsigned)scan.count, (unsigned)available_count);
        scan.count = available_count;
    }
    if (scan.count > WIFI_MANAGER_SCAN_MAX_APS) {
        scan.count = WIFI_MANAGER_SCAN_MAX_APS;
    }
    memcpy(scan.aps, event->payload + offsetof(setting_wifi_scan_payload_t, aps), scan.count * sizeof(scan.aps[0]));

    s_status.scanning = false;
    s_status.wifi_scan_valid = true;
    s_status.wifi_count = scan.count < SETTING_WIFI_MAX_APS ? scan.count : SETTING_WIFI_MAX_APS;
    SETTING_SERVICE_LOGI("WiFi scan parsed: count=%u limited_count=%u", (unsigned)scan.count, (unsigned)s_status.wifi_count);
    for (size_t i = 0; i < s_status.wifi_count; ++i) {
        snprintf(s_status.wifi_aps[i].ssid, sizeof(s_status.wifi_aps[i].ssid), "%s", scan.aps[i].ssid);
        s_status.wifi_aps[i].rssi = scan.aps[i].rssi;
        s_status.wifi_aps[i].authmode = scan.aps[i].authmode;
        s_status.wifi_aps[i].primary = scan.aps[i].primary;
        SETTING_SERVICE_LOGI("WiFi AP[%u]: ssid=%s rssi=%d auth=%d channel=%u",
                             (unsigned)i,
                             s_status.wifi_aps[i].ssid[0] ? s_status.wifi_aps[i].ssid : "<hidden>",
                             s_status.wifi_aps[i].rssi,
                             s_status.wifi_aps[i].authmode,
                             s_status.wifi_aps[i].primary);
    }
}

static void setting_service_handle_wifi_status(const data_center_event_t *event)
{
    SETTING_SERVICE_LOGI("handle WiFi status event: event=%p", (const void *)event);

    if (!event || event->payload_len < sizeof(wifi_manager_status_t)) {
        SETTING_SERVICE_LOGW("handle WiFi status skipped: invalid payload_len=%u", event ? (unsigned)event->payload_len : 0U);
        return;
    }

    wifi_manager_status_t status = {0};
    memcpy(&status, event->payload, sizeof(status));

    s_status.wifi_enabled = status.enabled;
    s_status.wifi_connected = status.connected;
    s_status.wifi_state = status.state;
    snprintf(s_status.wifi_ssid, sizeof(s_status.wifi_ssid), "%s", status.ssid);
    s_status.last_error = status.last_error;
    s_status.scanning = status.state == WIFI_MANAGER_STATE_SCANNING;
    if (!s_status.wifi_enabled) {
        s_status.wifi_scan_valid = false;
        s_status.wifi_count = 0;
    }
    SETTING_SERVICE_LOGI("WiFi status parsed: enabled=%d connected=%d state=%d last_error=%d scanning=%d",
                         status.enabled,
                         status.connected,
                         status.state,
                         status.last_error,
                         s_status.scanning);
}

static void setting_service_handle_data_event(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    SETTING_SERVICE_LOGI("handle data event: event=%p user_ctx=%p", (const void *)event, user_ctx);

    if (!event) {
        SETTING_SERVICE_LOGW("handle data event skipped: event is NULL");
        return;
    }

    SETTING_SERVICE_LOGI("data event received: source=%s target=%s topic=%s type=%u payload_len=%u flags=0x%lx",
                         event->source,
                         event->target,
                         event->topic,
                         (unsigned)event->type,
                         (unsigned)event->payload_len,
                         (unsigned long)event->flags);

    if (strcmp(event->topic, SETTING_WIFI_SCAN_TOPIC) == 0) {
        setting_service_handle_wifi_scan(event);
        setting_service_request_ui_refresh(APP_EVT_SETTING_WIFI_SCAN_DONE);
    } else if (strcmp(event->topic, SETTING_WIFI_STATUS_TOPIC) == 0) {
        setting_service_handle_wifi_status(event);
        setting_service_request_ui_refresh(APP_EVT_SETTING_WIFI_STATUS_CHANGED);
    } else {
        SETTING_SERVICE_LOGW("unhandled data event topic=%s type=%u", event->topic, (unsigned)event->type);
    }
}

esp_err_t setting_service_start(void)
{
    SETTING_SERVICE_LOGI("setting service start");

    data_center_filter_t filter = {0};
    snprintf(filter.owner, sizeof(filter.owner), "%s", SETTING_APP_ID);
    snprintf(filter.target, sizeof(filter.target), "%s", SETTING_APP_ID);
    filter.type = DATA_CENTER_EVENT_TYPE_ANY;

    esp_err_t ret = data_center_subscribe_event(&filter, setting_service_handle_data_event, (void *)&s_wifi_subscription);
    SETTING_SERVICE_LOGI("setting service subscribe: owner=%s target=%s type=%u ret=%s",
                         filter.owner,
                         filter.target,
                         (unsigned)filter.type,
                         esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_stop(void)
{
    SETTING_SERVICE_LOGI("setting service stop");

    esp_err_t ret = data_center_unsubscribe_event(setting_service_handle_data_event, (void *)&s_wifi_subscription);
    SETTING_SERVICE_LOGI("setting service unsubscribe: ret=%s", esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_enable_wifi(void)
{
    SETTING_SERVICE_LOGI("setting service enable WiFi");

    setting_wifi_command_t command = {
        .command = SETTING_WIFI_CMD_ENABLE,
    };

    data_center_event_t event = {0};
    snprintf(event.source, sizeof(event.source), "%s", SETTING_APP_ID);
    snprintf(event.target, sizeof(event.target), "%s", SETTING_WIFI_COMMAND_TARGET);
    snprintf(event.topic, sizeof(event.topic), "%s", SETTING_WIFI_COMMAND_TOPIC);
    event.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    event.flags = DATA_CENTER_FLAG_NO_SELF_ECHO;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));

    esp_err_t ret = data_center_publish_event(&event);
    SETTING_SERVICE_LOGI("publish WiFi command: source=%s target=%s topic=%s type=%u command=%d ret=%s",
                         event.source,
                         event.target,
                         event.topic,
                         (unsigned)event.type,
                         command.command,
                         esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_disable_wifi(void)
{
    SETTING_SERVICE_LOGI("setting service disable WiFi");

    setting_wifi_command_t command = {
        .command = SETTING_WIFI_CMD_DISABLE,
    };

    data_center_event_t event = {0};
    snprintf(event.source, sizeof(event.source), "%s", SETTING_APP_ID);
    snprintf(event.target, sizeof(event.target), "%s", SETTING_WIFI_COMMAND_TARGET);
    snprintf(event.topic, sizeof(event.topic), "%s", SETTING_WIFI_COMMAND_TOPIC);
    event.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    event.flags = DATA_CENTER_FLAG_NO_SELF_ECHO;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));

    esp_err_t ret = data_center_publish_event(&event);
    SETTING_SERVICE_LOGI("publish WiFi disable command: ret=%s", esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_scan_wifi(void)
{
    SETTING_SERVICE_LOGI("setting service scan WiFi");

    setting_wifi_command_t command = {
        .command = SETTING_WIFI_CMD_SCAN,
    };

    data_center_event_t event = {0};
    snprintf(event.source, sizeof(event.source), "%s", SETTING_APP_ID);
    snprintf(event.target, sizeof(event.target), "%s", SETTING_WIFI_COMMAND_TARGET);
    snprintf(event.topic, sizeof(event.topic), "%s", SETTING_WIFI_COMMAND_TOPIC);
    event.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    event.flags = DATA_CENTER_FLAG_NO_SELF_ECHO;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));

    esp_err_t ret = data_center_publish_event(&event);
    SETTING_SERVICE_LOGI("publish WiFi command: source=%s target=%s topic=%s type=%u command=%d ret=%s",
                         event.source,
                         event.target,
                         event.topic,
                         (unsigned)event.type,
                         command.command,
                         esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_request_wifi_status(void)
{
    SETTING_SERVICE_LOGI("setting service request WiFi status");

    s_status.wifi_scan_valid = false;

    setting_wifi_command_t command = {
        .command = SETTING_WIFI_CMD_GET_STATUS,
    };

    data_center_event_t event = {0};
    snprintf(event.source, sizeof(event.source), "%s", SETTING_APP_ID);
    snprintf(event.target, sizeof(event.target), "%s", SETTING_WIFI_COMMAND_TARGET);
    snprintf(event.topic, sizeof(event.topic), "%s", SETTING_WIFI_COMMAND_TOPIC);
    event.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    event.flags = DATA_CENTER_FLAG_NO_SELF_ECHO;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));

    esp_err_t ret = data_center_publish_event(&event);
    SETTING_SERVICE_LOGI("publish WiFi status request command: ret=%s", esp_err_to_name(ret));
    return ret;
}

esp_err_t setting_service_connect_wifi(const char *ssid, const char *password)
{
    SETTING_SERVICE_LOGI("setting service connect WiFi: ssid=%s", ssid ? ssid : "<null>");

    if (!ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    setting_wifi_command_t command = {
        .command = SETTING_WIFI_CMD_CONNECT,
    };
    snprintf(command.ssid, sizeof(command.ssid), "%s", ssid);
    snprintf(command.password, sizeof(command.password), "%s", password ? password : "");

    snprintf(s_status.wifi_ssid, sizeof(s_status.wifi_ssid), "%s", ssid);
    s_status.wifi_state = WIFI_MANAGER_STATE_CONNECTING;
    s_status.wifi_connected = false;

    data_center_event_t event = {0};
    snprintf(event.source, sizeof(event.source), "%s", SETTING_APP_ID);
    snprintf(event.target, sizeof(event.target), "%s", SETTING_WIFI_COMMAND_TARGET);
    snprintf(event.topic, sizeof(event.topic), "%s", SETTING_WIFI_COMMAND_TOPIC);
    event.type = DATA_CENTER_EVENT_WIFI_COMMAND;
    event.flags = DATA_CENTER_FLAG_NO_SELF_ECHO;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));

    esp_err_t ret = data_center_publish_event(&event);
    SETTING_SERVICE_LOGI("publish WiFi connect command: ssid=%s ret=%s", command.ssid, esp_err_to_name(ret));
    return ret;
}

void setting_service_get_status(setting_status_t *status)
{
    SETTING_SERVICE_LOGI("setting service get status: status=%p", (void *)status);

    if (!status) {
        SETTING_SERVICE_LOGW("setting service get status skipped: status is NULL");
        return;
    }

    memcpy(status, &s_status, sizeof(*status));
    SETTING_SERVICE_LOGI("setting status copied: wifi_enabled=%d wifi_state=%d scanning=%d wifi_count=%u last_error=%d",
                         status->wifi_enabled,
                         status->wifi_state,
                         status->scanning,
                         (unsigned)status->wifi_count,
                         status->last_error);
}