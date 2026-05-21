#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t ai_talk_view_open(lv_obj_t *root);
void ai_talk_view_render_count(int count);
