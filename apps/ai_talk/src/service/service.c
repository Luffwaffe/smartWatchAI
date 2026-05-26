#include "ai_talk/service/service.h"

#include "ai_talk/context.h"
#include "ai_talk/contract.h"
#include "ai_talk/store/store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_manager.h"

static void ai_talk_service_task(void *arg)
{
    ai_talk_context_t *context = ai_talk_context_get();
    QueueHandle_t queue = app_manager_get_event_queue();
while(1){
    for (int count = AI_TALK_COUNT_START_VALUE; count <= AI_TALK_COUNT_END_VALUE && !context->stop_requested; ++count) {
        app_event_t event = {
            .type = APP_EVT_COUNTDOWN_UPDATE,
            .value = count,
        };
        ai_talk_store_set_count(count);
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
}

esp_err_t ai_talk_service_start(void)
{
    ai_talk_context_t *context = ai_talk_context_get();

    if (context->task != NULL) {
        return ESP_OK;
    }

    context->stop_requested = false;
    ai_talk_store_set_count(AI_TALK_COUNT_START_VALUE);

    xTaskCreate(ai_talk_service_task,
                "ai_talk_svc",
                AI_TALK_BACKEND_TASK_STACK_SIZE,
                NULL,
                AI_TALK_BACKEND_TASK_PRIORITY,
                &context->task);
    return ESP_OK;
}

esp_err_t ai_talk_service_stop(void)
{
    ai_talk_context_t *context = ai_talk_context_get();

    context->stop_requested = true;
    return ESP_OK;
}
