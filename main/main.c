#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "app_init.h"
#include "ui/ui_manager.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " ESP32-C6-Touch-AMOLED-2.06 Development Template");
    ESP_LOGI(TAG, "=================================================");

    app_hw_status_t hw_status;
    ESP_ERROR_CHECK(app_system_init(&hw_status));
    ESP_ERROR_CHECK(ui_manager_start(&hw_status));

    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " Initialization Complete");
    ESP_LOGI(TAG, "=================================================");

    /* Main loop - Add your application logic here */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
