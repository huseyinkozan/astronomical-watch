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
#include "PartOrder.h"
#include "StackPage.h"
#include "PlanetPages.h"
#include "SkyPages.h"
#include "TablePart.h"
#include "lvgl.h"
#include "sdkconfig.h"

// AMOLED panel and touch through the BSP and LVGL, in active mode. The panel's power enable
// DSI_PWR_EN is pulled up from AXP2101 ALDO2, so the caller switches ALDO2 on first
// (docs/hardware.md). Three tiles side by side, a horizontal swipe moves between them: the watch
// face (its face pages, FacePage), the launcher (a scrolled list of the stacked pages' parts) and
// the page last opened from it. The stacked pages (clock, information, planets, settings) hold
// their parts one over the other (StackPage); a vertical swipe moves between them, and on the
// watch face between the face pages. Only one stacked page is built at a time.
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

    // The BOOT key's short press: from the opened page back to the launcher, from the launcher to
    // the watch face; on the face it closes the brightness control.
    void back();

    // Local time, its UTC offset in minutes, and the battery state; `timeValid` false: the clock
    // was never set or could not be read (the face's slots stay empty).
    void update( const Rtc::DateTime &localTime, int utcOffsetMinutes, const Battery &battery, bool timeValid );

    // After the setup portal changed the settings: shows them and applies the brightness.
    void refreshSettings();

    void showSetup( const Setup &setup );

    // The tiles side by side (Display.cpp: face, launcher, the opened page).
    static const int TileCount = 3;

    // The planets page: the moment and the place every second; the main task computes the rise
    // and set of a day it asks for (a few hundred milliseconds) and hands them over.
    void showPlanets( int64_t now, const Astronomy::Observer &observer, int utcOffsetMinutes, bool located );
    bool takePlanetsRequest( int64_t &localMidnight, Astronomy::Observer &observer );
    void showPlanetsDay( const Planets::Day &day, int64_t localMidnight, const Astronomy::Observer &observer );

    // The astronomy pages: values of the local day and of this moment; the face's events,
    // counters and cities. Also updates the face with the time of the last update(); `located`
    // false: no location set yet (the face's sky slots stay empty).
    void showSky( const Sky::Day &day, const Sky::Now &now, const Astronomy::Observer &observer, int utcOffsetMinutes,
                  const FaceTable &table, bool located );

    // The corner gauges, on every page.
    // `night`: the Sun is down (the brightness control names the night level).
    // `red`: the red night mode (NightRed), on every part but the Wi-Fi QR code.
    void showCorners( const Corners::Gauge gauges[Corners::CornerCount], bool night, bool red );

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

    // The alarm corner icons' state for the face (FaceText::Inputs::alarms).
    void setAlarmIcons( const FaceText::AlarmIconState &state );

    // The clock page's alarms part (ClockPages::setAlarms, takeAlarmToggle).
    void setAlarms( const AlarmStore::List &alarms, const int64_t *due );
    bool takeAlarmToggle( int &index );

    // The clock page's timers for the alarms (ClockPages): false when the lock was busy.
    bool timers( int64_t &countdownEndMs, bool &stopwatchRunning );
    void finishCountdown();
    void showHijriMonth( const Hijri::Month &month, int today, int method );

    // The Request bits tapped since the last call.
    int takeRequests();

    // Milliseconds since the last touch (LVGL inactivity time); the previous value while LVGL
    // holds the display lock.
    uint32_t inactiveMs();

    // Whether the touch reads as pressed, as of the last inactiveMs() call. A controller that
    // reports one press for minutes is stuck: it would keep active mode on (main.cpp).
    bool touchPressed() const;

    // The ringing alarm over every page: its name and the time, and a tap anywhere stops it
    // (alarmTapped() once true until the next showAlarm()).
    void showAlarm( const char *name, const char *time );
    void hideAlarm();
    bool alarmTapped() const;

    // For the event log: touch presses since start(), and whether touch input started.
    int touches() const;
    bool touchStarted() const;

    // While a brightness drag is held with its direction chosen: 0 up or down, 1 sideways; else
    // -1. A PWR press then mirrors the picture along that direction (toggleMirror).
    int adjustAxis();
    void toggleMirror( int axis );

    // Before deep sleep: takes the display lock and keeps it, so LVGL no longer uses the bus.
    void stop();

#if CONFIG_WATCH_DEV_LOGGING
    // Shows face page `page` without changing the setting (tools/screenshot.py).
    void showFaceStyle( int page );
    // Puts `item` into `slot` of face page `page` (not saved) and shows the page.
    void setFaceSlot( int page, int slot, int item );
    // Shows the brightness control as a long press does (a screenshot of it).
    void showLevels();
    // The galactic centre's altitude and azimuth and the Sun's altitude for the face from now on
    // (a picture of the Milky Way band; development only).
    void overrideSky( double altitude, double azimuth, double sunAltitude );
    // The mirror test (tools/guided_test.py mirror): MADCTL `value` and the column gap.
    void testAddressMode( int value, int gap );

    // The fixed scene of the screenshots: sample states on the clock page.
    void setScene( bool scene );

    // Presses a clock page button by name (ClockPages::press).
    bool pressClockButton( const char *name );

    // Swipes to tile `page` (0 to 2) with the page animation, as a finger would (speed
    // measurements); 3 and 4 scroll the launcher's list down and up by most of a screen.
    void swipeTo( int page );

    // Development screenshot (tools/screenshot.py): shows shot `page` (Display.cpp, s_shots: a
    // tile, or a stacked page's part opened as from the launcher), redraws the whole screen
    // and writes each pixel row to the console as "SHOT <y> <x> <colour runs>". Counts as activity.
    void capture( int page );
#endif

private:
    static void onSettingClicked( lv_event_t *event );
    static void onGesture( lv_event_t *event );
    static void onPageChanged( lv_event_t *event );
    static void onPagesScrolled( lv_event_t *event );
    // A part change on the opened page: a clock page part that takes sideways swipes itself
    // (ClockPages::swipesSideways) stops the page swipe while it is shown, the QR code part
    // dims the panel, and the part is the one to open next time.
    static void onAppPartChanged( lv_event_t *event );
    void updatePageSwipe();
    // The opened page is built while the launcher or the page is shown and freed at the watch
    // face (its objects hold internal heap the setup portal needs).
    void keepPages( int shownTile );
    // Builds the page last opened (s_appPage) in the App tile, freeing the one built there before.
    void ensureApp();
    void freeApp();
    // Opens `part` of stacked page `page` in the App tile and moves there.
    void openPart( int page, int part, bool animate );
    StackPage &stack( int page );
    // The parts of stacked page `page` to show, in order (PartOrder::decode of its settings).
    int decodeOrder( int page, int parts[PartOrder::MaxParts] ) const;
    int activeTile() const;
    // The launcher: its name over a scrolled list of the stacked pages and their parts.
    void buildLauncher();
    void fillLauncher();
    static void onLauncherClicked( lv_event_t *event );
    lv_obj_t *m_launcherList = nullptr;
    void endAdjust();
    void buildInfoPage();
    void buildSettingsPage();
    void showBattery();
    void applySetup();
    // The orders and hidden parts set in the portal (PartOrder), on the opened page.
    void applyOrders();
    // The face page of Settings::FaceStyle, one of those shown (FaceConfig::shown).
    void showFacePage( int direction );
    lv_obj_t *m_tiles[TileCount] = {};
    int m_appPage = -1;  // the stacked page built in the App tile, -1: none
    bool m_appReady = false;  // built and its part restored
    PlanetPages m_planets;
    FaceText::AlarmIconState m_alarmIcons = {};
    lv_obj_t *m_alarm = nullptr;
    lv_obj_t *m_alarmTime = nullptr;
    lv_obj_t *m_alarmName = nullptr;
    volatile bool m_alarmTapped = false;
    static void onAlarmPressed( lv_event_t *event );
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
    bool m_night = false;  // between sunset and sunrise (Corners::night): the always-on night level applies
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
    int m_lastX = 0;  // the finger's last point while adjusting
    int m_lastY = 0;
    // Where the finger touched the face, and whether it has moved away since: a swipe held
    // after the page step is no long press.
    int m_pressX = 0;
    int m_pressY = 0;
    bool m_pressMoved = false;
    lv_obj_t *m_faceTile = nullptr;
    StackPage m_info;
    StackPage m_settingsPage;
    PageMarks m_faceMarks;
    PageMarks m_pageMarks;  // the tiles, shown while they are swiped
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
    bool m_skyOverride = false;
    double m_overrideAltitude = 0;
    double m_overrideAzimuth = 0;
    double m_overrideSunAltitude = 0;
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
    // The panel at s_joinCodeBrightness while the QR code part shows, else at the setting.
    void updateJoinCodeBrightness();
    bool m_joinCodeDimmed = false;
    // The red night mode: wanted (showCorners) and shown (not over the QR code); read by the flush.
    void updateRed();
    bool m_redWanted = false;
    bool m_red = false;
    lv_obj_t *m_joinCode = nullptr;
    lv_obj_t *m_joinHint = nullptr;
    char m_joinPayload[80] = {};
    uint8_t m_joinModules[qrcodegen_BUFFER_LEN_FOR_VERSION( 5 )] = {};
    TablePart m_setupTable;
    std::atomic<int> m_requests { 0 };
    std::atomic<int> m_touches { 0 };
    bool m_touchStarted = false;
    static void onTouchCounted( lv_event_t *event );
    uint32_t m_inactiveMs = 0;
    lv_indev_t *m_touch = nullptr;
    bool m_touchPressed = false;
};
