#include "setting/controller/controller.h"

#include "setting/service/service.h"
#include "setting/view/view.h"

static bool s_scan_after_enable = false;
static bool s_auto_scan_after_status = false;

static void setting_controller_render_status(void)
{
    setting_status_t status;
    setting_service_get_status(&status);
    setting_view_set_status(&status);

    if ((s_scan_after_enable || s_auto_scan_after_status) && status.wifi_enabled && !status.scanning) {
        s_scan_after_enable = false;
        s_auto_scan_after_status = false;
        setting_service_scan_wifi();
    }
}

static void setting_controller_handle_wifi_scan(void)
{
    setting_status_t status;
    setting_service_get_status(&status);
    if (!status.wifi_enabled) {
        s_scan_after_enable = true;
        setting_service_enable_wifi();
        return;
    }
    if (status.scanning) {
        return;
    }

    setting_service_scan_wifi();
}

static void setting_controller_handle_wifi_disable(void)
{
    s_scan_after_enable = false;
    s_auto_scan_after_status = false;
    setting_service_disable_wifi();
}

static void setting_controller_handle_wifi_connect(const char *ssid, const char *password)
{
    setting_service_connect_wifi(ssid, password);
    setting_controller_render_status();
}

static void setting_controller_handle_wifi_status_request(void)
{
    s_auto_scan_after_status = true;
    setting_service_request_wifi_status();
}

esp_err_t setting_controller_open(lv_obj_t *root)
{
    setting_service_start();
    esp_err_t err = setting_view_open(root,
                                      setting_controller_handle_wifi_scan,
                                      setting_controller_handle_wifi_disable,
                                      setting_controller_handle_wifi_connect,
                                      setting_controller_handle_wifi_status_request);
    if (err != ESP_OK) {
        return err;
    }

    setting_controller_render_status();
    return ESP_OK;
}

void setting_controller_close(void)
{
    setting_view_close();
    setting_service_stop();
}

void setting_controller_handle_ui_event(const app_event_t *event)
{
    if (!event) {
        return;
    }

    switch (event->type) {
    case APP_EVT_SETTING_WIFI_STATUS_CHANGED:
    case APP_EVT_SETTING_WIFI_SCAN_DONE:
        setting_controller_render_status();
        break;
    default:
        break;
    }
}