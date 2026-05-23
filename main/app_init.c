/**
 * Hardware initialization for the smartwatch application.
 */

#include "app_init.h"

#include <string.h>

#include "app_config.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "qmi8658.h"
#include "rtc_manager.h"

#ifdef __cplusplus
extern "C" {
#endif
    extern esp_err_t axp2101_pmu_init(void);
    extern void axp2101_print_status(void);
#ifdef __cplusplus
}
#endif

static const char *TAG = "app_init";

static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;
static qmi8658_dev_t s_imu_device;
static lv_display_t *s_display = NULL;

static esp_err_t init_nvs(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Failed to erase NVS");
        ret = nvs_flash_init();
    }

    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize NVS");
    ESP_LOGI(TAG, "NVS initialized successfully");
    return ESP_OK;
}

static esp_err_t init_display(void)
{
    ESP_LOGI(TAG, "Initializing display and touch...");

    s_display = bsp_display_start();
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_FAIL, TAG, "Failed to initialize display");

    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Display initialized: %dx%d pixels", APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT);
    return ESP_OK;
}

static esp_err_t init_pmu(void)
{
    ESP_LOGI(TAG, "Initializing Power Management Unit (AXP2101)...");

    ESP_RETURN_ON_ERROR(axp2101_pmu_init(), TAG, "Failed to initialize PMU");
    axp2101_print_status();

    ESP_LOGI(TAG, "PMU initialized successfully");
    return ESP_OK;
}

static esp_err_t init_imu(void)
{
    ESP_LOGI(TAG, "Initializing IMU (QMI8658)...");

    s_i2c_bus_handle = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(s_i2c_bus_handle != NULL, ESP_FAIL, TAG, "Failed to get I2C bus handle");

    ESP_RETURN_ON_ERROR(qmi8658_init(&s_imu_device, s_i2c_bus_handle, APP_QMI8658_I2C_ADDRESS),
                        TAG, "Failed to initialize IMU");

    qmi8658_set_accel_range(&s_imu_device, APP_IMU_ACCEL_RANGE);
    qmi8658_set_accel_odr(&s_imu_device, APP_IMU_ACCEL_ODR);
    qmi8658_set_accel_unit_mps2(&s_imu_device, true);

    qmi8658_set_gyro_range(&s_imu_device, APP_IMU_GYRO_RANGE);
    qmi8658_set_gyro_odr(&s_imu_device, APP_IMU_GYRO_ODR);
    qmi8658_set_gyro_unit_rads(&s_imu_device, true);

    qmi8658_write_register(&s_imu_device, QMI8658_CTRL5, 0x03);

    ESP_LOGI(TAG, "IMU initialized: Accel 8G @ 500Hz, Gyro 512dps @ 500Hz");
    return ESP_OK;
}

static esp_err_t init_rtc(void)
{
    ESP_LOGI(TAG, "Initializing RTC (PCF85063)...");

    if (s_i2c_bus_handle == NULL) {
        s_i2c_bus_handle = bsp_i2c_get_handle();
    }
    ESP_RETURN_ON_FALSE(s_i2c_bus_handle != NULL, ESP_FAIL, TAG, "Failed to get I2C bus handle");

    ESP_RETURN_ON_ERROR(rtc_manager_init(s_i2c_bus_handle, APP_PCF85063_I2C_ADDRESS),
                        TAG, "Failed to initialize RTC");

    ESP_LOGI(TAG, "RTC initialized successfully");
    return ESP_OK;
}

static esp_err_t init_audio(void)
{
    ESP_LOGI(TAG, "Initializing audio codec (ES8311)...");

    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "Failed to initialize audio codec");

    esp_err_t ret = bsp_extra_codec_set_fs(APP_AUDIO_SAMPLE_RATE, APP_AUDIO_BIT_WIDTH, I2S_SLOT_MODE_STEREO);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set audio format: %s", esp_err_to_name(ret));
    }

    int volume_set = 0;
    ret = bsp_extra_codec_volume_set(APP_AUDIO_DEFAULT_VOLUME, &volume_set);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Audio volume set to %d%%", volume_set);
    }

    ESP_LOGI(TAG, "Audio codec initialized: %dHz, %d-bit, %d channels",
             APP_AUDIO_SAMPLE_RATE, APP_AUDIO_BIT_WIDTH, APP_AUDIO_CHANNELS);
    return ESP_OK;
}

static void log_optional_init_result(const char *name, esp_err_t ret)
{
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "%s initialization failed: %s", name, esp_err_to_name(ret));
    }
}

esp_err_t app_system_init(app_hw_status_t *status)
{
    app_hw_status_t local_status = {0};
    app_hw_status_t *hw = status ? status : &local_status;
    memset(hw, 0, sizeof(*hw));

    esp_err_t ret = init_nvs();
    hw->nvs_ok = (ret == ESP_OK);
    ESP_RETURN_ON_ERROR(ret, TAG, "Critical init failed: NVS");

    ret = init_display();
    hw->display_ok = (ret == ESP_OK);
    hw->touch_ok = hw->display_ok;
    ESP_RETURN_ON_ERROR(ret, TAG, "Critical init failed: display");

    ret = init_pmu();
    hw->pmu_ok = (ret == ESP_OK);
    log_optional_init_result("PMU", ret);

    ret = init_imu();
    hw->imu_ok = (ret == ESP_OK);
    log_optional_init_result("IMU", ret);

    ret = init_rtc();
    hw->rtc_ok = (ret == ESP_OK);
    log_optional_init_result("RTC", ret);

    ret = init_audio();
    hw->audio_ok = (ret == ESP_OK);
    hw->mic_ok = hw->audio_ok;
    log_optional_init_result("Audio", ret);

    return ESP_OK;
}
