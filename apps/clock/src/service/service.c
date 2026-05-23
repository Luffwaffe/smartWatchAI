#include "clock/service/service.h"

#include "clock/context.h"
#include "clock/contract.h"
#include "clock/service/rtc_update.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_manager.h"

#define CLOCK_RTC_RESYNC_INTERVAL_MS 60000
#define CLOCK_UI_REFRESH_INTERVAL_MS 1000

static void clock_service_update_datetime(const clock_rtc_datetime_t *datetime)
{
    if (datetime == NULL) {
        return;
    }

    clock_context_set_datetime(datetime);

    QueueHandle_t queue = app_manager_get_event_queue();
    if (queue == NULL) {
        return;
    }

    app_event_t event = {
        .type = APP_EVT_CLOCK_DATETIME_UPDATE,
        .value = 0,
    };
    xQueueSend(queue, &event, 0);
}

esp_err_t clock_service_prepare_datetime(void)
{
    esp_err_t ret = clock_rtc_update_sync_from_rtc();
    if (ret != ESP_OK) {
        return ret;
    }

    clock_rtc_datetime_t datetime;
    ret = clock_rtc_update_get_current(&datetime);
    if (ret != ESP_OK) {
        return ret;
    }

    clock_context_set_datetime(&datetime);
    return ESP_OK;
}

static void clock_service_task(void *arg)
{
    clock_context_t *context = clock_context_get();
    TickType_t last_rtc_sync_tick = xTaskGetTickCount();

    while (!context->stop_requested) {
        clock_rtc_datetime_t datetime;
        TickType_t now = xTaskGetTickCount();
        if ((now - last_rtc_sync_tick) >= pdMS_TO_TICKS(CLOCK_RTC_RESYNC_INTERVAL_MS)) {
            if (clock_rtc_update_sync_from_rtc() == ESP_OK) {
                last_rtc_sync_tick = now;
            }
        }

        if (clock_rtc_update_get_current(&datetime) == ESP_OK) {
            clock_service_update_datetime(&datetime);
        }

        vTaskDelay(pdMS_TO_TICKS(CLOCK_UI_REFRESH_INTERVAL_MS));
    }

    context->task = NULL;
    vTaskDelete(NULL);
}

esp_err_t clock_service_start(void)
{
    clock_context_t *context = clock_context_get();

    if (context->task != NULL) {
        return ESP_OK;
    }

    context->stop_requested = false;

    xTaskCreate(clock_service_task,
                "clock_svc",
                CLOCK_BACKEND_TASK_STACK_SIZE,
                NULL,
                CLOCK_BACKEND_TASK_PRIORITY,
                &context->task);
    return ESP_OK;
}

esp_err_t clock_service_stop(void)
{
    clock_context_t *context = clock_context_get();

    context->stop_requested = true;
    return ESP_OK;
}
