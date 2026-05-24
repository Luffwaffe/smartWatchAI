#include "ai_talk/view/view.h"

#include "ai_talk/context.h"
#include "ai_talk/store/store.h"
#include "app_common_config.h"

esp_err_t ai_talk_view_open(lv_obj_t *root)
{
    ai_talk_context_t *context = ai_talk_context_get();

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

    context->count_label = lv_label_create(container);
    lv_label_set_text_fmt(context->count_label, "%d", ai_talk_store_get_count());
    lv_obj_set_style_text_font(context->count_label, APP_COMMON_TITLE_FONT, 0);
    lv_obj_set_style_text_color(context->count_label, lv_color_hex(APP_COMMON_TEXT_COLOR), 0);
    lv_obj_set_width(context->count_label, LV_PCT(100));
    lv_obj_set_style_text_align(context->count_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(context->count_label, LV_ALIGN_TOP_MID, 0, 0);

    return ESP_OK;
}

void ai_talk_view_render_count(int count)
{
    ai_talk_context_t *context = ai_talk_context_get();

    if (context->count_label == NULL) {
        return;
    }

    lv_label_set_text_fmt(context->count_label, "%d", count);
}
