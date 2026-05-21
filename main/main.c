#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "app_init.h"
#include "ai_talk/ai_talk.h"
#include "clock/clock.h"
#include "setting/setting.h"
#include "test/test.h"
#include "app_manager.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " ESP32-C6-Touch-AMOLED-2.06 Development Template");
    ESP_LOGI(TAG, "=================================================");

    app_hw_status_t hw_status;
    ESP_ERROR_CHECK(app_system_init(&hw_status));
    ESP_ERROR_CHECK(app_manager_init());

    //App registration
    clock_app_register();
    ai_talk_app_register();
    setting_app_register();
    test_app_register();
    ESP_ERROR_CHECK(app_manager_show_launcher(&hw_status));

    //end
    
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " Initialization Complete");
    ESP_LOGI(TAG, "=================================================");

    /* Main loop - Add your application logic here */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
