#include "launcherView.h"

#include "app_common_config.h"

extern const lv_image_dsc_t quick_panel_background4;

#define LAUNCHER_VIEW_COLS 3
#define LAUNCHER_VIEW_ROWS 3
#define LAUNCHER_VIEW_BTN_W 100
#define LAUNCHER_VIEW_BTN_H 100
#define LAUNCHER_VIEW_CELL_H 130
#define LAUNCHER_VIEW_GRID_H (LAUNCHER_VIEW_ROWS * LAUNCHER_VIEW_CELL_H)
#define LAUNCHER_VIEW_GRID_PAD_X 16
#define LAUNCHER_VIEW_GRID_COL_GAP 8
#define LAUNCHER_VIEW_GRID_ROW_GAP 12
#define LAUNCHER_VIEW_APP_NAME_FONT APP_COMMON_BODY_FONT
#define LAUNCHER_VIEW_APP_NAME_TOP_PAD 3
#define LAUNCHER_VIEW_APP_NAME_COLOR 0x000000

static void launcher_view_bind(lv_obj_t *obj,
                               const app_t *app,
                               launcher_view_bind_app_event_cb_t bind_app_event)
{
    if (bind_app_event) {
        bind_app_event(obj, app);
    }
}

static void launcher_view_create_background(lv_obj_t *parent)
{
    lv_obj_t *bg = lv_img_create(parent);
    lv_img_set_src(bg, &quick_panel_background4);
    lv_obj_center(bg);
    lv_obj_move_background(bg);
}

static void launcher_view_create_app_cell(lv_obj_t *grid,
                                          const app_t *app,
                                          int row,
                                          int col,
                                          launcher_view_bind_app_event_cb_t bind_app_event)
{
    lv_obj_t *cell = lv_obj_create(grid);
    lv_obj_remove_style_all(cell);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_grid_cell(cell,
                         LV_GRID_ALIGN_STRETCH, col, 1,
                         LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    launcher_view_bind(cell, app, bind_app_event);

    lv_obj_t *btn = lv_btn_create(cell);
    lv_obj_set_size(btn, LAUNCHER_VIEW_BTN_W, LAUNCHER_VIEW_BTN_H);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(btn, 25, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFDFDFD), 0);
    lv_obj_set_style_clip_corner(btn, true, 0);
    launcher_view_bind(btn, app, bind_app_event);

    if (app && app->icon != NULL) {
        lv_obj_t *icon = lv_img_create(btn);
        lv_img_set_src(icon, app->icon);
        lv_obj_center(icon);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        launcher_view_bind(icon, app, bind_app_event);
    } else {
        lv_obj_t *placeholder = lv_label_create(btn);
        const char *title = app && app->name ? app->name : (app ? app->id : NULL);
        char initial[2] = { title && title[0] ? title[0] : '?', '\0' };
        lv_label_set_text(placeholder, initial);
        lv_obj_center(placeholder);
        launcher_view_bind(placeholder, app, bind_app_event);
    }

    lv_obj_t *lbl = lv_label_create(cell);
    lv_label_set_text(lbl, app && app->name ? app->name : (app ? app->id : ""));
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl, LAUNCHER_VIEW_APP_NAME_FONT, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(LAUNCHER_VIEW_APP_NAME_COLOR), 0);
    lv_obj_set_style_pad_top(lbl, LAUNCHER_VIEW_APP_NAME_TOP_PAD, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    launcher_view_bind(lbl, app, bind_app_event);
}

lv_obj_t *launcher_view_create(const app_t *const *apps,
                               int app_count,
                               launcher_view_bind_app_event_cb_t bind_app_event)
{
    static lv_coord_t col_dsc[] = {
        LV_GRID_FR(1),
        LV_GRID_FR(1),
        LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST,
    };
    static lv_coord_t row_dsc[] = {
        LAUNCHER_VIEW_CELL_H,
        LAUNCHER_VIEW_CELL_H,
        LAUNCHER_VIEW_CELL_H,
        LV_GRID_TEMPLATE_LAST,
    };

    lv_obj_t *scr = lv_obj_create(NULL);
    if (!scr) {
        return NULL;
    }

    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    launcher_view_create_background(scr);

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(grid, LV_PCT(100), LAUNCHER_VIEW_GRID_H);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_left(grid, LAUNCHER_VIEW_GRID_PAD_X, 0);
    lv_obj_set_style_pad_right(grid, LAUNCHER_VIEW_GRID_PAD_X, 0);
    lv_obj_set_style_pad_column(grid, LAUNCHER_VIEW_GRID_COL_GAP, 0);
    lv_obj_set_style_pad_row(grid, LAUNCHER_VIEW_GRID_ROW_GAP, 0);
    lv_obj_center(grid);

    int max_items = LAUNCHER_VIEW_COLS * LAUNCHER_VIEW_ROWS;
    for (int i = 0; apps && i < app_count && i < max_items; ++i) {
        launcher_view_create_app_cell(grid,
                                      apps[i],
                                      i / LAUNCHER_VIEW_COLS,
                                      i % LAUNCHER_VIEW_COLS,
                                      bind_app_event);
    }

    return scr;
}