#include "clock/view/view.h"

#include "clock/context.h"
#include "clock/view/theme.h"

extern const clock_theme_t clock_theme_analog;
extern const clock_theme_t clock_theme_digital;
extern const clock_theme_t clock_theme_simple;

static const clock_theme_t *s_themes[] = {
    &clock_theme_simple,
    &clock_theme_analog,
    &clock_theme_digital,
};

static int clock_view_theme_count(void)
{
    return sizeof(s_themes) / sizeof(s_themes[0]);
}

static const clock_theme_t *clock_view_current_theme(void)
{
    clock_context_t *context = clock_context_get();
    int theme_count = clock_view_theme_count();

    if (theme_count == 0) {
        return NULL;
    }

    if (context->current_theme < 0 || context->current_theme >= theme_count) {
        context->current_theme = 0;
    }

    return s_themes[context->current_theme];
}

static void clock_view_render_current_theme(void)
{
    clock_context_t *context = clock_context_get();
    const clock_theme_t *theme = clock_view_current_theme();

    if (context->theme_container) {
        lv_obj_del(context->theme_container);
        context->theme_container = NULL;
    }

    if (context->root == NULL || theme == NULL) {
        return;
    }

    context->theme_container = lv_obj_create(context->root);
    lv_obj_remove_style_all(context->theme_container);
    lv_obj_set_size(context->theme_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(context->theme_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(context->theme_container, LV_OBJ_FLAG_SCROLLABLE);

    if (theme->open) {
        theme->open(context->theme_container);
    }
}

esp_err_t clock_view_open(lv_obj_t *root)
{
    clock_context_t *context = clock_context_get();

    context->root = root;
    context->current_theme = 0;
    clock_view_render_current_theme();
    return ESP_OK;
}

void clock_view_close(void)
{
    clock_context_t *context = clock_context_get();
    const clock_theme_t *theme = clock_view_current_theme();

    if (theme && theme->close) {
        theme->close();
    }

    context->root = NULL;
    context->theme_container = NULL;
    context->current_theme = 0;
}

void clock_view_next_theme(void)
{
    clock_context_t *context = clock_context_get();
    int theme_count = clock_view_theme_count();

    if (theme_count == 0) {
        return;
    }

    const clock_theme_t *old_theme = clock_view_current_theme();
    if (old_theme && old_theme->close) {
        old_theme->close();
    }

    context->current_theme = (context->current_theme + 1) % theme_count;
    clock_view_render_current_theme();
}

void clock_view_prev_theme(void)
{
    clock_context_t *context = clock_context_get();
    int theme_count = clock_view_theme_count();

    if (theme_count == 0) {
        return;
    }

    const clock_theme_t *old_theme = clock_view_current_theme();
    if (old_theme && old_theme->close) {
        old_theme->close();
    }

    context->current_theme = (context->current_theme + theme_count - 1) % theme_count;
    clock_view_render_current_theme();
}