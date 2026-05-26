#include "notification_popup.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "app_common_config.h"
#include "esp_log.h"

#define NOTIFICATION_POPUP_WIDTH_PCT 86
#define NOTIFICATION_POPUP_HEIGHT 86
#define NOTIFICATION_POPUP_TOP_PAD 40
#define NOTIFICATION_POPUP_AUTO_HIDE_MS 6000
#define NOTIFICATION_POPUP_BG_COLOR 0xF8FAFC
#define NOTIFICATION_POPUP_TITLE_COLOR 0x111827
#define NOTIFICATION_POPUP_MESSAGE_COLOR 0x4B5563
#define NOTIFICATION_POPUP_ICON_BG_COLOR 0xE5EEF7
#define NOTIFICATION_POPUP_TEXT_MAX_LEN 96
#define NOTIFICATION_POPUP_MAX_WORDS 7

static const char *TAG = "notification_popup";

static lv_obj_t *s_popup = NULL;
static lv_timer_t *s_auto_hide_timer = NULL;

static bool notification_popup_is_utf8_continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

static bool notification_popup_valid_utf8_sequence(const unsigned char *text, size_t remaining, size_t *seq_len)
{
    unsigned char first = text[0];

    if (first < 0x80) {
        *seq_len = 1;
        return true;
    }
    if (first >= 0xC2 && first <= 0xDF && remaining >= 2 && notification_popup_is_utf8_continuation(text[1])) {
        *seq_len = 2;
        return true;
    }
    if (first >= 0xE0 && first <= 0xEF && remaining >= 3 &&
        notification_popup_is_utf8_continuation(text[1]) && notification_popup_is_utf8_continuation(text[2])) {
        *seq_len = 3;
        return true;
    }
    if (first >= 0xF0 && first <= 0xF4 && remaining >= 4 &&
        notification_popup_is_utf8_continuation(text[1]) && notification_popup_is_utf8_continuation(text[2]) &&
        notification_popup_is_utf8_continuation(text[3])) {
        *seq_len = 4;
        return true;
    }

    *seq_len = 1;
    return false;
}

static void notification_popup_sanitize_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    if (!src) {
        return;
    }

    const unsigned char *input = (const unsigned char *)src;
    size_t out = 0;
    bool replaced_invalid = false;

    while (*input && out + 1 < dst_size) {
        if (*input < 0x20 && *input != '\n' && *input != '\t') {
            input++;
            continue;
        }

        size_t remaining = 0;
        while (input[remaining] && remaining < 4) {
            remaining++;
        }

        size_t seq_len = 1;
        if (!notification_popup_valid_utf8_sequence(input, remaining, &seq_len)) {
            dst[out++] = '?';
            input++;
            replaced_invalid = true;
            continue;
        }

        if (out + seq_len >= dst_size) {
            break;
        }
        for (size_t i = 0; i < seq_len; ++i) {
            dst[out++] = (char)input[i];
        }
        input += seq_len;
    }

    dst[out] = '\0';
    if (replaced_invalid) {
        ESP_LOGW(TAG, "Notification text contained invalid UTF-8 bytes");
    }
}

static bool notification_popup_is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static void notification_popup_limit_words(char *dst, size_t dst_size, const char *src)
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
        while (notification_popup_is_space(*p)) {
            p++;
        }
        if (!*p) {
            break;
        }
        if (words == NOTIFICATION_POPUP_MAX_WORDS) {
            truncated = true;
            break;
        }

        const char *word_start = p;
        while (*p && !notification_popup_is_space(*p)) {
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
            while (out > 0 && notification_popup_is_utf8_continuation((unsigned char)dst[out])) {
                out--;
            }
        }
        memcpy(&dst[out], "...", suffix_len + 1);
    }
}

static void notification_popup_auto_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    notification_popup_hide();
}

void notification_popup_hide(void)
{
    if (s_auto_hide_timer) {
        lv_timer_del(s_auto_hide_timer);
        s_auto_hide_timer = NULL;
    }

    if (s_popup) {
        lv_obj_del_async(s_popup);
        s_popup = NULL;
    }
}

void notification_popup_show(lv_obj_t *parent, const char *source_app_id, const char *message, const void *icon)
{
    if (!parent) {
        return;
    }

    notification_popup_hide();

    s_popup = lv_obj_create(parent);
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, LV_PCT(NOTIFICATION_POPUP_WIDTH_PCT), NOTIFICATION_POPUP_HEIGHT);
    lv_obj_align(s_popup, LV_ALIGN_TOP_MID, 0, NOTIFICATION_POPUP_TOP_PAD);
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(NOTIFICATION_POPUP_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_popup, 35, 0);
    lv_obj_set_style_pad_all(s_popup, 12, 0);
    lv_obj_set_style_pad_column(s_popup, 12, 0);
    lv_obj_set_style_shadow_width(s_popup, 18, 0);
    lv_obj_set_style_shadow_opa(s_popup, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(s_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(s_popup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_popup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *icon_box = lv_obj_create(s_popup);
    lv_obj_remove_style_all(icon_box);
    lv_obj_set_size(icon_box, 54, 54);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(NOTIFICATION_POPUP_ICON_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(icon_box, 16, 0);
    lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);

    if (icon) {
        lv_obj_t *icon_img = lv_img_create(icon_box);
        lv_img_set_src(icon_img, icon);
        lv_obj_center(icon_img);
    } else {
        lv_obj_t *placeholder = lv_label_create(icon_box);
        const char *source = source_app_id && source_app_id[0] ? source_app_id : "?";
        char initial[2] = { source[0], '\0' };
        lv_label_set_text(placeholder, initial);
        lv_obj_set_style_text_font(placeholder, APP_COMMON_TITLE_FONT, 0);
        lv_obj_set_style_text_color(placeholder, lv_color_hex(NOTIFICATION_POPUP_TITLE_COLOR), 0);
        lv_obj_center(placeholder);
    }

    lv_obj_t *text_wrap = lv_obj_create(s_popup);
    lv_obj_remove_style_all(text_wrap);
    lv_obj_set_width(text_wrap, LV_PCT(78));
    lv_obj_set_height(text_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(text_wrap, 4, 0);
    lv_obj_set_flex_flow(text_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(text_wrap, LV_OBJ_FLAG_SCROLLABLE);

    char safe_message[NOTIFICATION_POPUP_TEXT_MAX_LEN];
    char display_message[NOTIFICATION_POPUP_TEXT_MAX_LEN];
    notification_popup_sanitize_text(safe_message, sizeof(safe_message), message);
    notification_popup_limit_words(display_message, sizeof(display_message), safe_message);

    lv_obj_t *body = lv_label_create(text_wrap);
    lv_label_set_long_mode(body, LV_LABEL_LONG_DOT);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_text(body, display_message);
    lv_obj_set_style_text_font(body, APP_COMMON_BODY_FONT, 0);
    lv_obj_set_style_text_color(body, lv_color_hex(NOTIFICATION_POPUP_MESSAGE_COLOR), 0);

    lv_obj_move_foreground(s_popup);
    s_auto_hide_timer = lv_timer_create(notification_popup_auto_hide_cb, NOTIFICATION_POPUP_AUTO_HIDE_MS, NULL);
    lv_timer_set_repeat_count(s_auto_hide_timer, 1);
}
