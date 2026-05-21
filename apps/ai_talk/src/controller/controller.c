#include "ai_talk/controller/controller.h"

#include "ai_talk/store/store.h"
#include "ai_talk/view/view.h"

void ai_talk_controller_handle_ui_event(const app_event_t *event)
{
    if (event == NULL) {
        return;
    }

    if (event->type == APP_EVT_COUNTDOWN_UPDATE) {
        ai_talk_store_set_count(event->value);
        ai_talk_view_render_count(event->value);
    }
}
