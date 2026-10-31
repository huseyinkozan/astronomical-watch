idf_component_register(SRCS "BatteryLog.cpp" "Peripherals.cpp" "DeepSleep.cpp" "Axp2101.cpp" "Rtc.cpp" "Display.cpp" "main.cpp"
                       INCLUDE_DIRS ""
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_usb_serial_jtag
                                     esp_lcd esp_timer)

# ESP-IDF defaults to the newest C++ standard; this project is written in C++17.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
