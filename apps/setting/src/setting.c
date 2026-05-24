#include "setting/setting.h"

#include "app_common_config.h"
#include "setting/contract.h"
#include "app_manager.h"

extern const lv_image_dsc_t setting_icon;

static esp_err_t setting_view_open(lv_obj_t *root)
{
    lv_obj_set_style_bg_color(root, lv_color_hex(APP_COMMON_BACKGROUND_COLOR), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    lv_obj_t *container = lv_obj_create(root);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(container, APP_COMMON_TITLE_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(container, APP_COMMON_TITLE_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_left(container, APP_COMMON_TITLE_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(container, APP_COMMON_TITLE_PAD_RIGHT, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, "setting app");
    lv_obj_set_style_text_font(label, APP_COMMON_TITLE_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(APP_COMMON_TEXT_COLOR), 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    return ESP_OK;
}

static const app_t s_setting_app = {
    .id = SETTING_APP_ID,
    .name = SETTING_APP_NAME,
    .icon = &setting_icon,
    .open = setting_view_open,
    .start_backend = NULL,
    .stop_backend = NULL,
    .event_handler = NULL,
};

void setting_app_register(void)
{
    app_manager_register_app(&s_setting_app);
}
