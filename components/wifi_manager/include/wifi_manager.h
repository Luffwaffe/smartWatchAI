#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_SSID_MAX_LEN 32
#define WIFI_MANAGER_PASSWORD_MAX_LEN 64
#define WIFI_MANAGER_SCAN_MAX_APS 16

typedef enum {
    WIFI_MANAGER_STATE_OFF = 0,
    WIFI_MANAGER_STATE_IDLE,
    WIFI_MANAGER_STATE_SCANNING,
    WIFI_MANAGER_STATE_CONNECTING,
    WIFI_MANAGER_STATE_CONNECTED,
    WIFI_MANAGER_STATE_DISCONNECTED,
    WIFI_MANAGER_STATE_ERROR,
} wifi_manager_state_t;

typedef enum {
    WIFI_MANAGER_EVT_STATUS_CHANGED = 1,
    WIFI_MANAGER_EVT_SCAN_DONE,
    WIFI_MANAGER_EVT_CONNECTED,
    WIFI_MANAGER_EVT_DISCONNECTED,
    WIFI_MANAGER_EVT_ERROR,
} wifi_manager_event_type_t;

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    int8_t rssi;
    wifi_auth_mode_t authmode;
    uint8_t primary;
} wifi_manager_ap_record_t;

typedef struct {
    bool enabled;
    bool connected;
    wifi_manager_state_t state;
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    uint8_t ip[4];
    int last_error;
} wifi_manager_status_t;

typedef struct {
    wifi_manager_event_type_t type;
    union {
        wifi_manager_status_t status;
        struct {
            wifi_manager_ap_record_t aps[WIFI_MANAGER_SCAN_MAX_APS];
            size_t count;
        } scan;
        int error_code;
    } data;
} wifi_manager_event_t;

typedef void (*wifi_manager_event_cb_t)(const wifi_manager_event_t *event, void *user_ctx);

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
esp_err_t wifi_manager_stop(void);

esp_err_t wifi_manager_enable(void);
esp_err_t wifi_manager_disable(void);
esp_err_t wifi_manager_scan(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_get_status(wifi_manager_status_t *status);

esp_err_t wifi_manager_register_callback(wifi_manager_event_cb_t cb, void *user_ctx);
esp_err_t wifi_manager_unregister_callback(wifi_manager_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
