#pragma once

#include "lvgl.h"

void notification_popup_show(lv_obj_t *parent, const char *source_app_id, const char *message, const void *icon);
void notification_popup_hide(void);
