#include "setting/setting.h"

#include "setting/controller/controller.h"
#include "setting/contract.h"
#include "app_manager.h"

extern const lv_image_dsc_t setting_icon;

static const app_t s_setting_app = {
    .id = SETTING_APP_ID,
    .name = SETTING_APP_NAME,
    .icon = &setting_icon,
    .open = setting_controller_open,
    .close = setting_controller_close,
    .start_backend = NULL,
    .stop_backend = NULL,
    .event_handler = setting_controller_handle_ui_event,
};

void setting_app_register(void)
{
    app_manager_register_app(&s_setting_app);
}
