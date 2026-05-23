#include "clock/controller/controller.h"

#include "clock/context.h"
#include "clock/service/service.h"
#include "clock/view/view.h"

static void clock_controller_render_datetime_from_context(void)
{
    clock_rtc_datetime_t datetime;

    if (!clock_context_get_datetime(&datetime)) {
        return;
    }

    clock_view_set_datetime(&datetime);
}

esp_err_t clock_controller_open(lv_obj_t *root)
{
    clock_service_prepare_datetime();
    return clock_view_open(root);
}

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
    case APP_EVT_CLOCK_DATETIME_UPDATE:
        clock_controller_render_datetime_from_context();
        break;
    default:
        break;
    }
}
