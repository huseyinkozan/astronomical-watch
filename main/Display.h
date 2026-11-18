#pragma once

#include <atomic>
#include <cstdint>

#include "Rtc.h"
#include "Settings.h"
#include "ClockPages.h"
#include "CornerView.h"
#include "FaceConfig.h"
#include "FacePage.h"
#include "FaceTable.h"
#include "libs/qrcode/qrcodegen.h"
#include "PageMarks.h"
#include "StackPage.h"
#include "SkyPages.h"
#include "lvgl.h"
#include "sdkconfig.h"

// AMOLED panel and touch through the BSP and LVGL, in active mode. The panel's power enable
// DSI_PWR_EN is pulled up from AXP2101 ALDO2, so the caller switches ALDO2 on first
// (docs/hardware.md). The pages are side by side; a horizontal swipe moves between them:
// always-on preview, watch face, information, settings. The information page holds sun,
// twilight, moon, sun compass, battery and about one over the other, the settings page the
// settings and the setup portal (CONFIG_WATCH_SETUP_PORTAL); a vertical swipe moves between them.
class Display
{
public:
    // The parts of the information page, top to bottom.
    enum InfoPart
    {
        InfoSun,
        InfoTwilight,
        InfoPrayer,
        InfoMoon,
        InfoCompass,
        InfoTime,
        InfoBattery,
        InfoAbout,
        InfoCount
    };

    struct Battery
    {
        uint16_t millivolts;
        uint8_t percent;
        bool vbusGood;
        const char *chargeState;
        int unpluggedMinutes;  // since USB power went away, -1 on USB or when not known
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
        NewPassword = 2,
        // The language changed: the main task saves the settings and restarts, so that every
        // page is built in it.
        Restart = 4
    };

    // Starts LVGL, the panel and touch (BSP), sets the brightness and builds the pages. A tap on
    // a settings row steps `settings` to the next value, in the LVGL task.
    bool start( Settings &settings );

    // The face pages (FaceStore), before start() and after the setup portal saved them.
    void setPages( const FaceConfig::Pages &pages );

    // Moves to the watch face, as a swipe would (a face page sent from the portal).
    void showFaceTile();

    // Local time, its UTC offset in minutes, and the battery state; `timeValid` false: the clock
    // was never set or could not be read (the face's slots stay empty).
    void update( const Rtc::DateTime &localTime, int utcOffsetMinutes, const Battery &battery, bool timeValid );

    // After the setup portal changed the settings: shows them and applies the brightness.
    void refreshSettings();

    void showSetup( const Setup &setup );

    // The astronomy pages: values of the local day and of this moment; the face's events,
    // counters and cities. Also updates the face with the time of the last update(); `located`
    // false: no location set yet (the face's sky slots stay empty).
    void showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes, const FaceTable &table,
                  bool located );

    // The corner gauges, on every page.
    void showCorners( const Corners::Gauge gauges[Corners::CornerCount] );

    // The clock page's Hijri calendar: a month it asked for (ClockPages::takeHijriRequest), and
    // the month computed by the main task.
    bool takeHijriRequest( ClockPages::HijriRequest &request );

    // What the setup portal set and the pages do not show (main: place, coordinates, time zone,
    // UTC offset, next clock change, Hijri method, when the clock was set); the place also goes
    // to the location-dependent pages.
    static const int SetupInfoRows = 7;
    void showSetupInfo( const char *const values[SetupInfoRows] );

    // The world clock's cities (a copy), at the start and after the setup portal.
    void setCities( const WorldClock &worldClock );
    void setCounters( const DayCounters &counters );
    void showHijriMonth( const Hijri::Month &month, int today, int method );

    // The Request bits tapped since the last call.
    int takeRequests();

    // Milliseconds since the last touch (LVGL inactivity time); the previous value while LVGL
    // holds the display lock.
    uint32_t inactiveMs();

    // For the event log: touch presses since start(), and whether touch input started.
    int touches() const;
    bool touchStarted() const;

    // Before deep sleep: takes the display lock and keeps it, so LVGL no longer uses the bus.
    void stop();

#if CONFIG_WATCH_DEV_LOGGING
    // Shows face page `page` without changing the setting (tools/screenshot.py).
    void showFaceStyle( int page );
    // Puts `item` into `slot` of face page `page` (not saved) and shows the page.
    void setFaceSlot( int page, int slot, int item );
    // Shows the brightness control as a long press does (a screenshot of it).
    void showLevels();

    // The fixed scene of the screenshots: sample states on the clock page.
    void setScene( bool scene );

    // Presses a clock page button by name (ClockPages::press).
    bool pressClockButton( const char *name );

    // Swipes to page `page` with the page animation, as a finger would (speed measurements).
    void swipeTo( int page );

    // Development screenshot (tools/screenshot.py): shows page `page`, redraws the whole screen
    // and writes each pixel row to the console as "SHOT <y> <x> <colour runs>". Counts as activity.
    void capture( int page );
#endif

private:
    static void onSettingClicked( lv_event_t *event );
    static void onGesture( lv_event_t *event );
    // The always-on page shows the corners as the always-on face draws them.
    static void onPageChanged( lv_event_t *event );
    static void onPagesScrolled( lv_event_t *event );
    // A clock page part that takes sideways swipes itself (ClockPages::swipesSideways) stops the
    // page swipe while it is shown.
    static void onClockPartChanged( lv_event_t *event );
    void updatePageSwipe();
    // Pages built on demand (the watch face and its neighbours at the start).
    void ensureBuilt( int page );
    void buildInfoPage();
    void buildSettingsPage();
    void showBattery();
    void applySetup();
    // The orders and hidden parts set in the portal (PartOrder), on the stacked pages built so far.
    void applyOrders();
    // The face page of Settings::FaceStyle, one of those shown (FaceConfig::shown).
    void showFacePage( int direction );
    lv_obj_t *m_tiles[4] = {};
    bool m_built[4] = {};
    Setup m_setup = {};
    char m_setupTexts[SetupInfoRows][40] = {};
    // A long press on the watch face, then a drag: vertically the active brightness,
    // sideways the always-on brightness of the moment (day or night), the panel at that level
    // while dragging. The first movement decides which. Release saves the value (with the other
    // settings when active mode ends).
    static void onBrightnessDrag( lv_event_t *event );
    void buildBrightnessControl();
    void startAdjust( int x, int y );
    static lv_obj_t *createLevelBox( lv_obj_t *layer, int y, lv_obj_t *&value, lv_obj_t *&title );
    static lv_obj_t *createLevelBar( lv_obj_t *layer, int x, int y, int width, int height );

    void showBrightness( int percent );
    static void onSetupClicked( lv_event_t *event );
    lv_obj_t *createRow( lv_obj_t *parent, const char *name, lv_event_cb_t onClicked );
    void buildSetupPage( lv_obj_t *page );
    void buildSetupCodePage( lv_obj_t *page );
    static void onFlushStart( lv_event_t *event );
#if CONFIG_WATCH_DEV_LOGGING
    static void onRefreshReady( lv_event_t *event );
    bool m_capturing = false;
#endif
    lv_obj_t *m_pages = nullptr;
    SkyPages m_sky;
    ClockPages m_clock;
    CornerView m_corners;
    bool m_night = false;  // the day gauge shows the night: the always-on night level applies
    // Brightness control.
    lv_obj_t *m_brightnessShade = nullptr;
    lv_obj_t *m_brightnessBox = nullptr;
    lv_obj_t *m_brightnessValue = nullptr;
    lv_obj_t *m_brightnessTitle = nullptr;
    lv_obj_t *m_brightnessBar = nullptr;
    // The always-on level, shown beside the face's until the drag's direction is chosen.
    lv_obj_t *m_aodBox = nullptr;
    lv_obj_t *m_aodValue = nullptr;
    lv_obj_t *m_aodTitle = nullptr;
    lv_obj_t *m_aodBar = nullptr;
    bool m_adjusting = false;
    Settings::Item m_adjustItem = Settings::Brightness;
    int m_adjustStart = 0;
    int m_adjustPercent = 0;
    int m_adjustX = 0;
    int m_adjustY = 0;
    bool m_adjustAxisChosen = false;
    lv_obj_t *m_faceTile = nullptr;
    lv_obj_t *m_clockTile = nullptr;
    StackPage m_info;
    StackPage m_settingsPage;
    PageMarks m_faceMarks;
    PageMarks m_pageMarks;  // the four pages, shown while they are swiped
    Rtc::DateTime m_localTime = {};
    int m_moonPercent = 0;
    bool m_waxing = true;
    void showSetting( int item );

    Settings *m_settings = nullptr;
    // Watch face page.
    bool m_started = false;
    FacePage m_face;
    FaceConfig::Pages m_facePages = {};
    // The last values shown, for a redraw after a face style change.
    Sky::Day m_day = {};
    Sky::Now m_now = {};
    int m_offsetMinutes = 0;
    bool m_haveSky = false;
    FaceTable m_faceTable = {};
    Battery m_battery = {};
    bool m_timeValid = false;
    // Battery page.
    lv_obj_t *m_percentLabel = nullptr;
    lv_obj_t *m_voltageLabel = nullptr;
    lv_obj_t *m_chargeLabel = nullptr;
    lv_obj_t *m_unpluggedLabel = nullptr;
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
    // The network as a Wi-Fi QR code on its own part while the portal runs, a hint otherwise.
    // One object draws the modules (qrcodegen's bit table, up to version 5); LVGL's qrcode widget
    // kept a 13 KB bitmap, which the portal's Wi-Fi needs.
    static void onDrawJoinCode( lv_event_t *event );
    static void onSettingsPartChanged( lv_event_t *event );
    // The panel at s_joinCodeBrightness while the QR code part shows, else at the setting.
    void updateJoinCodeBrightness();
    bool m_joinCodeDimmed = false;
    lv_obj_t *m_joinCode = nullptr;
    lv_obj_t *m_joinHint = nullptr;
    char m_joinPayload[80] = {};
    uint8_t m_joinModules[qrcodegen_BUFFER_LEN_FOR_VERSION( 5 )] = {};
    lv_obj_t *m_setupValues[SetupInfoRows] = {};
    std::atomic<int> m_requests { 0 };
    std::atomic<int> m_touches { 0 };
    bool m_touchStarted = false;
    static void onTouchCounted( lv_event_t *event );
    uint32_t m_inactiveMs = 0;
};
