#pragma once

#include "device/service/service.h"
#include "esp_err.h"
#include "lvgl.h"

typedef void (*device_view_bluetooth_toggle_cb_t)(bool enabled);

esp_err_t device_view_open(lv_obj_t *root, device_view_bluetooth_toggle_cb_t toggle_cb);
void device_view_close(void);
void device_view_set_status(const device_status_t *status);