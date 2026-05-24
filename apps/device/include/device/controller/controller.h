#pragma once

#include "app_manager.h"
#include "lvgl.h"

esp_err_t device_controller_open(lv_obj_t *root);
void device_controller_close(void);
void device_controller_handle_ui_event(const app_event_t *event);