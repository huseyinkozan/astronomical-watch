set(srcs "BatteryLog.cpp" "EventLog.cpp" "Peripherals.cpp" "DeepSleep.cpp" "Axp2101.cpp" "Rtc.cpp" "Display.cpp" "CornerView.cpp" "PageMarks.cpp" "StackPage.cpp" "ClockPages.cpp" "Timers.cpp" "WorldClock.cpp" "DayCounters.cpp" "Zones.cpp" "Dates.cpp" "PartOrder.cpp" "Language.cpp" "Corners.cpp" "Panel.cpp"
         "AlwaysOn.cpp" "AodGlyphs.c" "CornerTable.c" "Motion.cpp" "Settings.cpp" "Astronomy.cpp" "Hijri.cpp" "Sky.cpp" "Planets.cpp" "PlanetPages.cpp" "SkyPages.cpp" "TablePart.cpp" "Styles.cpp" "TableText.cpp" "FaceConfig.cpp" "FaceStore.cpp" "FacePage.cpp" "FaceText.cpp" "FaceTable.cpp" "SevenSegment.cpp" "AnalogClock.cpp" "MilkyWay.cpp" "Sound.cpp" "Alarms.cpp" "main.cpp"
         "fonts/jetbrains_mono_26.c" "fonts/jetbrains_mono_48.c"
         "fonts/jetbrains_mono_light_110.c" "fonts/jetbrains_mono_light_90.c" "fonts/jetbrains_mono_light_64.c")
# The setup portal's components are always listed: ESP-IDF collects the requirements before it
# reads the configuration. Without WATCH_SETUP_PORTAL no source uses them.
set(requires esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_usb_serial_jtag
             esp_lcd esp_timer nvs_flash espressif__esp_lcd_touch esp_driver_i2s espressif__esp_codec_dev esp_app_format
             esp_wifi esp_netif esp_event esp_http_server lwip espcoredump app_update spi_flash)

# Setup portal (Kconfig WATCH_SETUP_PORTAL).
if(CONFIG_WATCH_SETUP_PORTAL)
    list(APPEND srcs "Portal.cpp" "dns_server.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ""
                       PRIV_REQUIRES ${requires})

# The portal's pages go into the image gzipped (Portal.cpp sends them with Content-Encoding:
# gzip): a third of the bytes over Wi-Fi and in flash. A custom command makes each .gz in the
# build directory whenever its source changes; -n leaves out the name and time, so the same
# source always gives the same bytes. The custom target makes the component wait for them.
if(CONFIG_WATCH_SETUP_PORTAL)
    set(portal_gz "")
    foreach(name "index.html" "cities.js" "help.html" "update.html")
        set(gz "${CMAKE_CURRENT_BINARY_DIR}/${name}.gz")
        add_custom_command(OUTPUT "${gz}"
                           COMMAND gzip -9 -n -c "${COMPONENT_DIR}/portal/${name}" > "${gz}"
                           DEPENDS "${COMPONENT_DIR}/portal/${name}"
                           VERBATIM)
        list(APPEND portal_gz "${gz}")
    endforeach()
    add_custom_target(portal_gz DEPENDS ${portal_gz})
    add_dependencies(${COMPONENT_LIB} portal_gz)
    foreach(gz ${portal_gz})
        target_add_binary_data(${COMPONENT_LIB} "${gz}" BINARY)
    endforeach()
endif()

# ESP-IDF defaults to the newest C++ standard; this project is written in C++17.
# (dns_server.c is C.)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
