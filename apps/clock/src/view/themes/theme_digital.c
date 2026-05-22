#include "clock/view/theme.h"

#include "clock/assets.h"
#include "clock/context.h"

#define DIGITAL_BG_COLOR 0x080609
#define DIGITAL_FRAME_COLOR 0xC72E0F
#define DIGITAL_TEXT_TOP_COLOR 0xC72E0F
#define DIGITAL_TEXT_BOTTOM_COLOR 0xC72E0F
#define DIGITAL_LABEL_COLOR 0xFFC1AA

static lv_obj_t *digital_create_label(lv_obj_t *parent,
                                      const char *text,
                                      const lv_font_t *font,
                                      uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_outline_width(label, 0, 0);
    lv_obj_set_style_shadow_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    return label;
}

static void digital_style_value(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(DIGITAL_TEXT_BOTTOM_COLOR), 0);
    lv_obj_set_style_text_letter_space(label, 2, 0);
}

static void digital_make_transparent_box(lv_obj_t *box)
{
    lv_obj_remove_style_all(box);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_outline_width(box, 0, 0);
    lv_obj_set_style_shadow_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_pad_gap(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
}

static void digital_set_datetime(int hour, int minute, int second, int day, int month, int year)
{
    clock_context_t *context = clock_context_get();

    if (context->hour_label) lv_label_set_text_fmt(context->hour_label, "%02d", hour);
    if (context->minute_label) lv_label_set_text_fmt(context->minute_label, "%02d", minute);
    if (context->second_label) lv_label_set_text_fmt(context->second_label, "%02d", second);
    if (context->day_label) lv_label_set_text_fmt(context->day_label, "%02d", day);
    if (context->month_label) lv_label_set_text_fmt(context->month_label, "%02d", month);
    if (context->year_label) lv_label_set_text_fmt(context->year_label, "%04d", year);
}

static esp_err_t digital_open(lv_obj_t *parent)
{
    clock_context_t *context = clock_context_get();

    lv_obj_set_style_bg_color(parent, lv_color_hex(DIGITAL_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lv_obj_t *screen_border = lv_obj_create(parent);
    lv_obj_remove_style_all(screen_border);
    lv_obj_set_size(screen_border, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen_border, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(screen_border, 120, 0);
    lv_obj_set_style_border_width(screen_border, 5, 0);
    lv_obj_set_style_border_color(screen_border, lv_color_hex(DIGITAL_FRAME_COLOR), 0);
    lv_obj_set_style_bg_opa(screen_border, LV_OPA_TRANSP, 0);

    lv_obj_t *brand = digital_create_label(parent, "NAVY", &lv_font_montserrat_48, DIGITAL_TEXT_TOP_COLOR);
    lv_obj_set_style_text_letter_space(brand, 10, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 5, 80);

    lv_obj_t *time_box = lv_obj_create(parent);
    digital_make_transparent_box(time_box);
    lv_obj_set_size(time_box, 410, 86);
    lv_obj_align(time_box, LV_ALIGN_CENTER, 0, -36);
    lv_obj_set_flex_flow(time_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    context->hour_label = digital_create_label(time_box, "10", &Technology, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->hour_label);
    lv_obj_set_width(context->hour_label, 120);
    lv_obj_set_style_text_letter_space(context->hour_label, 0, 0);
    lv_obj_t *colon_1 = digital_create_label(time_box, ":", &Technology, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(colon_1);
    lv_obj_set_width(colon_1, 16);
    context->minute_label = digital_create_label(time_box, "08", &Technology, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->minute_label);
    lv_obj_set_width(context->minute_label, 120);
    lv_obj_set_style_text_letter_space(context->minute_label, 0, 0);
    lv_obj_t *colon_2 = digital_create_label(time_box, ":", &Technology, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(colon_2);
    lv_obj_set_width(colon_2, 16);
    context->second_label = digital_create_label(time_box, "36", &Technology, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->second_label);
    lv_obj_set_width(context->second_label, 120);
    lv_obj_set_style_text_letter_space(context->second_label, 0, 0);

    lv_obj_t *date_box = lv_obj_create(parent);
    digital_make_transparent_box(date_box);
    lv_obj_set_size(date_box, 330, 104);
    lv_obj_align(date_box, LV_ALIGN_CENTER, 0, 86);

    lv_obj_t *day_title = digital_create_label(date_box, "DAY", &lv_font_montserrat_16, DIGITAL_LABEL_COLOR);
    lv_obj_align(day_title, LV_ALIGN_TOP_LEFT, 32, 0);
    lv_obj_t *month_title = digital_create_label(date_box, "MONTH", &lv_font_montserrat_16, DIGITAL_LABEL_COLOR);
    lv_obj_align(month_title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *year_title = digital_create_label(date_box, "YEAR", &lv_font_montserrat_16, DIGITAL_LABEL_COLOR);
    lv_obj_align(year_title, LV_ALIGN_TOP_RIGHT, -30, 0);

    context->day_label = digital_create_label(date_box, "01", &lv_font_montserrat_48, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->day_label);
    lv_obj_set_style_text_letter_space(context->day_label, 5, 0);
    lv_obj_align(context->day_label, LV_ALIGN_BOTTOM_LEFT, 14, 0);

    context->month_label = digital_create_label(date_box, "05", &lv_font_montserrat_48, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->month_label);
    lv_obj_set_style_text_letter_space(context->month_label, 5, 0);
    lv_obj_align(context->month_label, LV_ALIGN_BOTTOM_MID, -14, 0);

    context->year_label = digital_create_label(date_box, "2025", &lv_font_montserrat_48, DIGITAL_TEXT_BOTTOM_COLOR);
    digital_style_value(context->year_label);
    lv_obj_set_style_text_letter_space(context->year_label, 3, 0);
    lv_obj_align(context->year_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    digital_set_datetime(8, 8, 8, 1, 5, 2025);

    return ESP_OK;
}

static void digital_close(void)
{
    clock_context_t *context = clock_context_get();
    context->hour_label = NULL;
    context->minute_label = NULL;
    context->second_label = NULL;
    context->day_label = NULL;
    context->month_label = NULL;
    context->year_label = NULL;
}

const clock_theme_t clock_theme_digital = {
    .id = "digital",
    .open = digital_open,
    .close = digital_close,
};
