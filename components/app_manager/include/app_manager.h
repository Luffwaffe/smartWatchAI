// app_manager: lightweight app registry, launcher, and UI event bridge.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
/* forward declare app_hw_status_t to avoid depending on main's header */
typedef struct app_hw_status_t app_hw_status_t;
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

typedef enum {
    APP_EVT_COUNTDOWN_UPDATE = 1,
    APP_EVT_APP_FINISHED = 2,
    APP_EVT_NAV_TOP = 3,
    APP_EVT_NAV_LEFT = 4,
    APP_EVT_NAV_BOTTOM = 5,
    APP_EVT_NAV_RIGHT = 6,
    APP_EVT_CLOCK_DATETIME_UPDATE = 7,
    APP_EVT_BLUETOOTH_STATUS_CHANGED = 8,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    int value;
} app_event_t;

typedef struct app {
    const char *id;
    const char *name;
    const void *icon; /* optional pointer to image resource */
    void (*close)(void);               /* called on UI thread before screen cleanup */
    esp_err_t (*open)(lv_obj_t *root); /* called on UI thread */
    esp_err_t (*start_backend)(void);  /* spawn backend task */
    esp_err_t (*stop_backend)(void);   /* request backend stop */
    void (*event_handler)(const app_event_t *ev); /* called on UI thread */
} app_t;

/* Creates UI task and queue. */
esp_err_t app_manager_init(void);

/* register an app before showing launcher */
esp_err_t app_manager_register_app(const app_t *app);

/* Open an app by id (creates screen, calls open(), then start_backend()). */
esp_err_t app_manager_open_app(const char *app_id);

/* Return from the current app to the launcher. */
esp_err_t app_manager_back_to_launcher(void);

/* Create/show the launcher grid. hw_status is currently optional/unused. */
esp_err_t app_manager_show_launcher(const app_hw_status_t *hw_status);

/* Backend tasks post UI events to this queue. */
QueueHandle_t app_manager_get_event_queue(void);
