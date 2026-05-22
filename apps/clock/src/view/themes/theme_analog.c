#include "clock/view/theme.h"

#include "clock/assets.h"
#include "clock/context.h"

static esp_err_t analog_open(lv_obj_t *parent)
{
    lv_obj_t *bg = lv_img_create(parent);
    lv_img_set_src(bg, &watchface);
    lv_obj_center(bg);
    lv_obj_move_background(bg);

    return ESP_OK;
}

static void analog_close(void)
{
}

const clock_theme_t clock_theme_analog = {
    .id = "analog",
    .open = analog_open,
    .close = analog_close,
};
