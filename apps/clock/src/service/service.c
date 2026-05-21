#include "clock/service/service.h"

#include "clock/context.h"
#include "clock/contract.h"
#include "clock/store/store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_manager.h"

static void clock_service_task(void *arg)
{
    clock_context_t *context = clock_context_get();
    QueueHandle_t queue = app_manager_get_event_queue();

    for (int count = CLOCK_COUNTDOWN_START_VALUE; count >= 0 && !context->stop_requested; --count) {
        app_event_t event = {
            .type = APP_EVT_COUNTDOWN_UPDATE,
            .value = count,
        };
        clock_store_set_count(count);
        xQueueSend(queue, &event, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    app_event_t finished_event = {
        .type = APP_EVT_APP_FINISHED,
        .value = 0,
    };
    xQueueSend(queue, &finished_event, 0);

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
    clock_store_set_count(CLOCK_COUNTDOWN_START_VALUE);

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