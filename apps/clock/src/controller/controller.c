#include "clock/controller/controller.h"

#include "clock/view/view.h"

void clock_controller_handle_ui_event(const app_event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->type) {
    case APP_EVT_NAV_LEFT:
        clock_view_prev_theme();
        break;
    case APP_EVT_NAV_RIGHT:
        clock_view_next_theme();
        break;
    default:
        break;
    }
}