#include "device/view/view.h"

#include "app_common_config.h"

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_switch = NULL;
static device_view_bluetooth_toggle_cb_t s_toggle_cb = NULL;

static void device_view_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || !s_toggle_cb) {
        return;
    }

    lv_obj_t *sw = lv_event_get_target(event);
    s_toggle_cb(lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void device_view_set_common_label_style(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
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

    lv_obj_t *bluetooth_row = lv_obj_create(container);
    lv_obj_remove_style_all(bluetooth_row);
    lv_obj_set_width(bluetooth_row, LV_PCT(100));
    lv_obj_set_height(bluetooth_row, 56);
    lv_obj_set_style_bg_color(bluetooth_row, lv_color_hex(APP_COMMON_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(bluetooth_row, LV_OPA_70, 0);
    lv_obj_set_style_radius(bluetooth_row, 18, 0);
    lv_obj_set_style_pad_left(bluetooth_row, 16, 0);
    lv_obj_set_style_pad_right(bluetooth_row, 16, 0);
    lv_obj_set_flex_flow(bluetooth_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bluetooth_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bluetooth_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bluetooth_label = lv_label_create(bluetooth_row);
    lv_label_set_text(bluetooth_label, "Bluetooth");
    lv_obj_set_style_text_font(bluetooth_label, APP_COMMON_BODY_FONT, 0);
    lv_obj_set_style_text_color(bluetooth_label, lv_color_hex(APP_COMMON_TEXT_COLOR), 0);

    s_switch = lv_switch_create(bluetooth_row);
    lv_obj_set_style_bg_color(s_switch, lv_color_hex(APP_COMMON_ACCENT_COLOR), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_switch, device_view_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return ESP_OK;
}

void device_view_close(void)
{
    s_root = NULL;
    s_status_label = NULL;
    s_switch = NULL;
    s_toggle_cb = NULL;
}

void device_view_set_status(const device_status_t *status)
{
    if (!status || !s_status_label || !s_switch) {
        return;
    }

    if (status->bluetooth_enabled) {
        lv_obj_add_state(s_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_switch, LV_STATE_CHECKED);
    }

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