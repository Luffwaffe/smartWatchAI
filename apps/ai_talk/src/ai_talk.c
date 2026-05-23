#include "ai_talk/ai_talk.h"

#include "ai_talk/contract.h"
#include "ai_talk/controller/controller.h"
#include "ai_talk/service/service.h"
#include "ai_talk/view/view.h"
#include "app_manager.h"

extern const lv_image_dsc_t ai_icon;

static const app_t s_ai_talk_app = {
    .id = AI_TALK_APP_ID,
    .name = AI_TALK_APP_NAME,
    .icon = &ai_icon,
    .open = ai_talk_view_open,
    .start_backend = ai_talk_service_start,
    .stop_backend = ai_talk_service_stop,
    .event_handler = ai_talk_controller_handle_ui_event,
};

void ai_talk_app_register(void)
{
    app_manager_register_app(&s_ai_talk_app);
}
