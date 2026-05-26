#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t quick_panel_show(lv_obj_t *parent);
esp_err_t quick_panel_update_message(const char *source_app_id, const char *message, const void *icon);
esp_err_t quick_panel_show_message(lv_obj_t *parent, const char *source_app_id, const char *message, const void *icon);
lv_obj_t *quick_panel_get_root(void);
void quick_panel_hide(void);
bool quick_panel_is_visible(void);

#ifdef __cplusplus
}
#endif
