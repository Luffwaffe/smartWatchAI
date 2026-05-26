#include "quick_panel.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_common_config.h"

#define QUICK_PANEL_MAX_VISIBLE_ITEMS 6
#define QUICK_PANEL_SOURCE_MAX_LEN 24
#define QUICK_PANEL_MESSAGE_MAX_LEN 96
#define QUICK_PANEL_MAX_MESSAGE_WORDS 10
#define QUICK_PANEL_BG_COLOR 0x101820
#define QUICK_PANEL_CARD_COLOR 0x1F2A33
#define QUICK_PANEL_TEXT_COLOR 0xF4F7FA
#define QUICK_PANEL_MUTED_TEXT_COLOR 0xA7B3BD
#define QUICK_PANEL_SOURCE_TEXT_COLOR 0x7FD7FF

extern const lv_image_dsc_t quick_panel_background4;

static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_parent = NULL;

typedef struct {
    bool used;
    char source_app_id[QUICK_PANEL_SOURCE_MAX_LEN];
    char message[QUICK_PANEL_MESSAGE_MAX_LEN];
    const void *icon;
} quick_panel_item_t;

static quick_panel_item_t s_items[QUICK_PANEL_MAX_VISIBLE_ITEMS];
static size_t s_item_count = 0;

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

static void quick_panel_copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static bool quick_panel_is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static bool quick_panel_is_utf8_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

static void quick_panel_copy_message_preview(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }

    const char *p = src;
    size_t out = 0;
    size_t words = 0;
    bool truncated = false;

    while (*p) {
        while (quick_panel_is_space(*p)) {
            p++;
        }
        if (!*p) {
            break;
        }
        if (words == QUICK_PANEL_MAX_MESSAGE_WORDS) {
            truncated = true;
            break;
        }

        const char *word_start = p;
        while (*p && !quick_panel_is_space(*p)) {
            p++;
        }
        size_t word_len = (size_t)(p - word_start);
        size_t need = word_len + (words > 0 ? 1 : 0);
        if (out + need >= dst_size) {
            truncated = true;
            break;
        }

        if (words > 0) {
            dst[out++] = ' ';
        }
        memcpy(&dst[out], word_start, word_len);
        out += word_len;
        dst[out] = '\0';
        words++;
    }

    if (truncated && dst_size > 4) {
        size_t suffix_len = 3;
        if (out + suffix_len >= dst_size) {
            out = dst_size - suffix_len - 1;
            while (out > 0 && quick_panel_is_utf8_continuation((unsigned char)dst[out])) {
                out--;
            }
        }
        memcpy(&dst[out], "...", suffix_len + 1);
    }
}

static void quick_panel_upsert_item(const char *source_app_id, const char *message, const void *icon)
{
    const char *source = (source_app_id && source_app_id[0]) ? source_app_id : "unknown";
    for (size_t i = 0; i < s_item_count; ++i) {
        if (strcmp(s_items[i].source_app_id, source) == 0) {
            quick_panel_copy_message_preview(s_items[i].message, sizeof(s_items[i].message), message);
            s_items[i].icon = icon;
            return;
        }
    }

    if (s_item_count == QUICK_PANEL_MAX_VISIBLE_ITEMS) {
        memmove(&s_items[0], &s_items[1], sizeof(s_items[0]) * (QUICK_PANEL_MAX_VISIBLE_ITEMS - 1));
        s_item_count--;
    }

    quick_panel_item_t *slot = &s_items[s_item_count++];
    memset(slot, 0, sizeof(*slot));
    slot->used = true;
    quick_panel_copy_string(slot->source_app_id, sizeof(slot->source_app_id), source);
    quick_panel_copy_message_preview(slot->message, sizeof(slot->message), message);
    slot->icon = icon;
}

static void quick_panel_add_item_card(lv_obj_t *list, const quick_panel_item_t *item)
{
    lv_obj_t *card = lv_obj_create(list);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(QUICK_PANEL_CARD_COLOR), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_column(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (item->icon) {
        lv_obj_t *icon = lv_img_create(card);
        lv_img_set_src(icon, item->icon);
        lv_obj_set_size(icon, 44, 44);
    }

    lv_obj_t *text_wrap = lv_obj_create(card);
    lv_obj_remove_style_all(text_wrap);
    lv_obj_set_width(text_wrap, item->icon ? LV_PCT(78) : LV_PCT(100));
    lv_obj_set_height(text_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(text_wrap, 4, 0);
    lv_obj_set_flex_flow(text_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(text_wrap, LV_OBJ_FLAG_SCROLLABLE);

    quick_panel_create_label(text_wrap, item->source_app_id, APP_COMMON_TITLE_FONT, lv_color_hex(QUICK_PANEL_SOURCE_TEXT_COLOR));
    quick_panel_create_label(text_wrap, item->message, APP_COMMON_BODY_FONT, lv_color_hex(QUICK_PANEL_TEXT_COLOR));
}

static void quick_panel_add_empty_state(lv_obj_t *list)
{
    lv_obj_t *card = lv_obj_create(list);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(QUICK_PANEL_CARD_COLOR), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_70, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    quick_panel_create_label(card, "No requested items", APP_COMMON_TITLE_FONT, lv_color_hex(QUICK_PANEL_TEXT_COLOR));
    quick_panel_create_label(card, "Publish a data_center message to show an item", APP_COMMON_BODY_FONT, lv_color_hex(QUICK_PANEL_MUTED_TEXT_COLOR));
}

esp_err_t quick_panel_show(lv_obj_t *parent)
{
    if (!parent) {
        return ESP_ERR_INVALID_ARG;
    }

    s_parent = parent;
    quick_panel_hide();

    s_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_panel);
    lv_obj_set_size(s_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(QUICK_PANEL_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(s_panel, lv_color_hex(0xFA1111), 0);
 
    // lv_obj_t *background = lv_img_create(s_panel);
    // lv_img_set_src(background, &quick_panel_background4);
    // lv_obj_set_style_bg_opa(background, LV_OPA_COVER, 0);
    // lv_obj_center(background);
    // lv_obj_move_background(background);

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

    if (s_item_count == 0) {
        quick_panel_add_empty_state(list);
    } else {
        for (size_t i = 0; i < s_item_count; ++i) {
            quick_panel_add_item_card(list, &s_items[i]);
        }
    }

    lv_obj_move_foreground(s_panel);
    return ESP_OK;
}

esp_err_t quick_panel_show_message(lv_obj_t *parent, const char *source_app_id, const char *message, const void *icon)
{
    if (!parent && !s_parent) {
        return ESP_ERR_INVALID_ARG;
    }

    lv_obj_t *target_parent = parent ? parent : s_parent;
    quick_panel_upsert_item(source_app_id, message, icon);
    return quick_panel_show(target_parent);
}

esp_err_t quick_panel_update_message(const char *source_app_id, const char *message, const void *icon)
{
    quick_panel_upsert_item(source_app_id, message, icon);
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
