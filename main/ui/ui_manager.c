/**
 * Minimal LVGL UI for the smartwatch application.
 */

#include "ui_manager.h"

#include <stdio.h>

#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui_manager";

static const char *status_text(bool ok)
{
    return ok ? "OK" : "FAIL";
}

esp_err_t ui_manager_start(const app_hw_status_t *hw_status)
{
    ESP_RETURN_ON_FALSE(hw_status != NULL, ESP_ERR_INVALID_ARG, TAG, "hw_status is NULL");

    char info[256];
    snprintf(info, sizeof(info),
             "Hardware status:\n"
             "- Display: %s\n"
             "- Touch: %s\n"
             "- PMU: %s\n"
             "- IMU: %s\n"
             "- RTC: %s\n"
             "- Audio: %s\n"
             "- Mic: %s\n"
             "\n"
             "Ready for development!",
             status_text(hw_status->display_ok),
             status_text(hw_status->touch_ok),
             status_text(hw_status->pmu_ok),
             status_text(hw_status->imu_ok),
             status_text(hw_status->rtc_ok),
             status_text(hw_status->audio_ok),
             status_text(hw_status->mic_ok));

    bsp_display_lock(0);

    lv_obj_t *title = lv_label_create(lv_screen_active());
    lv_label_set_text(title, "ESP32-C6 AMOLED");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *body = lv_label_create(lv_screen_active());
    lv_label_set_text(body, info);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 0);

    bsp_display_unlock();

    ESP_LOGI(TAG, "UI started");
    return ESP_OK;
}
