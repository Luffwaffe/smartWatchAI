#include "test/test.h"

#include "test/contract.h"
#include "app_manager.h"

static esp_err_t test_view_open(lv_obj_t *root)
{
    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "test");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return ESP_OK;
}

static const app_t s_test_app = {
    .id = TEST_APP_ID,
    .name = TEST_APP_NAME,
    .icon = NULL,
    .open = test_view_open,
    .start_backend = NULL,
    .stop_backend = NULL,
    .event_handler = NULL,
};

void test_app_register(void)
{
    app_manager_register_app(&s_test_app);
}
