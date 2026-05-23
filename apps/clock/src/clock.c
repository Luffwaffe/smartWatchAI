#include "clock/clock.h"

#include "clock/contract.h"
#include "clock/controller/controller.h"
#include "clock/service/service.h"
#include "clock/view/view.h"
#include "app_manager.h"

extern const lv_image_dsc_t clock_icon;

static const app_t s_clock_app = {
    .id = CLOCK_APP_ID,
    .name = CLOCK_APP_NAME,
    .icon = &clock_icon,
    .open = clock_controller_open,
    .close = clock_view_close,
    .start_backend = clock_service_start,
    .stop_backend = clock_service_stop,
    .event_handler = clock_controller_handle_ui_event,
};

void clock_app_register(void)
{
    app_manager_register_app(&s_clock_app);
}
