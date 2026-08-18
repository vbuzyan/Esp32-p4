idf_component_register(
    SRCS "crowpanel_p4_bringup.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_hw_support esp_timer
             esp_lcd esp_lcd_ek79007 esp_lcd_touch_gt911 lvgl
)
