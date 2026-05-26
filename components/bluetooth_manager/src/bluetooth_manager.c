#include "bluetooth_manager.h"

#include "data_center.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#if __has_include("esp_nimble_hci.h")
#include "esp_nimble_hci.h"
#define BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI 1
#else
#define BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI 0
#endif
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "store/config/ble_store_config.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

#define BLUETOOTH_MANAGER_QUEUE_LEN 8
#define BLUETOOTH_MANAGER_TASK_STACK_SIZE 4096
#define BLUETOOTH_MANAGER_TASK_PRIORITY 5
#define BLUETOOTH_MANAGER_MAX_CALLBACKS 8
#define BLUETOOTH_MANAGER_RX_BUFFER_SIZE 1024
#define BLUETOOTH_MANAGER_FRAME_HEADER_SIZE 6
#define BLUETOOTH_MANAGER_FRAME_MAGIC_0 0xA5
#define BLUETOOTH_MANAGER_FRAME_MAGIC_1 0x5A
#define BLUETOOTH_MANAGER_FRAME_VERSION 0x01
#define BLUETOOTH_MANAGER_ANCS_ATTR_TEXT_MAX_LEN 96
#define BLUETOOTH_MANAGER_ANCS_CONTROL_POINT_MAX_LEN 32
#define BLUETOOTH_MANAGER_ANCS_GATT_RETRY_DELAY_US 150000
#define BLUETOOTH_MANAGER_ANCS_GATT_MAX_RETRIES 3
#define BLUETOOTH_MANAGER_HID_REPORT_ID_CONSUMER 1
#define BLUETOOTH_MANAGER_HID_APPEARANCE_KEYBOARD 0x03c1
#define BLUETOOTH_MANAGER_HID_USAGE_PLAY_PAUSE 0x00cd
#define BLUETOOTH_MANAGER_HID_USAGE_NEXT_TRACK 0x00b5
#define BLUETOOTH_MANAGER_HID_USAGE_PREVIOUS_TRACK 0x00b6
#define BLUETOOTH_MANAGER_HID_USAGE_VOLUME_UP 0x00e9
#define BLUETOOTH_MANAGER_HID_USAGE_VOLUME_DOWN 0x00ea
#define BLUETOOTH_MANAGER_HID_USAGE_MUTE 0x00e2

static const char *TAG = "bt_manager";

#ifdef DEBUG
#define BT_ANCS_DEBUG_LOGI(format, ...) ESP_LOGI(TAG, "[ANCS_FLOW] " format, ##__VA_ARGS__)
#else
#define BT_ANCS_DEBUG_LOGI(format, ...) ((void)0)
#endif

static void bluetooth_manager_copy_string(char *dst, size_t dst_size, const char *src);

typedef enum {
    BLUETOOTH_MANAGER_CMD_ENABLE = 1,
    BLUETOOTH_MANAGER_CMD_DISABLE,
    BLUETOOTH_MANAGER_CMD_SEND_MESSAGE,
    BLUETOOTH_MANAGER_CMD_SEND_MEDIA_KEY,
    BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES,
    BLUETOOTH_MANAGER_CMD_MOCK_CONNECT,
    BLUETOOTH_MANAGER_CMD_STOP_TASK,
} bluetooth_manager_cmd_type_t;

typedef struct {
    bluetooth_manager_cmd_type_t type;
    union {
        bluetooth_manager_message_t tx_message;
        bluetooth_manager_media_key_t media_key;
        struct {
            uint8_t data[BLUETOOTH_MANAGER_MESSAGE_MAX_LEN];
            size_t len;
        } rx_bytes;
        struct {
            char name[BLUETOOTH_MANAGER_DEVICE_NAME_MAX_LEN];
            char address[BLUETOOTH_MANAGER_DEVICE_ADDR_MAX_LEN];
        } mock_connect;
    } data;
} bluetooth_manager_cmd_t;

typedef struct {
    bool used;
    bluetooth_manager_event_cb_t cb;
    void *user_ctx;
} bluetooth_manager_callback_entry_t;

static QueueHandle_t s_cmd_queue;
static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_started;
static bool s_backend_enabled;
static bool s_nimble_port_initialized;
static bluetooth_manager_status_t s_status;
static bluetooth_manager_callback_entry_t s_callbacks[BLUETOOTH_MANAGER_MAX_CALLBACKS];
static uint8_t s_rx_buffer[BLUETOOTH_MANAGER_RX_BUFFER_SIZE];
static size_t s_rx_len;

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_char_handle;
static uint16_t s_hid_consumer_input_handle;
static uint8_t s_own_addr_type;
static uint16_t s_ancs_notification_source_handle;
static uint16_t s_ancs_control_point_handle;
static uint16_t s_ancs_data_source_handle;
static uint16_t s_ancs_notification_source_cccd_handle;
static uint16_t s_ancs_data_source_cccd_handle;
static uint16_t s_ancs_service_start_handle;
static uint16_t s_ancs_service_end_handle;
static uint8_t s_ancs_cccd_discovery_pending;
static uint16_t s_ancs_notification_desc_start_handle;
static uint16_t s_ancs_notification_desc_end_handle;
static uint16_t s_ancs_data_desc_start_handle;
static uint16_t s_ancs_data_desc_end_handle;
static esp_timer_handle_t s_ancs_data_cccd_timer;
static uint8_t s_ancs_data_cccd_retry_count;

typedef enum {
    BLUETOOTH_MANAGER_ANCS_EVENT_ADDED = 0,
    BLUETOOTH_MANAGER_ANCS_EVENT_MODIFIED = 1,
    BLUETOOTH_MANAGER_ANCS_EVENT_REMOVED = 2,
} bluetooth_manager_ancs_event_id_t;

typedef enum {
    BLUETOOTH_MANAGER_ANCS_ATTR_APP_ID = 0,
    BLUETOOTH_MANAGER_ANCS_ATTR_TITLE = 1,
    BLUETOOTH_MANAGER_ANCS_ATTR_SUBTITLE = 2,
    BLUETOOTH_MANAGER_ANCS_ATTR_MESSAGE = 3,
    BLUETOOTH_MANAGER_ANCS_ATTR_MESSAGE_SIZE = 4,
    BLUETOOTH_MANAGER_ANCS_ATTR_DATE = 5,
    BLUETOOTH_MANAGER_ANCS_ATTR_POSITIVE_ACTION = 6,
    BLUETOOTH_MANAGER_ANCS_ATTR_NEGATIVE_ACTION = 7,
} bluetooth_manager_ancs_attr_id_t;

typedef struct {
    uint32_t uid;
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    char app_id[BLUETOOTH_MANAGER_ANCS_ATTR_TEXT_MAX_LEN];
    char title[BLUETOOTH_MANAGER_ANCS_ATTR_TEXT_MAX_LEN];
    char subtitle[BLUETOOTH_MANAGER_ANCS_ATTR_TEXT_MAX_LEN];
    char message[BLUETOOTH_MANAGER_ANCS_ATTR_TEXT_MAX_LEN];
    char date[24];
} bluetooth_manager_ancs_notification_t;

static bluetooth_manager_ancs_notification_t s_ancs_pending_notification;

static void bluetooth_manager_handle_rx_bytes(const uint8_t *data, size_t len);
static void bluetooth_manager_start_advertising(void);
static void bluetooth_manager_start_ancs_discovery(uint16_t conn_handle);
static void bluetooth_manager_reset_ancs_state(void);
static void bluetooth_manager_publish_ancs_notification(const bluetooth_manager_ancs_notification_t *notification);
static void bluetooth_manager_discover_ancs_data_cccd(uint16_t conn_handle);
static int bluetooth_manager_ancs_disc_desc_cb(uint16_t conn_handle,
                                               const struct ble_gatt_error *error,
                                               uint16_t chr_val_handle,
                                               const struct ble_gatt_dsc *dsc,
                                               void *arg);
static void bluetooth_manager_schedule_ancs_data_cccd_discovery(uint16_t conn_handle);

static const ble_uuid128_t s_service_uuid =
    BLE_UUID128_INIT(0x41, 0x49, 0x57, 0x00, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
static const ble_uuid128_t s_rx_char_uuid =
    BLE_UUID128_INIT(0x41, 0x49, 0x57, 0x01, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
static const ble_uuid128_t s_tx_char_uuid =
    BLE_UUID128_INIT(0x41, 0x49, 0x57, 0x02, 0x00, 0x00, 0x10, 0x00,
                     0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);
static const ble_uuid128_t s_ancs_service_uuid =
    BLE_UUID128_INIT(0xd0, 0x00, 0x2d, 0x12, 0x1e, 0x4b, 0x0f, 0xa4,
                     0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x79);
static const ble_uuid128_t s_ancs_notification_source_uuid =
    BLE_UUID128_INIT(0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c,
                     0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f);
static const ble_uuid128_t s_ancs_control_point_uuid =
    BLE_UUID128_INIT(0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98,
                     0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69);
static const ble_uuid128_t s_ancs_data_source_uuid =
    BLE_UUID128_INIT(0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe,
                     0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22);

static int bluetooth_manager_gatt_access_cb(uint16_t conn_handle,
                                            uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt,
                                            void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint8_t data[BLUETOOTH_MANAGER_MESSAGE_MAX_LEN];
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > sizeof(data)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    int rc = ble_hs_mbuf_to_flat(ctxt->om, data, sizeof(data), &len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    bluetooth_manager_handle_rx_bytes(data, len);
    return 0;
}

static const uint8_t s_hid_information[] = {0x11, 0x01, 0x00, 0x03};
static const uint8_t s_hid_report_map[] = {
    0x05, 0x0c,       // Usage Page (Consumer)
    0x09, 0x01,       // Usage (Consumer Control)
    0xa1, 0x01,       // Collection (Application)
    0x85, BLUETOOTH_MANAGER_HID_REPORT_ID_CONSUMER,
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xff, 0x03, // Logical Maximum (0x03ff)
    0x19, 0x00,       // Usage Minimum (0)
    0x2a, 0xff, 0x03, // Usage Maximum (0x03ff)
    0x75, 0x10,       // Report Size (16)
    0x95, 0x01,       // Report Count (1)
    0x81, 0x00,       // Input (Data, Array, Absolute)
    0xc0,             // End Collection
};
static uint8_t s_hid_protocol_mode = 1;
static uint8_t s_hid_control_point;
static const uint8_t s_hid_consumer_report_ref[] = {BLUETOOTH_MANAGER_HID_REPORT_ID_CONSUMER, 0x01};

static int bluetooth_manager_hid_access_cb(uint16_t conn_handle,
                                           uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt,
                                           void *arg)
{
    (void)conn_handle;
    (void)attr_handle;

    uintptr_t id = (uintptr_t)arg;
    const void *data = NULL;
    size_t len = 0;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (id == 3) {
            uint8_t value = 0;
            uint16_t value_len = OS_MBUF_PKTLEN(ctxt->om);
            if (value_len != 1 || ble_hs_mbuf_to_flat(ctxt->om, &value, sizeof(value), &value_len) != 0) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            s_hid_control_point = value;
            return 0;
        }
        if (id == 4) {
            uint8_t value = 0;
            uint16_t value_len = OS_MBUF_PKTLEN(ctxt->om);
            if (value_len != 1 || ble_hs_mbuf_to_flat(ctxt->om, &value, sizeof(value), &value_len) != 0) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            s_hid_protocol_mode = value;
            return 0;
        }
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR && ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    switch (id) {
    case 1:
        data = s_hid_information;
        len = sizeof(s_hid_information);
        break;
    case 2:
        data = s_hid_report_map;
        len = sizeof(s_hid_report_map);
        break;
    case 4:
        data = &s_hid_protocol_mode;
        len = sizeof(s_hid_protocol_mode);
        break;
    case 5: {
        static const uint16_t empty_report = 0;
        data = &empty_report;
        len = sizeof(empty_report);
        break;
    }
    case 6:
        data = s_hid_consumer_report_ref;
        len = sizeof(s_hid_consumer_report_ref);
        break;
    default:
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    return os_mbuf_append(ctxt->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x1812),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x2a4a),
                .access_cb = bluetooth_manager_hid_access_cb,
                .arg = (void *)1,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2a4b),
                .access_cb = bluetooth_manager_hid_access_cb,
                .arg = (void *)2,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2a4c),
                .access_cb = bluetooth_manager_hid_access_cb,
                .arg = (void *)3,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2a4e),
                .access_cb = bluetooth_manager_hid_access_cb,
                .arg = (void *)4,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2a4d),
                .access_cb = bluetooth_manager_hid_access_cb,
                .arg = (void *)5,
                .val_handle = &s_hid_consumer_input_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(0x2908),
                        .access_cb = bluetooth_manager_hid_access_cb,
                        .arg = (void *)6,
                        .att_flags = BLE_ATT_F_READ,
                    },
                    {0},
                },
            },
            {0},
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_rx_char_uuid.u,
                .access_cb = bluetooth_manager_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &s_tx_char_uuid.u,
                .access_cb = bluetooth_manager_gatt_access_cb,
                .val_handle = &s_tx_char_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};

static void bluetooth_manager_reset_ancs_state(void)
{
    s_ancs_notification_source_handle = 0;
    s_ancs_control_point_handle = 0;
    s_ancs_data_source_handle = 0;
    s_ancs_notification_source_cccd_handle = 0;
    s_ancs_data_source_cccd_handle = 0;
    s_ancs_service_start_handle = 0;
    s_ancs_service_end_handle = 0;
    s_ancs_cccd_discovery_pending = 0;
    s_ancs_notification_desc_start_handle = 0;
    s_ancs_notification_desc_end_handle = 0;
    s_ancs_data_desc_start_handle = 0;
    s_ancs_data_desc_end_handle = 0;
    s_ancs_data_cccd_retry_count = 0;
    if (s_ancs_data_cccd_timer) {
        esp_timer_stop(s_ancs_data_cccd_timer);
    }
    memset(&s_ancs_pending_notification, 0, sizeof(s_ancs_pending_notification));
}

static void bluetooth_manager_ancs_copy_attr(char *dst, size_t dst_size, const uint8_t *src, uint16_t src_len)
{
    if (!dst || dst_size == 0) {
        return;
    }
    size_t copy_len = src_len < dst_size - 1 ? src_len : dst_size - 1;
    if (src && copy_len > 0) {
        memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
}

static void bluetooth_manager_publish_ancs_notification(const bluetooth_manager_ancs_notification_t *notification)
{
    if (!notification || notification->event_id == BLUETOOTH_MANAGER_ANCS_EVENT_REMOVED) {
        BT_ANCS_DEBUG_LOGI("publish skipped: notification=%p event_id=%u",
                           (const void *)notification,
                           notification ? notification->event_id : 0xff);
        return;
    }

    char payload[DATA_CENTER_PAYLOAD_MAX_LEN];
    const char *title = notification->title[0] ? notification->title : notification->app_id;
    const char *message = notification->message[0] ? notification->message : notification->subtitle;
    int written = snprintf(payload, sizeof(payload), "%s%s%s",
                           title ? title : "Notification",
                           message && message[0] ? ": " : "",
                           message && message[0] ? message : "");
    if (written < 0) {
        return;
    }

    data_center_message_t data_message = {0};
    bluetooth_manager_copy_string(data_message.source, sizeof(data_message.source), "ancs");
    bluetooth_manager_copy_string(data_message.target, sizeof(data_message.target), DATA_CENTER_TARGET_BROADCAST);
    data_message.kind = DATA_CENTER_MESSAGE_KIND_NOTIFICATION;
    data_message.payload_len = (size_t)written < sizeof(data_message.payload) ? (size_t)written : sizeof(data_message.payload) - 1;
    memcpy(data_message.payload, payload, data_message.payload_len);
    data_message.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    data_message.priority = notification->event_id == BLUETOOTH_MANAGER_ANCS_EVENT_ADDED ? 2 : 1;
    BT_ANCS_DEBUG_LOGI("data_center_publish_message: source=%s target=%s kind=%d priority=%u payload_len=%u payload=%.*s",
                       data_message.source,
                       data_message.target,
                       data_message.kind,
                       data_message.priority,
                       (unsigned)data_message.payload_len,
                       (int)data_message.payload_len,
                       (const char *)data_message.payload);
    esp_err_t ret = data_center_publish_message(&data_message);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish ANCS notification: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_request_ancs_attributes(uint32_t uid)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_ancs_control_point_handle == 0) {
        BT_ANCS_DEBUG_LOGI("control point request skipped: uid=%lu conn=%u control_handle=%u",
                           (unsigned long)uid,
                           s_conn_handle,
                           s_ancs_control_point_handle);
        return;
    }

    uint8_t command[BLUETOOTH_MANAGER_ANCS_CONTROL_POINT_MAX_LEN];
    size_t pos = 0;
    command[pos++] = 0x00; // CommandIDGetNotificationAttributes
    command[pos++] = (uint8_t)(uid & 0xff);
    command[pos++] = (uint8_t)((uid >> 8) & 0xff);
    command[pos++] = (uint8_t)((uid >> 16) & 0xff);
    command[pos++] = (uint8_t)((uid >> 24) & 0xff);
    command[pos++] = BLUETOOTH_MANAGER_ANCS_ATTR_APP_ID;
    command[pos++] = BLUETOOTH_MANAGER_ANCS_ATTR_TITLE;
    command[pos++] = 64;
    command[pos++] = 0;
    command[pos++] = BLUETOOTH_MANAGER_ANCS_ATTR_SUBTITLE;
    command[pos++] = 64;
    command[pos++] = 0;
    command[pos++] = BLUETOOTH_MANAGER_ANCS_ATTR_MESSAGE;
    command[pos++] = 96;
    command[pos++] = 0;
    command[pos++] = BLUETOOTH_MANAGER_ANCS_ATTR_DATE;

    BT_ANCS_DEBUG_LOGI("Control Point request attributes: uid=%lu handle=%u command_len=%u",
                       (unsigned long)uid,
                       s_ancs_control_point_handle,
                       (unsigned)pos);
    int rc = ble_gattc_write_flat(s_conn_handle, s_ancs_control_point_handle,
                                  command, pos, NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to request ANCS attributes for uid=%lu: %d", (unsigned long)uid, rc);
    }
}

static void bluetooth_manager_handle_ancs_notification(const uint8_t *data, uint16_t len)
{
    if (!data || len < 8) {
        BT_ANCS_DEBUG_LOGI("Notification Source ignored: data=%p len=%u", (const void *)data, len);
        return;
    }

    uint32_t uid = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                   ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    memset(&s_ancs_pending_notification, 0, sizeof(s_ancs_pending_notification));
    s_ancs_pending_notification.event_id = data[0];
    s_ancs_pending_notification.event_flags = data[1];
    s_ancs_pending_notification.category_id = data[2];
    s_ancs_pending_notification.category_count = data[3];
    s_ancs_pending_notification.uid = uid;

    ESP_LOGI(TAG, "ANCS event=%u flags=0x%02x category=%u count=%u uid=%lu",
             data[0], data[1], data[2], data[3], (unsigned long)uid);
    BT_ANCS_DEBUG_LOGI("Notification Source received: event=%u flags=0x%02x category=%u count=%u uid=%lu",
                       data[0], data[1], data[2], data[3], (unsigned long)uid);

    if (data[0] == BLUETOOTH_MANAGER_ANCS_EVENT_REMOVED) {
        BT_ANCS_DEBUG_LOGI("Notification Source removed event ignored: uid=%lu", (unsigned long)uid);
        return;
    }
    bluetooth_manager_request_ancs_attributes(uid);
}

static void bluetooth_manager_handle_ancs_data_source(const uint8_t *data, uint16_t len)
{
    if (!data || len < 5 || data[0] != 0x00) {
        BT_ANCS_DEBUG_LOGI("Data Source ignored: data=%p len=%u command=%u",
                           (const void *)data,
                           len,
                           data ? data[0] : 0xff);
        return;
    }

    uint32_t uid = (uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                   ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
    BT_ANCS_DEBUG_LOGI("Data Source received: uid=%lu len=%u", (unsigned long)uid, len);
    if (uid != s_ancs_pending_notification.uid) {
        BT_ANCS_DEBUG_LOGI("Data Source uid changed: pending=%lu received=%lu",
                           (unsigned long)s_ancs_pending_notification.uid,
                           (unsigned long)uid);
        memset(&s_ancs_pending_notification, 0, sizeof(s_ancs_pending_notification));
        s_ancs_pending_notification.uid = uid;
    }

    size_t pos = 5;
    while (pos + 3 <= len) {
        uint8_t attr_id = data[pos++];
        uint16_t attr_len = (uint16_t)data[pos] | ((uint16_t)data[pos + 1] << 8);
        pos += 2;
        BT_ANCS_DEBUG_LOGI("Data Source attr: uid=%lu attr=%u len=%u",
                           (unsigned long)uid,
                           attr_id,
                           attr_len);
        if (pos + attr_len > len) {
            ESP_LOGW(TAG, "Truncated ANCS attribute uid=%lu attr=%u len=%u", (unsigned long)uid, attr_id, attr_len);
            return;
        }

        switch (attr_id) {
        case BLUETOOTH_MANAGER_ANCS_ATTR_APP_ID:
            bluetooth_manager_ancs_copy_attr(s_ancs_pending_notification.app_id,
                                             sizeof(s_ancs_pending_notification.app_id), &data[pos], attr_len);
            break;
        case BLUETOOTH_MANAGER_ANCS_ATTR_TITLE:
            bluetooth_manager_ancs_copy_attr(s_ancs_pending_notification.title,
                                             sizeof(s_ancs_pending_notification.title), &data[pos], attr_len);
            break;
        case BLUETOOTH_MANAGER_ANCS_ATTR_SUBTITLE:
            bluetooth_manager_ancs_copy_attr(s_ancs_pending_notification.subtitle,
                                             sizeof(s_ancs_pending_notification.subtitle), &data[pos], attr_len);
            break;
        case BLUETOOTH_MANAGER_ANCS_ATTR_MESSAGE:
            bluetooth_manager_ancs_copy_attr(s_ancs_pending_notification.message,
                                             sizeof(s_ancs_pending_notification.message), &data[pos], attr_len);
            break;
        case BLUETOOTH_MANAGER_ANCS_ATTR_DATE:
            bluetooth_manager_ancs_copy_attr(s_ancs_pending_notification.date,
                                             sizeof(s_ancs_pending_notification.date), &data[pos], attr_len);
            break;
        default:
            break;
        }
        pos += attr_len;
    }

    ESP_LOGI(TAG, "ANCS notification uid=%lu app=%s title=%s message=%s",
             (unsigned long)s_ancs_pending_notification.uid,
             s_ancs_pending_notification.app_id,
             s_ancs_pending_notification.title,
             s_ancs_pending_notification.message);
    BT_ANCS_DEBUG_LOGI("Data Source parsed: uid=%lu app=%s title=%s subtitle=%s message=%s date=%s",
                       (unsigned long)s_ancs_pending_notification.uid,
                       s_ancs_pending_notification.app_id,
                       s_ancs_pending_notification.title,
                       s_ancs_pending_notification.subtitle,
                       s_ancs_pending_notification.message,
                       s_ancs_pending_notification.date);
    bluetooth_manager_publish_ancs_notification(&s_ancs_pending_notification);
}

static void bluetooth_manager_handle_ancs_gatt_notify(uint16_t attr_handle, struct os_mbuf *om)
{
    if (!om) {
        return;
    }

    uint8_t data[128];
    uint16_t len = OS_MBUF_PKTLEN(om);
    if (len > sizeof(data)) {
        len = sizeof(data);
    }
    int rc = ble_hs_mbuf_to_flat(om, data, sizeof(data), &len);
    if (rc != 0) {
        BT_ANCS_DEBUG_LOGI("notify mbuf flatten failed: attr_handle=%u rc=%d", attr_handle, rc);
        return;
    }

    BT_ANCS_DEBUG_LOGI("BLE notify RX: attr_handle=%u len=%u notification_handle=%u data_handle=%u",
                       attr_handle,
                       len,
                       s_ancs_notification_source_handle,
                       s_ancs_data_source_handle);
    if (attr_handle == s_ancs_notification_source_handle) {
        bluetooth_manager_handle_ancs_notification(data, len);
    } else if (attr_handle == s_ancs_data_source_handle) {
        bluetooth_manager_handle_ancs_data_source(data, len);
    }
}

static int bluetooth_manager_ancs_subscribe_cb(uint16_t conn_handle,
                                               const struct ble_gatt_error *error,
                                               struct ble_gatt_attr *attr,
                                               void *arg)
{
    (void)conn_handle;
    (void)attr;
    const char *name = (const char *)arg;
    if (error && error->status != 0) {
        ESP_LOGW(TAG, "ANCS subscribe %s failed: %d", name, error->status);
    } else {
        ESP_LOGI(TAG, "ANCS subscribe %s ok", name);
        BT_ANCS_DEBUG_LOGI("subscribe %s ok", name);
    }
    return 0;
}

static void bluetooth_manager_subscribe_ancs(uint16_t conn_handle)
{
    const uint8_t notify_enable[2] = {0x01, 0x00};
    BT_ANCS_DEBUG_LOGI("subscribe ANCS: conn=%u notification_cccd=%u data_cccd=%u",
                       conn_handle,
                       s_ancs_notification_source_cccd_handle,
                       s_ancs_data_source_cccd_handle);
    if (s_ancs_notification_source_cccd_handle != 0) {
        ble_gattc_write_flat(conn_handle, s_ancs_notification_source_cccd_handle,
                             notify_enable, sizeof(notify_enable),
                             bluetooth_manager_ancs_subscribe_cb, "notification");
    }
    if (s_ancs_data_source_cccd_handle != 0) {
        ble_gattc_write_flat(conn_handle, s_ancs_data_source_cccd_handle,
                             notify_enable, sizeof(notify_enable),
                             bluetooth_manager_ancs_subscribe_cb, "data");
    }
}

static void bluetooth_manager_discover_ancs_data_cccd(uint16_t conn_handle)
{
    if (s_ancs_data_desc_start_handle == 0 || s_ancs_data_desc_end_handle == 0) {
        bluetooth_manager_subscribe_ancs(conn_handle);
        return;
    }

    int rc = ble_gattc_disc_all_dscs(conn_handle, s_ancs_data_source_handle,
                                     s_ancs_data_desc_end_handle,
                                     bluetooth_manager_ancs_disc_desc_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to discover ANCS data CCCD: %d", rc);
        if (s_ancs_data_cccd_retry_count < BLUETOOTH_MANAGER_ANCS_GATT_MAX_RETRIES) {
            s_ancs_data_cccd_retry_count++;
            bluetooth_manager_schedule_ancs_data_cccd_discovery(conn_handle);
        } else {
            bluetooth_manager_subscribe_ancs(conn_handle);
        }
    }
}

static void bluetooth_manager_ancs_data_cccd_timer_cb(void *arg)
{
    uint16_t conn_handle = (uint16_t)(uintptr_t)arg;
    bluetooth_manager_discover_ancs_data_cccd(conn_handle);
}

static void bluetooth_manager_schedule_ancs_data_cccd_discovery(uint16_t conn_handle)
{
    if (!s_ancs_data_cccd_timer) {
        const esp_timer_create_args_t timer_args = {
            .callback = bluetooth_manager_ancs_data_cccd_timer_cb,
            .arg = (void *)(uintptr_t)conn_handle,
            .name = "ancs_data_cccd",
        };
        esp_err_t ret = esp_timer_create(&timer_args, &s_ancs_data_cccd_timer);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to create ANCS data CCCD retry timer: %s", esp_err_to_name(ret));
            bluetooth_manager_subscribe_ancs(conn_handle);
            return;
        }
    }

    esp_timer_stop(s_ancs_data_cccd_timer);
    esp_err_t ret = esp_timer_start_once(s_ancs_data_cccd_timer,
                                         BLUETOOTH_MANAGER_ANCS_GATT_RETRY_DELAY_US);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to schedule ANCS data CCCD discovery: %s", esp_err_to_name(ret));
        bluetooth_manager_subscribe_ancs(conn_handle);
    }
}

static int bluetooth_manager_ancs_disc_desc_cb(uint16_t conn_handle,
                                               const struct ble_gatt_error *error,
                                               uint16_t chr_val_handle,
                                               const struct ble_gatt_dsc *dsc,
                                               void *arg)
{
    (void)arg;
    if (error && error->status == BLE_HS_EDONE) {
        BT_ANCS_DEBUG_LOGI("CCCD discovery done: notification_cccd=%u data_cccd=%u",
                           s_ancs_notification_source_cccd_handle,
                           s_ancs_data_source_cccd_handle);
        if (chr_val_handle == s_ancs_notification_source_handle) {
            bluetooth_manager_schedule_ancs_data_cccd_discovery(conn_handle);
        } else {
            bluetooth_manager_subscribe_ancs(conn_handle);
        }
        return 0;
    }
    if (!dsc || dsc->uuid.u.type != BLE_UUID_TYPE_16 ||
        BLE_UUID16(&dsc->uuid.u)->value != BLE_GATT_DSC_CLT_CFG_UUID16) {
        return 0;
    }
    if (chr_val_handle == s_ancs_notification_source_handle) {
        s_ancs_notification_source_cccd_handle = dsc->handle;
        BT_ANCS_DEBUG_LOGI("Notification Source CCCD found: handle=%u", dsc->handle);
    } else if (chr_val_handle == s_ancs_data_source_handle) {
        s_ancs_data_source_cccd_handle = dsc->handle;
        BT_ANCS_DEBUG_LOGI("Data Source CCCD found: handle=%u", dsc->handle);
    }
    return 0;
}

static uint16_t bluetooth_manager_ancs_next_char_or_service_end(uint16_t chr_handle)
{
    uint16_t end_handle = s_ancs_service_end_handle;

    if (s_ancs_notification_source_handle > chr_handle && s_ancs_notification_source_handle - 1 < end_handle) {
        end_handle = s_ancs_notification_source_handle - 1;
    }
    if (s_ancs_control_point_handle > chr_handle && s_ancs_control_point_handle - 1 < end_handle) {
        end_handle = s_ancs_control_point_handle - 1;
    }
    if (s_ancs_data_source_handle > chr_handle && s_ancs_data_source_handle - 1 < end_handle) {
        end_handle = s_ancs_data_source_handle - 1;
    }

    return end_handle;
}

static void bluetooth_manager_discover_ancs_cccds(uint16_t conn_handle)
{
    BT_ANCS_DEBUG_LOGI("discover CCCDs: conn=%u notification_handle=%u data_handle=%u service_end=%u",
                       conn_handle,
                       s_ancs_notification_source_handle,
                       s_ancs_data_source_handle,
                       s_ancs_service_end_handle);
    s_ancs_cccd_discovery_pending = 0;
    s_ancs_notification_desc_start_handle = 0;
    s_ancs_notification_desc_end_handle = 0;
    s_ancs_data_desc_start_handle = 0;
    s_ancs_data_desc_end_handle = 0;
    if (s_ancs_notification_source_handle != 0) {
        uint16_t notification_desc_end = bluetooth_manager_ancs_next_char_or_service_end(s_ancs_notification_source_handle);
        if (notification_desc_end > s_ancs_notification_source_handle) {
            s_ancs_notification_desc_start_handle = s_ancs_notification_source_handle + 1;
            s_ancs_notification_desc_end_handle = notification_desc_end;
        }
    }
    if (s_ancs_data_source_handle != 0) {
        uint16_t data_desc_end = bluetooth_manager_ancs_next_char_or_service_end(s_ancs_data_source_handle);
        if (data_desc_end > s_ancs_data_source_handle) {
            s_ancs_data_desc_start_handle = s_ancs_data_source_handle + 1;
            s_ancs_data_desc_end_handle = data_desc_end;
        }
    }

    if (s_ancs_notification_desc_start_handle != 0) {
        int rc = ble_gattc_disc_all_dscs(conn_handle, s_ancs_notification_source_handle,
                                         s_ancs_notification_desc_end_handle,
                                         bluetooth_manager_ancs_disc_desc_cb, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to discover ANCS notification CCCD: %d", rc);
            bluetooth_manager_discover_ancs_data_cccd(conn_handle);
        }
    } else if (s_ancs_data_desc_start_handle != 0) {
        bluetooth_manager_discover_ancs_data_cccd(conn_handle);
    } else {
        ESP_LOGW(TAG, "ANCS CCCD handles not found, cannot subscribe");
    }
}

static int bluetooth_manager_ancs_disc_chr_cb(uint16_t conn_handle,
                                              const struct ble_gatt_error *error,
                                              const struct ble_gatt_chr *chr,
                                              void *arg)
{
    (void)arg;
    if (error && error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "ANCS chars: notif=%u control=%u data=%u",
                 s_ancs_notification_source_handle,
                 s_ancs_control_point_handle,
                 s_ancs_data_source_handle);
        BT_ANCS_DEBUG_LOGI("characteristic discovery done: notification=%u control=%u data=%u",
                           s_ancs_notification_source_handle,
                           s_ancs_control_point_handle,
                           s_ancs_data_source_handle);
        bluetooth_manager_discover_ancs_cccds(conn_handle);
        return 0;
    }
    if (!chr) {
        return 0;
    }
    if (ble_uuid_cmp(&chr->uuid.u, &s_ancs_notification_source_uuid.u) == 0) {
        s_ancs_notification_source_handle = chr->val_handle;
        BT_ANCS_DEBUG_LOGI("Notification Source characteristic found: handle=%u", chr->val_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &s_ancs_control_point_uuid.u) == 0) {
        s_ancs_control_point_handle = chr->val_handle;
        BT_ANCS_DEBUG_LOGI("Control Point characteristic found: handle=%u", chr->val_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &s_ancs_data_source_uuid.u) == 0) {
        s_ancs_data_source_handle = chr->val_handle;
        BT_ANCS_DEBUG_LOGI("Data Source characteristic found: handle=%u", chr->val_handle);
    }
    return 0;
}

static int bluetooth_manager_ancs_disc_svc_cb(uint16_t conn_handle,
                                              const struct ble_gatt_error *error,
                                              const struct ble_gatt_svc *service,
                                              void *arg)
{
    (void)arg;
    if (error && error->status == BLE_HS_EDONE) {
        if (s_ancs_service_start_handle == 0) {
            ESP_LOGW(TAG, "ANCS service not found. iPhone may require pairing/authorization first.");
            BT_ANCS_DEBUG_LOGI("service discovery done: ANCS service not found");
        } else {
            BT_ANCS_DEBUG_LOGI("service discovery done: start=%u end=%u",
                               s_ancs_service_start_handle,
                               s_ancs_service_end_handle);
        }
        return 0;
    }
    if (!service) {
        return 0;
    }

    s_ancs_service_start_handle = service->start_handle;
    s_ancs_service_end_handle = service->end_handle;
    ESP_LOGI(TAG, "ANCS service found: start=%u end=%u",
             s_ancs_service_start_handle, s_ancs_service_end_handle);
    BT_ANCS_DEBUG_LOGI("ANCS service found: conn=%u start=%u end=%u",
                       conn_handle,
                       s_ancs_service_start_handle,
                       s_ancs_service_end_handle);
    ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                            bluetooth_manager_ancs_disc_chr_cb, NULL);
    return 0;
}

static void bluetooth_manager_start_ancs_discovery(uint16_t conn_handle)
{
    bluetooth_manager_reset_ancs_state();
    BT_ANCS_DEBUG_LOGI("discover ANCS service: conn=%u", conn_handle);
    int rc = ble_gattc_disc_svc_by_uuid(conn_handle, &s_ancs_service_uuid.u,
                                        bluetooth_manager_ancs_disc_svc_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to start ANCS discovery: %d", rc);
    }
}
#endif

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static void bluetooth_manager_nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#endif

static esp_err_t bluetooth_manager_enqueue_enabled_command(bool enabled)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = enabled ? BLUETOOTH_MANAGER_CMD_ENABLE : BLUETOOTH_MANAGER_CMD_DISABLE,
    };
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void bluetooth_manager_handle_data_event(const data_center_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (!event || event->type != DATA_CENTER_EVENT_BLUETOOTH_COMMAND ||
        event->payload_len < sizeof(data_center_bluetooth_command_t)) {
        return;
    }

    data_center_bluetooth_command_t command;
    memcpy(&command, event->payload, sizeof(command));
    if (command.command != DATA_CENTER_BLUETOOTH_CMD_SET_ENABLED) {
        ESP_LOGW(TAG, "Unsupported bluetooth command: %d", command.command);
        return;
    }

    esp_err_t ret = bluetooth_manager_enqueue_enabled_command(command.enabled);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enqueue bluetooth command: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static data_center_message_kind_t bluetooth_manager_to_data_center_kind(bluetooth_manager_message_type_t type)
{
    switch (type) {
    case BLUETOOTH_MANAGER_MESSAGE_NOTIFICATION:
        return DATA_CENTER_MESSAGE_KIND_NOTIFICATION;
    case BLUETOOTH_MANAGER_MESSAGE_COMMAND:
        return DATA_CENTER_MESSAGE_KIND_COMMAND;
    case BLUETOOTH_MANAGER_MESSAGE_TIME_SYNC:
        return DATA_CENTER_MESSAGE_KIND_TIME_SYNC;
    case BLUETOOTH_MANAGER_MESSAGE_DEVICE_INFO:
        return DATA_CENTER_MESSAGE_KIND_DEVICE_INFO;
    case BLUETOOTH_MANAGER_MESSAGE_AI_TEXT:
        return DATA_CENTER_MESSAGE_KIND_AI_TEXT;
    case BLUETOOTH_MANAGER_MESSAGE_AI_AUDIO_CHUNK:
        return DATA_CENTER_MESSAGE_KIND_AI_AUDIO_CHUNK;
    case BLUETOOTH_MANAGER_MESSAGE_UNKNOWN:
    default:
        return DATA_CENTER_MESSAGE_KIND_INFO;
    }
}

static void bluetooth_manager_notify(const bluetooth_manager_event_t *event)
{
    bluetooth_manager_callback_entry_t callbacks[BLUETOOTH_MANAGER_MAX_CALLBACKS];
    memset(callbacks, 0, sizeof(callbacks));

    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        memcpy(callbacks, s_callbacks, sizeof(callbacks));
        xSemaphoreGive(s_lock);
    }

    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (callbacks[i].used && callbacks[i].cb) {
            callbacks[i].cb(event, callbacks[i].user_ctx);
        }
    }
}

static void bluetooth_manager_set_status(bluetooth_manager_state_t state, bool enabled)
{
    bluetooth_manager_status_t status;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    s_status.enabled = enabled;
    status = s_status;
    xSemaphoreGive(s_lock);

    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_STATUS_CHANGED,
        .data.status = status,
    };
    bluetooth_manager_notify(&event);

    data_center_event_t data_event = {0};
    bluetooth_manager_copy_string(data_event.source, sizeof(data_event.source), "bluetooth");
    bluetooth_manager_copy_string(data_event.target, sizeof(data_event.target), "device");
    bluetooth_manager_copy_string(data_event.topic, sizeof(data_event.topic), "bluetooth.status");
    data_event.type = DATA_CENTER_EVENT_BLUETOOTH_STATUS_CHANGED;
    data_event.flags = DATA_CENTER_FLAG_RETAIN_LATEST;
    data_event.payload_len = sizeof(status) < sizeof(data_event.payload) ? sizeof(status) : sizeof(data_event.payload);
    memcpy(data_event.payload, &status, data_event.payload_len);
    data_event.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    data_event.priority = 1;
    esp_err_t ret = data_center_publish_event(&data_event);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish bluetooth status: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_set_last_error(esp_err_t error)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.last_error = (int)error;
    xSemaphoreGive(s_lock);
}

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static int bluetooth_manager_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            BT_ANCS_DEBUG_LOGI("iPhone connect: conn=%u status=%d",
                               s_conn_handle,
                               event->connect.status);
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_CONNECTED, true);
            int rc = ble_gap_security_initiate(s_conn_handle);
            BT_ANCS_DEBUG_LOGI("BLE security initiate: conn=%u rc=%d", s_conn_handle, rc);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to initiate BLE security: %d", rc);
                BT_ANCS_DEBUG_LOGI("security initiate failed, fallback discover ANCS: conn=%u rc=%d",
                                   s_conn_handle,
                                   rc);
                bluetooth_manager_start_ancs_discovery(s_conn_handle);
            }
        } else {
            BT_ANCS_DEBUG_LOGI("connect failed: status=%d", event->connect.status);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
            bluetooth_manager_start_advertising();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        BT_ANCS_DEBUG_LOGI("disconnect: conn=%u reason=%d", s_conn_handle, event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        bluetooth_manager_reset_ancs_state();
        bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
        bluetooth_manager_start_advertising();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "BLE encrypted, starting ANCS discovery");
            BT_ANCS_DEBUG_LOGI("BLE security/encryption ok: conn=%u", event->enc_change.conn_handle);
            bluetooth_manager_start_ancs_discovery(event->enc_change.conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE encryption failed: %d", event->enc_change.status);
            BT_ANCS_DEBUG_LOGI("BLE security/encryption failed: conn=%u status=%d",
                               event->enc_change.conn_handle,
                               event->enc_change.status);
        }
        return 0;
    case BLE_GAP_EVENT_NOTIFY_RX:
        BT_ANCS_DEBUG_LOGI("GAP notify RX: attr_handle=%u", event->notify_rx.attr_handle);
        bluetooth_manager_handle_ancs_gatt_notify(event->notify_rx.attr_handle,
                                                  event->notify_rx.om);
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_backend_enabled && s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            bluetooth_manager_start_advertising();
        }
        return 0;
    default:
        return 0;
    }
}

static void bluetooth_manager_start_advertising(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer BLE address type: %d", rc);
        return;
    }

    struct ble_hs_adv_fields fields = {0};
    ble_uuid16_t hid_uuid = BLE_UUID16_INIT(0x1812);
    const char *name = ble_svc_gap_device_name();
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    fields.appearance = BLUETOOTH_MANAGER_HID_APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;
    fields.uuids16 = &hid_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.uuids128 = &s_service_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set scan response fields: %d", rc);
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.channel_map = BLE_GAP_ADV_DFLT_CHANNEL_MAP;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bluetooth_manager_gap_event_cb, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as AIWatch HID, addr_type=%u, channels=0x%02x",
                 s_own_addr_type, adv_params.channel_map);
    }
}

static void bluetooth_manager_on_sync(void)
{
    bluetooth_manager_start_advertising();
}
#endif

static esp_err_t bluetooth_manager_backend_init(void)
{
    if (s_backend_enabled) {
        return ESP_OK;
    }

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
    esp_err_t ret = ESP_OK;

#if BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI
    esp_bt_controller_status_t controller_status = esp_bt_controller_get_status();
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (controller_status == ESP_BT_CONTROLLER_STATUS_IDLE) {
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        controller_status = esp_bt_controller_get_status();
    }

    if (controller_status != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
    }

#if BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI
    ret = esp_nimble_hci_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
#endif
#endif

    if (!s_nimble_port_initialized) {
        ret = nimble_port_init();
        if (ret != ESP_OK) {
            return ret;
        }
        s_nimble_port_initialized = true;
    }

    ble_hs_cfg.store_read_cb = ble_store_config_read;
    ble_hs_cfg.store_write_cb = ble_store_config_write;
    ble_hs_cfg.store_delete_cb = ble_store_config_delete;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_device_name_set("Navy AIWatch");
    ble_svc_gatt_init();
    ble_hs_cfg.sync_cb = bluetooth_manager_on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        return ESP_FAIL;
    }

    nimble_port_freertos_init(bluetooth_manager_nimble_host_task);
    s_backend_enabled = true;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Bluetooth backend is not enabled in sdkconfig. Enable CONFIG_BT_ENABLED and CONFIG_BT_NIMBLE_ENABLED.");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void bluetooth_manager_publish_message(const bluetooth_manager_message_t *message)
{
    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_MESSAGE_RECEIVED,
        .data.message = *message,
    };
    bluetooth_manager_notify(&event);

    data_center_message_t data_message = {0};
    bluetooth_manager_copy_string(data_message.source, sizeof(data_message.source), "phone");
    bluetooth_manager_copy_string(data_message.target, sizeof(data_message.target), message->target);
    data_message.kind = bluetooth_manager_to_data_center_kind(message->type);
    data_message.payload_len = message->data_len < sizeof(data_message.payload) ? message->data_len : sizeof(data_message.payload);
    memcpy(data_message.payload, message->data, data_message.payload_len);
    data_message.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    data_message.priority = message->type == BLUETOOTH_MANAGER_MESSAGE_COMMAND ? 2 : 1;
    esp_err_t ret = data_center_publish_message(&data_message);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue message for data center: %s", esp_err_to_name(ret));
    }
}

static void bluetooth_manager_parse_rx_buffer(void)
{
    while (s_rx_len >= BLUETOOTH_MANAGER_FRAME_HEADER_SIZE) {
        if (s_rx_buffer[0] != BLUETOOTH_MANAGER_FRAME_MAGIC_0 || s_rx_buffer[1] != BLUETOOTH_MANAGER_FRAME_MAGIC_1) {
            memmove(&s_rx_buffer[0], &s_rx_buffer[1], s_rx_len - 1);
            s_rx_len--;
            continue;
        }

        if (s_rx_buffer[2] != BLUETOOTH_MANAGER_FRAME_VERSION) {
            memmove(&s_rx_buffer[0], &s_rx_buffer[1], s_rx_len - 1);
            s_rx_len--;
            continue;
        }

        uint16_t payload_len = ((uint16_t)s_rx_buffer[4] << 8) | s_rx_buffer[5];
        size_t frame_len = BLUETOOTH_MANAGER_FRAME_HEADER_SIZE + payload_len;
        if (payload_len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
            ESP_LOGW(TAG, "Dropping oversized frame: %u", payload_len);
            s_rx_len = 0;
            return;
        }
        if (s_rx_len < frame_len) {
            return;
        }

        bluetooth_manager_message_t message = {0};
        message.type = (bluetooth_manager_message_type_t)s_rx_buffer[3];
        message.data_len = payload_len;
        memcpy(message.data, &s_rx_buffer[BLUETOOTH_MANAGER_FRAME_HEADER_SIZE], payload_len);
        bluetooth_manager_publish_message(&message);

        size_t remaining = s_rx_len - frame_len;
        memmove(s_rx_buffer, &s_rx_buffer[frame_len], remaining);
        s_rx_len = remaining;
    }
}

static void bluetooth_manager_handle_rx_bytes(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }

    if (len > sizeof(s_rx_buffer) - s_rx_len) {
        ESP_LOGW(TAG, "RX buffer overflow, clearing buffer");
        s_rx_len = 0;
    }

    size_t copy_len = len < sizeof(s_rx_buffer) - s_rx_len ? len : sizeof(s_rx_buffer) - s_rx_len;
    memcpy(&s_rx_buffer[s_rx_len], data, copy_len);
    s_rx_len += copy_len;
    bluetooth_manager_parse_rx_buffer();
}

static void bluetooth_manager_handle_send_message(const bluetooth_manager_message_t *message)
{
#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE && s_tx_char_handle != 0 && message && message->data_len > 0) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(message->data, message->data_len);
        if (om) {
            int rc = ble_gattc_notify_custom(s_conn_handle, s_tx_char_handle, om);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to notify TX characteristic: %d", rc);
            }
        }
    }
#else
    (void)message;
#endif
    bluetooth_manager_event_t event = {
        .type = BLUETOOTH_MANAGER_EVT_MESSAGE_SENT,
    };
    bluetooth_manager_notify(&event);
}

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static uint16_t bluetooth_manager_media_key_to_hid_usage(bluetooth_manager_media_key_t key)
{
    switch (key) {
    case BLUETOOTH_MANAGER_MEDIA_KEY_PLAY_PAUSE:
        return BLUETOOTH_MANAGER_HID_USAGE_PLAY_PAUSE;
    case BLUETOOTH_MANAGER_MEDIA_KEY_NEXT_TRACK:
        return BLUETOOTH_MANAGER_HID_USAGE_NEXT_TRACK;
    case BLUETOOTH_MANAGER_MEDIA_KEY_PREVIOUS_TRACK:
        return BLUETOOTH_MANAGER_HID_USAGE_PREVIOUS_TRACK;
    case BLUETOOTH_MANAGER_MEDIA_KEY_VOLUME_UP:
        return BLUETOOTH_MANAGER_HID_USAGE_VOLUME_UP;
    case BLUETOOTH_MANAGER_MEDIA_KEY_VOLUME_DOWN:
        return BLUETOOTH_MANAGER_HID_USAGE_VOLUME_DOWN;
    case BLUETOOTH_MANAGER_MEDIA_KEY_MUTE:
        return BLUETOOTH_MANAGER_HID_USAGE_MUTE;
    default:
        return 0;
    }
}

static esp_err_t bluetooth_manager_notify_hid_consumer_usage(uint16_t usage)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_hid_consumer_input_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t report[] = {
        BLUETOOTH_MANAGER_HID_REPORT_ID_CONSUMER,
        (uint8_t)(usage & 0xff),
        (uint8_t)((usage >> 8) & 0xff),
    };
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));
    if (!om) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gattc_notify_custom(s_conn_handle, s_hid_consumer_input_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

static void bluetooth_manager_handle_send_media_key(bluetooth_manager_media_key_t key)
{
    uint16_t usage = bluetooth_manager_media_key_to_hid_usage(key);
    if (usage == 0) {
        ESP_LOGW(TAG, "Unsupported media key: %d", key);
        return;
    }

    esp_err_t ret = bluetooth_manager_notify_hid_consumer_usage(usage);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send HID media key press: %s", esp_err_to_name(ret));
        return;
    }

    ret = bluetooth_manager_notify_hid_consumer_usage(0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send HID media key release: %s", esp_err_to_name(ret));
    }
}
#endif

static void bluetooth_manager_disable_backend(void)
{
#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
    if (!s_backend_enabled) {
        return;
    }

    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    ble_gap_adv_stop();
    bluetooth_manager_reset_ancs_state();
    nimble_port_stop();
    nimble_port_deinit();
    s_nimble_port_initialized = false;
#if BLUETOOTH_MANAGER_HAS_ESP_NIMBLE_HCI
    esp_nimble_hci_deinit();
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_bt_controller_disable();
    }
#endif
    s_backend_enabled = false;
#endif
}

static void bluetooth_manager_task(void *arg)
{
    (void)arg;
    bluetooth_manager_cmd_t cmd;

    while (true) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (cmd.type) {
        case BLUETOOTH_MANAGER_CMD_ENABLE:
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_ON, true);
            esp_err_t enable_ret = bluetooth_manager_backend_init();
            if (enable_ret == ESP_OK) {
                bluetooth_manager_set_last_error(ESP_OK);
                bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
            } else {
                bluetooth_manager_set_last_error(enable_ret);
                bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ERROR, false);
            }
            break;
        case BLUETOOTH_MANAGER_CMD_DISABLE:
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_OFF, false);
            bluetooth_manager_set_last_error(ESP_OK);
            bluetooth_manager_disable_backend();
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_status.connected_name[0] = '\0';
            s_status.connected_address[0] = '\0';
            xSemaphoreGive(s_lock);
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_OFF, false);
            break;
        case BLUETOOTH_MANAGER_CMD_SEND_MESSAGE:
            bluetooth_manager_handle_send_message(&cmd.data.tx_message);
            break;
        case BLUETOOTH_MANAGER_CMD_SEND_MEDIA_KEY:
#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
            bluetooth_manager_handle_send_media_key(cmd.data.media_key);
#endif
            break;
        case BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES:
            bluetooth_manager_handle_rx_bytes(cmd.data.rx_bytes.data, cmd.data.rx_bytes.len);
            break;
        case BLUETOOTH_MANAGER_CMD_MOCK_CONNECT:
            xSemaphoreTake(s_lock, portMAX_DELAY);
            bluetooth_manager_copy_string(s_status.connected_name, sizeof(s_status.connected_name), cmd.data.mock_connect.name);
            bluetooth_manager_copy_string(s_status.connected_address, sizeof(s_status.connected_address), cmd.data.mock_connect.address);
            xSemaphoreGive(s_lock);
            bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_CONNECTED, true);
            break;
        case BLUETOOTH_MANAGER_CMD_STOP_TASK:
            s_task_handle = NULL;
            vTaskDelete(NULL);
            return;
        default:
            break;
        }
    }
}

esp_err_t bluetooth_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    s_cmd_queue = xQueueCreate(BLUETOOTH_MANAGER_QUEUE_LEN, sizeof(bluetooth_manager_cmd_t));
    if (!s_cmd_queue) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = BLUETOOTH_MANAGER_STATE_OFF;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t bluetooth_manager_init_ble_stack(void)
{
    esp_err_t ret = bluetooth_manager_init();
    if (ret != ESP_OK) {
        return ret;
    }

    bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_TURNING_ON, true);
    ret = bluetooth_manager_backend_init();
    bluetooth_manager_set_last_error(ret);
    if (ret != ESP_OK) {
        bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ERROR, false);
        return ret;
    }

    bluetooth_manager_set_status(BLUETOOTH_MANAGER_STATE_ON, true);
    return ESP_OK;
}

esp_err_t bluetooth_manager_start(void)
{
    if (!s_initialized) {
        esp_err_t ret = bluetooth_manager_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    if (s_started) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(bluetooth_manager_task,
                                 "bluetooth_manager",
                                 BLUETOOTH_MANAGER_TASK_STACK_SIZE,
                                 NULL,
                                 BLUETOOTH_MANAGER_TASK_PRIORITY,
                                 &s_task_handle);
    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    data_center_filter_t filter = {0};
    bluetooth_manager_copy_string(filter.owner, sizeof(filter.owner), "bluetooth");
    bluetooth_manager_copy_string(filter.target, sizeof(filter.target), "bluetooth");
    bluetooth_manager_copy_string(filter.topic, sizeof(filter.topic), "bluetooth.command");
    filter.type = DATA_CENTER_EVENT_BLUETOOTH_COMMAND;
    esp_err_t sub_ret = data_center_subscribe_event(&filter, bluetooth_manager_handle_data_event, NULL);
    if (sub_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe bluetooth commands: %s", esp_err_to_name(sub_ret));
    }
    return ESP_OK;
}

esp_err_t bluetooth_manager_stop(void)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_OK;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_STOP_TASK,
    };
    xQueueSend(s_cmd_queue, &cmd, portMAX_DELAY);
    data_center_unsubscribe_event(bluetooth_manager_handle_data_event, NULL);
    s_started = false;
    return ESP_OK;
}

esp_err_t bluetooth_manager_set_enabled(bool enabled)
{
    return bluetooth_manager_enqueue_enabled_command(enabled);
}

esp_err_t bluetooth_manager_get_status(bluetooth_manager_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t bluetooth_manager_send_message(const bluetooth_manager_message_t *message)
{
    if (!message || message->data_len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_SEND_MESSAGE,
        .data.tx_message = *message,
    };
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_send_raw(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    bluetooth_manager_message_t message = {
        .type = BLUETOOTH_MANAGER_MESSAGE_UNKNOWN,
        .data_len = len < BLUETOOTH_MANAGER_MESSAGE_MAX_LEN ? len : BLUETOOTH_MANAGER_MESSAGE_MAX_LEN,
    };
    memcpy(message.data, data, message.data_len);
    return bluetooth_manager_send_message(&message);
}

esp_err_t bluetooth_manager_send_media_key(bluetooth_manager_media_key_t key)
{
    if (key < BLUETOOTH_MANAGER_MEDIA_KEY_PLAY_PAUSE || key > BLUETOOTH_MANAGER_MEDIA_KEY_MUTE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_SEND_MEDIA_KEY,
        .data.media_key = key,
    };
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_on_raw_received(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > BLUETOOTH_MANAGER_MESSAGE_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_PROCESS_RX_BYTES,
    };
    cmd.data.rx_bytes.len = len;
    memcpy(cmd.data.rx_bytes.data, data, len);
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_mock_connect_phone(const char *name, const char *address)
{
    if (!s_started || !s_cmd_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    bluetooth_manager_cmd_t cmd = {
        .type = BLUETOOTH_MANAGER_CMD_MOCK_CONNECT,
    };
    bluetooth_manager_copy_string(cmd.data.mock_connect.name, sizeof(cmd.data.mock_connect.name), name);
    bluetooth_manager_copy_string(cmd.data.mock_connect.address, sizeof(cmd.data.mock_connect.address), address);
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bluetooth_manager_register_callback(bluetooth_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb || !s_lock) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (!s_callbacks[i].used) {
            s_callbacks[i].used = true;
            s_callbacks[i].cb = cb;
            s_callbacks[i].user_ctx = user_ctx;
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t bluetooth_manager_unregister_callback(bluetooth_manager_event_cb_t cb, void *user_ctx)
{
    if (!cb || !s_lock) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < BLUETOOTH_MANAGER_MAX_CALLBACKS; ++i) {
        if (s_callbacks[i].used && s_callbacks[i].cb == cb && s_callbacks[i].user_ctx == user_ctx) {
            memset(&s_callbacks[i], 0, sizeof(s_callbacks[i]));
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
    }
    xSemaphoreGive(s_lock);
    return ESP_ERR_NOT_FOUND;
}