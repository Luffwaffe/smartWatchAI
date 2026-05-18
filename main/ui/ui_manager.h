/**
 * LVGL UI entry points.
 */

#pragma once

#include "esp_err.h"
#include "app_init.h"

esp_err_t ui_manager_start(const app_hw_status_t *hw_status);
