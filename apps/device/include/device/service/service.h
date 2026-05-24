#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    DEVICE_CONNECTION_BLUETOOTH_OFF = 0,
    DEVICE_CONNECTION_NO_CONNECTION,
    DEVICE_CONNECTION_CONNECTED,
} device_connection_state_t;

typedef struct {
    bool bluetooth_enabled;
    device_connection_state_t connection_state;
    char phone_name[32];
    char phone_address[32];
} device_status_t;

esp_err_t device_service_start(void);
esp_err_t device_service_stop(void);
esp_err_t device_service_set_bluetooth_enabled(bool enabled);
void device_service_get_status(device_status_t *status);