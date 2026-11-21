#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

#include "AlwaysOn.h"
#include "Astronomy.h"
#include "Hijri.h"
#include "HolyDays.h"
#include "Planets.h"
#include "Sky.h"
#include "Axp2101.h"
#include "BatteryLimit.h"
#include "BatteryLog.h"
#include "ClockPages.h"
#include "Corners.h"
#include "Dates.h"
#include "EventLog.h"
#include "esp_core_dump.h"
#include "esp_ota_ops.h"
#include "DeepSleep.h"
#include "Display.h"
#include "FaceStore.h"
#include "FaceTable.h"
#include "Motion.h"
#include "Panel.h"
#include "Peripherals.h"
#if CONFIG_WATCH_SETUP_PORTAL
#include "Portal.h"
#endif
#include "Rtc.h"
#include "Settings.h"
#include "Sound.h"
#include "Fonts.h"
#include "DayCounters.h"
#include "AlarmStore.h"
#include "Ringer.h"
#include "Language.h"
#include "WorldClock.h"
#include "Zones.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "main";

static const gpio_num_t s_powerKeyGpio = GPIO_NUM_10;
// The BOOT key: low while pressed (pull-up R8), read in active mode only, never a wake source. A
// press held this long counts (the PWR key's 20 ms polls, twice); this long is a long press.
static const gpio_num_t s_bootKeyGpio = GPIO_NUM_0;
static const uint32_t s_bootPressMs = 40;
static const uint32_t s_bootLongMs = 1000;

static Axp2101 s_pmu;
static DeepSleep s_deepSleep;
static Peripherals s_peripherals;
static Rtc s_rtc;
static Display s_display;
static bool s_displayOn = false;
// For the event log of the active session.
static uint8_t s_touchModeStart = 0xFF;
static bool s_touchRecovered = false;
// For the event log: this session switched the PWR key's internal pull-up on (powerKeyMissed).
static bool s_powerKeyPullUpSwitched = false;
static bool s_portalUsed = false;
static AlwaysOn s_alwaysOn;
static BatteryLog s_batteryLog;
static Motion s_motion;
static Settings s_settings;
static WorldClock s_worldClock;
static DayCounters s_dayCounters;
// The alarms' definitions (NVS); s_alarmsChanged: their due times must be computed again.
static AlarmStore::List s_alarms;
static bool s_alarmsChanged = false;
// The ringing alarm (which: 0-3 an alarm, 4 the countdown).
static const int s_countdownAlarm = 4;
static Ringer s_ringer;
static bool s_ringing = false;
static int s_ringWhich = -1;
static int64_t s_ringStartUs = 0;
// The RTC's Unix time at the last display update in active mode; 0 while the clock is not set.
static int64_t s_rtcNow = 0;
static bool s_pmuReady = false;
#if CONFIG_WATCH_DEV_LOGGING
// "R <seconds>": a test alarm in RAM, due that many seconds from now.
static int64_t s_testDue = 0;
#endif
// Alarm pending (docs/superpowers/specs/2026-11-19-alarms-design.md): the due times of the
// alarms (0-3) and the countdown (4), computed in active mode and kept through deep sleep, where
// neither the definitions nor the astronomy are read.
static const int s_dueCount = Alarms::MaxAlarms + 1;
struct AlarmPlan
{
    int64_t due[s_dueCount];
    uint32_t key;        // of the place and the definitions they were computed from
    int64_t computedAt;  // RTC Unix time
    bool anyOn;
};
RTC_DATA_ATTR static AlarmPlan s_plan;
// The due times already rung or skipped, kept through a reset or a crash (RTC_NOINIT; checked
// after a power-on), so that an alarm due in that minute rings once and is not lost.
RTC_NOINIT_ATTR static int64_t s_handled[s_dueCount];
RTC_NOINIT_ATTR static uint32_t s_handledCheck;
static void storeHandled( int which, int64_t due );
// What a timer wake is for when the always-on face is not running.
enum WakePurpose : uint8_t
{
    WakeAlwaysOn,
    WakePanelOff,  // the first wake of the pending sleep: the panel to deep standby
    WakeAlarm      // the pending sleep: an alarm, or the hourly battery check
};
RTC_DATA_ATTR static uint8_t s_wakePurpose;
// The stopwatch or the countdown runs (the hourglass icon), from the clock page each second.
static bool s_timerRunning = false;
#if CONFIG_WATCH_SETUP_PORTAL
static Portal s_portal;
#endif

// The own time zone for the always-on face (the UTC offset and its next clock changes), copied
// when active mode ends: the minute wakes do not read NVS.
RTC_DATA_ATTR static Zone s_alwaysOnZone;
// The development mode setting, copied when active mode ends (minute wakes read no NVS).
RTC_DATA_ATTR static bool s_alwaysOnDevMode;
// The Hijri calendar's next religious day and what it was searched for: the local day, the
// location, the UTC offset and the Hijri method (main.cpp searches again when one changes).
RTC_DATA_ATTR static int s_holyKey[5];
RTC_DATA_ATTR static HolyDays::Next s_holyNext;
// Development mode writes an always-on summary to the event log this often.
static const int s_statusEveryMinutes = 30;
// The AXP2101 powers off below this VSYS by itself (REG24): under BatteryLimit's 3.45 V, with room
// for the dip of a small cell under the Wi-Fi's and an alarm's current.
static const int s_pmuPowerOffMillivolts = 3000;
// A touch reported as pressed without a break for this long ends active mode (runActiveMode).
static const uint32_t s_touchStuckMs = 60000;

static void logPowerKeyIrq()
{
    bool shortPress = false;
    bool longPress = false;
    bool pressStart = false;
    if ( s_pmu.clearPowerKeyIrq( shortPress, longPress, pressStart ) == ESP_OK ) {
        ESP_LOGI( TAG, "PWR key IRQ: short %d, long %d, press start %d", shortPress, longPress, pressStart );
    } else {
        ESP_LOGE( TAG, "PWR key IRQ read failed" );
    }
}

// The face slots' events, day counters and cities; built again with the sky day and when the
// portal changes the counters or the cities.
static FaceTable s_faceTable;
static bool s_faceTableStale = true;
// The face pages (FaceStore), loaded when active mode starts and changed by the portal.
static FaceConfig::Pages s_facePages;
// What the always-on face shows (filled by fillLook; too large for the main task's stack).
static AlwaysOn::Look s_look;
static void fillLook( int page );

// Set by the host commands "P" and "N": the same as a tap on the setup page's rows.
[[maybe_unused]] static int s_hostRequests = 0;
// Set by the host command "O": active mode ends.
[[maybe_unused]] static bool s_hostEndActive = false;
// Set by the host command "Q": active mode ends as with the always-on face disabled.
[[maybe_unused]] static bool s_hostNoFace = false;
// Set by the host command "Y": the next active mode starts without the display, as when it fails.
RTC_DATA_ATTR [[maybe_unused]] static bool s_hostSkipDisplay = false;
// Set by the host command "H": the next PWR wake counts as a phantom (phantomPowerKey).
RTC_DATA_ATTR [[maybe_unused]] static bool s_hostPhantomKey = false;
// Set by the host command "I <0|1>": the red night mode on whatever the Sun (1), or as set (0).
[[maybe_unused]] static bool s_hostNightRed = false;
static char s_hostLine[32];
// The scene of the published screenshots, so that they stay the same and do not show the
// watch's location: Istanbul on 2026-06-21 at 19:19 local time (UTC+3), the local Hijri method.
// The settings are not changed, so that nothing of it reaches NVS.
[[maybe_unused]] static bool s_scene = false;
// Development: the faces drawn as without a valid time ("Z"), for screenshots of "--:--".
[[maybe_unused]] static bool s_noTime = false;
[[maybe_unused]] static const int s_sceneInputs[5] = { 41008200, 28978400, 180, 0, 17 };
[[maybe_unused]] static const Rtc::DateTime s_sceneTime = { 2026, 6, 21, 16, 19, 0 };
static int s_hostLineLength = 0;

// Development commands from the host: the guided test sends the UTC time as
// "T YYYY-MM-DD HH:MM:SS", timed so that the RTC's next second matches the host's;
// tools/screenshot.py sends "S <page>" for a screenshot of that page (Display::capture) and "A <page>" for one of
// the always-on face of that face page (at 19:19); "P" switches the setup
// portal on or off and "N" makes a new portal password (so that a published screenshot does not
// show the one in use). "D" shows a fixed scene until the next reset (s_sceneInputs); "F <page>"
// shows that face page without changing the setting, "G <page> <slot> <item>" puts an item into
// a slot of it, "H <page> <style>" makes it analog, "J <page> <template>"
// sets its template, "C <corner> <item> <parameter>" a corner gauge (none saved); "O" ends active mode as a
// PWR press does; "U <0|1>" sets the PWR key's internal pull-up; "K" shows the brightness
// control; "X <hex> <gap>" sets the panel's MADCTL and column gap (guided test "mirror"); "V <altitude>
// <azimuth> <sun altitude>" puts the galactic centre and the Sun there for the face; "Z" draws the
// faces as without a valid time ("--:--"), again "Z" with it. Only
// "E" (the event log, read by tools/read_events.py) also works without CONFIG_WATCH_DEV_LOGGING.
// Returns true when a command arrived: like a touch, it restarts the display timeout, so that
// the watch stays in active mode while tools/screenshot.py works.
static bool pollHostCommands()
{
    bool received = false;
    char c = 0;
    while ( usb_serial_jtag_read_bytes( &c, 1, 0 ) == 1 ) {
        if ( c == '\r' ) {
            continue;
        }
        if ( c != '\n' ) {
            if ( s_hostLineLength < static_cast<int>( sizeof( s_hostLine ) ) - 1 ) {
                s_hostLine[s_hostLineLength++] = c;
            }
            continue;
        }
        s_hostLine[s_hostLineLength] = '\0';
        s_hostLineLength = 0;
        received = true;
        // "M": the saved face pages as FaceConfig::encode writes them (for FaceConfig::defaults).
        if ( s_hostLine[0] == 'M' && s_hostLine[1] == '\0' ) {
            FaceConfig::Pages saved = {};
            FaceStore::load( saved );
            static char s_encoded[( FaceConfig::MaxPages * 22 + 8 ) * 6 + 16];
            if ( FaceConfig::encode( saved, s_encoded, sizeof( s_encoded ) ) > 0 ) {
                printf( "FACES %s\n", s_encoded );
            }
            continue;
        }
        if ( s_hostLine[0] == 'E' && s_hostLine[1] == '\0' ) {
            EventLog::print( s_settings.value( Settings::UtcOffset ) );
            continue;
        }
#if CONFIG_WATCH_DEV_LOGGING
        int swipePage = 0;
        int testSeconds = 0;
        if ( sscanf( s_hostLine, "R %d", &testSeconds ) == 1 ) {
            s_testDue = s_rtcNow != 0 ? s_rtcNow + testSeconds : 0;
            ESP_LOGI( TAG, "Test alarm in %d s", testSeconds );
            continue;
        }
        if ( sscanf( s_hostLine, "W %d", &swipePage ) == 1 ) {
            s_display.swipeTo( swipePage );
            continue;
        }
        // "n <mode>": the launcher rows' trial (Display::setLauncherMode); every capital is taken.
        int launcherMode = 0;
        if ( sscanf( s_hostLine, "n %d", &launcherMode ) == 1 ) {
            s_display.setLauncherMode( launcherMode );
            continue;
        }
        int language = 0;
        if ( sscanf( s_hostLine, "L %d", &language ) == 1 ) {
            // As the settings row does: save, then restart in the new language.
            s_settings.setValue( Settings::Language, language );
            s_settings.save();
            esp_restart();
        }
        if ( s_hostLine[0] == 'B' && s_hostLine[1] == ' ' ) {
            ESP_LOGI( TAG, "Button %s: %s", s_hostLine + 2,
                      s_display.pressClockButton( s_hostLine + 2 ) ? "pressed" : "unknown" );
            continue;
        }
        int page = 0;
        if ( sscanf( s_hostLine, "S %d", &page ) == 1 ) {
            s_display.capture( page );
            continue;
        }
        if ( s_hostLine[0] == 'P' && s_hostLine[1] == '\0' ) {
            s_hostRequests |= Display::TogglePortal;
            continue;
        }
        int style = 0;
        if ( sscanf( s_hostLine, "A %d", &style ) == 1 ) {
            fillLook( style );
            AlwaysOn::capture( s_look, style, !s_noTime );
            continue;
        }
        if ( sscanf( s_hostLine, "F %d", &style ) == 1 ) {
            s_display.showFaceStyle( style );
            continue;
        }
        float galacticAltitude = 0;
        float galacticAzimuth = 0;
        float sunAltitude = 0;
        if ( sscanf( s_hostLine, "V %f %f %f", &galacticAltitude, &galacticAzimuth, &sunAltitude ) == 3 ) {
            s_display.overrideSky( galacticAltitude, galacticAzimuth, sunAltitude );
            AlwaysOn::overrideSky( galacticAltitude, galacticAzimuth, sunAltitude );
            continue;
        }
        unsigned addressMode = 0;
        int gap = 0;
        if ( sscanf( s_hostLine, "X %x %d", &addressMode, &gap ) == 2 ) {
            s_display.testAddressMode( static_cast<int>( addressMode ), gap );
            continue;
        }
        int pullUp = 0;
        if ( sscanf( s_hostLine, "U %d", &pullUp ) == 1 ) {
            // Settings::PowerKeyPullUp by hand, e.g. after a press taken as missed by mistake.
            s_settings.setValue( Settings::PowerKeyPullUp, pullUp != 0 ? 1 : 0 );
            s_settings.save();
            s_deepSleep.setPowerKeyPullUp( pullUp != 0 );
            ESP_LOGI( TAG, "PWR internal pull-up %s", pullUp != 0 ? "on" : "off" );
            continue;
        }
        if ( s_hostLine[0] == 'K' && s_hostLine[1] == '\0' ) {
            s_display.showLevels();
            continue;
        }
        int slot = 0;
        int item = 0;
        if ( sscanf( s_hostLine, "H %d %d", &style, &item ) == 2 ) {
            if ( style >= 0 && style < s_facePages.count && item >= 0 && item < FaceConfig::AnalogStyleCount ) {
                FaceConfig::Page &page = s_facePages.pages[style];
                page.clock = FaceConfig::AnalogClock;
                page.analog = static_cast<uint8_t>( item );
                // As the portal does: Classic with the seconds hand.
                page.flags = static_cast<uint8_t>( item == FaceConfig::ClassicHands ? page.flags | FaceConfig::SecondsHandFlag
                                                                                    : page.flags & ~FaceConfig::SecondsHandFlag );
                s_display.setPages( s_facePages );
            }
            s_display.showFaceStyle( style );
            continue;
        }
        if ( sscanf( s_hostLine, "J %d %d", &style, &item ) == 2 ) {
            if ( style >= 0 && style < s_facePages.count && item >= 0 && item < FaceConfig::TemplateCount ) {
                FaceConfig::Page &page = s_facePages.pages[style];
                page.layout = static_cast<uint8_t>( item );
                const uint8_t sky = FaceConfig::MilkyWayFlag | FaceConfig::PlanetsFlag;
                page.flags = static_cast<uint8_t>( item != FaceConfig::Large ? page.flags | sky : page.flags & ~sky );
                s_display.setPages( s_facePages );
            }
            s_display.showFaceStyle( style );
            continue;
        }
        int gaugeParameter = 0;
        if ( sscanf( s_hostLine, "C %d %d %d", &slot, &item, &gaugeParameter ) == 3 ) {
            if ( slot >= 0 && slot < 4 && item >= 0 && item < FaceConfig::GaugeItemCount ) {
                s_facePages.gauges[slot] = static_cast<uint8_t>( item );
                s_facePages.gaugeParameters[slot] = static_cast<uint8_t>( gaugeParameter );
            }
            continue;
        }
        if ( sscanf( s_hostLine, "G %d %d %d", &style, &slot, &item ) == 3 ) {
            // The always-on face ("A") draws from these pages.
            if ( style >= 0 && style < s_facePages.count && slot >= 0 && slot < FaceConfig::SlotCount && item >= 0 &&
                 item < 256 ) {
                s_facePages.pages[style].items[slot] = static_cast<uint8_t>( item );
            }
            s_display.setFaceSlot( style, slot, item );
            continue;
        }
        if ( s_hostLine[0] == 'N' && s_hostLine[1] == '\0' ) {
            s_hostRequests |= Display::NewPassword;
            continue;
        }
        if ( s_hostLine[0] == 'Y' && s_hostLine[1] == '\0' ) {
            s_hostSkipDisplay = true;
            ESP_LOGI( TAG, "The next active mode starts without the display" );
            continue;
        }
        if ( s_hostLine[0] == 'W' && s_hostLine[1] == '\0' ) {
            // Starves the idle task: the task watchdog must reset the watch with a core dump.
            ESP_LOGW( TAG, "Busy without a break: the task watchdog should reset the watch" );
            const int64_t until = esp_timer_get_time() + 10 * 1000000;
            while ( esp_timer_get_time() < until ) {
            }
            ESP_LOGE( TAG, "Still running after 10 s: the task watchdog did not reset" );
            continue;
        }
        int nightRed = 0;
        if ( sscanf( s_hostLine, "I %d", &nightRed ) == 1 ) {
            s_hostNightRed = nightRed != 0;
            ESP_LOGI( TAG, "Night mode %s", s_hostNightRed ? "forced on" : "as set" );
            continue;
        }
        if ( s_hostLine[0] == 'H' && s_hostLine[1] == '\0' ) {
            s_hostPhantomKey = true;
            ESP_LOGI( TAG, "The next PWR wake counts as a phantom" );
            continue;
        }
        if ( s_hostLine[0] == 'Q' && s_hostLine[1] == '\0' ) {
            s_hostNoFace = true;
            s_hostEndActive = true;
            continue;
        }
        // "K <alarm> <Alarms::encode text>": defines an alarm as the portal does (saved).
        int alarmIndex = 0;
        char alarmText[24] = {};
        if ( sscanf( s_hostLine, "K %d %23s", &alarmIndex, alarmText ) == 2 && alarmIndex >= 0 &&
             alarmIndex < Alarms::MaxAlarms ) {
            Alarms::Definition alarm = {};
            if ( Alarms::decode( alarmText, alarm ) ) {
                s_alarms.alarms[alarmIndex] = alarm;
                if ( s_alarms.count <= alarmIndex ) {
                    s_alarms.count = alarmIndex + 1;
                }
                AlarmStore::save( s_alarms );
                s_alarmsChanged = true;
                storeHandled( alarmIndex, s_rtcNow );
            }
            ESP_LOGI( TAG, "Alarm %d: %s", alarmIndex, alarmText );
            continue;
        }
        if ( s_hostLine[0] == 'O' && s_hostLine[1] == '\0' ) {
            // As a PWR press: active mode ends (a measurement of the minute wakes on USB).
            s_hostEndActive = true;
            continue;
        }
        if ( s_hostLine[0] == 'Z' && s_hostLine[1] == '\0' ) {
            s_noTime = !s_noTime;
            continue;
        }
        if ( s_hostLine[0] == 'D' && s_hostLine[1] == '\0' ) {
            s_scene = true;
            s_faceTableStale = true;
            s_display.setScene( true );
            // Never the real place in a published picture.
            char sceneDate[24];
            snprintf( sceneDate, sizeof( sceneDate ), "21 %s 2026", Language::shortMonth( 6 ) );
            const char *const sceneSetup[Display::SetupInfoRows] = {
                "Istanbul", "41.01N 28.98E", "Istanbul", "UTC+3", Language::text( Language::None ),
                Language::text( Language::Crescent ), sceneDate };
            s_display.showSetupInfo( sceneSetup );
            // tools/screenshot.py waits for this line: without the scene the pictures would show
            // the real time and location.
            ESP_LOGI( TAG, "Fixed scene on" );
            continue;
        }
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        if ( sscanf( s_hostLine, "T %d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second ) != 6 ) {
            continue;
        }
        Rtc::DateTime time = {};
        time.year = static_cast<uint16_t>( year );
        time.month = static_cast<uint8_t>( month );
        time.day = static_cast<uint8_t>( day );
        time.hour = static_cast<uint8_t>( hour );
        time.minute = static_cast<uint8_t>( minute );
        time.second = static_cast<uint8_t>( second );
        const esp_err_t err = s_rtc.set( time );
        ESP_LOGI( TAG, "RTC set to %04d-%02d-%02d %02d:%02d:%02d UTC: %s", year, month, day, hour, minute, second,
                  esp_err_to_name( err ) );
#endif
    }
    return received;
}

// Deep-sleep floor check: a line that a chip holds low in sleep is still low right after the
// wake. Read before the BSP configures the I2C pads. I2C, touch INT and RTC INT have pull-ups to
// VCC3V3, so a low level there means current through the pull-up while sleeping.
[[maybe_unused]] static void logPadLevels()
{
    // GPIO19/20 (USB) are left out: switching them to GPIO would drop the USB console.
    const gpio_num_t pads[11] = { GPIO_NUM_0,  GPIO_NUM_1,  GPIO_NUM_2,  GPIO_NUM_3,  GPIO_NUM_14, GPIO_NUM_15,
                                  GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_38, GPIO_NUM_39, GPIO_NUM_46 };
    char text[160] = "";
    int length = 0;
    for ( int i = 0; i < 11; ++i ) {
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << pads[i];
        config.mode = GPIO_MODE_INPUT;
        gpio_config( &config );
        length += snprintf( text + length, sizeof( text ) - length, " %d=%d", pads[i], gpio_get_level( pads[i] ) );
    }
    ESP_LOGI( TAG, "Pad levels after wake:%s", text );
}

// The astronomy pages' values for the local day, kept in RTC memory across deep sleep: they take
// about 0.5 s on the ESP32-S3, so they are computed once a day, or when a setting they depend on
// changes (s_skyInputs: location, UTC offset, Hijri method, the night prayer's angle).
RTC_DATA_ATTR static Sky::Day s_skyDay;
RTC_DATA_ATTR static int s_skyInputs[5];
RTC_DATA_ATTR static bool s_skyValid;

static int64_t unixTime( const Rtc::DateTime &time )
{
    // The firmware never sets TZ, so mktime() works in UTC.
    tm fields = {};
    fields.tm_year = time.year - 1900;
    fields.tm_mon = time.month - 1;
    fields.tm_mday = time.day;
    fields.tm_hour = time.hour;
    fields.tm_min = time.minute;
    fields.tm_sec = time.second;
    return mktime( &fields );
}

// Writes an event with the battery state and the RTC time (the RTC must be initialised).
static void logEvent( EventLog::Entry entry, bool pmuReady )
{
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK ) {
            entry.batteryMillivolts = millivolts;
            entry.batteryPercent = percent;
            entry.flags |= vbusGood ? EventLog::Usb : 0;
        }
    }
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    const bool timeRead = s_rtc.read( time, oscillatorStopped ) == ESP_OK && !oscillatorStopped;
    EventLog::add( entry, timeRead ? static_cast<uint32_t>( unixTime( time ) ) : 0 );
}


static void buildFaceTable( const Astronomy::Observer &observer, int offsetMinutes, int ishaAngle )
{
    s_faceTable = {};
    s_faceTable.build( s_skyDay, offsetMinutes, observer, ishaAngle );
    for ( int i = 0; i < s_dayCounters.count() && i < FaceTable::CounterCount; ++i ) {
        s_faceTable.setCounter( i, s_dayCounters.counter( i ).name, s_dayCounters.counter( i ).day,
                                s_dayCounters.savedDay( i ) );
    }
    for ( int i = 0; i < s_worldClock.count() && i < FaceTable::CityCount; ++i ) {
        s_faceTable.setCity( i, s_worldClock.city( i ).name, s_worldClock.city( i ).zone );
    }
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_scene ) {
        // The published pictures show the scene's counters and cities, not the owner's.
        for ( int i = 0; i < FaceTable::CounterCount; ++i ) {
            s_faceTable.setCounter( i, ClockPages::SceneCounterNames[Language::current()][i],
                                    Dates::dayNumber( 2026, 6, 21 ) + ClockPages::SceneCounterDays[i] );
        }
        for ( int i = 0; i < FaceTable::CityCount; ++i ) {
            s_faceTable.setCity( i, ClockPages::SceneCityNames[i], { ClockPages::SceneCityOffsets[i], { 0, 0 }, { 0, 0 } } );
        }
    }
#endif
    s_faceTableStale = false;
}

// `inputs`: latitude and longitude (micro-degrees), UTC offset (minutes), Hijri method, isha
// angle.
static void updateSky( int64_t now, const int inputs[5] )
{
    const int offsetMinutes = inputs[2];
    const Astronomy::Observer observer = { inputs[0] / 1e6, inputs[1] / 1e6 };
    const int day = static_cast<int>( ( now + offsetMinutes * 60 ) / 86400 );
    bool same = true;
    for ( int i = 0; i < 5; ++i ) {
        same = same && s_skyInputs[i] == inputs[i];
    }
    if ( !s_skyValid || s_skyDay.day != day || !same ) {
        for ( int i = 0; i < 5; ++i ) {
            s_skyInputs[i] = inputs[i];
        }
        const int64_t start = esp_timer_get_time();
        s_skyDay = Sky::computeDay( day, offsetMinutes, observer, static_cast<Hijri::Method>( inputs[3] ), inputs[4] );
        s_skyValid = true;
        s_faceTableStale = true;
        // The stack margin left: the computation is the deepest call of the main task.
        ESP_LOGI( TAG, "Sky day computed in %d ms, main stack %u bytes never used",
                  static_cast<int>( ( esp_timer_get_time() - start ) / 1000 ),
                  static_cast<unsigned>( uxTaskGetStackHighWaterMark( nullptr ) ) );
    }
    if ( s_faceTableStale || s_faceTable.localDay != s_skyDay.day ) {
        buildFaceTable( observer, offsetMinutes, inputs[4] );
    }
    s_display.showSky( s_skyDay, Sky::computeNow( now, s_skyDay, observer ), observer, offsetMinutes, s_faceTable,
                       inputs[0] != 0 || inputs[1] != 0 );
    s_display.showPlanets( now, observer, offsetMinutes, inputs[0] != 0 || inputs[1] != 0 );
    // The planets page asks for its day's rise and set when it is built or a new day begins.
    int64_t localMidnight = 0;
    Astronomy::Observer planetsObserver = {};
    if ( s_display.takePlanetsRequest( localMidnight, planetsObserver ) ) {
        const int64_t start = esp_timer_get_time();
        const Planets::Day planets = Planets::day( localMidnight, planetsObserver );
        ESP_LOGI( TAG, "Planets' day computed in %d ms", static_cast<int>( ( esp_timer_get_time() - start ) / 1000 ) );
        s_display.showPlanetsDay( planets, localMidnight, planetsObserver );
    }
}

// The always-on face of face page `page` (Settings::FaceStyle when active mode ends) into s_look.
static Zone homeZone();

// The corner icons' state: each alarm's days (a "once" alarm: the day it rings), on or off,
// any alarm on, a timer running.
static FaceText::AlarmIconState alarmIcons()
{
    FaceText::AlarmIconState state = {};
    for ( int i = 0; i < s_alarms.count && i < Alarms::MaxAlarms; ++i ) {
        const Alarms::Definition &alarm = s_alarms.alarms[i];
        state.on[i] = alarm.on;
        state.days[i] = alarm.days;
        if ( alarm.days == 0 && s_plan.due[i] != 0 ) {
            const int64_t local = s_plan.due[i] + static_cast<int64_t>( Zones::offsetAt( homeZone(), s_plan.due[i] ) ) * 60;
            state.days[i] = static_cast<uint8_t>( 1 << Dates::weekday( static_cast<int>( local / 86400 ) ) );
        }
    }
    state.anyOn = s_plan.anyOn;
    state.timerRunning = s_timerRunning;
    state.next = -1;
    for ( int i = 0; i < s_alarms.count && i < Alarms::MaxAlarms; ++i ) {
        if ( s_alarms.alarms[i].on && s_plan.due[i] != 0 &&
             ( state.next < 0 || s_plan.due[i] < s_plan.due[state.next] ) ) {
            state.next = static_cast<int8_t>( i );
        }
    }
    return state;
}

static void fillLook( int page )
{
    // The pages active mode showed (loaded when it started, changed by the portal).
    if ( s_facePages.count == 0 ) {
        FaceStore::load( s_facePages );
    }
    s_look = {};
    s_look.page = s_facePages.pages[page >= 0 && page < s_facePages.count ? page : 0];
    for ( int i = 0; i < 4; ++i ) {
        s_look.gauges[i] = s_facePages.gauges[i];
        s_look.gaugeParameters[i] = s_facePages.gaugeParameters[i];
    }
    s_look.dayBrightness = s_settings.value( Settings::AodDayBrightness );
    s_look.nightBrightness = s_settings.value( Settings::AodNightBrightness );
    s_look.language = s_settings.value( Settings::Language );
    s_look.nightMode = s_hostNightRed ? AlwaysOn::NightForced : s_settings.value( Settings::NightMode );
    s_look.alarms = alarmIcons();
    if ( s_skyValid ) {
        s_look.moonPercent = static_cast<int>( s_skyDay.phase.illumination * 100 + 0.5 );
        s_look.waxing = s_skyDay.phase.elongation < 180;
        s_look.corners = Corners::fromSky( s_skyDay );
        // The place and the table of the sky day shown (the fixed scene's in a screenshot).
        // Without a location (0, 0 until the portal sets one) the sky slots stay empty.
        s_look.skyValid = !s_faceTableStale && ( s_skyInputs[0] != 0 || s_skyInputs[1] != 0 );
        s_look.table = s_faceTable;
        s_look.observer = { s_skyInputs[0] / 1e6, s_skyInputs[1] / 1e6 };
        s_look.ishaAngle = s_skyInputs[4];
    }
}

// USB power and the battery at the last display update (the "On USB: stay on" setting, the
// portal's update condition).
static bool s_usbPower = false;
static int s_batteryPercent = -1;

// The own time zone from the settings (Settings::UtcOffset and the Home* items).
static Zone homeZone()
{
    return { s_settings.value( Settings::UtcOffset ),
             { s_settings.value( Settings::HomeChange1 ), s_settings.value( Settings::HomeChange2 ) },
             { s_settings.value( Settings::HomeAfter1 ), s_settings.value( Settings::HomeAfter2 ) } };
}

// A clock change that has passed becomes the UTC offset, saved at once (rare: twice a year).
static void advanceZones( int64_t utcSeconds )
{
    Zone zone = homeZone();
    if ( Zones::advance( zone, utcSeconds ) ) {
        s_settings.setValue( Settings::UtcOffset, zone.offset );
        s_settings.setValue( Settings::HomeChange1, zone.changeAt[0] );
        s_settings.setValue( Settings::HomeAfter1, zone.offsetAfter[0] );
        s_settings.setValue( Settings::HomeChange2, zone.changeAt[1] );
        s_settings.setValue( Settings::HomeAfter2, zone.offsetAfter[1] );
        s_settings.save();
        ESP_LOGI( TAG, "Clock change: UTC offset now %d min", zone.offset );
    }
    s_worldClock.advance( utcSeconds );
}

// Whether Settings::UnpluggedMinute holds a time (1) or 0 (0), -1 before the first check; in RTC
// memory, so that the minute wakes read the settings only when USB power came or went.
RTC_DATA_ATTR static int s_unpluggedSaved = -1;

// Keeps the time USB power went away (Settings::UnpluggedMinute): 0 while it is present, the
// minute it was first seen gone otherwise. Writes NVS only when that changes, twice per charge.
// A cable pulled while the watch was off is seen at the next start.
static void trackUnplugged( bool vbusGood, int64_t utcSeconds, bool settingsLoaded )
{
    if ( s_unpluggedSaved == ( vbusGood ? 0 : 1 ) || utcSeconds <= 0 ) {
        return;
    }
    if ( !settingsLoaded ) {
        s_settings.load();
    }
    const int saved = s_settings.value( Settings::UnpluggedMinute );
    if ( vbusGood && saved != 0 ) {
        s_settings.setValue( Settings::UnpluggedMinute, 0 );
        s_settings.save();
    } else if ( !vbusGood && saved == 0 ) {
        s_settings.setValue( Settings::UnpluggedMinute, static_cast<int>( utcSeconds / 60 ) );
        s_settings.save();
    }
    s_unpluggedSaved = vbusGood ? 0 : 1;
}

// When the last active mode ended (the RTC's Unix time; 0: never, or RTC memory lost). A PWR wake
// this soon after it shows the page that was shown then (Display::start); later the face.
RTC_DATA_ATTR static int64_t s_activeEndedAt = 0;
static const int s_resumeSeconds = 60;

// The RTC's Unix time, 0 when it cannot be read or was never set.
static int64_t rtcUnixTime()
{
    Rtc::DateTime time = {};
    bool stopped = false;
    if ( s_rtc.read( time, stopped ) != ESP_OK || stopped || time.month == 0 ) {
        return 0;
    }
    return unixTime( time );
}

// The red night mode at the start of active mode, so that the first frame comes in it (it came in
// colour and turned red at the first second's update): the day's sky kept in RTC memory and the
// RTC's time, the same rule as updateDisplay(). Without the sky (a power-on) it waits for that.
static bool redAtStart( int64_t now )
{
    if ( s_hostNightRed ) {
        return true;
    }
    const bool located = s_settings.value( Settings::Latitude ) != 0 || s_settings.value( Settings::Longitude ) != 0;
    if ( !located || s_settings.value( Settings::NightMode ) == 0 || !s_skyValid || now == 0 ) {
        return false;
    }
    return Corners::dark( Corners::fromSky( s_skyDay ), now );
}

// Whether this active mode goes on where the last one ended.
static bool resumeAtStart( int64_t now )
{
    const bool resume = s_activeEndedAt != 0 && now >= s_activeEndedAt && now - s_activeEndedAt <= s_resumeSeconds;
    ESP_LOGI( TAG, "Resume: ended at %lld, now %lld, %s", static_cast<long long>( s_activeEndedAt ),
              static_cast<long long>( now ), resume ? "resumed" : "face" );
    return resume;
}

static void updateDisplay()
{
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    Display::Battery battery = {};
    s_rtc.read( time, oscillatorStopped );
    const bool timeValid = !oscillatorStopped && time.month != 0;
    if ( timeValid ) {
        advanceZones( unixTime( time ) );
    }
    s_rtcNow = timeValid ? unixTime( time ) : 0;
    const bool batteryRead = s_pmu.readBattery( battery.millivolts, battery.percent, battery.vbusGood ) == ESP_OK;
    battery.chargeState = s_pmu.chargeState();
    battery.unpluggedMinutes = -1;
    if ( batteryRead && timeValid ) {
        trackUnplugged( battery.vbusGood, unixTime( time ), true );
        const int unplugged = s_settings.value( Settings::UnpluggedMinute );
        const int64_t minutes = unixTime( time ) / 60 - unplugged;
        if ( !battery.vbusGood && unplugged > 0 && minutes >= 0 ) {
            battery.unpluggedMinutes = static_cast<int>( minutes );
        }
    }
    s_usbPower = batteryRead && battery.vbusGood;
    s_batteryPercent = batteryRead ? battery.percent : -1;
    int inputs[5] = { s_settings.value( Settings::Latitude ), s_settings.value( Settings::Longitude ),
                      s_settings.value( Settings::UtcOffset ), s_settings.value( Settings::HijriMethod ),
                      s_settings.value( Settings::IshaAngle ) };
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_scene ) {
        time = s_sceneTime;
        for ( int i = 0; i < 5; ++i ) {
            inputs[i] = s_sceneInputs[i];
        }
    }
#endif
    const Rtc::DateTime local = Rtc::addMinutes( time, inputs[2] );
    s_display.update( local, inputs[2], battery, ( timeValid || s_scene ) && !s_noTime );
    // The clock page's Hijri calendar asks for a month now and then (up to 1 s of computation).
    ClockPages::HijriRequest request = {};
    if ( local.month != 0 && s_display.takeHijriRequest( request ) ) {
        const Astronomy::Observer observer = { inputs[0] / 1e6, inputs[1] / 1e6 };
        const Hijri::Method method = static_cast<Hijri::Method>( inputs[3] );
        const int day = request.step > 0 ? request.fromDay + request.length
                                         : ( request.step < 0 ? request.fromDay - 1 : request.fromDay );
        const Hijri::Month month = Hijri::month( day, inputs[2], observer, method );
        const int today = static_cast<int>( unixTime( local ) / 86400 );
        // The next religious day, searched once a day (up to HolyDays::MaxMonths months).
        const int key[5] = { today, inputs[0], inputs[1], inputs[2], inputs[3] };
        if ( memcmp( key, s_holyKey, sizeof( key ) ) != 0 ) {
            const int64_t start = esp_timer_get_time();
            Hijri::Month months[HolyDays::MaxMonths] = {};
            months[0] = request.step == 0 && request.fromDay == today ? month : Hijri::month( today, inputs[2], observer, method );
            int count = 1;
            s_holyNext = HolyDays::next( today, months, count );
            while ( s_holyNext.day == HolyDays::None && count < HolyDays::MaxMonths ) {
                const Hijri::Month &last = months[count - 1];
                months[count] = Hijri::month( last.firstDay + last.length, inputs[2], observer, method );
                ++count;
                s_holyNext = HolyDays::next( today, months, count );
            }
            memcpy( s_holyKey, key, sizeof( key ) );
            ESP_LOGI( TAG, "Next religious day %d in %d days, %d months in %d ms", s_holyNext.day, s_holyNext.daysLeft, count,
                      static_cast<int>( ( esp_timer_get_time() - start ) / 1000 ) );
        }
        s_display.showHijriMonth( month, today, method, s_holyNext );
    }
    updateSky( unixTime( time ), inputs );
    Corners::Gauge gauges[Corners::CornerCount] = {};
    const Corners::Input corners = s_skyValid ? Corners::fromSky( s_skyDay ) : Corners::Input {};
    Corners::Context context = {};
    context.now = unixTime( time );
    context.utcOffsetMinutes = inputs[2];
    context.batteryPercent = batteryRead ? battery.percent : -1;
    context.charging = s_usbPower && s_pmu.charging();
    context.hijriDay = s_skyValid ? s_skyDay.hijri.day : 0;
    const bool located = inputs[0] != 0 || inputs[1] != 0;
    for ( int i = 0; i < Corners::CornerCount; ++i ) {
        if ( s_facePages.gauges[i] == FaceConfig::SunAltitudeGauge && located && !context.sunAltitudeValid ) {
            context.sunAltitude = Astronomy::sunPosition( context.now, { inputs[0] / 1e6, inputs[1] / 1e6 } ).altitude;
            context.sunAltitudeValid = true;
        }
        context.counterDays[i] = s_faceTable.counterDays[i];
        context.counterSavedDays[i] = s_faceTable.counterSavedDays[i];
        context.counterValid[i] = s_faceTable.counterNames[i][0] != '\0';
    }
    Corners::compute( corners, context, s_facePages.gauges, s_facePages.gaugeParameters, gauges );
    // The red night mode from civil dusk to civil dawn; never without a location.
    const bool red = s_hostNightRed ||
                     ( located && s_settings.value( Settings::NightMode ) != 0 && Corners::dark( corners, context.now ) );
    s_display.showCorners( gauges, Corners::night( corners, context.now ), red );
}

#if CONFIG_WATCH_SETUP_PORTAL
static void showSetup()
{
    Display::Setup setup = {};
    setup.running = s_portal.isRunning();
    setup.ssid = s_portal.ssid();
    setup.password = s_portal.password();
    setup.address = s_portal.address();
    setup.secondsLeft = CONFIG_WATCH_SETUP_PORTAL_TIMEOUT_S - s_portal.idleSeconds();
    s_display.showSetup( setup );
}

// What the setup page sent: the settings at once (and into NVS), the time at its next second.
// What the setup portal set and the pages do not show: the settings page's Setup part, and the
// place on the location-dependent pages.
static void showSetupInfo()
{
    char texts[Display::SetupInfoRows][40] = {};
    const double latitude = s_settings.value( Settings::Latitude ) / 1e6;
    const double longitude = s_settings.value( Settings::Longitude ) / 1e6;
    snprintf( texts[1], sizeof( texts[1] ), "%.2f%c %.2f%c", fabs( latitude ), latitude < 0 ? 'S' : 'N',
              fabs( longitude ), longitude < 0 ? 'W' : 'E' );
    snprintf( texts[0], sizeof( texts[0] ), "%s", s_worldClock.homePlace()[0] != '\0' ? s_worldClock.homePlace() : texts[1] );
    // The zone's last part ("Europe/Istanbul": "Istanbul"), which fits the row.
    const char *zoneName = s_worldClock.homeZoneName();
    const char *slash = strrchr( zoneName, '/' );
    snprintf( texts[2], sizeof( texts[2] ), "%s", zoneName[0] == '\0' ? Language::text( Language::Fixed ) : ( slash != nullptr ? slash + 1 : zoneName ) );
    for ( char *c = texts[2]; *c != '\0'; ++c ) {
        *c = *c == '_' ? ' ' : *c;
    }
    const Zone zone = homeZone();
    const int offset = zone.offset < 0 ? -zone.offset : zone.offset;
    int length = snprintf( texts[3], sizeof( texts[3] ), "UTC%c%d", zone.offset < 0 ? '-' : '+', offset / 60 );
    if ( offset % 60 != 0 ) {
        length += snprintf( texts[3] + length, sizeof( texts[3] ) - length, ":%02d", offset % 60 );
    }
    if ( zone.changeAt[0] != 0 ) {
        // The date of the change in the time before it.
        const time_t when = static_cast<time_t>( zone.changeAt[0] ) * 60 + zone.offset * 60;
        tm fields = {};
        gmtime_r( &when, &fields );
        snprintf( texts[4], sizeof( texts[4] ), "%d %s %02d:%02d", fields.tm_mday, Language::shortMonth( fields.tm_mon + 1 ),
                  fields.tm_hour, fields.tm_min );
    } else {
        snprintf( texts[4], sizeof( texts[4] ), "%s", Language::text( Language::None ) );
    }
    snprintf( texts[5], sizeof( texts[5] ), "%s",
              s_settings.value( Settings::HijriMethod ) == Hijri::Diyanet ? "Diyanet" : Language::text( Language::Crescent ) );
    const int setMinute = s_settings.value( Settings::RtcSetMinute );
    if ( setMinute != 0 ) {
        const time_t when = static_cast<time_t>( setMinute ) * 60 + zone.offset * 60;
        tm fields = {};
        gmtime_r( &when, &fields );
        snprintf( texts[6], sizeof( texts[6] ), "%d %s %d", fields.tm_mday, Language::shortMonth( fields.tm_mon + 1 ),
                  fields.tm_year + 1900 );
    } else {
        snprintf( texts[6], sizeof( texts[6] ), "%s", Language::text( Language::NeverSet ) );
    }
    const char *values[Display::SetupInfoRows] = {};
    for ( int i = 0; i < Display::SetupInfoRows; ++i ) {
        values[i] = texts[i];
    }
    s_display.showSetupInfo( values );
}

// The clock's rate from its error since the portal last set it: the Offset register is corrected
// by whole steps of 4.34 ppm (docs/hardware.md, PCF85063A).
static void correctRtcRate( const Portal::Received &received )
{
    const int lastSetMinute = s_settings.value( Settings::RtcSetMinute );
    int64_t errorMs = 0;
    if ( lastSetMinute == 0 || s_rtc.measureErrorMs( received.unixMs, received.atTimerUs, errorMs ) != ESP_OK ) {
        return;
    }
    const int64_t elapsedS = received.unixMs / 1000 - static_cast<int64_t>( lastSetMinute ) * 60;
    // Two days at least: the browser's time is good to about 0.1 s, under 1 ppm over two days.
    if ( elapsedS < 2 * 86400 ) {
        return;
    }
    const double ppm = errorMs * 1000.0 / elapsedS;
    const int steps = static_cast<int>( lround( ppm / 4.34 ) );
    // Far more than a crystal drifts: the time was changed some other way; keep the rate.
    if ( fabs( ppm ) > 100.0 || steps == 0 ) {
        ESP_LOGI( TAG, "RTC error %lld ms over %lld s (%.1f ppm): rate kept", errorMs, elapsedS, ppm );
        return;
    }
    const int offset = s_settings.value( Settings::RtcOffset ) + steps;
    s_settings.setValue( Settings::RtcOffset, offset < -64 ? -64 : ( offset > 63 ? 63 : offset ) );
    s_rtc.applyCalibration( s_settings.value( Settings::RtcOffset ) );
    ESP_LOGI( TAG, "RTC error %lld ms over %lld s (%.1f ppm): offset %d steps", errorMs, elapsedS, ppm,
              s_settings.value( Settings::RtcOffset ) );
}

// Set by applyPortalData when the page changed the language: the watch restarts once the portal
// has stopped, so that every page is built in it.
static bool s_languageChanged = false;

// Today's local day (days since 1970-01-01) for a counter's saved day: the browser's time when
// the page sent it, else the RTC's; 0 when neither is known.
static int localDayNow( const Portal::Received &received )
{
    int64_t utc = 0;
    if ( received.timeValid ) {
        utc = received.unixMs / 1000;
    } else {
        Rtc::DateTime time = {};
        bool stopped = false;
        if ( s_rtc.read( time, stopped ) == ESP_OK && !stopped && time.month != 0 ) {
            utc = unixTime( time );
        }
    }
    return utc > 0 ? static_cast<int>( ( utc + s_settings.value( Settings::UtcOffset ) * 60 ) / 86400 ) : 0;
}

static void applyPortalData( const Portal::Received &received )
{
    const int language = s_settings.value( Settings::Language );
    for ( int i = 0; i < Settings::ItemCount; ++i ) {
        const Settings::Item item = static_cast<Settings::Item>( i );
        if ( received.valueValid[i] && !s_settings.setValue( item, received.values[i] ) ) {
            ESP_LOGW( TAG, "Setup page: value %d refused for setting %d", received.values[i], i );
        }
    }
    s_settings.save();
    s_display.refreshSettings();
    s_languageChanged = s_settings.value( Settings::Language ) != language;
    if ( received.alarmsValid ) {
        s_alarms = received.alarms;
        AlarmStore::save( s_alarms );
        s_alarmsChanged = true;
        // New definitions (their places may have moved) wait for their next due time.
        for ( int i = 0; i < Alarms::MaxAlarms; ++i ) {
            storeHandled( i, s_rtcNow );
        }
    }
    if ( received.countersValid ) {
        s_dayCounters.set( received.counters, received.counterCount, localDayNow( received ) );
        s_display.setCounters( s_dayCounters );
        s_faceTableStale = true;
    }
    if ( received.citiesValid ) {
        s_worldClock.set( received.cities, received.cityCount, received.homeZoneName );
        s_display.setCities( s_worldClock );
        s_faceTableStale = true;
    }
    if ( received.facesValid ) {
        s_facePages = received.faces;
        FaceStore::save( s_facePages );
        // "Show on the watch": that page, on the face tile (setPages shows the page set here).
        if ( received.faceShow >= 0 && received.faceShow < s_facePages.count ) {
            s_settings.setValue( Settings::FaceStyle, received.faceShow );
        }
        s_display.setPages( s_facePages );
        if ( received.faceShow >= 0 ) {
            s_display.showFaceTile();
        }
    }
    if ( received.placeValid ) {
        s_worldClock.setHomePlace( received.place );
    }
    showSetupInfo();
    if ( received.timeValid ) {
        correctRtcRate( received );
        s_settings.setValue( Settings::RtcSetMinute, static_cast<int>( received.unixMs / 60000 ) );
        s_settings.save();
        const esp_err_t err = s_rtc.setFromUnixMs( received.unixMs, received.atTimerUs );
        ESP_LOGI( TAG, "RTC set from the setup page: %s", esp_err_to_name( err ) );
    }
}

// Setup page taps, data from the browser and the portal timeout. Returns true while the portal
// runs, which keeps active mode on.
static bool servicePortal( bool everySecond )
{
    int requests = s_display.takeRequests();
#if CONFIG_WATCH_DEV_LOGGING
    requests |= s_hostRequests;
    s_hostRequests = 0;
#endif
    if ( requests & Display::Restart ) {
        // A new language: every page is built again in it after the restart.
        ESP_LOGI( TAG, "Language changed: restart" );
        s_settings.save();
        esp_restart();
    }
    if ( requests & Display::TogglePortal ) {
        if ( s_portal.isRunning() ) {
            s_portal.stop();
        } else {
            s_portal.start( s_settings, s_worldClock, s_dayCounters, s_facePages, s_alarms );
            s_portalUsed = true;
        }
    }
    if ( requests & Display::NewPassword ) {
        s_portal.newPassword();
    }
    Portal::Received received = {};
    if ( s_portal.takeReceived( received ) ) {
        applyPortalData( received );
        // "Show on the watch" keeps the portal on for the next change.
        if ( !received.keepRunning ) {
            // Let the browser receive its answer before the access point goes.
            vTaskDelay( pdMS_TO_TICKS( 500 ) );
            s_portal.stop();
        }
        if ( s_languageChanged ) {
            ESP_LOGI( TAG, "Language changed: restart" );
            esp_restart();
        }
    }
    s_portal.setBattery( s_batteryPercent, s_usbPower );
    if ( s_portal.takeUpdated() ) {
        // The new image starts after a full restart (deep-sleep wakes skip the bootloader's check).
        ESP_LOGI( TAG, "Update written: restart" );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
        s_portal.stop();
        esp_restart();
    }
    if ( s_portal.isRunning() && s_portal.idleSeconds() >= CONFIG_WATCH_SETUP_PORTAL_TIMEOUT_S ) {
        ESP_LOGI( TAG, "Setup portal timeout" );
        s_portal.stop();
    }
    if ( requests != 0 || everySecond ) {
        showSetup();
    }
    return s_portal.isRunning();
}
#endif

// A PWR press that reached only the AXP2101: on a board without the key's pull-up resistor (R11,
// docs/hardware.md) GPIO10 stays low. Switches the internal pull-up on for good.
// `gpioSawPress`: GPIO10 rose since the last read; the AXP2101's flag then belongs to that press
// (a PWR press that mirrors the picture keeps active mode, so its flag is read afterwards).
static bool powerKeyMissed( bool &flagsCleared, bool gpioSawPress )
{
    bool shortPress = false;
    bool longPress = false;
    bool pressStart = false;
    if ( s_pmu.clearPowerKeyIrq( shortPress, longPress, pressStart ) != ESP_OK ) {
        return false;
    }
    // The first read only clears the flags left by the press that woke the watch. GPIO10 is read
    // after the AXP2101, so that a press starting between the two reads is not taken as missed.
    const bool missed = flagsCleared && pressStart && !gpioSawPress && gpio_get_level( s_powerKeyGpio ) == 0;
    flagsCleared = true;
    if ( missed && s_settings.value( Settings::PowerKeyPullUp ) == 0 ) {
        ESP_LOGW( TAG, "PWR press seen only by the AXP2101: internal pull-up on" );
        s_settings.setValue( Settings::PowerKeyPullUp, 1 );
        s_settings.save();
        s_deepSleep.setPowerKeyPullUp( true );
        s_powerKeyPullUpSwitched = true;
    }
    return missed;
}

// Active mode: the display shows the pages until the display timeout has passed since the
// start or the last touch, or until a new PWR press. The press that woke the watch may still
// be held; only a later press counts.
// Returns 1 at the display timeout, 2 at a PWR press (EventLog::ActiveEnd codes).
// The name shown while an alarm rings: the event and its offset, "Alarm", or the countdown.
static void alarmName( int which, char *text, int size )
{
    if ( which == s_countdownAlarm ) {
        snprintf( text, size, "%s", Language::text( Language::Timer ) );
        return;
    }
    const bool event = which >= 0 && which < s_alarms.count && s_alarms.alarms[which].kind == Alarms::SkyEvent;
    if ( !event ) {
        snprintf( text, size, "%s", Language::text( Language::AlarmTitle ) );
        return;
    }
    const Alarms::Definition &alarm = s_alarms.alarms[which];
    if ( alarm.offsetMinutes == 0 ) {
        snprintf( text, size, "%s", Language::alarmEvent( alarm.event ) );
    } else {
        snprintf( text, size, "%s %+d", Language::alarmEvent( alarm.event ), alarm.offsetMinutes );
    }
}

static void startRinging( int which )
{
    char name[40];
    alarmName( which, name, sizeof( name ) );
    const int64_t local = s_rtcNow + static_cast<int64_t>( Zones::offsetAt( homeZone(), s_rtcNow ) ) * 60;
    char time[8];
    snprintf( time, sizeof( time ), "%02d:%02d", static_cast<int>( local / 3600 % 24 ), static_cast<int>( local / 60 % 60 ) );
    s_display.showAlarm( name, time );
    s_ringing = s_ringer.start();
    s_ringWhich = which;
    s_ringStartUs = esp_timer_get_time();
    ESP_LOGI( TAG, "Alarm %d rings: %s, at %lld (due %lld)", which, name, static_cast<long long>( s_rtcNow ),
              static_cast<long long>( which >= 0 && which < s_dueCount ? s_handled[which] : 0 ) );
}

static void stopRinging( EventLog::AlarmEnd end )
{
    s_ringer.stop();
    s_display.hideAlarm();
    EventLog::Entry entry = {};
    entry.type = EventLog::Alarm;
    entry.code = static_cast<uint8_t>( s_ringWhich < 0 ? 0 : s_ringWhich );
    entry.touches = end;
    entry.duration = static_cast<uint16_t>( ( esp_timer_get_time() - s_ringStartUs ) / 1000000 );
    logEvent( entry, s_pmuReady );
    ESP_LOGI( TAG, "Alarm %d ended (%d) after %u s, heap min %u", s_ringWhich, end, entry.duration,
              static_cast<unsigned>( heap_caps_get_minimum_free_size( MALLOC_CAP_INTERNAL ) ) );
    s_ringing = false;
    s_ringWhich = -1;
}

// FNV-1a.
static uint32_t fnv( uint32_t hash, const void *data, int size )
{
    const uint8_t *bytes = static_cast<const uint8_t *>( data );
    for ( int i = 0; i < size; ++i ) {
        hash = ( hash ^ bytes[i] ) * 16777619u;
    }
    return hash;
}

static uint32_t handledCheck()
{
    return fnv( 2166136261u, s_handled, sizeof( s_handled ) ) ^ 0x51A4A1A5u;
}

// After a power-on RTC_NOINIT memory holds noise.
static void checkHandled()
{
    if ( s_handledCheck != handledCheck() ) {
        memset( s_handled, 0, sizeof( s_handled ) );
        s_handledCheck = handledCheck();
    }
}

static void storeHandled( int which, int64_t due )
{
    s_handled[which] = due;
    s_handledCheck = handledCheck();
}

static Alarms::Place alarmPlace()
{
    Alarms::Place place = {};
    place.observer = { s_settings.value( Settings::Latitude ) / 1e6, s_settings.value( Settings::Longitude ) / 1e6 };
    place.zone = homeZone();
    place.ishaAngle = s_settings.value( Settings::IshaAngle );
    return place;
}

// The due times, computed again when the place or the definitions changed, the clock went back,
// or a day passed (an event not found within the days searched); the countdown every second.
static void planAlarms( bool force )
{
    if ( s_rtcNow == 0 ) {
        return;
    }
    const Alarms::Place place = alarmPlace();
    const uint32_t key = fnv( Alarms::placeKey( place ), &s_alarms, sizeof( s_alarms ) );
    if ( force || key != s_plan.key || s_rtcNow < s_plan.computedAt || s_rtcNow - s_plan.computedAt >= 86400 ) {
        const int64_t start = esp_timer_get_time();
        const bool located = place.observer.latitude != 0 || place.observer.longitude != 0;
        s_plan.anyOn = false;
        for ( int i = 0; i < Alarms::MaxAlarms; ++i ) {
            s_plan.due[i] = 0;
            if ( i >= s_alarms.count || !s_alarms.alarms[i].on ) {
                continue;
            }
            s_plan.anyOn = true;
            if ( s_alarms.alarms[i].kind == Alarms::SkyEvent && !located ) {
                continue;
            }
            s_plan.due[i] = Alarms::next( s_alarms.alarms[i], Alarms::planFrom( s_rtcNow, s_handled[i] ), place );
        }
        s_plan.key = key;
        s_plan.computedAt = s_rtcNow;
        ESP_LOGI( TAG, "Alarms planned in %d ms: %lld %lld %lld %lld", static_cast<int>( ( esp_timer_get_time() - start ) / 1000 ),
                  static_cast<long long>( s_plan.due[0] ), static_cast<long long>( s_plan.due[1] ),
                  static_cast<long long>( s_plan.due[2] ), static_cast<long long>( s_plan.due[3] ) );
    }
    int64_t endMs = 0;
    bool stopwatch = false;
    if ( s_display.timers( endMs, stopwatch ) ) {
        s_timerRunning = stopwatch || endMs > 0;
        const int64_t end = endMs > 0 ? ( endMs + 999 ) / 1000 : 0;
        s_plan.due[s_countdownAlarm] = end == s_handled[s_countdownAlarm] ? 0 : end;
    }
}

static void logSkipped( int which )
{
    EventLog::Entry entry = {};
    entry.type = EventLog::Alarm;
    entry.code = static_cast<uint8_t>( which );
    entry.touches = EventLog::AlarmSkipped;
    logEvent( entry, s_pmuReady );
    ESP_LOGI( TAG, "Alarm %d skipped: more than %d s late", which, Alarms::LateSeconds );
}

// Once a second in active mode: an alarm due now starts ringing (two in the same minute ring
// once), one found too late is skipped; a ringing one ends by itself.
static void serviceAlarms()
{
    if ( s_ringing ) {
        if ( s_display.alarmTapped() ) {
            stopRinging( EventLog::AlarmTapped );
        } else if ( !s_ringer.ringing() ) {
            stopRinging( EventLog::AlarmTimedOut );
        }
        return;
    }
    // A row of the clock page's alarms part switches its alarm on or off.
    int toggled = -1;
    if ( s_display.takeAlarmToggle( toggled ) && toggled < s_alarms.count ) {
        s_alarms.alarms[toggled].on = !s_alarms.alarms[toggled].on;
        // Switched on after its time today: it waits for the next one (Alarms::planFrom).
        storeHandled( toggled, s_rtcNow );
        AlarmStore::save( s_alarms );
        s_alarmsChanged = true;
        ESP_LOGI( TAG, "Alarm %d switched %s", toggled, s_alarms.alarms[toggled].on ? "on" : "off" );
    }
    planAlarms( s_alarmsChanged );
    s_alarmsChanged = false;
    s_display.setAlarms( s_alarms, s_plan.due );
    s_display.setAlarmIcons( alarmIcons() );
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_testDue != 0 && Alarms::check( s_testDue, s_rtcNow ) == Alarms::Ring ) {
        s_testDue = 0;
        startRinging( 0 );
        return;
    }
#endif
    int ring = -1;
    bool handled = false;
    for ( int i = 0; i < s_dueCount; ++i ) {
        const Alarms::Due due = Alarms::check( s_plan.due[i], s_rtcNow, s_handled[i] );
        if ( due == Alarms::Wait ) {
            continue;
        }
        storeHandled( i, s_plan.due[i] );
        handled = true;
        if ( due == Alarms::Ring && ring < 0 ) {
            ring = i;
        } else if ( due == Alarms::Skip ) {
            logSkipped( i );
        }
        if ( i == s_countdownAlarm ) {
            s_display.finishCountdown();
        } else if ( s_alarms.alarms[i].days == 0 ) {
            // A "once" alarm switches itself off.
            s_alarms.alarms[i].on = false;
            AlarmStore::save( s_alarms );
        }
    }
    // The sound first: planning the sky events again can take a few tenths of a second.
    if ( ring >= 0 ) {
        startRinging( ring );
    }
    if ( handled ) {
        planAlarms( true );
    }
}

// Active mode without the display: the RTC's time for the alarms, as updateDisplay() reads it.
static void readRtcNow()
{
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    const bool read = s_rtc.read( time, oscillatorStopped ) == ESP_OK && !oscillatorStopped && time.month != 0;
    s_rtcNow = read ? unixTime( time ) : 0;
}

static int runActiveMode()
{
    int lastLevel = gpio_get_level( s_powerKeyGpio );
    // An input, its pull left as it is: R8 holds the line high.
    gpio_set_direction( s_bootKeyGpio, GPIO_MODE_INPUT );
    uint32_t bootHeldMs = 0;
    bool bootUsed = false;  // the press stopped an alarm or was long: no step back at its release
    ESP_LOGI( TAG, "Active mode, display timeout %d s", s_settings.value( Settings::DisplayTimeout ) );
    uint32_t activeMs = 0;
    int sinceUpdateMs = 1000;
    // The AXP2101's key flags are read after the first second (the waking press is over by then
    // or no longer starts) and then every 200 ms.
    uint32_t elapsedMs = 0;
    int sinceKeyFlagsMs = 0;
    bool keyFlagsCleared = false;
    bool gpioSawPress = false;
    uint32_t touchHeldMs = 0;
    while ( true ) {
        const bool secondTick = sinceUpdateMs >= 1000;
        const bool everySecond = s_displayOn && secondTick;
        if ( secondTick ) {
            if ( s_displayOn ) {
                updateDisplay();
            } else {
                // The display did not start: the alarms still ring (the sound needs no display).
                readRtcNow();
            }
            sinceUpdateMs = 0;
        }
        // A tap ends a ringing at once; the start waits for the second's update.
        if ( secondTick || s_ringing ) {
            serviceAlarms();
        }
        if ( s_ringing ) {
            // No display timeout while an alarm rings.
            activeMs = 0;
        }
#if CONFIG_WATCH_SETUP_PORTAL
        if ( s_displayOn && servicePortal( everySecond ) ) {
            // The display stays on while the portal runs; the timeout counts again after it.
            activeMs = 0;
        }
#endif
        const int level = gpio_get_level( s_powerKeyGpio );
        if ( level != lastLevel ) {
            lastLevel = level;
            gpioSawPress = gpioSawPress || level == 1;
            // During a held brightness drag the press mirrors the picture instead.
            const int axis = level == 1 && s_displayOn ? s_display.adjustAxis() : -1;
            if ( level == 1 && s_ringing ) {
                // PWR stops a ringing alarm; active mode goes on.
                stopRinging( EventLog::AlarmPowerKey );
                activeMs = 0;
            } else if ( axis >= 0 ) {
                s_display.toggleMirror( axis );
            } else if ( level == 1 ) {
                s_deepSleep.addEvent( DeepSleep::Event::PowerKeyPressed );
                ESP_LOGI( TAG, "PWR press: active mode ends" );
                return 2;
            }
        }
        // BOOT: a short press steps back (Display::back), at the release so that a long press is
        // none; while an alarm rings, the press stops it at once like PWR.
        if ( gpio_get_level( s_bootKeyGpio ) == 0 ) {
            bootHeldMs += 20;
            if ( bootHeldMs == s_bootPressMs && s_ringing ) {
                stopRinging( EventLog::AlarmBootKey );
                activeMs = 0;
                bootUsed = true;
            } else if ( bootHeldMs == s_bootLongMs && !bootUsed ) {
                // Reserved (freeing background pages, later).
                ESP_LOGI( TAG, "BOOT long press" );
                activeMs = 0;
                bootUsed = true;
            }
        } else {
            if ( bootHeldMs >= s_bootPressMs && !bootUsed && s_displayOn ) {
                ESP_LOGI( TAG, "BOOT press" );
                s_display.back();
                activeMs = 0;
            }
            bootHeldMs = 0;
            bootUsed = false;
        }
        if ( elapsedMs >= 1000 && sinceKeyFlagsMs >= 200 ) {
            sinceKeyFlagsMs = 0;
            const bool missed = powerKeyMissed( keyFlagsCleared, gpioSawPress );
            gpioSawPress = gpio_get_level( s_powerKeyGpio ) == 1;
            if ( missed ) {
                const int axis = s_displayOn ? s_display.adjustAxis() : -1;
                if ( s_ringing ) {
                    stopRinging( EventLog::AlarmPowerKey );
                    activeMs = 0;
                } else if ( axis >= 0 ) {
                    s_display.toggleMirror( axis );
                } else {
                    s_deepSleep.addEvent( DeepSleep::Event::PowerKeyPressed );
                    ESP_LOGI( TAG, "PWR press (AXP2101): active mode ends" );
                    return 2;
                }
            }
        }
        uint32_t idleMs = activeMs;
        if ( s_displayOn ) {
            const uint32_t touchIdleMs = s_display.inactiveMs();
            if ( touchIdleMs < idleMs ) {
                idleMs = touchIdleMs;
            }
            // A press held this long is a stuck touch controller, not a finger: it would keep
            // active mode on until the battery runs flat.
            touchHeldMs = s_display.touchPressed() ? touchHeldMs + 20 : 0;
            if ( touchHeldMs >= s_touchStuckMs && !s_ringing ) {
                ESP_LOGW( TAG, "Touch held %u s: active mode ends", static_cast<unsigned>( touchHeldMs / 1000 ) );
                return 3;
            }
        }
        // The torch keeps active mode on until its own auto-off, which ends it as a timeout does.
        const int torchLeft = s_displayOn ? s_display.torchSecondsLeft() : -1;
        if ( torchLeft == 0 ) {
            ESP_LOGI( TAG, "Torch auto-off: active mode ends" );
            return 1;
        }
        if ( torchLeft > 0 ) {
            activeMs = 0;
            idleMs = 0;
        }
        // Read every time: the settings page can change it.
        const uint32_t timeoutMs = s_settings.value( Settings::DisplayTimeout ) * 1000;
        if ( idleMs >= timeoutMs ) {
            ESP_LOGI( TAG, "Display timeout: active mode ends" );
            return 1;
        }
#if CONFIG_WATCH_DEV_LOGGING
        if ( s_hostEndActive ) {
            s_hostEndActive = false;
            ESP_LOGI( TAG, "Host command: active mode ends" );
            return 1;
        }
#endif
        if ( pollHostCommands() || ( s_usbPower && s_settings.value( Settings::UsbStayOn ) != 0 ) ) {
            activeMs = 0;
        }
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
        activeMs += 20;
        sinceUpdateMs += 20;
        elapsedMs += 20;
        sinceKeyFlagsMs += 20;
    }
}

#if CONFIG_WATCH_SLEEP_SD_LINES_LOW
// Battery run R: the SD lines have 10k pull-ups to VCC3V3 and no card is fitted.
static const gpio_num_t s_sdLines[4] = { GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_17 };

static void setSdLinesLow( bool low )
{
    for ( int i = 0; i < 4; ++i ) {
        if ( low ) {
            gpio_set_level( s_sdLines[i], 0 );
            gpio_set_direction( s_sdLines[i], GPIO_MODE_OUTPUT );
            gpio_hold_en( s_sdLines[i] );
        } else {
            gpio_hold_dis( s_sdLines[i] );
            gpio_reset_pin( s_sdLines[i] );
        }
    }
}
#endif

#if CONFIG_WATCH_SLEEP_ISOLATE_USB_PADS
// Battery run S: USB D- and D+ have no pull on the board and float while USB is unplugged.
static const gpio_num_t s_usbPads[2] = { GPIO_NUM_19, GPIO_NUM_20 };

static void isolateUsbPads( bool isolate )
{
    for ( int i = 0; i < 2; ++i ) {
        if ( isolate ) {
            rtc_gpio_isolate( s_usbPads[i] );
        } else {
            // Back to the digital pad, where the USB-Serial-JTAG PHY drives it again.
            rtc_gpio_hold_dis( s_usbPads[i] );
            rtc_gpio_deinit( s_usbPads[i] );
        }
    }
}
#endif

static void sleepNow()
{
#if CONFIG_WATCH_SLEEP_ISOLATE_USB_PADS
    ESP_LOGI( TAG, "USB pads isolated for deep sleep" );
    fflush( stdout );
    isolateUsbPads( true );
#endif
#if CONFIG_WATCH_SLEEP_SD_LINES_LOW
    setSdLinesLow( true );
    ESP_LOGI( TAG, "SD lines held low for deep sleep (calibration load)" );
#endif
#if CONFIG_WATCH_AXP_MEASURE_OFF_IN_SLEEP
    s_pmu.setMeasurement( false );
#endif
#if CONFIG_WATCH_SLEEP_AXP_SLEEP_MODE
    s_pmu.setSleepMode( true );
#endif
#if CONFIG_WATCH_SLEEP_DCDC1_MV > 0
    const esp_err_t dcdc1Err = s_pmu.setDcdc1Voltage( CONFIG_WATCH_SLEEP_DCDC1_MV );
    ESP_LOGI( TAG, "DCDC1 %d mV for deep sleep: %s", CONFIG_WATCH_SLEEP_DCDC1_MV, esp_err_to_name( dcdc1Err ) );
#endif
    s_peripherals.holdSleepLines();
    s_deepSleep.enter();
}

// Display-off state (CLAUDE.md, off mode): the AXP2101 cuts every rail except RTCLDO and the PWR
// key powers the watch on again. Battery-log builds keep deep sleep for their timer wake, and a
// failed power-off also ends in deep sleep.
static void powerOff( [[maybe_unused]] bool pmuReady, [[maybe_unused]] EventLog::PowerOffReason reason )
{
#if !CONFIG_WATCH_BATTERY_LOG
    EventLog::Entry entry = {};
    entry.type = EventLog::PowerOff;
    entry.code = reason;
    if ( s_alwaysOn.isActive() ) {
        entry.duration = static_cast<uint16_t>( s_alwaysOn.minutes() );
        entry.longestWakeMs = static_cast<uint16_t>( s_deepSleep.longestTimerWakeMs() );
    }
    logEvent( entry, pmuReady );
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        s_pmu.readBattery( millivolts, percent, vbusGood );
        ESP_LOGI( TAG, "Powering off: battery %u mV, %u %%, VBUS %d", millivolts, percent, vbusGood );
        // The USB console loses its last lines unless they are flushed before the rails fall.
        fflush( stdout );
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
        s_pmu.powerOff();
        ESP_LOGE( TAG, "Power-off failed; entering deep sleep" );
    }
#endif
    sleepNow();
}

// The power-off limit of the battery, for a reading taken without USB power. Battery-log runs
// may discharge the cell fully and never stop at the limit.
static bool batteryEmpty( uint16_t millivolts, uint8_t percent )
{
#if CONFIG_WATCH_BATTERY_LOG
    return false;
#else
    const bool empty = BatteryLimit::empty( millivolts, percent );
    if ( empty ) {
        ESP_LOGI( TAG, "Battery at the power-off limit: %u mV, %u %%", millivolts, percent );
    }
    return empty;
#endif
}

// Alarm pending: an alarm on or the countdown running keeps the watch from powering off.
static bool alarmPending()
{
    const int64_t countdown = s_plan.due[s_countdownAlarm];
    return s_plan.anyOn || ( countdown != 0 && countdown != s_handled[s_countdownAlarm] );
}

// The earliest due time, 0 when none.
static int64_t nextDue()
{
    int64_t next = 0;
    for ( int i = 0; i < s_dueCount; ++i ) {
        // A due time handled but not planned again (active mode without the display) is past.
        if ( s_plan.due[i] != 0 && s_plan.due[i] != s_handled[i] && ( next == 0 || s_plan.due[i] < next ) ) {
            next = s_plan.due[i];
        }
    }
    return next;
}

// To the pending sleep without a face, from active mode or always-on: a wake half a second later
// puts the panel to deep standby from a fresh start, as always-on's first drawing does.
static void enterPendingSleep( i2c_master_bus_handle_t bus )
{
    s_alwaysOn.end();
    s_peripherals.sleepTouch( bus );
    s_wakePurpose = WakePanelOff;
    ESP_LOGI( TAG, "Alarm pending: sleeping without the face" );
    Panel::holdPins();
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( 500 * 1000 );
}

// A timer wake of the pending sleep: the panel to deep standby (the first one), the battery,
// then sleep again, or return so that active mode starts for the due alarm.
static void pendingTick( i2c_master_bus_handle_t bus, bool pmuReady )
{
    if ( s_wakePurpose == WakePanelOff ) {
        Panel panel;
        if ( panel.open() ) {
            panel.sleep();
        }
        if ( pmuReady ) {
            s_pmu.setPanelPower( false );
        }
        s_wakePurpose = WakeAlarm;
    }
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK && !vbusGood &&
             BatteryLimit::atFloor( millivolts, percent ) ) {
            powerOff( pmuReady, EventLog::BatteryLimit );
        }
    }
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    const bool timeRead = s_rtc.init( bus ) == ESP_OK && s_rtc.read( time, oscillatorStopped ) == ESP_OK &&
                          !oscillatorStopped;
    // Without the time nothing can be scheduled: check again in an hour.
    const int64_t seconds = timeRead ? Alarms::sleepBefore( nextDue(), unixTime( time ) ) : 3600;
    if ( seconds == 0 ) {
        ESP_LOGI( TAG, "Alarm due: active mode" );
        return;
    }
    Panel::holdPins();
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( static_cast<uint64_t>( seconds ) * 1000000 );
}

// Always-on timer wake: redraw the face and sleep until the next minute; nothing else starts.
// A battery-log build with the face also records the battery here when a sample is due.
static void alwaysOnTick( i2c_master_bus_handle_t bus, bool pmuReady )
{
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    const bool timeRead = s_rtc.init( bus ) == ESP_OK && s_rtc.read( time, oscillatorStopped ) == ESP_OK;
    // For the battery gauge; at the power-off limit (and not charging) the watch switches off.
    int batteryPercent = -1;
    bool charging = false;
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK ) {
            batteryPercent = percent;
            if ( !vbusGood && batteryEmpty( millivolts, percent ) ) {
                // Below 10 % the face goes; with an alarm pending the watch sleeps on to the floor.
                if ( alarmPending() && !BatteryLimit::atFloor( millivolts, percent ) ) {
                    enterPendingSleep( bus );
                }
                powerOff( pmuReady, EventLog::BatteryLimit );
            }
            charging = vbusGood && s_pmu.charging();
            if ( timeRead ) {
                trackUnplugged( vbusGood, unixTime( time ), false );
            }
        }
    }
    // An alarm due within seconds: active mode rings it (app_main goes on from here).
    const int64_t due = nextDue();
    if ( timeRead && !oscillatorStopped && due != 0 && Alarms::sleepBefore( due, unixTime( time ) ) == 0 ) {
        ESP_LOGI( TAG, "Alarm due: always-on ends" );
        return;
    }
    const int offsetMinutes = Zones::offsetAt( s_alwaysOnZone, unixTime( time ) );
    uint64_t sleepUs = s_alwaysOn.update( Rtc::addMinutes( time, offsetMinutes ), unixTime( time ),
                                                timeRead, !oscillatorStopped, batteryPercent, charging );
    // Without the device the readings fail, which the motion check counts as no motion.
    s_motion.init( bus );
#if CONFIG_WATCH_MOTION_LOG
    s_motion.recordLog();
#endif
    const bool still = sleepUs != 0 && s_motion.stillTooLong();
    if ( pmuReady ) {
        s_batteryLog.recordIfDue( s_pmu );
    }
    if ( sleepUs == 0 || still ) {
        // No motion for the set time (or the panel or the RTC failed).
        if ( alarmPending() ) {
            enterPendingSleep( bus );
        }
        powerOff( pmuReady, still ? EventLog::NoMotion : EventLog::FaceFailed );
    }
    // Not past the next due time (a stopped oscillator's time cannot shorten it: a sleep of 0
    // would arm no timer at all).
    if ( due != 0 && timeRead && !oscillatorStopped ) {
        const uint64_t dueUs = static_cast<uint64_t>( Alarms::sleepBefore( due, unixTime( time ) ) ) * 1000000;
        if ( dueUs > 0 && dueUs < sleepUs ) {
            sleepUs = dueUs;
        }
    }
    if ( s_alwaysOnDevMode && s_alwaysOn.minutes() % s_statusEveryMinutes == 0 ) {
        EventLog::Entry entry = {};
        entry.type = EventLog::AlwaysOnStatus;
        entry.duration = static_cast<uint16_t>( s_alwaysOn.minutes() );
        entry.longestWakeMs = static_cast<uint16_t>( s_deepSleep.longestTimerWakeMs() );
        logEvent( entry, pmuReady );
    }
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( sleepUs );
}

// End of active mode: the always-on face follows. Its first drawing is done by a timer wake
// half a second later, the same path as every minute.
[[maybe_unused]] static void enterAlwaysOn( i2c_master_bus_handle_t bus, bool pmuReady )
{
    if ( !s_displayOn || s_settings.value( Settings::AlwaysOnFace ) == 0 || s_hostNoFace ) {
        if ( alarmPending() ) {
            enterPendingSleep( bus );
        }
        powerOff( pmuReady, EventLog::AlwaysOnDisabled );
    }
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK && !vbusGood &&
             batteryEmpty( millivolts, percent ) ) {
            if ( alarmPending() && !BatteryLimit::atFloor( millivolts, percent ) ) {
                enterPendingSleep( bus );
            }
            powerOff( pmuReady, EventLog::BatteryLimit );
        }
    }
    s_wakePurpose = WakeAlwaysOn;
    s_peripherals.sleepTouch( bus );
    fillLook( s_settings.value( Settings::FaceStyle ) );
    s_alwaysOn.begin( s_look );
    s_deepSleep.clearLongestTimerWake();
    s_alwaysOnDevMode = s_settings.value( Settings::DevMode ) != 0;
    s_motion.restart( s_settings.value( Settings::AutoOff ) );
    s_alwaysOnZone = homeZone();
    Panel::holdPins();
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( 500 * 1000 );
}

#if CONFIG_WATCH_SOUND_TEST
// The speaker validator (docs/hardware.md, "Speaker and alarm sound"): steady states of 15 s
// each, their number on the screen, for battery current readings with the meter and loudness
// readings with a phone's dB meter. Battery only: USB would power the board.
static lv_obj_t *s_testLabel = nullptr;
static const int s_stepMs = 15000;

static void showTestStep( const char *text )
{
    ESP_LOGI( TAG, "Sound test: %s", text );
    if ( !bsp_display_lock( 1000 ) ) {
        return;
    }
    if ( s_testLabel == nullptr ) {
        s_testLabel = lv_label_create( lv_layer_top() );
        lv_obj_set_style_text_font( s_testLabel, &jetbrains_mono_48, 0 );
        lv_obj_set_style_text_color( s_testLabel, lv_color_white(), 0 );
        lv_obj_set_style_bg_color( s_testLabel, lv_color_black(), 0 );
        lv_obj_set_style_bg_opa( s_testLabel, LV_OPA_COVER, 0 );
        lv_obj_set_style_pad_all( s_testLabel, 12, 0 );
    }
    lv_label_set_text( s_testLabel, text );
    lv_obj_center( s_testLabel );
    bsp_display_unlock();
}

static void testTone( Sound &sound, const char *text, int frequency, int volume, bool square )
{
    showTestStep( text );
    sound.setVolume( volume );
    sound.tone( frequency, s_stepMs, square );
}

#if CONFIG_WATCH_SOUND_TEST_SWEEP
// The speaker's response: tones from 500 to 4000 Hz in 100 Hz steps of 3 s at -12 dB, twice,
// the watch turned by 90 degrees between the passes (one position can favour a frequency by
// room reflections). A 1 kHz mark of 2 s first, to find the start in the recording.
static void soundSweep( Sound &sound )
{
    if ( sound.start() != ESP_OK ) {
        showTestStep( "no codec" );
        vTaskDelay( pdMS_TO_TICKS( 5000 ) );
        return;
    }
    sound.setAmplifier( true );
    sound.silence( 200 );
    sound.setVolume( 0xA7 );
    showTestStep( "mark" );
    sound.tone( 1000, 2000, false );
    sound.silence( 2000 );
    for ( int pass = 0; pass < 2; ++pass ) {
        for ( int frequency = 500; frequency <= 4000; frequency += 100 ) {
            char text[16];
            snprintf( text, sizeof( text ), "%d Hz", frequency );
            showTestStep( text );
            sound.tone( frequency, 3000, false );
        }
        if ( pass == 0 ) {
            showTestStep( "turn 90" );
            sound.silence( 10000 );
        }
    }
    showTestStep( "end" );
    sound.stop();
    vTaskDelay( pdMS_TO_TICKS( 3000 ) );
    if ( bsp_display_lock( 1000 ) ) {
        lv_obj_delete( s_testLabel );
        s_testLabel = nullptr;
        bsp_display_unlock();
    }
}
#endif

static void soundTest()
{
    // The owner prepares the meter, its bridge and the phone first; a tap starts the sequence.
    showTestStep( "tap to start" );
    const int touches = s_display.touches();
    while ( s_display.touches() == touches ) {
        vTaskDelay( pdMS_TO_TICKS( 50 ) );
    }
    Sound sound;
#if CONFIG_WATCH_SOUND_TEST_SWEEP
    soundSweep( sound );
    return;
#endif
    // Second meter run (10 A input; 2026-11-19 the 200 mA range's burden voltage browned the
    // board out at -6 dB): the loud steps, with two steps of the first run to compare.
    showTestStep( "1 base" );
    vTaskDelay( pdMS_TO_TICKS( s_stepMs ) );
    if ( sound.start() != ESP_OK ) {
        showTestStep( "no codec" );
        vTaskDelay( pdMS_TO_TICKS( 5000 ) );
        return;
    }
    sound.setAmplifier( true );
    sound.silence( 200 );
    testTone( sound, "2 2700 -12", 2700, 0xA7, false );
    testTone( sound, "3 2700 -6", 2700, 0xB3, false );
    testTone( sound, "4 2700 0dB", 2700, 0xBF, false );
    // The alarm's own sound at its loudest (Ringer: 1000/1500 Hz beeps, -6 dB).
    showTestStep( "5 alarm -6" );
    sound.setVolume( 0xB3 );
    for ( int beep = 0; beep < s_stepMs / 250; ++beep ) {
        sound.tone( beep % 2 == 0 ? 1000 : 1500, 100, false );
        sound.silence( 150 );
    }
    testTone( sound, "6 1000 -6", 1000, 0xB3, false );
    testTone( sound, "7 1500 -6", 1500, 0xB3, false );
    testTone( sound, "8 1000 -12", 1000, 0xA7, false );
    testTone( sound, "9 1500 -12", 1500, 0xA7, false );
    showTestStep( "10 off" );
    sound.stop();
    vTaskDelay( pdMS_TO_TICKS( s_stepMs ) );
    showTestStep( "end" );
    vTaskDelay( pdMS_TO_TICKS( 3000 ) );
    if ( bsp_display_lock( 1000 ) ) {
        lv_obj_delete( s_testLabel );
        s_testLabel = nullptr;
        bsp_display_unlock();
    }
}
#endif

// A PWR wake that the AXP2101 did not count as a press: a pulse on GPIO10 alone (another
// project saw 1-2 ms pulses on one board waking it, research notes 2026-11-20; the AXP2101
// debounces the key and latches REG49 bit1 at a press start, docs/hardware.md). The key must also
// be up now, so that a real press is never taken for one.
static bool phantomPowerKey()
{
    uint8_t status = 0;
    if ( s_pmu.readRegister( 0x49, status ) != ESP_OK ) {
        return false;
    }
    bool pressStart = ( status & 0x02 ) != 0;
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_hostPhantomKey ) {
        s_hostPhantomKey = false;
        pressStart = false;
    }
#endif
    return !pressStart && gpio_get_level( s_powerKeyGpio ) == 0;
}

extern "C" void app_main( void )
{
#if CONFIG_WATCH_SLEEP_ISOLATE_USB_PADS
    // Before onBoot, which waits for the USB host.
    isolateUsbPads( false );
#endif
    [[maybe_unused]] const DeepSleep::Event wake = s_deepSleep.onBoot();
#if CONFIG_WATCH_DEV_LOGGING
    if ( wake != DeepSleep::Event::WakeTimer ) {
        logPadLevels();
    }
#endif

    gpio_config_t keyConfig = {};
    keyConfig.pin_bit_mask = 1ULL << s_powerKeyGpio;
    keyConfig.mode = GPIO_MODE_INPUT;
    gpio_config( &keyConfig );

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    const bool pmuReady = s_pmu.init( bus ) == ESP_OK;
    s_pmuReady = pmuReady;
    if ( !pmuReady ) {
        ESP_LOGE( TAG, "AXP2101 init failed" );
    }
#if CONFIG_WATCH_SLEEP_AXP_SLEEP_MODE
    if ( pmuReady ) {
        s_pmu.setSleepMode( false );
    }
#endif
#if CONFIG_WATCH_SLEEP_DCDC1_MV > 0
    // 3300 mV is the power-on value of DCDC1 on this board (docs/hardware.md).
    if ( pmuReady ) {
        s_pmu.setDcdc1Voltage( 3300 );
    }
#endif
#if CONFIG_WATCH_AXP_MEASURE_OFF_IN_SLEEP
    // The ADC needs time for a fresh VBAT result; its conversion time is not in the datasheet.
    if ( pmuReady ) {
        uint16_t staleMv = 0;
        uint16_t freshMv = 0;
        uint8_t percent = 0;
        bool vbus = false;
        s_pmu.readBattery( staleMv, percent, vbus );
        s_pmu.setMeasurement( true );
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
        s_pmu.readBattery( freshMv, percent, vbus );
        ESP_LOGI( TAG, "VBAT with the ADC off: %u mV, 100 ms after switching it on: %u mV", staleMv, freshMv );
    }
#endif

    checkHandled();
    if ( wake == DeepSleep::Event::WakePowerKey && pmuReady && ( s_alwaysOn.isActive() || s_wakePurpose == WakePanelOff ||
                                                                 s_wakePurpose == WakeAlarm ) && phantomPowerKey() ) {
        // Back to the sleep it came from, as after a timer wake (they return only when an alarm
        // is due, and active mode then rings it).
        ESP_LOGW( TAG, "PWR wake without a press: back to sleep" );
        if ( s_alwaysOn.isActive() ) {
            alwaysOnTick( bus, pmuReady );
        } else {
            pendingTick( bus, pmuReady );
        }
    }
    if ( wake == DeepSleep::Event::WakeTimer ) {
        if ( s_alwaysOn.isActive() ) {
            alwaysOnTick( bus, pmuReady );
        } else if ( s_wakePurpose == WakePanelOff || s_wakePurpose == WakeAlarm ) {
            pendingTick( bus, pmuReady );
        }
#if CONFIG_WATCH_BATTERY_LOG
        if ( pmuReady ) {
            s_batteryLog.record( s_pmu );
        }
        sleepNow();
#endif
    }
#if CONFIG_WATCH_SLEEP_SD_LINES_LOW
    setSdLinesLow( false );
#endif
    // Active mode: the BSP takes over the panel pins and resets the panel and touch.
    const bool alwaysOnEnded = s_alwaysOn.isActive();
    const int alwaysOnMinutes = s_alwaysOn.minutes();
    s_alwaysOn.end();
    Panel::releasePins();
    s_peripherals.releaseSleepLines();

    // Reading USB input (host commands) needs the driver. Its writes never block: without a host
    // they return at once, and a stalled host costs at most 50 ms before output is dropped.
    usb_serial_jtag_driver_config_t usbConfig = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if ( usb_serial_jtag_driver_install( &usbConfig ) == ESP_OK ) {
        usb_serial_jtag_vfs_use_driver();
    }

    s_peripherals.quiesceOnce( bus, s_pmu );
    s_settings.load();
    s_deepSleep.setPowerKeyPullUp( s_settings.value( Settings::PowerKeyPullUp ) != 0 );
    Language::set( s_settings.value( Settings::Language ) );
#if CONFIG_WATCH_SETUP_PORTAL
    s_portal.init();
#endif
    if ( s_rtc.init( bus ) == ESP_OK ) {
        s_rtc.applyCalibration( s_settings.value( Settings::RtcOffset ) );
        s_rtc.logAtTick();
    } else {
        ESP_LOGE( TAG, "RTC init failed" );
    }
    // The event log (NVS, after Settings::load): a boot other than a deep-sleep wake, and the end
    // of the always-on face by this PWR press.
    if ( esp_reset_reason() != ESP_RST_DEEPSLEEP ) {
        EventLog::Entry entry = {};
        entry.type = EventLog::Boot;
        entry.code = static_cast<uint8_t>( esp_reset_reason() );
        entry.wakes = static_cast<uint16_t>( s_deepSleep.bootCount() );
        entry.flags = esp_core_dump_image_check() == ESP_OK ? EventLog::CoreDump : 0;
        // What powered the AXP2101 on and why it last went off: tells a PWR press from a USB plug.
        if ( pmuReady ) {
            s_pmu.readRegister( 0x20, entry.pmuPowerOn );
            s_pmu.readRegister( 0x21, entry.pmuPowerOff );
        }
        logEvent( entry, pmuReady );
    }
    if ( alwaysOnEnded ) {
        EventLog::Entry entry = {};
        entry.type = EventLog::AlwaysOnEnd;
        entry.code = wake == DeepSleep::Event::WakeTimer ? 2 : 1;
        entry.duration = static_cast<uint16_t>( alwaysOnMinutes );
        entry.longestWakeMs = static_cast<uint16_t>( s_deepSleep.longestTimerWakeMs() );
        logEvent( entry, pmuReady );
    }
#if CONFIG_WATCH_MOTION_LOG
    s_motion.printLog();
#endif
    if ( pmuReady ) {
        s_pmu.setPowerOnPress();
        s_pmu.setPowerOffVoltage( s_pmuPowerOffMillivolts );
        if ( s_pmu.setChargeCurrents() != ESP_OK ) {
            ESP_LOGE( TAG, "Charger setup failed" );
        }
        s_pmu.logStatus();
        logPowerKeyIrq();
#if CONFIG_WATCH_BATTERY_LOG && !CONFIG_WATCH_BATTERY_LOG_AOD
        // Battery runs without the face keep the panel in the deep standby of the sleep
        // preparation, as in runs A to D: nothing switches the panel off before deep sleep.
#else
        s_pmu.setPanelPower( true );
        FaceStore::load( s_facePages );
        s_display.setPages( s_facePages );
        // The spirit level and the alidade read the accelerometer while they show (powered down
        // otherwise, as between the always-on motion checks).
        s_motion.init( bus );
        s_display.setMotion( &s_motion );
#if CONFIG_WATCH_DEV_LOGGING
        if ( s_hostSkipDisplay ) {
            s_hostSkipDisplay = false;
            s_displayOn = false;
            ESP_LOGW( TAG, "Display not started (host command Y)" );
        } else {
            const int64_t now = rtcUnixTime();
            s_displayOn = s_display.start( s_settings, redAtStart( now ), resumeAtStart( now ) );
        }
#else
        const int64_t now = rtcUnixTime();
        s_displayOn = s_display.start( s_settings, redAtStart( now ), resumeAtStart( now ) );
#endif
        // A new image from the portal is kept once it has drawn its pages; a reset before this
        // returns to the previous one. The call stays out of the log macro: the release build
        // compiles ESP_LOGI out with its arguments, which had left every update unconfirmed.
        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        if ( s_displayOn && esp_ota_get_state_partition( esp_ota_get_running_partition(), &state ) == ESP_OK &&
             state == ESP_OTA_IMG_PENDING_VERIFY ) {
            const esp_err_t confirmed = esp_ota_mark_app_valid_cancel_rollback();
            if ( confirmed == ESP_OK ) {
                ESP_LOGI( TAG, "New software confirmed: %s", esp_app_get_description()->version );
            } else {
                ESP_LOGW( TAG, "New software not confirmed: %s", esp_err_to_name( confirmed ) );
            }
        }
        s_worldClock.load();
        s_display.setCities( s_worldClock );
        s_dayCounters.load();
        AlarmStore::load( s_alarms );
        s_display.setCounters( s_dayCounters );
        showSetupInfo();
#endif
        s_touchModeStart = s_peripherals.touchPowerMode( bus );
        // After a wake from always-on the BSP's reset does not always wake the touch controller
        // (silent on I2C, no touch interrupts; 2026-11-14, docs/hardware.md); a reset of our own
        // with the datasheet's start-up time does.
        if ( s_touchModeStart == 0xFF && s_peripherals.wakeTouch( bus ) ) {
            s_touchRecovered = true;
        }
#if CONFIG_WATCH_BATTERY_LOG
        s_batteryLog.record( s_pmu );
        s_batteryLog.print();
#endif
    }

#if CONFIG_WATCH_SOUND_TEST
    if ( s_displayOn ) {
        soundTest();
    }
#endif
    s_deepSleep.activeModeStarted();
    const int64_t activeStartUs = esp_timer_get_time();
    const int endCode = runActiveMode();
    // Before any sleep: a ringing alarm stopped properly (the codec to its power-down state),
    // and a change of the definitions made while it rang planned.
    if ( s_ringing ) {
        stopRinging( EventLog::AlarmActiveEnded );
    }
    planAlarms( s_alarmsChanged );
    s_alarmsChanged = false;
    logPowerKeyIrq();
    EventLog::Entry active = {};
    active.type = EventLog::ActiveEnd;
    active.code = static_cast<uint8_t>( endCode );
    active.duration = static_cast<uint16_t>( ( esp_timer_get_time() - activeStartUs ) / 1000000 );
    active.touches = static_cast<uint16_t>( s_display.touches() );
    active.touchModeStart = s_touchModeStart;
    active.touchModeEnd = s_peripherals.touchPowerMode( bus );
    active.flags = ( s_displayOn ? EventLog::DisplayStarted : 0 ) | ( s_display.touchStarted() ? EventLog::TouchStarted : 0 ) |
                   ( s_portalUsed ? EventLog::PortalUsed : 0 ) | ( s_touchRecovered ? EventLog::TouchRecovered : 0 ) |
                   ( s_powerKeyPullUpSwitched ? EventLog::PowerKeyPullUp : 0 );
#if CONFIG_WATCH_SETUP_PORTAL
    s_portal.stop();
#endif
    // No setting can change once LVGL has stopped.
    s_display.stop();
    s_activeEndedAt = rtcUnixTime();
    s_settings.save();
    logEvent( active, pmuReady );

#if CONFIG_WATCH_BATTERY_LOG && !CONFIG_WATCH_BATTERY_LOG_AOD
    // Battery-log runs without the face sleep with their logging timer instead.
    powerOff( pmuReady, EventLog::AlwaysOnDisabled );
#else
    enterAlwaysOn( bus, pmuReady );
#endif
}
