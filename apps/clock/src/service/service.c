#include "clock/service/service.h"

#include "clock/context.h"
#include "clock/contract.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void clock_service_task(void *arg)
{
    clock_context_t *context = clock_context_get();

    while (!context->stop_requested) {
        vTaskDelay(pdMS_TO_TICKS(1000));
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