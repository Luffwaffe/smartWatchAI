#include "device/device.h"

#include "device/contract.h"
#include "app_manager.h"

extern const lv_image_dsc_t device_icon;

static esp_err_t device_view_open(lv_obj_t *root)
{
    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "device");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return ESP_OK;
}

static const app_t s_device_app = {
    .id = DEVICE_APP_ID,
    .name = DEVICE_APP_NAME,
    .icon = &device_icon,
    .open = device_view_open,
    .start_backend = NULL,
    .stop_backend = NULL,
    .event_handler = NULL,
};

void device_app_register(void)
{
    app_manager_register_app(&s_device_app);
}
