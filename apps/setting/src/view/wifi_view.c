#include "setting/view/view.h"

#include "app_common_config.h"

#include <stdio.h>
#include <string.h>

#define SETTING_WIFI_SCAN_INTERVAL_MS 20000
#define SETTING_WIFI_KB_MODE_KEY "MODE"
#define SETTING_WIFI_KB_CTRL_KEY (LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_CHECKED)
#define SETTING_WIFI_KB_KEY(width) (LV_BUTTONMATRIX_CTRL_POPOVER | (width))

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_page = NULL;
static lv_obj_t *s_switch = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_password_textarea = NULL;
static lv_obj_t *s_keyboard = NULL;
static lv_timer_t *s_scan_timer = NULL;
static setting_view_wifi_scan_cb_t s_scan_cb = NULL;
static setting_view_wifi_disable_cb_t s_disable_cb = NULL;
static setting_view_wifi_connect_cb_t s_connect_cb = NULL;
static setting_view_wifi_status_request_cb_t s_status_request_cb = NULL;
static bool s_auto_scan_enabled = false;
static bool s_has_rendered_scan = false;
static bool s_syncing_status = false;
static char s_selected_ssid[SETTING_WIFI_SSID_MAX_LEN + 1] = {0};
static char s_item_ssids[SETTING_WIFI_MAX_APS][SETTING_WIFI_SSID_MAX_LEN + 1];

typedef enum {
    SETTING_WIFI_KB_MODE_LOWER = 0,
    SETTING_WIFI_KB_MODE_NUMBER,
    SETTING_WIFI_KB_MODE_SPECIAL,
    SETTING_WIFI_KB_MODE_UPPER,
} setting_wifi_kb_mode_t;

static setting_wifi_kb_mode_t s_keyboard_mode = SETTING_WIFI_KB_MODE_LOWER;

static const char *const s_wifi_kb_map_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_BACKSPACE, "\n",
    "z", "x", "c", "v", "b", "n", "m", ".", "@", "\n",
    SETTING_WIFI_KB_MODE_KEY, " ", "Enter", ""
};

static const lv_buttonmatrix_ctrl_t s_wifi_kb_ctrl_lower[] = {
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_CTRL_KEY | 4,
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_CTRL_KEY | 5, 12, SETTING_WIFI_KB_CTRL_KEY | 5
};

static const char *const s_wifi_kb_map_number[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    SETTING_WIFI_KB_MODE_KEY, "0", LV_SYMBOL_BACKSPACE, "\n",
    " ", "Enter", ""
};

static const lv_buttonmatrix_ctrl_t s_wifi_kb_ctrl_number[] = {
    SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5),
    SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5),
    SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_KEY(5),
    SETTING_WIFI_KB_CTRL_KEY | 5, SETTING_WIFI_KB_KEY(5), SETTING_WIFI_KB_CTRL_KEY | 5,
    10, SETTING_WIFI_KB_CTRL_KEY | 5
};

static const char *const s_wifi_kb_map_special[] = {
    "-", "+", "~", "_", "=", "|", "\\", "/", "\n",
    "!", "?", "#", "$", "%", "&", "*", "@", "\n",
    "(", ")", "[", "]", "{", "}", ":", ";", "\n",
    SETTING_WIFI_KB_MODE_KEY, ".", ",", "'", "\"", LV_SYMBOL_BACKSPACE, "\n",
    " ", "Enter", ""
};

static const lv_buttonmatrix_ctrl_t s_wifi_kb_ctrl_special[] = {
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_CTRL_KEY | 5, SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_CTRL_KEY | 5,
    12, SETTING_WIFI_KB_CTRL_KEY | 5
};

static const char *const s_wifi_kb_map_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
    "Z", "X", "C", "V", "B", "N", "M", ".", "@", "\n",
    SETTING_WIFI_KB_MODE_KEY, " ", "Enter", ""
};

static const lv_buttonmatrix_ctrl_t s_wifi_kb_ctrl_upper[] = {
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_CTRL_KEY | 4,
    SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3), SETTING_WIFI_KB_KEY(3),
    SETTING_WIFI_KB_CTRL_KEY | 5, 12, SETTING_WIFI_KB_CTRL_KEY | 5
};

static void setting_wifi_view_apply_keyboard_map(void)
{
    if (!s_keyboard) {
        return;
    }

    switch (s_keyboard_mode) {
    case SETTING_WIFI_KB_MODE_NUMBER:
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_USER_1, (const char **)s_wifi_kb_map_number, s_wifi_kb_ctrl_number);
        break;
    case SETTING_WIFI_KB_MODE_SPECIAL:
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_USER_1, (const char **)s_wifi_kb_map_special, s_wifi_kb_ctrl_special);
        break;
    case SETTING_WIFI_KB_MODE_UPPER:
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_USER_1, (const char **)s_wifi_kb_map_upper, s_wifi_kb_ctrl_upper);
        break;
    case SETTING_WIFI_KB_MODE_LOWER:
    default:
        lv_keyboard_set_map(s_keyboard, LV_KEYBOARD_MODE_USER_1, (const char **)s_wifi_kb_map_lower, s_wifi_kb_ctrl_lower);
        break;
    }
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_USER_1);
}

static void setting_wifi_view_keyboard_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || !s_keyboard) {
        return;
    }

    uint32_t button_id = lv_keyboard_get_selected_button(s_keyboard);
    const char *button_text = lv_keyboard_get_button_text(s_keyboard, button_id);
    if (!button_text || strcmp(button_text, SETTING_WIFI_KB_MODE_KEY) != 0) {
        return;
    }

    if (s_password_textarea) {
        for (size_t i = 0; i < strlen(SETTING_WIFI_KB_MODE_KEY); ++i) {
            lv_textarea_delete_char(s_password_textarea);
        }
    }

    s_keyboard_mode = (setting_wifi_kb_mode_t)((s_keyboard_mode + 1) % 4);
    setting_wifi_view_apply_keyboard_map();
}

static void setting_wifi_view_set_label_style(lv_obj_t *label, const lv_font_t *font, lv_color_t color, lv_text_align_t align)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
}

static void setting_wifi_view_scan_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || !s_switch) {
        return;
    }
    if (s_syncing_status) {
        return;
    }

    s_auto_scan_enabled = lv_obj_has_state(s_switch, LV_STATE_CHECKED);
    if (!s_auto_scan_enabled) {
        if (s_hint_label) {
            lv_label_set_text(s_hint_label, "Turn on WiFi to scan networks");
        }
        if (s_disable_cb) {
            s_disable_cb();
        }
        return;
    }

    if (s_scan_cb) {
        s_scan_cb();
    }
}

static void setting_wifi_view_scan_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_page || !s_auto_scan_enabled || !s_scan_cb || !s_has_rendered_scan) {
        return;
    }

    s_scan_cb();
}

static const char *setting_wifi_view_status_text(const setting_status_t *status, const char *ssid)
{
    if (!status || !ssid || ssid[0] == '\0' || strcmp(status->wifi_ssid, ssid) != 0) {
        return "";
    }

    switch (status->wifi_state) {
    case WIFI_MANAGER_STATE_CONNECTING:
        return "connecting...";
    case WIFI_MANAGER_STATE_CONNECTED:
        return status->wifi_connected ? "connected" : "";
    case WIFI_MANAGER_STATE_DISCONNECTED:
        return "disconnected";
    case WIFI_MANAGER_STATE_ERROR:
        return "error";
    default:
        return "";
    }
}

static void setting_wifi_view_close_popup(void)
{
    if (s_popup) {
        lv_obj_del(s_popup);
    }
    s_popup = NULL;
    s_password_textarea = NULL;
    s_keyboard = NULL;
}

static void setting_wifi_view_popup_connect_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_connect_cb || s_selected_ssid[0] == '\0') {
        return;
    }

    const char *password = s_password_textarea ? lv_textarea_get_text(s_password_textarea) : "";
    s_connect_cb(s_selected_ssid, password);
    setting_wifi_view_close_popup();
}

static void setting_wifi_view_popup_cancel_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        setting_wifi_view_close_popup();
    }
}

static void setting_wifi_view_show_connect_popup(const char *ssid)
{
    if (!s_root || !ssid || ssid[0] == '\0') {
        return;
    }

    snprintf(s_selected_ssid, sizeof(s_selected_ssid), "%s", ssid);
    setting_wifi_view_close_popup();

    s_popup = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, LV_PCT(92), LV_PCT(88));
    lv_obj_center(s_popup);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    // lv_obj_set_style_border_color(s_popup, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    // lv_obj_set_style_border_width(s_popup, 1, 0);
    lv_obj_set_style_radius(s_popup, 20, 0);
    lv_obj_set_style_pad_all(s_popup, 14, 0);
    lv_obj_set_style_pad_row(s_popup, 8, 0);
    lv_obj_set_flex_flow(s_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_popup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_popup);
    char title_text[64] = {0};
    snprintf(title_text, sizeof(title_text), "Password: %s", ssid);
    lv_label_set_text(title, title_text);
    setting_wifi_view_set_label_style(title, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(title, LV_PCT(100));

    s_password_textarea = lv_textarea_create(s_popup);
    lv_obj_set_width(s_password_textarea, LV_PCT(100));
    lv_obj_set_height(s_password_textarea, 46);
    lv_textarea_set_one_line(s_password_textarea, true);
    lv_textarea_set_password_mode(s_password_textarea, true);
    lv_textarea_set_placeholder_text(s_password_textarea, "WiFi password");

    s_keyboard = lv_keyboard_create(s_popup);
    lv_obj_set_width(s_keyboard, LV_PCT(100));
    lv_obj_set_flex_grow(s_keyboard, 1);
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    s_keyboard_mode = SETTING_WIFI_KB_MODE_LOWER;
    setting_wifi_view_apply_keyboard_map();
    lv_obj_add_event_cb(s_keyboard, setting_wifi_view_keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *button_row = lv_obj_create(s_popup);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_width(button_row, LV_PCT(100));
    lv_obj_set_height(button_row, 44);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cancel_btn = lv_btn_create(button_row);
    lv_obj_set_size(cancel_btn, 96, 40);
    lv_obj_add_event_cb(cancel_btn, setting_wifi_view_popup_cancel_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_t *connect_btn = lv_btn_create(button_row);
    lv_obj_set_size(connect_btn, 110, 40);
    lv_obj_add_event_cb(connect_btn, setting_wifi_view_popup_connect_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);
}

static void setting_wifi_view_ap_item_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const char *ssid = (const char *)lv_event_get_user_data(event);
    setting_wifi_view_show_connect_popup(ssid);
}

static lv_obj_t *setting_wifi_view_create_switch_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 62);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 20, 0);
    lv_obj_set_style_pad_left(row, 16, 0);
    lv_obj_set_style_pad_right(row, 16, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, "WiFi");
    setting_wifi_view_set_label_style(label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_LEFT);

    s_switch = lv_switch_create(row);
    lv_obj_set_size(s_switch, 80, 40);
    lv_obj_add_event_cb(s_switch, setting_wifi_view_scan_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return row;
}

static lv_obj_t *setting_wifi_view_create_ap_item(lv_obj_t *parent, const setting_wifi_ap_t *ap, size_t index, const setting_status_t *status)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, 76);
    // lv_obj_set_style_border_color(item, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    // lv_obj_set_style_border_width(item, 1, 0);
    // lv_obj_set_style_radius(item, 20, 0);
    lv_obj_set_style_pad_left(item, 0, 0);
    lv_obj_set_style_pad_right(item, 0, 0);
    lv_obj_set_style_pad_top(item, 0, 0);
    lv_obj_set_style_pad_bottom(item, 0, 0);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(item, setting_wifi_view_ap_item_event_cb, LV_EVENT_CLICKED, s_item_ssids[index]);

    char title[64] = {0};
    snprintf(title, sizeof(title), "%u. %s", (unsigned)index + 1U, ap->ssid[0] ? ap->ssid : "<hidden>");
    lv_obj_t *title_row = lv_obj_create(item);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(title_row, 24, 0);
    lv_obj_set_style_pad_right(title_row, 24, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(title_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title_row, setting_wifi_view_ap_item_event_cb, LV_EVENT_CLICKED, s_item_ssids[index]);

    lv_obj_t *title_label = lv_label_create(title_row);
    lv_label_set_text(title_label, title);
    setting_wifi_view_set_label_style(title_label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_LEFT);
    lv_obj_set_flex_grow(title_label, 1);
    lv_obj_add_flag(title_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title_label, setting_wifi_view_ap_item_event_cb, LV_EVENT_CLICKED, s_item_ssids[index]);

    lv_obj_t *status_label = lv_label_create(title_row);
    lv_label_set_text(status_label, setting_wifi_view_status_text(status, ap->ssid));
    setting_wifi_view_set_label_style(status_label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_MUTED_TEXT_COLOR), LV_TEXT_ALIGN_RIGHT);
    lv_obj_add_flag(status_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_label, setting_wifi_view_ap_item_event_cb, LV_EVENT_CLICKED, s_item_ssids[index]);

    // char subtitle[48] = {0};
    // snprintf(subtitle, sizeof(subtitle), "%d dBm  |  CH %u", ap->rssi, ap->primary);
    // lv_obj_t *subtitle_label = lv_label_create(item);
    // lv_label_set_text(subtitle_label, subtitle);
    // setting_wifi_view_set_label_style(subtitle_label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_MUTED_TEXT_COLOR), LV_TEXT_ALIGN_LEFT);
    // lv_obj_set_width(subtitle_label, LV_PCT(100));
    // lv_obj_set_style_pad_left(subtitle_label, 24, 0);
    // lv_obj_set_style_pad_right(subtitle_label, 24, 0);

    lv_obj_t *divider = lv_obj_create(item);
    lv_obj_remove_style_all(divider);
    lv_obj_set_width(divider, LV_PCT(80));
    lv_obj_set_height(divider, 1);
    lv_obj_set_align(divider, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(divider, lv_color_hex(APP_COMMON_MUTED_TEXT_COLOR), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_50, 0);

    return item;
}

esp_err_t setting_wifi_view_open(lv_obj_t *root,
                                 setting_view_wifi_scan_cb_t scan_cb,
                                 setting_view_wifi_disable_cb_t disable_cb,
                                 setting_view_wifi_connect_cb_t connect_cb,
                                 setting_view_wifi_status_request_cb_t status_request_cb)
{
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    s_root = root;
    s_scan_cb = scan_cb;
    s_disable_cb = disable_cb;
    s_connect_cb = connect_cb;
    s_status_request_cb = status_request_cb;
    lv_obj_clean(root);

    lv_obj_set_style_bg_color(root, lv_color_hex(APP_COMMON_BACKGROUND_COLOR), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    s_page = lv_obj_create(root);
    lv_obj_remove_style_all(s_page);
    lv_obj_set_size(s_page, LV_PCT(95), LV_PCT(92));
    lv_obj_center(s_page);
    lv_obj_set_style_pad_top(s_page, APP_COMMON_TITLE_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(s_page, APP_COMMON_TITLE_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_left(s_page, APP_COMMON_TITLE_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(s_page, APP_COMMON_TITLE_PAD_RIGHT, 0);
    lv_obj_set_style_pad_row(s_page, 10, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_page);
    lv_label_set_text(title, "WiFi");
    setting_wifi_view_set_label_style(title, APP_COMMON_TITLE_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(title, LV_PCT(100));

    setting_wifi_view_create_switch_row(s_page);

    s_hint_label = lv_label_create(s_page);
    lv_label_set_text(s_hint_label, "Turn on WiFi to scan networks");
    setting_wifi_view_set_label_style(s_hint_label, APP_COMMON_BODY_FONT, lv_color_hex(APP_COMMON_MUTED_TEXT_COLOR), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_hint_label, LV_PCT(100));
    lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);

    s_list = lv_obj_create(s_page);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_list, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(s_list, 1, 0);
    lv_obj_set_style_radius(s_list, 20, 0);
    lv_obj_set_style_clip_corner(s_list, true, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    s_scan_timer = lv_timer_create(setting_wifi_view_scan_timer_cb, SETTING_WIFI_SCAN_INTERVAL_MS, NULL);
    s_auto_scan_enabled = false;
    s_has_rendered_scan = false;

    lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_label, "Loading WiFi status...");
    if (s_status_request_cb) {
        s_status_request_cb();
    }

    return ESP_OK;
}

void setting_wifi_view_close(void)
{
    if (s_scan_timer) {
        lv_timer_del(s_scan_timer);
    }
    s_root = NULL;
    s_page = NULL;
    s_switch = NULL;
    s_list = NULL;
    s_hint_label = NULL;
    setting_wifi_view_close_popup();
    s_scan_timer = NULL;
    s_scan_cb = NULL;
    s_disable_cb = NULL;
    s_connect_cb = NULL;
    s_status_request_cb = NULL;
    s_auto_scan_enabled = false;
    s_has_rendered_scan = false;
    s_syncing_status = false;
    s_selected_ssid[0] = '\0';
}

void setting_wifi_view_set_status(const setting_status_t *status)
{
    if (!status || !s_page || !s_switch || !s_list || !s_hint_label) {
        return;
    }

    s_syncing_status = true;
    if (status->wifi_enabled) {
        s_auto_scan_enabled = true;
        lv_obj_add_state(s_switch, LV_STATE_CHECKED);
    } else {
        s_auto_scan_enabled = false;
        lv_obj_clear_state(s_switch, LV_STATE_CHECKED);
        s_syncing_status = false;
        s_has_rendered_scan = false;
        lv_obj_clean(s_list);
        lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_hint_label, "Turn on WiFi to scan networks");
        return;
    }
    s_syncing_status = false;

    if (status->scanning) {
        if (s_has_rendered_scan) {
            lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    if (!status->wifi_enabled) {
        lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_hint_label, "WiFi is starting...");
        return;
    }
    if (status->wifi_state == WIFI_MANAGER_STATE_ERROR) {
        char text[64] = {0};
        snprintf(text, sizeof(text), "WiFi error: %d", status->last_error);
        lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_hint_label, text);
        return;
    }
    if (!status->wifi_scan_valid) {
        if (s_has_rendered_scan) {
            lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (status->wifi_count == 0) {
        if (!s_has_rendered_scan) {
            lv_obj_clean(s_list);
            lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_hint_label, "No WiFi network found");
            s_has_rendered_scan = true;
        }
        return;
    }

    lv_obj_clean(s_list);
    lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < status->wifi_count; ++i) {
        snprintf(s_item_ssids[i], sizeof(s_item_ssids[i]), "%s", status->wifi_aps[i].ssid);
        setting_wifi_view_create_ap_item(s_list, &status->wifi_aps[i], i, status);
    }
    s_has_rendered_scan = true;
}
