#include "device/service/service.h"

#include "bluetooth_manager.h"
#include "data_center.h"
#include "app_manager.h"

#include <stdio.h>
#include <string.h>

static device_status_t s_status = {
    .bluetooth_enabled = false,
    .connection_state = DEVICE_CONNECTION_BLUETOOTH_OFF,
};

typedef enum {
    DEVICE_SERVICE_SUB_BLUETOOTH_STATUS,
    DEVICE_SERVICE_SUB_PHONE_MESSAGE,
} device_service_subscription_t;

static const device_service_subscription_t s_bt_status_subscription = DEVICE_SERVICE_SUB_BLUETOOTH_STATUS;
static const device_service_subscription_t s_phone_message_subscription = DEVICE_SERVICE_SUB_PHONE_MESSAGE;

static void device_service_request_ui_refresh(void)
{
    QueueHandle_t queue = app_manager_get_event_queue();
    if (queue == NULL) {
        return;
    }

    app_event_t event = {
        .type = APP_EVT_BLUETOOTH_STATUS_CHANGED,
        .value = 0,
    };
    xQueueSend(queue, &event, 0);
}

static void device_service_apply_bluetooth_status(const bluetooth_manager_status_t *bt_status)
{
    if (!bt_status) {
        return;
    }

    s_status.bluetooth_enabled = bt_status->enabled;
    if (!bt_status->enabled || bt_status->state == BLUETOOTH_MANAGER_STATE_OFF ||
        bt_status->state == BLUETOOTH_MANAGER_STATE_TURNING_OFF) {
        s_status.connection_state = DEVICE_CONNECTION_BLUETOOTH_OFF;
        s_status.phone_name[0] = '\0';
        s_status.phone_address[0] = '\0';
    } else if (bt_status->state == BLUETOOTH_MANAGER_STATE_CONNECTED) {
        s_status.connection_state = DEVICE_CONNECTION_CONNECTED;
        snprintf(s_status.phone_name, sizeof(s_status.phone_name), "%s", bt_status->connected_name);
        snprintf(s_status.phone_address, sizeof(s_status.phone_address), "%s", bt_status->connected_address);
    } else {
        s_status.connection_state = DEVICE_CONNECTION_NO_CONNECTION;
        s_status.phone_name[0] = '\0';
        s_status.phone_address[0] = '\0';
    }
}

static void device_service_handle_data_event(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;

    if (!event) {
        return;
    }

    switch (event->type) {
    case DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED:
        if (event->payload_len < sizeof(bluetooth_manager_status_t)) {
            return;
        }

        bluetooth_manager_status_t bt_status;
        memcpy(&bt_status, event->payload, sizeof(bt_status));
        device_service_apply_bluetooth_status(&bt_status);
        device_service_request_ui_refresh();
        break;
    case DATA_CENTER_EVENT_PHONE_MESSAGE: {
        size_t copy_len = event->payload_len < sizeof(s_status.phone_message) - 1
                              ? event->payload_len
                              : sizeof(s_status.phone_message) - 1;
        memcpy(s_status.phone_message, event->payload, copy_len);
        s_status.phone_message[copy_len] = '\0';
        device_service_request_ui_refresh();
        break;
    }
    default:
        break;
    }
}

static void device_service_refresh_connection_state(void)
{
    data_center_event_t latest_event;
    if (data_center_get_latest_event("bluetooth.status", DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED, &latest_event) == ESP_OK &&
        latest_event.payload_len >= sizeof(bluetooth_manager_status_t)) {
        bluetooth_manager_status_t bt_status;
        memcpy(&bt_status, latest_event.payload, sizeof(bt_status));
        device_service_apply_bluetooth_status(&bt_status);
        return;
    }

    bluetooth_manager_status_t bt_status;
    if (bluetooth_manager_get_status(&bt_status) != ESP_OK) {
        s_status.bluetooth_enabled = false;
        s_status.connection_state = DEVICE_CONNECTION_BLUETOOTH_OFF;
        s_status.phone_name[0] = '\0';
        s_status.phone_address[0] = '\0';
        return;
    }

    device_service_apply_bluetooth_status(&bt_status);
}

esp_err_t device_service_start(void)
{
    data_center_filter_t bt_filter = {0};
    snprintf(bt_filter.owner, sizeof(bt_filter.owner), "%s", "device");
    snprintf(bt_filter.topic, sizeof(bt_filter.topic), "%s", "bluetooth.status");
    bt_filter.type = DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED;
    data_center_subscribe_event(&bt_filter, device_service_handle_data_event, (void *)&s_bt_status_subscription);

    data_center_filter_t phone_filter = {0};
    snprintf(phone_filter.owner, sizeof(phone_filter.owner), "%s", "device");
    snprintf(phone_filter.topic, sizeof(phone_filter.topic), "%s", "phone.notification");
    phone_filter.type = DATA_CENTER_EVENT_PHONE_MESSAGE;
    data_center_subscribe_event(&phone_filter, device_service_handle_data_event, (void *)&s_phone_message_subscription);

    device_service_refresh_connection_state();
    return ESP_OK;
}

esp_err_t device_service_stop(void)
{
    data_center_unsubscribe_event(device_service_handle_data_event, (void *)&s_bt_status_subscription);
    data_center_unsubscribe_event(device_service_handle_data_event, (void *)&s_phone_message_subscription);
    return ESP_OK;
}

esp_err_t device_service_set_bluetooth_enabled(bool enabled)
{
    data_center_bluetooth_command_t command = {
        .command = DATA_CENTER_BLUETOOTH_CMD_SET_ENABLED,
        .enabled = enabled,
    };
    data_center_event_t event = {0};

    snprintf(event.source, sizeof(event.source), "%s", "device");
    snprintf(event.target, sizeof(event.target), "%s", "bluetooth");
    snprintf(event.topic, sizeof(event.topic), "%s", "bluetooth.command");
    event.type = DATA_CENTER_EVENT_BLUETOOTH_COMMAND;
    event.payload_len = sizeof(command);
    memcpy(event.payload, &command, sizeof(command));
    event.priority = 2;

    esp_err_t ret = data_center_publish_event(&event);
    device_service_refresh_connection_state();
    return ret;
}

void device_service_get_status(device_status_t *status)
{
    if (!status) {
        return;
    }

    memcpy(status, &s_status, sizeof(*status));
}