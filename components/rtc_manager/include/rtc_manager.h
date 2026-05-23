#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "pcf85063a.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_manager_datetime_t;

esp_err_t rtc_manager_init(i2c_master_bus_handle_t bus_handle, uint8_t i2c_addr);
bool rtc_manager_is_ready(void);
pcf85063a_dev_t *rtc_manager_get_device(void);

esp_err_t rtc_manager_get_datetime(rtc_manager_datetime_t *out_datetime);
esp_err_t rtc_manager_set_datetime(const rtc_manager_datetime_t *datetime);

#ifdef __cplusplus
}
#endif
