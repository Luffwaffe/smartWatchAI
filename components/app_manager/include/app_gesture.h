// app_gesture: shared gesture and navigation action types.
#pragma once

#include "app_manager.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_GESTURE_NONE = 0,
    APP_GESTURE_SWIPE_TOP_DOWN,
    APP_GESTURE_SWIPE_BOTTOM_UP,
    APP_GESTURE_SWIPE_LEFT_RIGHT,
    APP_GESTURE_SWIPE_RIGHT_LEFT,
} app_gesture_type_t;

typedef enum {
    APP_GESTURE_EDGE_TOP = 0,
    APP_GESTURE_EDGE_BOTTOM,
    APP_GESTURE_EDGE_LEFT,
    APP_GESTURE_EDGE_RIGHT,
} app_gesture_edge_t;

typedef enum {
    APP_NAV_ACTION_NONE = 0,
    APP_NAV_ACTION_DISPATCH_APP_EVENT,
    APP_NAV_ACTION_BACK_TO_LAUNCHER,
    APP_NAV_ACTION_SHOW_QUICK_PANEL,
} app_nav_action_type_t;

typedef struct {
    app_nav_action_type_t type;
    app_event_type_t app_event;
} app_nav_action_t;

typedef enum {
    APP_GESTURE_HOST_STATE_LAUNCHER = 0,
    APP_GESTURE_HOST_STATE_APP,
    APP_GESTURE_HOST_STATE_TRANSITION,
} app_gesture_host_state_t;

typedef struct {
    app_gesture_host_state_t (*get_state)(void *user_ctx);
    void (*dispatch_app_event)(app_event_type_t type, void *user_ctx);
    void (*back_to_launcher)(void *user_ctx);
    void (*show_quick_panel)(void *user_ctx);
    void *user_ctx;
} app_gesture_host_t;

void app_gesture_reset(void);
void app_gesture_set_host(const app_gesture_host_t *host);
lv_obj_t *app_gesture_attach(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif