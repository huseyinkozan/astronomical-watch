idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer waveshare__esp32_s3_touch_amoled_2_06
                       INCLUDE_DIRS "")
