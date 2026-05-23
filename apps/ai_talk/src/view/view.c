#include "ai_talk/view/view.h"

#include "ai_talk/context.h"
#include "ai_talk/store/store.h"

esp_err_t ai_talk_view_open(lv_obj_t *root)
{
    ai_talk_context_t *context = ai_talk_context_get();

    context->count_label = lv_label_create(root);
    lv_label_set_text_fmt(context->count_label, "%d", ai_talk_store_get_count());
    lv_obj_set_style_text_font(context->count_label, &lv_font_montserrat_20, 0);
    lv_obj_align(context->count_label, LV_ALIGN_CENTER, 0, 0);

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
