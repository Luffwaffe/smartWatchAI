#pragma once

#include "app_manager.h"
#include "lvgl.h"

esp_err_t clock_controller_open(lv_obj_t *root);
void clock_controller_handle_ui_event(const app_event_t *event);
