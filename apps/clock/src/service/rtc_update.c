#include "clock/service/rtc_update.h"

#include <stdbool.h>

#include "esp_timer.h"
#include "rtc_manager.h"

#define CLOCK_RTC_US_PER_SEC 1000000LL

static clock_rtc_datetime_t s_base_datetime;
static int64_t s_base_time_us;
static bool s_cache_valid = false;

static bool clock_rtc_update_is_leap_year(uint16_t year)
{
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static uint8_t clock_rtc_update_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month == 2 && clock_rtc_update_is_leap_year(year)) {
        return 29;
    }

    return days[month - 1];
}

static void clock_rtc_update_add_seconds(clock_rtc_datetime_t *datetime, int64_t seconds)
{
    while (seconds-- > 0) {
        if (++datetime->second < 60) {
            continue;
        }
        datetime->second = 0;

        if (++datetime->minute < 60) {
            continue;
        }
        datetime->minute = 0;

        if (++datetime->hour < 24) {
            continue;
        }
        datetime->hour = 0;
        datetime->weekday = (datetime->weekday + 1) % 7;

        if (++datetime->day <= clock_rtc_update_days_in_month(datetime->year, datetime->month)) {
            continue;
        }
        datetime->day = 1;

        if (++datetime->month <= 12) {
            continue;
        }
        datetime->month = 1;
        datetime->year++;
    }
}

static void clock_rtc_update_cache_set(const clock_rtc_datetime_t *datetime)
{
    s_base_datetime = *datetime;
    s_base_time_us = esp_timer_get_time();
    s_cache_valid = true;
}

static void clock_rtc_update_from_manager_datetime(const rtc_manager_datetime_t *src,
                                                   clock_rtc_datetime_t *dst)
{
    dst->year = src->year;
    dst->month = src->month;
    dst->day = src->day;
    dst->weekday = src->weekday;
    dst->hour = src->hour;
    dst->minute = src->minute;
    dst->second = src->second;
}

static void clock_rtc_update_to_manager_datetime(const clock_rtc_datetime_t *src,
                                                 rtc_manager_datetime_t *dst)
{
    dst->year = src->year;
    dst->month = src->month;
    dst->day = src->day;
    dst->weekday = src->weekday;
    dst->hour = src->hour;
    dst->minute = src->minute;
    dst->second = src->second;
}

esp_err_t clock_rtc_update_read(clock_rtc_datetime_t *out_datetime)
{
    if (out_datetime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    rtc_manager_datetime_t rtc_datetime;
    esp_err_t ret = rtc_manager_get_datetime(&rtc_datetime);
    if (ret != ESP_OK) {
        return ret;
    }

    clock_rtc_update_from_manager_datetime(&rtc_datetime, out_datetime);
    return ESP_OK;
}

esp_err_t clock_rtc_update_write(const clock_rtc_datetime_t *datetime)
{
    if (datetime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    rtc_manager_datetime_t rtc_datetime;
    clock_rtc_update_to_manager_datetime(datetime, &rtc_datetime);

    esp_err_t ret = rtc_manager_set_datetime(&rtc_datetime);
    if (ret != ESP_OK) {
        return ret;
    }

    clock_rtc_update_cache_set(datetime);
    return ESP_OK;
}

esp_err_t clock_rtc_update_sync_from_rtc(void)
{
    clock_rtc_datetime_t datetime;
    esp_err_t ret = clock_rtc_update_read(&datetime);
    if (ret != ESP_OK) {
        return ret;
    }

    clock_rtc_update_cache_set(&datetime);
    return ESP_OK;
}

esp_err_t clock_rtc_update_get_current(clock_rtc_datetime_t *out_datetime)
{
    if (out_datetime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_cache_valid) {
        esp_err_t ret = clock_rtc_update_sync_from_rtc();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    *out_datetime = s_base_datetime;
    int64_t elapsed_sec = (esp_timer_get_time() - s_base_time_us) / CLOCK_RTC_US_PER_SEC;
    clock_rtc_update_add_seconds(out_datetime, elapsed_sec);
    return ESP_OK;
}
