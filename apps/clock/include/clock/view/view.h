#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t clock_view_open(lv_obj_t *root);
void clock_view_render_count(int count);