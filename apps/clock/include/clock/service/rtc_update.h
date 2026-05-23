#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} clock_rtc_datetime_t;

esp_err_t clock_rtc_update_read(clock_rtc_datetime_t *out_datetime);
esp_err_t clock_rtc_update_write(const clock_rtc_datetime_t *datetime);
esp_err_t clock_rtc_update_sync_from_rtc(void);
esp_err_t clock_rtc_update_get_current(clock_rtc_datetime_t *out_datetime);