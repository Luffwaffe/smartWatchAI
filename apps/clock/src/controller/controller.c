#include "clock/controller/controller.h"

#include "clock/store/store.h"
#include "clock/view/view.h"

void clock_controller_handle_ui_event(const app_event_t *event)
{
    if (event == NULL) {
        return;
    }

    if (event->type == APP_EVT_COUNTDOWN_UPDATE) {
        clock_store_set_count(event->value);
        clock_view_render_count(event->value);
    }
}