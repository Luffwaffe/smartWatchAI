#include "app_gesture.h"

#include "esp_log.h"
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "app_gesture";

#ifdef DEBUG
#define APP_GESTURE_DEBUG_LOG(...) ESP_LOGI(TAG, __VA_ARGS__)
#define APP_GESTURE_OVERLAY_DEBUG_OPA LV_OPA_30
#else
#define APP_GESTURE_DEBUG_LOG(...) do { } while (0)
#define APP_GESTURE_OVERLAY_DEBUG_OPA LV_OPA_TRANSP
#endif

#define APP_GESTURE_OVERLAY_BOTTOM_W_PCT 50
#define APP_GESTURE_OVERLAY_BOTTOM_H 25
#define APP_GESTURE_OVERLAY_BOTTOM_COLOR 0x0B0F14
#define APP_GESTURE_OVERLAY_BOTTOM_OPA APP_GESTURE_OVERLAY_DEBUG_OPA
#define APP_GESTURE_OVERLAY_TOP_W_PCT 100
#define APP_GESTURE_OVERLAY_TOP_H 25
#define APP_GESTURE_OVERLAY_TOP_COLOR 0x0B0F14
#define APP_GESTURE_OVERLAY_TOP_OPA APP_GESTURE_OVERLAY_DEBUG_OPA
#define APP_GESTURE_OVERLAY_LEFT_W 25
#define APP_GESTURE_OVERLAY_LEFT_H_PCT 100
#define APP_GESTURE_OVERLAY_LEFT_COLOR 0x0B0F14
#define APP_GESTURE_OVERLAY_LEFT_OPA APP_GESTURE_OVERLAY_DEBUG_OPA
#define APP_GESTURE_OVERLAY_RIGHT_W 25
#define APP_GESTURE_OVERLAY_RIGHT_H_PCT 100
#define APP_GESTURE_OVERLAY_RIGHT_COLOR 0x0B0F14
#define APP_GESTURE_OVERLAY_RIGHT_OPA APP_GESTURE_OVERLAY_DEBUG_OPA
#define APP_GESTURE_BOTTOM_NAV_W_PCT 70
#define APP_GESTURE_BOTTOM_NAV_H APP_GESTURE_OVERLAY_BOTTOM_H
#define APP_GESTURE_HOME_PILL_H 3
#define APP_GESTURE_HOME_LINE_BOTTOM_PAD 14

#define APP_GESTURE_SWIPE_UP_MIN_DISTANCE 36
#define APP_GESTURE_SWIPE_DOWN_MIN_DISTANCE 36
#define APP_GESTURE_SWIPE_SIDE_MIN_DISTANCE 36

typedef struct {
    bool pressed;
    lv_point_t press_start;
} app_gesture_recognizer_t;

static app_gesture_recognizer_t s_top_gesture = {0};
static app_gesture_recognizer_t s_bottom_gesture = {0};
static app_gesture_recognizer_t s_left_gesture = {0};
static app_gesture_recognizer_t s_right_gesture = {0};
static app_gesture_host_t s_host = {0};

static app_gesture_type_t app_gesture_update(app_gesture_edge_t edge, lv_event_code_t code, lv_point_t point);
static app_nav_action_t app_nav_route_gesture(app_gesture_type_t gesture);
static void app_gesture_pointer_cb(lv_event_t *e);

void app_gesture_set_host(const app_gesture_host_t *host)
{
    s_host = host ? *host : (app_gesture_host_t){0};
}

void app_gesture_reset(void)
{
    s_top_gesture = (app_gesture_recognizer_t){0};
    s_bottom_gesture = (app_gesture_recognizer_t){0};
    s_left_gesture = (app_gesture_recognizer_t){0};
    s_right_gesture = (app_gesture_recognizer_t){0};
}

static app_gesture_recognizer_t *app_gesture_recognizer_for_edge(app_gesture_edge_t edge)
{
    switch (edge) {
    case APP_GESTURE_EDGE_TOP:
        return &s_top_gesture;
    case APP_GESTURE_EDGE_BOTTOM:
        return &s_bottom_gesture;
    case APP_GESTURE_EDGE_LEFT:
        return &s_left_gesture;
    case APP_GESTURE_EDGE_RIGHT:
        return &s_right_gesture;
    default:
        return NULL;
    }
}

static app_gesture_type_t app_gesture_recognizer_match(app_gesture_edge_t edge, lv_point_t start, lv_point_t current)
{
    switch (edge) {
    case APP_GESTURE_EDGE_TOP:
        return (current.y - start.y >= APP_GESTURE_SWIPE_DOWN_MIN_DISTANCE)
                   ? APP_GESTURE_SWIPE_TOP_DOWN
                   : APP_GESTURE_NONE;
    case APP_GESTURE_EDGE_BOTTOM:
        return (start.y - current.y >= APP_GESTURE_SWIPE_UP_MIN_DISTANCE)
                   ? APP_GESTURE_SWIPE_BOTTOM_UP
                   : APP_GESTURE_NONE;
    case APP_GESTURE_EDGE_LEFT:
        return (current.x - start.x >= APP_GESTURE_SWIPE_SIDE_MIN_DISTANCE)
                   ? APP_GESTURE_SWIPE_LEFT_RIGHT
                   : APP_GESTURE_NONE;
    case APP_GESTURE_EDGE_RIGHT:
        return (start.x - current.x >= APP_GESTURE_SWIPE_SIDE_MIN_DISTANCE)
                   ? APP_GESTURE_SWIPE_RIGHT_LEFT
                   : APP_GESTURE_NONE;
    default:
        return APP_GESTURE_NONE;
    }
}

static app_gesture_type_t app_gesture_update(app_gesture_edge_t edge, lv_event_code_t code, lv_point_t point)
{
    app_gesture_recognizer_t *recognizer = app_gesture_recognizer_for_edge(edge);
    if (!recognizer) {
        return APP_GESTURE_NONE;
    }

    if (code == LV_EVENT_PRESSED) {
        recognizer->pressed = true;
        recognizer->press_start = point;
        return APP_GESTURE_NONE;
    }

    if (!recognizer->pressed) {
        return APP_GESTURE_NONE;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        recognizer->pressed = false;
        return APP_GESTURE_NONE;
    }

    if (code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return APP_GESTURE_NONE;
    }

    app_gesture_type_t gesture = app_gesture_recognizer_match(edge, recognizer->press_start, point);
    if (gesture != APP_GESTURE_NONE || code == LV_EVENT_RELEASED) {
        recognizer->pressed = false;
    }
    return gesture;
}

static app_nav_action_t app_nav_route_gesture(app_gesture_type_t gesture)
{
    switch (gesture) {
    case APP_GESTURE_SWIPE_TOP_DOWN:
        return (app_nav_action_t){
            .type = APP_NAV_ACTION_SHOW_QUICK_PANEL,
        };
    case APP_GESTURE_SWIPE_BOTTOM_UP:
        return (app_nav_action_t){
            .type = APP_NAV_ACTION_BACK_TO_LAUNCHER,
        };
    case APP_GESTURE_SWIPE_LEFT_RIGHT:
        return (app_nav_action_t){
            .type = APP_NAV_ACTION_DISPATCH_APP_EVENT,
            .app_event = APP_EVT_NAV_LEFT,
        };
    case APP_GESTURE_SWIPE_RIGHT_LEFT:
        return (app_nav_action_t){
            .type = APP_NAV_ACTION_DISPATCH_APP_EVENT,
            .app_event = APP_EVT_NAV_RIGHT,
        };
    default:
        return (app_nav_action_t){ .type = APP_NAV_ACTION_NONE };
    }
}

static app_gesture_host_state_t app_gesture_get_host_state(void)
{
    if (!s_host.get_state) {
        return APP_GESTURE_HOST_STATE_TRANSITION;
    }
    return s_host.get_state(s_host.user_ctx);
}

static void app_gesture_execute_action(const app_nav_action_t *action)
{
    if (!action) {
        return;
    }

    switch (action->type) {
    case APP_NAV_ACTION_DISPATCH_APP_EVENT:
        if (s_host.dispatch_app_event) {
            s_host.dispatch_app_event(action->app_event, s_host.user_ctx);
        }
        break;
    case APP_NAV_ACTION_BACK_TO_LAUNCHER:
        if (s_host.back_to_launcher) {
            s_host.back_to_launcher(s_host.user_ctx);
        }
        break;
    case APP_NAV_ACTION_SHOW_QUICK_PANEL:
        if (s_host.show_quick_panel) {
            s_host.show_quick_panel(s_host.user_ctx);
        }
        break;
    case APP_NAV_ACTION_NONE:
    default:
        break;
    }
}

static void app_gesture_handle(app_gesture_type_t gesture)
{
    if (gesture == APP_GESTURE_NONE || app_gesture_get_host_state() == APP_GESTURE_HOST_STATE_TRANSITION) {
        return;
    }

    app_nav_action_t action = app_nav_route_gesture(gesture);
    app_gesture_execute_action(&action);
}

static void app_gesture_bind_events(lv_obj_t *obj, app_gesture_edge_t edge)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, app_gesture_pointer_cb, LV_EVENT_PRESSED, (void *)(intptr_t)edge);
    lv_obj_add_event_cb(obj, app_gesture_pointer_cb, LV_EVENT_PRESSING, (void *)(intptr_t)edge);
    lv_obj_add_event_cb(obj, app_gesture_pointer_cb, LV_EVENT_RELEASED, (void *)(intptr_t)edge);
    lv_obj_add_event_cb(obj, app_gesture_pointer_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)edge);
}

static lv_obj_t *app_gesture_create_overlay_bottom(lv_obj_t *parent)
{
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(APP_GESTURE_OVERLAY_BOTTOM_W_PCT), APP_GESTURE_OVERLAY_BOTTOM_H);
    lv_obj_align(overlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(APP_GESTURE_OVERLAY_BOTTOM_COLOR), 0);
    lv_obj_set_style_bg_opa(overlay, APP_GESTURE_OVERLAY_BOTTOM_OPA, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    return overlay;
}

static lv_obj_t *app_gesture_create_overlay_top(lv_obj_t *parent)
{
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(APP_GESTURE_OVERLAY_TOP_W_PCT), APP_GESTURE_OVERLAY_TOP_H);
    lv_obj_align(overlay, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(APP_GESTURE_OVERLAY_TOP_COLOR), 0);
    lv_obj_set_style_bg_opa(overlay, APP_GESTURE_OVERLAY_TOP_OPA, 0);
    app_gesture_bind_events(overlay, APP_GESTURE_EDGE_TOP);
    return overlay;
}

static lv_obj_t *app_gesture_create_overlay_left(lv_obj_t *parent)
{
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, APP_GESTURE_OVERLAY_LEFT_W, LV_PCT(APP_GESTURE_OVERLAY_LEFT_H_PCT));
    lv_obj_align(overlay, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(APP_GESTURE_OVERLAY_LEFT_COLOR), 0);
    lv_obj_set_style_bg_opa(overlay, APP_GESTURE_OVERLAY_LEFT_OPA, 0);
    app_gesture_bind_events(overlay, APP_GESTURE_EDGE_LEFT);
    return overlay;
}

static lv_obj_t *app_gesture_create_overlay_right(lv_obj_t *parent)
{
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, APP_GESTURE_OVERLAY_RIGHT_W, LV_PCT(APP_GESTURE_OVERLAY_RIGHT_H_PCT));
    lv_obj_align(overlay, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(APP_GESTURE_OVERLAY_RIGHT_COLOR), 0);
    lv_obj_set_style_bg_opa(overlay, APP_GESTURE_OVERLAY_RIGHT_OPA, 0);
    app_gesture_bind_events(overlay, APP_GESTURE_EDGE_RIGHT);
    return overlay;
}

static lv_obj_t *app_gesture_create_bottom_nav_area(lv_obj_t *parent)
{
    lv_obj_t *zone = lv_obj_create(parent);
    lv_obj_remove_style_all(zone);
    lv_obj_set_size(zone, LV_PCT(APP_GESTURE_BOTTOM_NAV_W_PCT), APP_GESTURE_BOTTOM_NAV_H);
    lv_obj_align(zone, LV_ALIGN_BOTTOM_MID, 0, 0);
    app_gesture_bind_events(zone, APP_GESTURE_EDGE_BOTTOM);

    lv_obj_t *pill = lv_obj_create(zone);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_PCT(100), APP_GESTURE_HOME_PILL_H);
    lv_obj_align(pill, LV_ALIGN_BOTTOM_MID, 0, -APP_GESTURE_HOME_LINE_BOTTOM_PAD);
    lv_obj_set_style_radius(pill, APP_GESTURE_HOME_PILL_H / 2, 0);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_50, 0);
    app_gesture_bind_events(pill, APP_GESTURE_EDGE_BOTTOM);

    return zone;
}

lv_obj_t *app_gesture_attach(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    app_gesture_reset();

    lv_obj_t *overlay_top = app_gesture_create_overlay_top(parent);
    lv_obj_t *overlay_bottom = app_gesture_create_overlay_bottom(parent);
    lv_obj_t *overlay_left = app_gesture_create_overlay_left(parent);
    lv_obj_t *overlay_right = app_gesture_create_overlay_right(parent);
    app_gesture_create_bottom_nav_area(overlay_bottom);

    lv_obj_move_foreground(overlay_top);
    lv_obj_move_foreground(overlay_bottom);
    lv_obj_move_foreground(overlay_left);
    lv_obj_move_foreground(overlay_right);
    return overlay_bottom;
}

static void app_gesture_pointer_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    app_gesture_edge_t edge = (app_gesture_edge_t)(intptr_t)lv_event_get_user_data(e);

    APP_GESTURE_DEBUG_LOG("pointer_cb: edge=%d code=%d state=%d", edge, code, app_gesture_get_host_state());

    if (app_gesture_get_host_state() == APP_GESTURE_HOST_STATE_TRANSITION) {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    app_gesture_type_t gesture = app_gesture_update(edge, code, point);
    app_gesture_handle(gesture);
}