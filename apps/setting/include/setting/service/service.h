#pragma once

#include "esp_err.h"
#include "wifi_manager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SETTING_WIFI_MAX_APS WIFI_MANAGER_SCAN_MAX_APS
#define SETTING_WIFI_SSID_MAX_LEN WIFI_MANAGER_SSID_MAX_LEN
#define SETTING_WIFI_PASSWORD_MAX_LEN WIFI_MANAGER_PASSWORD_MAX_LEN

typedef struct {
    char ssid[SETTING_WIFI_SSID_MAX_LEN + 1];
    int8_t rssi;
    wifi_auth_mode_t authmode;
    uint8_t primary;
} setting_wifi_ap_t;

typedef struct {
    bool wifi_enabled;
    bool scanning;
    bool wifi_scan_valid;
    wifi_manager_state_t wifi_state;
    bool wifi_connected;
    char wifi_ssid[SETTING_WIFI_SSID_MAX_LEN + 1];
    int last_error;
    size_t wifi_count;
    setting_wifi_ap_t wifi_aps[SETTING_WIFI_MAX_APS];
} setting_status_t;

esp_err_t setting_service_start(void);
esp_err_t setting_service_stop(void);
esp_err_t setting_service_enable_wifi(void);
esp_err_t setting_service_disable_wifi(void);
esp_err_t setting_service_scan_wifi(void);
esp_err_t setting_service_request_wifi_status(void);
esp_err_t setting_service_connect_wifi(const char *ssid, const char *password);
void setting_service_get_status(setting_status_t *status);