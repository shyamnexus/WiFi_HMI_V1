#include "hmi_screen.h"
#include "bsp.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "wireless_data.h"

#define HMI_ROWS_PER_PAGE 5
#define HMI_REFRESH_MS    1000
#define HMI_W             800
#define HMI_H             480

static const char *TAG = "hmi";

typedef struct {
    lv_obj_t *table;
    lv_obj_t *page_label;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    uint32_t page;
} hmi_ctx_t;

static hmi_ctx_t s_hmi;

static void table_set_if_changed(lv_obj_t *table, uint32_t row, uint32_t col, const char *text)
{
    const char *old = lv_table_get_cell_value(table, row, col);
    if (!old || strcmp(old, text) != 0) {
        lv_table_set_cell_value(table, row, col, text);
    }
}

static uint32_t calc_total_pages(uint32_t count)
{
    if (count == 0) {
        return 1;
    }
    return (count + HMI_ROWS_PER_PAGE - 1) / HMI_ROWS_PER_PAGE;
}

static void draw_page(void)
{
    char text[64];
    wireless_device_t node;

    uint32_t total = wireless_data_count();
    uint32_t total_pages = calc_total_pages(total);
    if (s_hmi.page >= total_pages) {
        s_hmi.page = total_pages - 1;
    }

    table_set_if_changed(s_hmi.table, 0, 0, "Device");
    table_set_if_changed(s_hmi.table, 0, 1, "Data1");
    table_set_if_changed(s_hmi.table, 0, 2, "Data2");
    table_set_if_changed(s_hmi.table, 0, 3, "Data3");
    table_set_if_changed(s_hmi.table, 0, 4, "Data4");

    for (uint32_t row = 1; row <= HMI_ROWS_PER_PAGE; row++) {
        uint32_t index = (s_hmi.page * HMI_ROWS_PER_PAGE) + (row - 1);
        if (wireless_data_get_by_index(index, &node)) {
            table_set_if_changed(s_hmi.table, row, 0, node.device_name);
            table_set_if_changed(s_hmi.table, row, 1, node.data1);
            table_set_if_changed(s_hmi.table, row, 2, node.data2);
            table_set_if_changed(s_hmi.table, row, 3, node.data3);
            table_set_if_changed(s_hmi.table, row, 4, node.data4);
        } else {
            table_set_if_changed(s_hmi.table, row, 0, "");
            table_set_if_changed(s_hmi.table, row, 1, "");
            table_set_if_changed(s_hmi.table, row, 2, "");
            table_set_if_changed(s_hmi.table, row, 3, "");
            table_set_if_changed(s_hmi.table, row, 4, "");
        }
    }

    snprintf(text, sizeof(text), "Page %lu/%lu", (unsigned long)(s_hmi.page + 1), (unsigned long)total_pages);
    lv_label_set_text(s_hmi.page_label, text);

    if (total_pages <= 1) {
        lv_obj_add_state(s_hmi.btn_prev, LV_STATE_DISABLED);
        lv_obj_add_state(s_hmi.btn_next, LV_STATE_DISABLED);
    } else {
        if (s_hmi.page == 0) {
            lv_obj_add_state(s_hmi.btn_prev, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_hmi.btn_prev, LV_STATE_DISABLED);
        }

        if (s_hmi.page >= (total_pages - 1)) {
            lv_obj_add_state(s_hmi.btn_next, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_hmi.btn_next, LV_STATE_DISABLED);
        }
    }
}

static void on_prev_clicked(lv_event_t *e)
{
    (void)e;
    if (s_hmi.page > 0) {
        s_hmi.page--;
        draw_page();
    }
}

static void on_next_clicked(lv_event_t *e)
{
    (void)e;
    uint32_t total_pages = calc_total_pages(wireless_data_count());
    if (s_hmi.page + 1 < total_pages) {
        s_hmi.page++;
        draw_page();
    }
}

static void hmi_task(void *arg)
{
    (void)arg;

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Wireless Generic Data");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0FBFC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_hmi.table = lv_table_create(scr);
    lv_obj_set_size(s_hmi.table, 770, 360);
    lv_obj_align(s_hmi.table, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(s_hmi.table, lv_color_hex(0x1C2541), 0);
    lv_obj_set_style_text_color(s_hmi.table, lv_color_hex(0xE0FBFC), 0);
    // Use a larger font for readability with generic string payload fields.
    lv_obj_set_style_text_font(s_hmi.table, &lv_font_montserrat_28, LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_hmi.table, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_hmi.table, 1, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(s_hmi.table, 1, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(s_hmi.table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(s_hmi.table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(s_hmi.table, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_hmi.table, 2, LV_PART_MAIN);
    lv_table_set_column_count(s_hmi.table, 5);
    lv_table_set_row_count(s_hmi.table, HMI_ROWS_PER_PAGE + 1);

    lv_table_set_column_width(s_hmi.table, 0, 160);
    lv_table_set_column_width(s_hmi.table, 1, 152);
    lv_table_set_column_width(s_hmi.table, 2, 152);
    lv_table_set_column_width(s_hmi.table, 3, 152);
    lv_table_set_column_width(s_hmi.table, 4, 152);

    s_hmi.page_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_hmi.page_label, lv_color_hex(0xE0FBFC), 0);
    lv_obj_set_style_text_font(s_hmi.page_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_hmi.page_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    s_hmi.btn_prev = lv_button_create(scr);
    lv_obj_set_size(s_hmi.btn_prev, 110, 42);
    lv_obj_align(s_hmi.btn_prev, LV_ALIGN_BOTTOM_LEFT, 16, -8);
    lv_obj_add_event_cb(s_hmi.btn_prev, on_prev_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *prev_label = lv_label_create(s_hmi.btn_prev);
    lv_label_set_text(prev_label, "Prev");
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_28, 0);
    lv_obj_center(prev_label);

    s_hmi.btn_next = lv_button_create(scr);
    lv_obj_set_size(s_hmi.btn_next, 110, 42);
    lv_obj_align(s_hmi.btn_next, LV_ALIGN_BOTTOM_RIGHT, -16, -8);
    lv_obj_add_event_cb(s_hmi.btn_next, on_next_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_label = lv_label_create(s_hmi.btn_next);
    lv_label_set_text(next_label, "Next");
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_28, 0);
    lv_obj_center(next_label);

    wireless_data_init();
    draw_page();
    lvgl_port_unlock();
    } /* end lvgl_port_lock */

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(HMI_REFRESH_MS));
        if (wireless_data_mock_enabled()) {
            wireless_data_mock_tick();
        }
        if (lvgl_port_lock(0)) {
            draw_page();
            lvgl_port_unlock();
        }
    }
}

bool hmi_screen_start(void)
{
    /* 1. Hardware: I2C bus, CH422G, LCD reset, RGB panel, GT911 touch, backlight */
    if (bsp_init() != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed");
        return false;
    }
    const bsp_handles_t *bsp = bsp_get_handles();

    /* 2. LVGL port — creates the LVGL tick timer and task on Core 1 */
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 4;
    port_cfg.task_stack      = 8192;
    port_cfg.task_affinity   = 1;
    port_cfg.timer_period_ms = 5;
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed");
        return false;
    }

    /* 3. Register RGB display (double-buffered PSRAM fbs, bounce-buffer sync) */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = NULL,
        .panel_handle = bsp->lcd_panel,
        .hres         = HMI_W,
        .vres         = HMI_H,
        .buffer_size  = HMI_W * HMI_H,
        .double_buffer = true,
        .monochrome   = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .direct_mode = true, .full_refresh = true },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = { .bb_mode = true, .avoid_tearing = true },
    };
    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return false;
    }

    /* 4. Register GT911 touch */
    const lvgl_port_touch_cfg_t touch_cfg = { .disp = disp, .handle = bsp->touch };
    if (!lvgl_port_add_touch(&touch_cfg)) {
        ESP_LOGW(TAG, "Touch registration failed — continuing without touch");
    }

    /* 5. Start HMI task (builds UI + runs update loop, both use lvgl_port_lock) */
    BaseType_t ok = xTaskCreatePinnedToCore(
        hmi_task, "hmi_task", 8192, NULL, 5, NULL, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HMI task");
        return false;
    }

    ESP_LOGI(TAG, "HMI screen started");
    return true;
}

