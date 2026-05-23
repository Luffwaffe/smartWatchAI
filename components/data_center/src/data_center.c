#include "data_center.h"

#include <stdio.h>
#include <string.h>

static data_center_item_t s_items[DATA_CENTER_MAX_ITEMS];
static size_t s_item_count = 0;

static void data_center_copy_string(char *dst, size_t dst_size, const char *src)
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

esp_err_t data_center_publish(const data_center_item_t *item)
{
    if (!item) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_item_count == DATA_CENTER_MAX_ITEMS) {
        memmove(&s_items[0], &s_items[1], sizeof(s_items[0]) * (DATA_CENTER_MAX_ITEMS - 1));
        s_item_count--;
    }

    data_center_item_t *slot = &s_items[s_item_count++];
    memset(slot, 0, sizeof(*slot));
    data_center_copy_string(slot->source_app_id, sizeof(slot->source_app_id), item->source_app_id);
    data_center_copy_string(slot->title, sizeof(slot->title), item->title);
    data_center_copy_string(slot->message, sizeof(slot->message), item->message);
    slot->type = item->type;
    slot->timestamp_ms = item->timestamp_ms;
    slot->priority = item->priority;
    return ESP_OK;
}

size_t data_center_get_snapshot(data_center_item_t *items, size_t max_items)
{
    if (!items || max_items == 0) {
        return 0;
    }

    size_t count = s_item_count < max_items ? s_item_count : max_items;
    for (size_t i = 0; i < count; ++i) {
        items[i] = s_items[i];
    }
    return count;
}

void data_center_clear(void)
{
    memset(s_items, 0, sizeof(s_items));
    s_item_count = 0;
}

size_t data_center_count(void)
{
    return s_item_count;
}
