#pragma once

#include <cstdint>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "sdkconfig.h"

// Direct access to the CO5300 panel for the always-on face, without the BSP: the BSP resets the
// panel on every start, which would blank the image that the panel keeps in its own memory
// while the ESP32 deep-sleeps. Coordinates and sizes must be even (the panel's 2-pixel rounding).
class Panel
{
public:
    static const int WIDTH = 410;
    static const int HEIGHT = 502;

    // Releases the pin holds and opens the QSPI bus and the panel without a reset or init
    // commands, so the image stays and only changed areas need drawing.
    bool open();

    // White at full drive; the brightness setting dims it.
    void fill( int x, int y, int width, int height, bool white );

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot of the always-on face (tools/screenshot.py): instead of drawing,
    // fill() writes "SHOT RECT <x> <y> <width> <height> <0|1>" to the console. No open() needed.
    void record();
#endif

    // Brightness (DBV, command 0x51). It also waits until all queued drawing is sent.
    void setBrightness( int percent );

    // Idle mode on (39h): 8 colours from the MSB of R, G and B; a reset turns it off.
    void setIdleMode();

    // Holds CS high and the clock and data lines low through deep sleep, so the panel sees no
    // command while the ESP32 pads would otherwise float.
    static void holdPins();

    // Releases those holds; the BSP or open() then takes the pins over.
    static void releasePins();

private:
#if CONFIG_WATCH_DEV_LOGGING
    bool m_recording = false;
#endif
    esp_lcd_panel_io_handle_t m_io = nullptr;
    esp_lcd_panel_handle_t m_panel = nullptr;
};
