/**
 * AXP2101 Power Management IC C++ Wrapper
 *
 * This file provides C-callable functions for the XPowersLib C++ library.
 * The AXP2101 handles battery charging, power rail management, and monitoring.
 */

#include <stdio.h>
#include <cstring>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "bsp/esp32_c6_touch_amoled_2_06.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static const char *TAG = "AXP2101";

static XPowersPMU PMU;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t pmu_dev_handle = NULL;

/**
 * @brief Read register from PMU via I2C
 */
int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    if (pmu_dev_handle == NULL) {
        return -1;
    }

    esp_err_t ret = i2c_master_transmit_receive(pmu_dev_handle, &regAddr, 1, data, len, -1);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief Write register to PMU via I2C
 */
int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    if (pmu_dev_handle == NULL) {
        return -1;
    }

    uint8_t buffer[len + 1];
    buffer[0] = regAddr;
    memcpy(&buffer[1], data, len);

    esp_err_t ret = i2c_master_transmit(pmu_dev_handle, buffer, len + 1, -1);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief Initialize I2C device for PMU
 */
static esp_err_t init_i2c_device(void)
{
    // Get I2C bus handle from BSP
    i2c_bus_handle = bsp_i2c_get_handle();

    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle");
        return ESP_FAIL;
    }

    // Add PMU device to I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_SLAVE_ADDRESS,
        .scl_speed_hz = 400000,
        .scl_wait_us = 0,
        .flags = {0},
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &pmu_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add PMU device to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Initialize PMU (C-callable function)
 */
extern "C" esp_err_t axp2101_pmu_init(void)
{
    // Initialize I2C device
    esp_err_t ret = init_i2c_device();
    if (ret != ESP_OK) {
        return ret;
    }

    // Initialize PMU library
    if (!PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGE(TAG, "Failed to initialize PMU library");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "PMU initialized successfully");

    // Enable voltage and current measurement
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();
    PMU.disableTSPinMeasure();

    // Set battery charging parameters
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    return ESP_OK;
}

/**
 * @brief Print PMU status (C-callable function)
 */
extern "C" void axp2101_print_status(void)
{
    ESP_LOGI(TAG, "=== Power Management Status ===");

    ESP_LOGI(TAG, "DC-DC Converters:");
    ESP_LOGI(TAG, "  DC1: %s %4u mV", PMU.isEnableDC1() ? "ON " : "OFF", PMU.getDC1Voltage());
    ESP_LOGI(TAG, "  DC2: %s %4u mV", PMU.isEnableDC2() ? "ON " : "OFF", PMU.getDC2Voltage());
    ESP_LOGI(TAG, "  DC3: %s %4u mV", PMU.isEnableDC3() ? "ON " : "OFF", PMU.getDC3Voltage());
    ESP_LOGI(TAG, "  DC4: %s %4u mV", PMU.isEnableDC4() ? "ON " : "OFF", PMU.getDC4Voltage());
    ESP_LOGI(TAG, "  DC5: %s %4u mV", PMU.isEnableDC5() ? "ON " : "OFF", PMU.getDC5Voltage());

    ESP_LOGI(TAG, "LDO Regulators (ALDO):");
    ESP_LOGI(TAG, "  ALDO1: %s %4u mV", PMU.isEnableALDO1() ? "ON " : "OFF", PMU.getALDO1Voltage());
    ESP_LOGI(TAG, "  ALDO2: %s %4u mV", PMU.isEnableALDO2() ? "ON " : "OFF", PMU.getALDO2Voltage());
    ESP_LOGI(TAG, "  ALDO3: %s %4u mV", PMU.isEnableALDO3() ? "ON " : "OFF", PMU.getALDO3Voltage());
    ESP_LOGI(TAG, "  ALDO4: %s %4u mV", PMU.isEnableALDO4() ? "ON " : "OFF", PMU.getALDO4Voltage());

    ESP_LOGI(TAG, "LDO Regulators (BLDO):");
    ESP_LOGI(TAG, "  BLDO1: %s %4u mV", PMU.isEnableBLDO1() ? "ON " : "OFF", PMU.getBLDO1Voltage());
    ESP_LOGI(TAG, "  BLDO2: %s %4u mV", PMU.isEnableBLDO2() ? "ON " : "OFF", PMU.getBLDO2Voltage());

    ESP_LOGI(TAG, "LDO Regulators (DLDO):");
    ESP_LOGI(TAG, "  DLDO1: %s %4u mV", PMU.isEnableDLDO1() ? "ON " : "OFF", PMU.getDLDO1Voltage());
    ESP_LOGI(TAG, "  DLDO2: %s %4u mV", PMU.isEnableDLDO2() ? "ON " : "OFF", PMU.getDLDO2Voltage());

    ESP_LOGI(TAG, "CPU LDO:");
    ESP_LOGI(TAG, "  CPUSLDO: %s %4u mV", PMU.isEnableCPUSLDO() ? "ON " : "OFF", PMU.getCPUSLDOVoltage());

    if (PMU.isBatteryConnect()) {
        ESP_LOGI(TAG, "Battery:");
        ESP_LOGI(TAG, "  Voltage: %u mV", PMU.getBattVoltage());
        ESP_LOGI(TAG, "  Charging: %s", PMU.isCharging() ? "Yes" : "No");
        if (PMU.isCharging()) {
            ESP_LOGI(TAG, "  Charge Current: %u mA", PMU.getChargerConstantCurr());
        }
    } else {
        ESP_LOGI(TAG, "Battery: Not connected");
    }

    ESP_LOGI(TAG, "System:");
    ESP_LOGI(TAG, "  VBUS: %u mV", PMU.getVbusVoltage());
    ESP_LOGI(TAG, "  System Voltage: %u mV", PMU.getSystemVoltage());
    ESP_LOGI(TAG, "  Temperature: %.1f °C", PMU.getTemperature());

    ESP_LOGI(TAG, "===============================");
}
