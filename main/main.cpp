#include <cstdio>
#include <ctime>

#include "AlwaysOn.h"
#include "Astronomy.h"
#include "Hijri.h"
#include "Sky.h"
#include "Axp2101.h"
#include "BatteryLog.h"
#include "DeepSleep.h"
#include "Display.h"
#include "Motion.h"
#include "Panel.h"
#include "Peripherals.h"
#if CONFIG_WATCH_SETUP_PORTAL
#include "Portal.h"
#endif
#include "Rtc.h"
#include "Settings.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "main";

static const gpio_num_t s_powerKeyGpio = GPIO_NUM_10;

static Axp2101 s_pmu;
static DeepSleep s_deepSleep;
static Peripherals s_peripherals;
static Rtc s_rtc;
static Display s_display;
static bool s_displayOn = false;
static AlwaysOn s_alwaysOn;
static BatteryLog s_batteryLog;
static Motion s_motion;
static Settings s_settings;
#if CONFIG_WATCH_SETUP_PORTAL
static Portal s_portal;
#endif

// The UTC offset for the always-on face, copied when active mode ends: the minute wakes do not
// read NVS.
RTC_DATA_ATTR static int s_alwaysOnOffsetMinutes;
static const int s_lowBatteryPercent = 15;
// At or below this charge (and not on USB) the watch powers off instead of keeping the always-on
// face, so that the cell still runs the RTC for days in the off state (CLAUDE.md, power
// management). The gauge reads too low for a while after a power cycle (docs/hardware.md, run D),
// so VBAT must agree before the watch switches itself off; 3.65 V is about 10 % on a Li-ion cell.
static const int s_powerOffPercent = 10;
static const uint16_t s_powerOffMillivolts = 3650;

static void logPowerKeyIrq()
{
    bool shortPress = false;
    bool longPress = false;
    if ( s_pmu.clearPowerKeyIrq( shortPress, longPress ) == ESP_OK ) {
        ESP_LOGI( TAG, "PWR key IRQ: short %d, long %d", shortPress, longPress );
    } else {
        ESP_LOGE( TAG, "PWR key IRQ read failed" );
    }
}

#if CONFIG_WATCH_DEV_LOGGING
// Set by the host commands "P" and "N": the same as a tap on the setup page's rows.
static int s_hostRequests = 0;
static char s_hostLine[32];
// The scene of the published screenshots, so that they stay the same and do not show the
// watch's location: Istanbul on 2026-06-21 at 19:19 local time (UTC+3), the local Hijri method.
// The settings are not changed, so that nothing of it reaches NVS.
static bool s_scene = false;
static const int s_sceneInputs[4] = { 41008200, 28978400, 180, 0 };
static const Rtc::DateTime s_sceneTime = { 2026, 6, 21, 16, 19, 0 };
static int s_hostLineLength = 0;

// Development commands from the host: the guided test sends the UTC time as
// "T YYYY-MM-DD HH:MM:SS", timed so that the RTC's next second matches the host's;
// tools/screenshot.py sends "S <page>" for a screenshot of that page and "A <style>" for one of
// the always-on face in that style (at 19:19); "P" switches the setup
// portal on or off and "N" makes a new portal password (so that a published screenshot does not
// show the one in use). "D" shows a fixed scene until the next reset (s_sceneInputs); "F <style>"
// shows the watch face in that style without changing the setting.
// Returns true when a command arrived: like a touch, it restarts the display timeout, so that
// the watch stays in active mode while tools/screenshot.py works.
static bool pollHostCommands()
{
    bool received = false;
    char c = 0;
    while ( usb_serial_jtag_read_bytes( &c, 1, 0 ) == 1 ) {
        if ( c != '\n' ) {
            if ( s_hostLineLength < static_cast<int>( sizeof( s_hostLine ) ) - 1 ) {
                s_hostLine[s_hostLineLength++] = c;
            }
            continue;
        }
        s_hostLine[s_hostLineLength] = '\0';
        s_hostLineLength = 0;
        received = true;
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
            AlwaysOn::capture( style );
            continue;
        }
        if ( sscanf( s_hostLine, "F %d", &style ) == 1 ) {
            s_display.showFaceStyle( style );
            continue;
        }
        if ( s_hostLine[0] == 'N' && s_hostLine[1] == '\0' ) {
            s_hostRequests |= Display::NewPassword;
            continue;
        }
        if ( s_hostLine[0] == 'D' && s_hostLine[1] == '\0' ) {
            s_scene = true;
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
    }
    return received;
}
#endif

// Deep-sleep floor check: a line that a chip holds low in sleep is still low right after the
// wake. Read before the BSP configures the I2C pads. I2C, touch INT and RTC INT have pull-ups to
// VCC3V3, so a low level there means current through the pull-up while sleeping.
static void logPadLevels()
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
// changes (s_skyInputs: location, UTC offset, Hijri method).
RTC_DATA_ATTR static Sky::Day s_skyDay;
RTC_DATA_ATTR static int s_skyInputs[4];
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

// `inputs`: latitude and longitude (micro-degrees), UTC offset (minutes), Hijri method.
static void updateSky( int64_t now, const int inputs[4] )
{
    const int offsetMinutes = inputs[2];
    const Astronomy::Observer observer = { inputs[0] / 1e6, inputs[1] / 1e6 };
    const int day = static_cast<int>( ( now + offsetMinutes * 60 ) / 86400 );
    bool same = true;
    for ( int i = 0; i < 4; ++i ) {
        same = same && s_skyInputs[i] == inputs[i];
    }
    if ( !s_skyValid || s_skyDay.day != day || !same ) {
        for ( int i = 0; i < 4; ++i ) {
            s_skyInputs[i] = inputs[i];
        }
        const int64_t start = esp_timer_get_time();
        s_skyDay = Sky::computeDay( day, offsetMinutes, observer, static_cast<Hijri::Method>( inputs[3] ) );
        s_skyValid = true;
        ESP_LOGI( TAG, "Sky day computed in %d ms", static_cast<int>( ( esp_timer_get_time() - start ) / 1000 ) );
    }
    s_display.showSky( s_skyDay, Sky::computeNow( now, s_skyDay, observer ), offsetMinutes );
}

static void updateDisplay()
{
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    Display::Battery battery = {};
    s_rtc.read( time, oscillatorStopped );
    s_pmu.readBattery( battery.millivolts, battery.percent, battery.vbusGood );
    battery.chargeState = s_pmu.chargeState();
    int inputs[4] = { s_settings.value( Settings::Latitude ), s_settings.value( Settings::Longitude ),
                      s_settings.value( Settings::UtcOffset ), s_settings.value( Settings::HijriMethod ) };
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_scene ) {
        time = s_sceneTime;
        for ( int i = 0; i < 4; ++i ) {
            inputs[i] = s_sceneInputs[i];
        }
    }
#endif
    s_display.update( Rtc::addMinutes( time, inputs[2] ), inputs[2], battery );
    updateSky( unixTime( time ), inputs );
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
static void applyPortalData( const Portal::Received &received )
{
    for ( int i = 0; i < Settings::ItemCount; ++i ) {
        const Settings::Item item = static_cast<Settings::Item>( i );
        if ( received.valueValid[i] && !s_settings.setValue( item, received.values[i] ) ) {
            ESP_LOGW( TAG, "Setup page: value %d refused for setting %d", received.values[i], i );
        }
    }
    s_settings.save();
    s_display.refreshSettings();
    if ( received.timeValid ) {
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
    if ( requests & Display::TogglePortal ) {
        if ( s_portal.isRunning() ) {
            s_portal.stop();
        } else {
            s_portal.start( s_settings );
        }
    }
    if ( requests & Display::NewPassword ) {
        s_portal.newPassword();
    }
    Portal::Received received = {};
    if ( s_portal.takeReceived( received ) ) {
        applyPortalData( received );
        // Let the browser receive its answer before the access point goes.
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
        s_portal.stop();
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

// Active mode: the display shows the pages until the display timeout has passed since the
// start or the last touch, or until a new PWR press. The press that woke the watch may still
// be held; only a later press counts.
static void runActiveMode()
{
    int lastLevel = gpio_get_level( s_powerKeyGpio );
    ESP_LOGI( TAG, "Active mode, display timeout %d s", s_settings.value( Settings::DisplayTimeout ) );
    uint32_t activeMs = 0;
    int sinceUpdateMs = 1000;
    while ( true ) {
        const bool everySecond = s_displayOn && sinceUpdateMs >= 1000;
        if ( everySecond ) {
            updateDisplay();
            sinceUpdateMs = 0;
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
            if ( level == 1 ) {
                s_deepSleep.addEvent( DeepSleep::Event::PowerKeyPressed );
                ESP_LOGI( TAG, "PWR press: active mode ends" );
                return;
            }
        }
        uint32_t idleMs = activeMs;
        if ( s_displayOn ) {
            const uint32_t touchIdleMs = s_display.inactiveMs();
            if ( touchIdleMs < idleMs ) {
                idleMs = touchIdleMs;
            }
        }
        // Read every time: the settings page can change it.
        const uint32_t timeoutMs = s_settings.value( Settings::DisplayTimeout ) * 1000;
        if ( idleMs >= timeoutMs ) {
            ESP_LOGI( TAG, "Display timeout: active mode ends" );
            return;
        }
#if CONFIG_WATCH_DEV_LOGGING
        if ( pollHostCommands() ) {
            activeMs = 0;
        }
#endif
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
        activeMs += 20;
        sinceUpdateMs += 20;
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
static void powerOff( [[maybe_unused]] bool pmuReady )
{
#if !CONFIG_WATCH_BATTERY_LOG
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
    const bool empty = percent <= s_powerOffPercent && millivolts <= s_powerOffMillivolts;
    if ( empty ) {
        ESP_LOGI( TAG, "Battery at the power-off limit: %u mV, %u %%", millivolts, percent );
    }
    return empty;
#endif
}

// Always-on timer wake: redraw the face and sleep until the next minute; nothing else starts.
// A battery-log build with the face also records the battery here when a sample is due.
static void alwaysOnTick( i2c_master_bus_handle_t bus, bool pmuReady )
{
    Rtc::DateTime time = {};
    bool oscillatorStopped = false;
    const bool timeRead = s_rtc.init( bus ) == ESP_OK && s_rtc.read( time, oscillatorStopped ) == ESP_OK;
    // The low-battery mark: below s_lowBatteryPercent and not charging.
    bool batteryLow = false;
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK && !vbusGood ) {
            batteryLow = percent < s_lowBatteryPercent;
            if ( batteryEmpty( millivolts, percent ) ) {
                powerOff( pmuReady );
            }
        }
    }
    const uint64_t sleepUs =
        s_alwaysOn.update( Rtc::addMinutes( time, s_alwaysOnOffsetMinutes ), timeRead, batteryLow );
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
        powerOff( pmuReady );
    }
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( sleepUs );
}

// End of active mode: the always-on face follows. Its first drawing is done by a timer wake
// half a second later, the same path as every minute.
[[maybe_unused]] static void enterAlwaysOn( i2c_master_bus_handle_t bus, bool pmuReady )
{
    if ( !s_displayOn || s_settings.value( Settings::AlwaysOnFace ) == 0 ) {
        powerOff( pmuReady );
    }
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        if ( s_pmu.readBattery( millivolts, percent, vbusGood ) == ESP_OK && !vbusGood &&
             batteryEmpty( millivolts, percent ) ) {
            powerOff( pmuReady );
        }
    }
    s_peripherals.sleepTouch( bus );
    AlwaysOn::Look look = {};
    look.style = s_settings.value( Settings::AodStyle );
    look.items = s_settings.value( Settings::AodItems );
    if ( s_skyValid ) {
        look.moonPercent = static_cast<int>( s_skyDay.phase.illumination * 100 + 0.5 );
        look.waxing = s_skyDay.phase.elongation < 180;
    }
    s_alwaysOn.begin( look );
    s_motion.restart( s_settings.value( Settings::AutoOff ) );
    s_alwaysOnOffsetMinutes = s_settings.value( Settings::UtcOffset );
    Panel::holdPins();
    s_peripherals.holdSleepLines();
    s_deepSleep.enter( 500 * 1000 );
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

    if ( wake == DeepSleep::Event::WakeTimer ) {
        if ( s_alwaysOn.isActive() ) {
            alwaysOnTick( bus, pmuReady );
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
    s_alwaysOn.end();
    Panel::releasePins();
    s_peripherals.releaseSleepLines();

#if CONFIG_WATCH_DEV_LOGGING
    // Reading USB input needs the driver. Its writes never block: without a host they return at
    // once, and a stalled host costs at most 50 ms before output is dropped.
    usb_serial_jtag_driver_config_t usbConfig = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if ( usb_serial_jtag_driver_install( &usbConfig ) == ESP_OK ) {
        usb_serial_jtag_vfs_use_driver();
    }
#endif

    s_peripherals.quiesceOnce( bus, s_pmu );
    s_settings.load();
#if CONFIG_WATCH_SETUP_PORTAL
    s_portal.init();
#endif
    if ( s_rtc.init( bus ) == ESP_OK ) {
        s_rtc.applyCalibration();
        s_rtc.logAtTick();
    } else {
        ESP_LOGE( TAG, "RTC init failed" );
    }
#if CONFIG_WATCH_MOTION_LOG
    s_motion.printLog();
#endif
    if ( pmuReady ) {
        s_pmu.setPowerOnPress();
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
        s_displayOn = s_display.start( s_settings );
#endif
#if CONFIG_WATCH_BATTERY_LOG
        s_batteryLog.record( s_pmu );
        s_batteryLog.print();
#endif
    }

    runActiveMode();
    logPowerKeyIrq();
#if CONFIG_WATCH_SETUP_PORTAL
    s_portal.stop();
#endif
    // No setting can change once LVGL has stopped.
    s_display.stop();
    s_settings.save();

#if CONFIG_WATCH_BATTERY_LOG && !CONFIG_WATCH_BATTERY_LOG_AOD
    // Battery-log runs without the face sleep with their logging timer instead.
    powerOff( pmuReady );
#else
    enterAlwaysOn( bus, pmuReady );
#endif
}
