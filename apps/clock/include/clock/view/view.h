#pragma once

#include "esp_err.h"
#include "lvgl.h"

#include "clock/service/rtc_update.h"

esp_err_t clock_view_open(lv_obj_t *root);
void clock_view_close(void);
void clock_view_next_theme(void);
void clock_view_prev_theme(void);
void clock_view_set_datetime(const clock_rtc_datetime_t *datetime);
