#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_CENTER_TITLE_MAX_LEN 32
#define DATA_CENTER_MESSAGE_MAX_LEN 96
#define DATA_CENTER_SOURCE_MAX_LEN 24
#define DATA_CENTER_MAX_ITEMS 12

typedef enum {
    DATA_CENTER_ITEM_INFO = 0,
    DATA_CENTER_ITEM_WARNING,
    DATA_CENTER_ITEM_ACTION,
} data_center_item_type_t;

typedef struct {
    char source_app_id[DATA_CENTER_SOURCE_MAX_LEN];
    char title[DATA_CENTER_TITLE_MAX_LEN];
    char message[DATA_CENTER_MESSAGE_MAX_LEN];
    data_center_item_type_t type;
    uint32_t timestamp_ms;
    uint8_t priority;
} data_center_item_t;

esp_err_t data_center_publish(const data_center_item_t *item);
size_t data_center_get_snapshot(data_center_item_t *items, size_t max_items);
void data_center_clear(void);
size_t data_center_count(void);

#ifdef __cplusplus
}
#endif
