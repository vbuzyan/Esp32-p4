/*
 * CrowPanel Advanced 9" ESP32-P4 HMI (DHE04209D, board rev V1.0) — LVGL UI demo.
 *
 * NOT GhostESP yet. Proves the front-end stack GhostESP needs on the P4:
 * MIPI-DSI EK79007 panel + GT911 touch + LVGL, rendering a live touch UI.
 *
 * This uses a small, direct LVGL v8 integration (NOT esp_lvgl_port) so we
 * control init ordering: the DPI DMA-completion callback is registered BEFORE
 * the LVGL task starts, so every flush reliably signals lv_disp_flush_ready.
 * (esp_lvgl_port 2.8.0 starts flushing before registering that callback on
 * IDF 6.0.2, which deadlocks — see docs/PORTING.md.)
 *
 * Hardware values are from Elecrow's V1.0 factory source. Built on ESP-IDF 6.0.2.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"

#include "lvgl.h"

static const char *TAG = "p4_ui";

/* --- Verified V1.0 hardware config (Elecrow factory source) --------------- */
#define LCD_H_RES                 1024
#define LCD_V_RES                 600
#define LCD_BITS_PER_PIXEL        16
#define MIPI_DSI_LANE_NUM         2
#define MIPI_DSI_LANE_BITRATE_MBPS 900
#define DPI_CLOCK_FREQ_MHZ        51

#define LDO_MIPI_PHY_CHAN         3
#define LDO_MIPI_PHY_MV           2500
#define LDO_PANEL_CHAN            4
#define LDO_PANEL_MV              3300

#define PIN_LCD_BLIGHT            31
#define PIN_TP_I2C_SCL            46
#define PIN_TP_I2C_SDA            45
#define PIN_TP_INT                42
#define PIN_TP_RST                40
#define TP_I2C_PORT               0

#define LVGL_BUF_LINES            120         /* partial buffer height */
#define LVGL_TICK_PERIOD_MS       2

static esp_lcd_panel_handle_t   s_panel = NULL;
static esp_lcd_touch_handle_t   s_touch = NULL;
static esp_ldo_channel_handle_t s_ldo_phy = NULL;
static esp_ldo_channel_handle_t s_ldo_panel = NULL;

static lv_disp_drv_t     s_disp_drv;
static lv_disp_draw_buf_t s_draw_buf;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

static lv_obj_t *s_coord_label = NULL;
static lv_obj_t *s_tap_label = NULL;
static int s_taps = 0;

/* ----------------------------------------------------------------------------
 * Power + backlight
 * -------------------------------------------------------------------------- */
static void power_ldos(void)
{
    esp_ldo_channel_config_t phy = { .chan_id = LDO_MIPI_PHY_CHAN, .voltage_mv = LDO_MIPI_PHY_MV };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&phy, &s_ldo_phy));
    esp_ldo_channel_config_t pan = { .chan_id = LDO_PANEL_CHAN, .voltage_mv = LDO_PANEL_MV };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&pan, &s_ldo_panel));
    ESP_LOGI(TAG, "LDOs up: PHY ch%d=%dmV panel ch%d=%dmV",
             LDO_MIPI_PHY_CHAN, LDO_MIPI_PHY_MV, LDO_PANEL_CHAN, LDO_PANEL_MV);
}

static void backlight_on(void)
{
    gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_LCD_BLIGHT, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(PIN_LCD_BLIGHT, 1);
}

/* ----------------------------------------------------------------------------
 * MIPI-DSI + EK79007 panel
 * -------------------------------------------------------------------------- */
static void display_init(void)
{
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = { .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io));

    esp_lcd_dpi_panel_config_t dpi_config = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = DPI_CLOCK_FREQ_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = LCD_H_RES, .v_size = LCD_V_RES,
            .hsync_back_porch = 160, .hsync_pulse_width = 70, .hsync_front_porch = 160,
            .vsync_back_porch = 23,  .vsync_pulse_width = 10, .vsync_front_porch = 12,
        },
    };
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = { .dsi_bus = dsi_bus, .dpi_config = &dpi_config },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(dbi_io, &panel_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    /* DMA2D so draw_bitmap fires on_color_trans_done (our flush-ready signal). */
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d(s_panel));
    ESP_LOGI(TAG, "EK79007 panel up (%dx%d)", LCD_H_RES, LCD_V_RES);
}

/* ----------------------------------------------------------------------------
 * GT911 touch
 * -------------------------------------------------------------------------- */
static void touch_init(void)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = TP_I2C_PORT,
        .sda_io_num = PIN_TP_I2C_SDA,
        .scl_io_num = PIN_TP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES, .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TP_RST, .int_gpio_num = PIN_TP_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    esp_err_t err = esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch);
    if (err != ESP_OK) {
        tp_io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch));
    }
    ESP_LOGI(TAG, "GT911 ready");
}

/* ----------------------------------------------------------------------------
 * Direct LVGL v8 integration
 * -------------------------------------------------------------------------- */
static bool on_dpi_color_done(esp_lcd_panel_handle_t panel,
                              esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(drv);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
    /* lv_disp_flush_ready() is called from on_dpi_color_done when DMA completes */
}

static void lvgl_tick_cb(void *arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); }

static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x = 0, y = 0, strength = 0;
    uint8_t cnt = 0;
    esp_lcd_touch_read_data(s_touch);
    bool pressed = esp_lcd_touch_get_coordinates(s_touch, &x, &y, &strength, &cnt, 1);
    if (pressed && cnt > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_task(void *arg)
{
    while (1) {
        uint32_t delay_ms = 10;
        if (xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            delay_ms = lv_timer_handler();
            xSemaphoreGive(s_lvgl_mutex);
        }
        if (delay_ms > 20) delay_ms = 20;
        if (delay_ms < 2)  delay_ms = 2;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ----------------------------------------------------------------------------
 * UI
 * -------------------------------------------------------------------------- */
static void on_button_clicked(lv_event_t *e)
{
    (void)e;
    s_taps++;
    lv_label_set_text_fmt(s_tap_label, "Taps: %d", s_taps);
}

static void on_screen_pressing(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_label_set_text_fmt(s_coord_label, "Touch: %d, %d", (int)p.x, (int)p.y);
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0d0d14), 0);
    lv_obj_add_event_cb(scr, on_screen_pressing, LV_EVENT_PRESSING, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "GhostESP  -  CrowPanel P4 (V1.0)");
    lv_obj_set_style_text_color(title, lv_color_hex(0x8b5cf6), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "ESP32-P4  |  1024x600 EK79007 MIPI-DSI  |  GT911 touch  |  LVGL");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x9aa0b5), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 260, 90);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x7c5cff), 0);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_add_event_cb(btn, on_button_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "TAP ME");
    lv_obj_center(btn_lbl);

    s_tap_label = lv_label_create(scr);
    lv_label_set_text(s_tap_label, "Taps: 0");
    lv_obj_set_style_text_color(s_tap_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(s_tap_label, LV_ALIGN_CENTER, 0, 70);

    s_coord_label = lv_label_create(scr);
    lv_label_set_text(s_coord_label, "Touch: -, -");
    lv_obj_set_style_text_color(s_coord_label, lv_color_hex(0x4ade80), 0);
    lv_obj_align(s_coord_label, LV_ALIGN_BOTTOM_MID, 0, -30);
}

/* -------------------------------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "CrowPanel 9\" ESP32-P4 (V1.0) LVGL UI demo");
    power_ldos();
    display_init();
    touch_init();

    /* --- LVGL core --- */
    lv_init();

    size_t buf_px = LCD_H_RES * LVGL_BUF_LINES;
    lv_color_t *buf1 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, buf_px);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LCD_H_RES;
    s_disp_drv.ver_res = LCD_V_RES;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&s_disp_drv);

    /* Register the DMA-done callback BEFORE any flush can happen. */
    esp_lcd_dpi_panel_event_callbacks_t cbs = { .on_color_trans_done = on_dpi_color_done };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(s_panel, &cbs, &s_disp_drv));

    /* LVGL tick via esp_timer. */
    const esp_timer_create_args_t tick_args = { .callback = lvgl_tick_cb, .name = "lv_tick" };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    /* Touch input device. */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    indev_drv.disp = disp;
    lv_indev_drv_register(&indev_drv);

    /* Build UI before the task starts (no locking needed yet). */
    build_ui();
    backlight_on();

    s_lvgl_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "UI up — tap the button / drag to see coordinates");
}
