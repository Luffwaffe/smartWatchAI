#include "setting/view/view.h"

#include "app_common_config.h"

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_page = NULL;
static setting_view_wifi_scan_cb_t s_scan_cb = NULL;
static setting_view_wifi_disable_cb_t s_disable_cb = NULL;
static setting_view_wifi_connect_cb_t s_connect_cb = NULL;
static setting_view_wifi_status_request_cb_t s_status_request_cb = NULL;

esp_err_t setting_wifi_view_open(lv_obj_t *root,
                                 setting_view_wifi_scan_cb_t scan_cb,
                                 setting_view_wifi_disable_cb_t disable_cb,
                                 setting_view_wifi_connect_cb_t connect_cb,
                                 setting_view_wifi_status_request_cb_t status_request_cb);
void setting_wifi_view_close(void);
void setting_wifi_view_set_status(const setting_status_t *status);

static void setting_view_set_label_style(lv_obj_t *label, const lv_font_t *font, lv_color_t color, lv_text_align_t align)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, align, 0);
}

static void setting_view_open_wifi_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_root) {
        return;
    }

    setting_wifi_view_open(s_root, s_scan_cb, s_disable_cb, s_connect_cb, s_status_request_cb);
}

static lv_obj_t *setting_view_create_list_item(lv_obj_t *parent, const char *title, const char *subtitle)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, 72);
    lv_obj_set_style_bg_color(item, lv_color_hex(APP_COMMON_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(APP_COMMON_PANEL_BORDER_COLOR), 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_radius(item, 22, 0);
    lv_obj_set_style_pad_left(item, 18, 0);
    lv_obj_set_style_pad_right(item, 18, 0);
    lv_obj_set_style_pad_top(item, 10, 0);
    lv_obj_set_style_pad_bottom(item, 10, 0);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
    setting_view_set_label_style(title_label, APP_COMMON_TITLE_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(title_label, LV_PCT(100));

    return item;
}

esp_err_t setting_view_open(lv_obj_t *root,
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
    lv_obj_set_size(s_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(s_page, APP_COMMON_TITLE_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(s_page, APP_COMMON_TITLE_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_left(s_page, APP_COMMON_TITLE_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(s_page, APP_COMMON_TITLE_PAD_RIGHT, 0);
    lv_obj_set_style_pad_row(s_page, APP_COMMON_CONTAINER_PAD_ROW, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_page);
    lv_label_set_text(title, "Setting");
    setting_view_set_label_style(title, APP_COMMON_TITLE_FONT, lv_color_hex(APP_COMMON_TEXT_COLOR), LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_pad_bottom(title, APP_COMMON_TITLE_CONTENT_GAP, 0);

    lv_obj_t *wifi_item = setting_view_create_list_item(s_page, "WiFi", "Scan and select nearby networks");
    lv_obj_add_event_cb(wifi_item, setting_view_open_wifi_event_cb, LV_EVENT_CLICKED, NULL);

    return ESP_OK;
}

void setting_view_close(void)
{
    setting_wifi_view_close();
    s_root = NULL;
    s_page = NULL;
    s_scan_cb = NULL;
    s_disable_cb = NULL;
    s_connect_cb = NULL;
    s_status_request_cb = NULL;
}

void setting_view_set_status(const setting_status_t *status)
{
    setting_wifi_view_set_status(status);
}
