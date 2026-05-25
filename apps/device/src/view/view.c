#include "device/view/view.h"

#include "app_common_config.h"

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_power_btn = NULL;
static lv_obj_t *s_power_btn_label = NULL;
static bool s_bluetooth_enabled = false;
static device_view_bluetooth_toggle_cb_t s_toggle_cb = NULL;

static void device_view_power_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toggle_cb) {
        return;
    }

    s_toggle_cb(!s_bluetooth_enabled);
}

static void device_view_set_common_label_style(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static void device_view_style_power_button(bool enabled)
{
    if (!s_power_btn || !s_power_btn_label) {
        return;
    }

    lv_obj_set_style_bg_color(s_power_btn,
                              enabled ? lv_color_hex(0x20C36A) : lv_color_hex(APP_COMMON_ACCENT_COLOR),
                              0);
    lv_obj_set_style_bg_opa(s_power_btn, LV_OPA_COVER, 0);
    lv_label_set_text(s_power_btn_label, enabled ? "Turn off" : "Turn on");
}

esp_err_t device_view_open(lv_obj_t *root, device_view_bluetooth_toggle_cb_t toggle_cb)
{
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    s_root = root;
    s_toggle_cb = toggle_cb;

    lv_obj_set_style_bg_color(root, lv_color_hex(APP_COMMON_BACKGROUND_COLOR), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *container = lv_obj_create(root);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(container, APP_COMMON_TITLE_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(container, APP_COMMON_TITLE_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_left(container, APP_COMMON_TITLE_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(container, APP_COMMON_TITLE_PAD_RIGHT, 0);
    lv_obj_set_style_pad_row(container, APP_COMMON_CONTAINER_PAD_ROW, 0);
    lv_obj_set_style_pad_column(container, APP_COMMON_CONTAINER_PAD_COLUMN, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, "Phone connect");
    device_view_set_common_label_style(title, APP_COMMON_TITLE_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR));
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_pad_bottom(title, APP_COMMON_TITLE_CONTENT_GAP, 0);

    lv_obj_t *status_panel = lv_obj_create(container);
    lv_obj_remove_style_all(status_panel);
    lv_obj_set_width(status_panel, LV_PCT(100));
    lv_obj_set_height(status_panel, 112);
    lv_obj_set_style_bg_color(status_panel, lv_color_hex(APP_COMMON_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(status_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(status_panel, 2, 0);
    lv_obj_set_style_radius(status_panel, 22, 0);
    lv_obj_set_style_pad_all(status_panel, 14, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_status_label = lv_label_create(status_panel);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    device_view_set_common_label_style(s_status_label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_MUTED_TEXT_COLOR));
    lv_obj_center(s_status_label);

    s_power_btn = lv_btn_create(container);
    lv_obj_remove_style_all(s_power_btn);
    lv_obj_set_width(s_power_btn, LV_PCT(100));
    lv_obj_set_height(s_power_btn, 68);
    lv_obj_set_style_radius(s_power_btn, 24, 0);
    lv_obj_set_style_shadow_width(s_power_btn, 18, 0);
    lv_obj_set_style_shadow_opa(s_power_btn, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(s_power_btn, lv_color_hex(APP_COMMON_ACCENT_COLOR), 0);
    lv_obj_add_flag(s_power_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_power_btn, device_view_power_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_power_btn_label = lv_label_create(s_power_btn);
    device_view_set_common_label_style(s_power_btn_label, APP_COMMON_BODY_FONT, lv_color_hex(0xFFFFFF));
    lv_obj_center(s_power_btn_label);
    device_view_style_power_button(false);

    return ESP_OK;
}

void device_view_close(void)
{
    s_root = NULL;
    s_status_label = NULL;
    s_power_btn = NULL;
    s_power_btn_label = NULL;
    s_bluetooth_enabled = false;
    s_toggle_cb = NULL;
}

void device_view_set_status(const device_status_t *status)
{
    if (!status || !s_status_label || !s_power_btn) {
        return;
    }

    s_bluetooth_enabled = status->bluetooth_enabled;
    device_view_style_power_button(s_bluetooth_enabled);

    switch (status->connection_state) {
    case DEVICE_CONNECTION_CONNECTED:
        lv_label_set_text_fmt(s_status_label,
                              "Connected\n%s\n%s",
                              status->phone_name[0] ? status->phone_name : "Unknown phone",
                              status->phone_address[0] ? status->phone_address : "No address");
        break;
    case DEVICE_CONNECTION_NO_CONNECTION:
        lv_label_set_text(s_status_label, "no connection");
        break;
    case DEVICE_CONNECTION_BLUETOOTH_OFF:
    default:
        lv_label_set_text(s_status_label, "bluetooth off");
        break;
    }
}