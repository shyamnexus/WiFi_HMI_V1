idf_component_register(
    SRCS
        "bsp_io_expander.c"
        "bsp_lcd.c"
        "bsp_touch.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_lcd
        esp_lcd_touch
        esp_lcd_touch_gt911
        log
)
