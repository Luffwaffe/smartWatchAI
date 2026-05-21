#include "clock/view/view.h"

#include "bsp/esp-bsp.h"
#include "clock/context.h"
#include "clock/store/store.h"

esp_err_t clock_view_open(lv_obj_t *root)
{
    clock_context_t *context = clock_context_get();

    bsp_display_lock(0);
    context->count_label = lv_label_create(root);
    lv_label_set_text_fmt(context->count_label, "%d", clock_store_get_count());
    lv_obj_set_style_text_font(context->count_label, &lv_font_montserrat_20, 0);
    lv_obj_align(context->count_label, LV_ALIGN_CENTER, 0, 0);
    bsp_display_unlock();

    return ESP_OK;
}

void clock_view_render_count(int count)
{
    clock_context_t *context = clock_context_get();

    if (context->count_label == NULL) {
        return;
    }

    lv_label_set_text_fmt(context->count_label, "%d", count);
}