#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "clock/service/rtc_update.h"
#include "lvgl.h"

typedef struct {
    TaskHandle_t task;
    bool stop_requested;
    lv_obj_t *root;
    lv_obj_t *theme_container;
    lv_obj_t *hour_label;
    lv_obj_t *minute_label;
    lv_obj_t *second_label;
    lv_obj_t *day_label;
    lv_obj_t *month_label;
    lv_obj_t *year_label;
    int current_theme;
    clock_rtc_datetime_t current_datetime;
    bool current_datetime_valid;
} clock_context_t;

clock_context_t *clock_context_get(void);
void clock_context_set_datetime(const clock_rtc_datetime_t *datetime);
bool clock_context_get_datetime(clock_rtc_datetime_t *out_datetime);
