/**
 * Minimal app manager: registry, launcher, UI task and event queue.
 * UI updates are routed through registered app callbacks so backend tasks
 * can post events safely to the queue.
 */

#include "app_manager.h"
#include "app_gesture.h"
#include "launcherView.h"
#include "quick_panel.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include <string.h>

static const char *TAG = "app_manager";

#ifdef DEBUG
#define APP_MANAGER_DEBUG_LOG(...) ESP_LOGI(TAG, __VA_ARGS__)
#else
#define APP_MANAGER_DEBUG_LOG(...) do { } while (0)
#endif

typedef enum {
    APP_MANAGER_STATE_LAUNCHER,
    APP_MANAGER_STATE_APP,
    APP_MANAGER_STATE_TRANSITION,
} app_manager_state_t;

static const app_t *s_registry[16];
static int s_reg_count = 0;
static QueueHandle_t s_ui_queue = NULL;
static lv_obj_t *s_current_root = NULL;
static const app_t *s_current_app = NULL;
static lv_obj_t *s_launcher_root = NULL;
static app_manager_state_t s_state = APP_MANAGER_STATE_LAUNCHER;
static bool s_opening_app = false;

static void launcher_icon_cb(lv_event_t *e)
{
    const app_t *app = (const app_t *)lv_event_get_user_data(e);
    APP_MANAGER_DEBUG_LOG("launcher_icon_cb: app=%s state=%d opening=%d",
                          app ? app->id : "NULL", s_state, s_opening_app);

    if (app && s_state == APP_MANAGER_STATE_LAUNCHER && !s_opening_app) {
        ESP_LOGI(TAG, "Launcher selected app %s", app->id);
        app_manager_open_app(app->id);
    }
}

static void launcher_bind_app_event(lv_obj_t *obj, const app_t *app)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, launcher_icon_cb, LV_EVENT_CLICKED, (void *)app);
}

static void app_manager_task(void *arg);
static app_gesture_host_state_t app_manager_gesture_get_state(void *user_ctx);
static void app_manager_gesture_dispatch_app_event(app_event_type_t type, void *user_ctx);
static void app_manager_gesture_back_to_launcher(void *user_ctx);
static void app_manager_gesture_show_quick_panel(void *user_ctx);

static lv_obj_t *app_manager_create_content_root(lv_obj_t *parent)
{
    lv_obj_t *content = lv_obj_create(parent);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    return content;
}

static void app_manager_create_nav_overlays(lv_obj_t *parent)
{
    app_gesture_attach(parent);
}

static void app_manager_gesture_dispatch_app_event(app_event_type_t type, void *user_ctx)
{
    (void)user_ctx;
    if (s_state != APP_MANAGER_STATE_APP || !s_current_app || !s_current_app->event_handler) {
        return;
    }

    app_event_t event = {
        .type = type,
        .value = 0,
    };
    s_current_app->event_handler(&event);
}

static void app_manager_gesture_show_quick_panel(void *user_ctx)
{
    (void)user_ctx;
    lv_obj_t *parent = lv_scr_act();
    esp_err_t err = quick_panel_show(parent);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to show quick panel: %s", esp_err_to_name(err));
        return;
    }

    lv_obj_t *quick_panel_root = quick_panel_get_root();
    if (quick_panel_root) {
        app_gesture_attach(quick_panel_root);
    }
}

static void app_manager_gesture_back_to_launcher(void *user_ctx)
{
    (void)user_ctx;
    if (quick_panel_is_visible()) {
        quick_panel_hide();
        return;
    }

    app_manager_back_to_launcher();
}

static app_gesture_host_state_t app_manager_gesture_get_state(void *user_ctx)
{
    (void)user_ctx;
    switch (s_state) {
    case APP_MANAGER_STATE_LAUNCHER:
        return APP_GESTURE_HOST_STATE_LAUNCHER;
    case APP_MANAGER_STATE_APP:
        return APP_GESTURE_HOST_STATE_APP;
    case APP_MANAGER_STATE_TRANSITION:
    default:
        return APP_GESTURE_HOST_STATE_TRANSITION;
    }
}

esp_err_t app_manager_init(void)
{
    if (s_ui_queue) return ESP_OK;
    s_ui_queue = xQueueCreate(16, sizeof(app_event_t));
    if (!s_ui_queue) {
        ESP_LOGE(TAG, "Failed to create UI event queue");
        return ESP_ERR_NO_MEM;
    }

    /* UI task: processes app events; BSP's LVGL port runs lv_timer_handler(). */
    xTaskCreate(app_manager_task, "app_mgr", 6144, NULL, 5, NULL);

    app_gesture_set_host(&(app_gesture_host_t){
        .get_state = app_manager_gesture_get_state,
        .dispatch_app_event = app_manager_gesture_dispatch_app_event,
        .back_to_launcher = app_manager_gesture_back_to_launcher,
        .show_quick_panel = app_manager_gesture_show_quick_panel,
    });

    ESP_LOGI(TAG, "app_manager initialized");
    return ESP_OK;
}

static void app_manager_task(void *arg)
{
    app_event_t ev;
    for (;;) {
        /* Process app events only; BSP's LVGL port owns lv_timer_handler(). */
        while (xQueueReceive(s_ui_queue, &ev, 0) == pdTRUE) {
            if (s_current_app && s_current_app->event_handler) {
                bsp_display_lock(0);
                s_current_app->event_handler(&ev);
                bsp_display_unlock();
            } else {
                if (ev.type == APP_EVT_APP_FINISHED) app_manager_back_to_launcher();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t app_manager_register_app(const app_t *app)
{
    if (!app || !app->id) return ESP_ERR_INVALID_ARG;
    if (s_reg_count >= (int)(sizeof(s_registry)/sizeof(s_registry[0]))) return ESP_ERR_NO_MEM;
    s_registry[s_reg_count++] = app;
    ESP_LOGI(TAG, "Registered app %s (%s)", app->id, app->name ? app->name : "");
    return ESP_OK;
}

QueueHandle_t app_manager_get_event_queue(void)
{
    return s_ui_queue;
}

esp_err_t app_manager_show_launcher(const app_hw_status_t *hw_status)
{
    (void)hw_status;

    if (!s_ui_queue) return ESP_ERR_INVALID_STATE;
    if (s_state == APP_MANAGER_STATE_TRANSITION) return ESP_ERR_INVALID_STATE;

    bsp_display_lock(0);
    lv_obj_t *old_launcher = s_launcher_root;
    lv_obj_t *scr = launcher_view_create(s_registry, s_reg_count, launcher_bind_app_event);
    if (!scr) {
        bsp_display_unlock();
        return ESP_ERR_NO_MEM;
    }

    app_manager_create_nav_overlays(scr);

    lv_scr_load(scr);
    s_launcher_root = scr;
    s_state = APP_MANAGER_STATE_LAUNCHER;
    s_opening_app = false;
    if (old_launcher && old_launcher != scr) lv_obj_del_async(old_launcher);
    bsp_display_unlock();
    ESP_LOGI(TAG, "Launcher shown with %d apps", s_reg_count);
    return ESP_OK;
}

static const app_t *find_app_by_id(const char *id)
{
    for (int i = 0; i < s_reg_count; ++i) {
        if (strcmp(s_registry[i]->id, id) == 0) return s_registry[i];
    }
    return NULL;
}

esp_err_t app_manager_open_app(const char *app_id)
{
    const app_t *app = find_app_by_id(app_id);
    if (!app) return ESP_ERR_NOT_FOUND;
    if (!s_ui_queue) return ESP_ERR_INVALID_STATE;
    if (s_state != APP_MANAGER_STATE_LAUNCHER || s_opening_app) return ESP_ERR_INVALID_STATE;

    s_opening_app = true;
    s_state = APP_MANAGER_STATE_TRANSITION;

    if (s_current_app && s_current_app->stop_backend) s_current_app->stop_backend();
    if (s_current_app && s_current_app->close) s_current_app->close();
    lv_obj_t *old_root = s_current_root;

    bsp_display_lock(0);
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *content = app_manager_create_content_root(scr);
    app_manager_create_nav_overlays(scr);

    s_current_root = scr;
    s_current_app = app;
    if (app->open) app->open(content);
    lv_scr_load(scr);
    if (old_root) lv_obj_del_async(old_root);
    s_state = APP_MANAGER_STATE_APP;
    s_opening_app = false;
    bsp_display_unlock();

    if (app->start_backend) app->start_backend();

    ESP_LOGI(TAG, "Opened app %s", app->id);
    return ESP_OK;
}

esp_err_t app_manager_back_to_launcher(void)
{
    if (!s_ui_queue) return ESP_ERR_INVALID_STATE;
    if (s_state != APP_MANAGER_STATE_APP) return ESP_OK;

    s_state = APP_MANAGER_STATE_TRANSITION;

    if (s_current_app && s_current_app->stop_backend) s_current_app->stop_backend();

    bsp_display_lock(0);

    lv_obj_t *old_app_screen = s_current_root;
    if (s_current_app && s_current_app->close) s_current_app->close();
    s_current_root = NULL;
    s_current_app = NULL;

    if (s_launcher_root) lv_scr_load(s_launcher_root);
    if (old_app_screen) lv_obj_del_async(old_app_screen);

    s_state = APP_MANAGER_STATE_LAUNCHER;
    s_opening_app = false;

    bsp_display_unlock();

    if (!s_launcher_root) return app_manager_show_launcher(NULL);

    ESP_LOGI(TAG, "Returned to launcher");
    return ESP_OK;
}
