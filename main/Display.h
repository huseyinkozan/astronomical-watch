#pragma once

#include <cstdint>

#include "Rtc.h"
#include "lvgl.h"

// AMOLED panel through the BSP and LVGL. The panel's power enable DSI_PWR_EN is pulled up from
// AXP2101 ALDO2, so the caller switches ALDO2 on first (docs/hardware.md).
class Display
{
public:
    // Starts LVGL, the panel and touch (BSP), sets the brightness and builds the status screen.
    bool start( int brightnessPercent );

    // Time, date and battery state; the time is UTC until the settings provide an offset.
    void showStatus( const Rtc::DateTime &time, uint16_t millivolts, uint8_t percent, bool vbusGood );

private:
    lv_obj_t *m_timeLabel = nullptr;
    lv_obj_t *m_dateLabel = nullptr;
    lv_obj_t *m_batteryLabel = nullptr;
};
