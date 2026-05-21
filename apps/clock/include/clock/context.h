#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

typedef struct {
    TaskHandle_t task;
    bool stop_requested;
    lv_obj_t *count_label;
} clock_context_t;

clock_context_t *clock_context_get(void);