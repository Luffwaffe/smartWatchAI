#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    const char *id;
    esp_err_t (*open)(lv_obj_t *parent);
    void (*close)(void);
} clock_theme_t;
