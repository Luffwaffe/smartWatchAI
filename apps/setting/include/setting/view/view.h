#pragma once

#include "setting/service/service.h"
#include "esp_err.h"
#include "lvgl.h"

typedef void (*setting_view_wifi_scan_cb_t)(void);
typedef void (*setting_view_wifi_disable_cb_t)(void);
typedef void (*setting_view_wifi_connect_cb_t)(const char *ssid, const char *password);
typedef void (*setting_view_wifi_status_request_cb_t)(void);

esp_err_t setting_view_open(lv_obj_t *root,
                            setting_view_wifi_scan_cb_t scan_cb,
                            setting_view_wifi_disable_cb_t disable_cb,
                            setting_view_wifi_connect_cb_t connect_cb,
                            setting_view_wifi_status_request_cb_t status_request_cb);
void setting_view_close(void);
void setting_view_set_status(const setting_status_t *status);