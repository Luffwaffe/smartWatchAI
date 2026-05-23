#include "quick_panel.h"

#include "data_center.h"
#include <stdbool.h>

#define QUICK_PANEL_MAX_VISIBLE_ITEMS 6
#define QUICK_PANEL_BG_COLOR 0x101820
#define QUICK_PANEL_CARD_COLOR 0x1F2A33
#define QUICK_PANEL_TEXT_COLOR 0xF4F7FA
#define QUICK_PANEL_MUTED_TEXT_COLOR 0xA7B3BD

extern const lv_image_dsc_t quick_panel_background4;

static lv_obj_t *s_panel = NULL;

static void quick_panel_seed_demo_items(void)
{
    if (data_center_count() > 0) {
        return;
    }

    data_center_publish(&(data_center_item_t){
        .source_app_id = "system",
        .title = "Wifi",
        .message = "Connected to AIWatch-Net",
        .type = DATA_CENTER_ITEM_INFO,
        .priority = 2,
    });

    data_center_publish(&(data_center_item_t){
        .source_app_id = "system",
        .title = "Bluetooth",
        .message = "Enabled, no device connected",
        .type = DATA_CENTER_ITEM_INFO,
        .priority = 1,
    });
}

static lv_obj_t *quick_panel_create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, LV_PCT(100));
    return label;
}

static void quick_panel_add_item_card(lv_obj_t *list, const data_center_item_t *item)
{
    lv_obj_t *card = lv_obj_create(list);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(QUICK_PANEL_CARD_COLOR), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    quick_panel_create_label(card, item->title, &lv_font_montserrat_20, lv_color_hex(QUICK_PANEL_TEXT_COLOR));
    quick_panel_create_label(card, item->message, &lv_font_montserrat_20, lv_color_hex(QUICK_PANEL_MUTED_TEXT_COLOR));
}

esp_err_t quick_panel_show(lv_obj_t *parent)
{
    if (!parent) {
        return ESP_ERR_INVALID_ARG;
    }

    quick_panel_seed_demo_items();
    quick_panel_hide();

    s_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_panel);
    lv_obj_set_size(s_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *background = lv_img_create(s_panel);
    lv_img_set_src(background, &quick_panel_background4);
    lv_obj_set_style_bg_opa(background, LV_OPA_COVER, 0);
    lv_obj_center(background);
    lv_obj_move_background(background);

    lv_obj_t *content = lv_obj_create(s_panel);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(content);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *list = lv_obj_create(content);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_style_pad_top(list, 50, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    data_center_item_t items[QUICK_PANEL_MAX_VISIBLE_ITEMS];
    size_t item_count = data_center_get_snapshot(items, QUICK_PANEL_MAX_VISIBLE_ITEMS);
    for (size_t i = 0; i < item_count; ++i) {
        quick_panel_add_item_card(list, &items[i]);
    }

    lv_obj_move_foreground(s_panel);
    return ESP_OK;
}

lv_obj_t *quick_panel_get_root(void)
{
    return s_panel;
}

void quick_panel_hide(void)
{
    if (s_panel) {
        lv_obj_del_async(s_panel);
        s_panel = NULL;
    }
}

bool quick_panel_is_visible(void)
{
    return s_panel != NULL;
}
