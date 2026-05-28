#pragma once

#include "app_manager.h"
#include "lvgl.h"

esp_err_t setting_controller_open(lv_obj_t *root);
void setting_controller_close(void);
void setting_controller_handle_ui_event(const app_event_t *event);