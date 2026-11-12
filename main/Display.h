#pragma once

#include <atomic>
#include <cstdint>

#include "Rtc.h"
#include "Settings.h"
#include "lvgl.h"
#include "sdkconfig.h"

// AMOLED panel and touch through the BSP and LVGL, in active mode. The panel's power enable
// DSI_PWR_EN is pulled up from AXP2101 ALDO2, so the caller switches ALDO2 on first
// (docs/hardware.md). The pages are side by side; a horizontal swipe moves between them: watch
// face, battery, settings, setup portal (CONFIG_WATCH_SETUP_PORTAL), about.
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

    // What the setup page shows about the portal.
    struct Setup
    {
        bool running;
        const char *ssid;
        const char *password;
        const char *address;
        int secondsLeft;
    };

    // Taps on the setup page, handled by the main task (Wi-Fi does not start in the LVGL task).
    enum Request
    {
        TogglePortal = 1,
        NewPassword = 2
    };

    // Starts LVGL, the panel and touch (BSP), sets the brightness and builds the pages. A tap on
    // a settings row steps `settings` to the next value, in the LVGL task.
    bool start( Settings &settings );

    // Local time, its UTC offset in minutes, and the battery state.
    void update( const Rtc::DateTime &localTime, int utcOffsetMinutes, const Battery &battery );

    // After the setup portal changed the settings: shows them and applies the brightness.
    void refreshSettings();

    void showSetup( const Setup &setup );

    // The Request bits tapped since the last call.
    int takeRequests();

    // Milliseconds since the last touch (LVGL inactivity time); the previous value while LVGL
    // holds the display lock.
    uint32_t inactiveMs();

    // Before deep sleep: takes the display lock and keeps it, so LVGL no longer uses the bus.
    void stop();

#if CONFIG_WATCH_DEV_LOGGING
    // Development screenshot (tools/screenshot.py): shows page `page`, redraws the whole screen
    // and writes each pixel row to the console as "SHOT <y> <x> <colour runs>". Counts as activity.
    void capture( int page );
#endif

private:
    static void onSettingClicked( lv_event_t *event );
    static void onSetupClicked( lv_event_t *event );
    lv_obj_t *createRow( lv_obj_t *parent, const char *name, lv_event_cb_t onClicked );
    void buildSetupPage( lv_obj_t *page );
#if CONFIG_WATCH_DEV_LOGGING
    static void onFlushStart( lv_event_t *event );
    static void onRefreshReady( lv_event_t *event );
    bool m_capturing = false;
#endif
    lv_obj_t *m_pages = nullptr;
    void showSetting( int item );

    Settings *m_settings = nullptr;
    // Watch face page.
    lv_obj_t *m_timeLabel = nullptr;
    lv_obj_t *m_dateLabel = nullptr;
    lv_obj_t *m_batteryLabel = nullptr;
    // Battery page.
    lv_obj_t *m_percentLabel = nullptr;
    lv_obj_t *m_voltageLabel = nullptr;
    lv_obj_t *m_chargeLabel = nullptr;
    // Settings page: one row per Settings::Item on the page.
    lv_obj_t *m_settingRows[Settings::PageItemCount] = {};
    lv_obj_t *m_settingValues[Settings::PageItemCount] = {};
    // Setup page.
    lv_obj_t *m_portalRow = nullptr;
    lv_obj_t *m_portalValue = nullptr;
    lv_obj_t *m_passwordRow = nullptr;
    lv_obj_t *m_networkLabel = nullptr;
    lv_obj_t *m_passwordLabel = nullptr;
    lv_obj_t *m_setupInfo = nullptr;
    std::atomic<int> m_requests { 0 };
    uint32_t m_inactiveMs = 0;
};
