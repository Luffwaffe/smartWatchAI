#include "clock/view/theme.h"

#include "clock/assets.h"
#include "clock/context.h"

#define SIMPLE_TEXT_COLOR 0x3A68AF

static lv_obj_t *simple_create_label(lv_obj_t *parent,
                                     const char *text,
                                     const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(SIMPLE_TEXT_COLOR), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void simple_set_datetime(int hour, int minute, int second, int day, int month, int year)
{
    clock_context_t *context = clock_context_get();

    if (context->hour_label) lv_label_set_text_fmt(context->hour_label, "%02d", hour);
    if (context->minute_label) lv_label_set_text_fmt(context->minute_label, "%02d", minute);
    if (context->day_label) lv_label_set_text_fmt(context->day_label, "%02d/%02d/%04d", day, month, year);
}

static esp_err_t simple_open(lv_obj_t *parent)
{
    clock_context_t *context = clock_context_get();

    lv_obj_t *bg = lv_img_create(parent);
    lv_img_set_src(bg, &theme_simple_background);
    lv_obj_center(bg);
    lv_obj_move_background(bg);

    lv_obj_t *brand = simple_create_label(parent, "NAVY", &lv_font_montserrat_40);
    lv_obj_set_width(brand, LV_PCT(100));
    lv_obj_set_style_text_letter_space(brand, 12, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *time_box = lv_obj_create(parent);
    lv_obj_remove_style_all(time_box);
    lv_obj_set_size(time_box, 410, 210);
    lv_obj_align(time_box, LV_ALIGN_CENTER, 0, -38);
    lv_obj_set_flex_flow(time_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(time_box, LV_OBJ_FLAG_SCROLLABLE);

    context->hour_label = simple_create_label(time_box, "10", &clock_font_montserrat_bold_137);
    lv_obj_set_width(context->hour_label, 200);

    lv_obj_t *colon_1 = simple_create_label(time_box, ":", &clock_font_montserrat_bold_60);
    lv_obj_set_width(colon_1, 10);

    context->minute_label = simple_create_label(time_box, "08", &clock_font_montserrat_bold_137);
    lv_obj_set_width(context->minute_label, 200);

    lv_obj_t *date_box = lv_obj_create(parent);
    lv_obj_remove_style_all(date_box);
    lv_obj_set_size(date_box, 360, 52);
    lv_obj_align(date_box, LV_ALIGN_CENTER, 0, 92);
    lv_obj_clear_flag(date_box, LV_OBJ_FLAG_SCROLLABLE);

    context->day_label = simple_create_label(date_box, "01/05/2025", &lv_font_montserrat_36);
    lv_obj_set_width(context->day_label, LV_PCT(100));
    lv_obj_center(context->day_label);
    context->second_label = NULL;
    context->month_label = NULL;
    context->year_label = NULL;

    simple_set_datetime(8, 8, 36, 4, 9, 2045);

    return ESP_OK;
}

static void simple_close(void)
{
    clock_context_t *context = clock_context_get();
    context->hour_label = NULL;
    context->minute_label = NULL;
    context->second_label = NULL;
    context->day_label = NULL;
    context->month_label = NULL;
    context->year_label = NULL;
}

const clock_theme_t clock_theme_simple = {
    .id = "simple",
    .open = simple_open,
    .close = simple_close,
};
