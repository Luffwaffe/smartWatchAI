#pragma once

#include "app_manager.h"

typedef void (*launcher_view_bind_app_event_cb_t)(lv_obj_t *obj, const app_t *app);

lv_obj_t *launcher_view_create(const app_t *const *apps,
                               int app_count,
                               launcher_view_bind_app_event_cb_t bind_app_event);