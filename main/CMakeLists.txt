set(srcs "BatteryLog.cpp" "Peripherals.cpp" "DeepSleep.cpp" "Axp2101.cpp" "Rtc.cpp" "Display.cpp" "Panel.cpp"
         "AlwaysOn.cpp" "AodGlyphs.c" "AodPage.cpp" "Motion.cpp" "Settings.cpp" "Astronomy.cpp" "Hijri.cpp" "Sky.cpp" "SkyPages.cpp" "WatchFace.cpp" "main.cpp"
         "fonts/jetbrains_mono_26.c" "fonts/jetbrains_mono_48.c"
         "fonts/jetbrains_mono_light_110.c" "fonts/jetbrains_mono_light_90.c" "fonts/jetbrains_mono_thin_170.c")
# The setup portal's components are always listed: ESP-IDF collects the requirements before it
# reads the configuration. Without WATCH_SETUP_PORTAL no source uses them.
set(requires esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_usb_serial_jtag
             esp_lcd esp_timer nvs_flash espressif__esp_lcd_touch esp_app_format
             esp_wifi esp_netif esp_event esp_http_server lwip)
set(embedded "")

# Setup portal (Kconfig WATCH_SETUP_PORTAL).
if(CONFIG_WATCH_SETUP_PORTAL)
    list(APPEND srcs "Portal.cpp" "dns_server.c")
    list(APPEND embedded "portal/index.html" "portal/cities.js")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ""
                       PRIV_REQUIRES ${requires}
                       EMBED_TXTFILES ${embedded})

# ESP-IDF defaults to the newest C++ standard; this project is written in C++17.
# (dns_server.c is C.)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
