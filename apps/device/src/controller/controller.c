#include "device/controller/controller.h"

#include "device/service/service.h"
#include "device/view/view.h"

static void device_controller_render_status(void)
{
    device_status_t status;
    device_service_get_status(&status);
    device_view_set_status(&status);
}

static void device_controller_handle_bluetooth_toggle(bool enabled)
{
    device_service_set_bluetooth_enabled(enabled);
    device_controller_render_status();
}

esp_err_t device_controller_open(lv_obj_t *root)
{
    device_service_start();
    esp_err_t err = device_view_open(root, device_controller_handle_bluetooth_toggle);
    if (err != ESP_OK) {
        return err;
    }

    device_controller_render_status();
    return ESP_OK;
}

void device_controller_close(void)
{
    device_view_close();
}

void device_controller_handle_ui_event(const app_event_t *event)
{
    if (!event) {
        return;
    }

    switch (event->type) {
    case APP_EVT_BLUETOOTH_STATUS_CHANGED:
        device_controller_render_status();
        break;
    default:
        break;
    }
}