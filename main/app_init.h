/**
 * Board/application initialization API.
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct app_hw_status_t {
    bool nvs_ok;
    bool display_ok;
    bool touch_ok;
    bool pmu_ok;
    bool imu_ok;
    bool rtc_ok;
    bool audio_ok;
    bool mic_ok;
} app_hw_status_t;

esp_err_t app_system_init(app_hw_status_t *status);
