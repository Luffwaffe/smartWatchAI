#include "device/device.h"

#include "device/controller/controller.h"
#include "device/contract.h"

extern const lv_image_dsc_t device_icon;

static const app_t s_device_app = {
    .id = DEVICE_APP_ID,
    .name = DEVICE_APP_NAME,
    .icon = &device_icon,
    .open = device_controller_open,
    .close = device_controller_close,
    .start_backend = NULL,
    .stop_backend = NULL,
    .event_handler = device_controller_handle_ui_event,
};

void device_app_register(void)
{
    app_manager_register_app(&s_device_app);
}
