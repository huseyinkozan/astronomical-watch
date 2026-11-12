#pragma once

#include <cstdint>

#include "Rtc.h"
#include "lvgl.h"

// AMOLED panel and touch through the BSP and LVGL, in active mode. The panel's power enable
// DSI_PWR_EN is pulled up from AXP2101 ALDO2, so the caller switches ALDO2 on first
// (docs/hardware.md). The pages are side by side; a horizontal swipe moves between them.
class Display
{
public:
    struct Battery
    {
        uint16_t millivolts;
        uint8_t percent;
        bool vbusGood;
        const char *chargeState;
    };

    // Starts LVGL, the panel and touch (BSP), sets the brightness and builds the pages.
    bool start( int brightnessPercent );

    // Time, date and battery state; the time is UTC until the settings provide an offset.
    void update( const Rtc::DateTime &time, const Battery &battery );

    // Milliseconds since the last touch (LVGL inactivity time); the previous value while LVGL
    // holds the display lock.
    uint32_t inactiveMs();

    // Before deep sleep: takes the display lock and keeps it, so LVGL no longer uses the bus.
    void stop();

private:
    // Watch face page.
    lv_obj_t *m_timeLabel = nullptr;
    lv_obj_t *m_dateLabel = nullptr;
    lv_obj_t *m_batteryLabel = nullptr;
    // Battery page.
    lv_obj_t *m_percentLabel = nullptr;
    lv_obj_t *m_voltageLabel = nullptr;
    lv_obj_t *m_chargeLabel = nullptr;
    uint32_t m_inactiveMs = 0;
};
