#include "rtc_manager.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "rtc_manager";

static pcf85063a_dev_t s_rtc_device;
static bool s_rtc_ready = false;

static bool rtc_manager_datetime_is_valid(const rtc_manager_datetime_t *datetime)
{
    if (datetime == NULL) {
        return false;
    }

    return datetime->year >= 1970 &&
           datetime->month >= 1 && datetime->month <= 12 &&
           datetime->day >= 1 && datetime->day <= 31 &&
           datetime->weekday <= 6 &&
           datetime->hour <= 23 &&
           datetime->minute <= 59 &&
           datetime->second <= 59;
}

static void rtc_manager_from_pcf_datetime(const pcf85063a_datetime_t *src,
                                          rtc_manager_datetime_t *dst)
{
    dst->year = src->year;
    dst->month = src->month;
    dst->day = src->day;
    dst->weekday = src->dotw;
    dst->hour = src->hour;
    dst->minute = src->min;
    dst->second = src->sec;
}

static void rtc_manager_to_pcf_datetime(const rtc_manager_datetime_t *src,
                                        pcf85063a_datetime_t *dst)
{
    dst->year = src->year;
    dst->month = src->month;
    dst->day = src->day;
    dst->dotw = src->weekday;
    dst->hour = src->hour;
    dst->min = src->minute;
    dst->sec = src->second;
}

esp_err_t rtc_manager_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr)
{
    if (s_rtc_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(bus_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "I2C bus handle is NULL");

    esp_err_t ret = pcf85063a_init(&s_rtc_device, bus_handle, i2c_addr);
    if (ret != ESP_OK) {
        s_rtc_ready = false;
        return ret;
    }

    s_rtc_ready = true;
    ESP_LOGI(TAG, "RTC manager initialized");
    return ESP_OK;
}

bool rtc_manager_is_ready(void)
{
    return s_rtc_ready;
}

pcf85063a_dev_t *rtc_manager_get_device(void)
{
    return s_rtc_ready ? &s_rtc_device : NULL;
}

esp_err_t rtc_manager_get_datetime(rtc_manager_datetime_t *out_datetime)
{
    ESP_RETURN_ON_FALSE(out_datetime != NULL, ESP_ERR_INVALID_ARG, TAG, "Output datetime is NULL");
    ESP_RETURN_ON_FALSE(s_rtc_ready, ESP_ERR_INVALID_STATE, TAG, "RTC manager is not ready");

    pcf85063a_datetime_t rtc_datetime;
    ESP_RETURN_ON_ERROR(pcf85063a_get_time_date(&s_rtc_device, &rtc_datetime), TAG, "Failed to read RTC");

    rtc_manager_from_pcf_datetime(&rtc_datetime, out_datetime);
    return ESP_OK;
}

esp_err_t rtc_manager_set_datetime(const rtc_manager_datetime_t *datetime)
{
    ESP_RETURN_ON_FALSE(rtc_manager_datetime_is_valid(datetime), ESP_ERR_INVALID_ARG, TAG, "Invalid datetime");
    ESP_RETURN_ON_FALSE(s_rtc_ready, ESP_ERR_INVALID_STATE, TAG, "RTC manager is not ready");

    pcf85063a_datetime_t rtc_datetime;
    rtc_manager_to_pcf_datetime(datetime, &rtc_datetime);

    return pcf85063a_set_time_date(&s_rtc_device, rtc_datetime);
}
