#include "Settings.h"

#include <cstdint>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
[[maybe_unused]] static const char *s_namespace = "settings";

#if CONFIG_WATCH_ALWAYS_ON
static const int s_alwaysOnDefault = 1;
#else
static const int s_alwaysOnDefault = 0;
#endif

// An item either has a list of values (count > 0) or takes any value from minimum to maximum.
struct ItemInfo
{
    const char *key;  // NVS key, at most 15 characters
    int defaultValue;
    int count;
    int values[5];
    int minimum;
    int maximum;
};

// The auto-off times are multiples of the motion check interval (10 min by default), which
// decides when the time is reached. UTC offsets run from -12:00 to +14:00.
static const ItemInfo s_items[Settings::ItemCount] = {
    { "timeout_s", CONFIG_WATCH_DISPLAY_TIMEOUT_S, 5, { 5, 10, 20, 30, 60 }, 0, 0 },
    { "always_on", s_alwaysOnDefault, 2, { 1, 0 }, 0, 0 },
    { "dev_mode", 0, 2, { 0, 1 }, 0, 0 },
    { "auto_off_min", CONFIG_WATCH_MOTION_OFF_MIN, 4, { 20, 30, 60, 0 }, 0, 0 },
    { "usb_stay_on", 1, 2, { 1, 0 }, 0, 0 },
    { "language", 0, 2, { 0, 1 }, 0, 0 },
    { "night_mode", 1, 2, { 1, 0 }, 0, 0 },
    { "boot_counter", 1, 2, { 1, 0 }, 0, 0 },
    { "utc_offset_min", 0, 0, {}, -720, 840 },
    { "latitude_ud", 0, 0, {}, -90000000, 90000000 },
    { "longitude_ud", 0, 0, {}, -180000000, 180000000 },
    { "hijri_method", 0, 2, { 0, 1 }, 0, 0 },
    { "isha_angle", 17, 2, { 17, 16 }, 0, 0 },
    { "face_style", 0, 0, {}, 0, 7 },
    { "brightness", CONFIG_WATCH_DISPLAY_BRIGHTNESS, 0, {}, 5, 100 },
    { "aod_day_pct", CONFIG_WATCH_AOD_DAY_BRIGHTNESS, 0, {}, 1, 100 },
    { "aod_night_pct", CONFIG_WATCH_AOD_NIGHT_BRIGHTNESS, 0, {}, 1, 100 },
    { "home_chg1", 0, 0, {}, 0, 70000000 },
    { "home_aft1", 0, 0, {}, -720, 840 },
    { "home_chg2", 0, 0, {}, 0, 70000000 },
    { "home_aft2", 0, 0, {}, -720, 840 },
    // -10: measured on the first board (docs/hardware.md, PCF85063A); the portal corrects it.
    { "rtc_offset", -10, 0, {}, -64, 63 },
    { "rtc_set_min", 0, 0, {}, 0, 70000000 },
    { "clock_order", 0, 0, {}, 0, 16777215 },
    { "clock_hidden", 0, 0, {}, 0, 255 },
    // The information page lost its battery and about parts to the settings page and got the tide
    // (2026-11-23): new keys, so that an order or mask saved for the old parts is not read; the
    // tide after the Moon and hidden (Display::InfoPart).
    { "info_order2", 0 | 1 << 3 | 2 << 6 | 3 << 9 | 6 << 12 | 4 << 15 | 5 << 18, 0, {}, 0, 16777215 },
    { "info_hidden2", 1 << 6, 0, {}, 0, 255 },
    // The battery and the about parts before the factory reset (Display.cpp, SettingsPart); a new
    // key, so that an order saved for the six parts before them gives way to this default.
#if CONFIG_WATCH_SETUP_PORTAL
    { "set_order2", 0 | 1 << 3 | 2 << 6 | 3 << 9 | 4 << 12 | 6 << 15 | 7 << 18 | 5 << 21, 0, {}, 0, 16777215 },
#else
    { "set_order2", 0 | 1 << 3 | 2 << 6 | 4 << 9 | 5 << 12 | 3 << 15, 0, {}, 0, 16777215 },
#endif
    { "pwr_pullup", 0, 2, { 0, 1 }, 0, 0 },
    { "unplug_min", 0, 0, {}, 0, 70000000 },
    { "set_hidden", 0, 0, {}, 0, 255 },
    { "mirror", 0, 0, {}, 0, 3 },
    { "plan_order", 0, 0, {}, 0, 16777215 },
    { "plan_hidden", 0, 0, {}, 0, 255 },
    // Clock, information, tools, observing, planets, settings (Display.cpp, Page: the pages added
    // later have the higher numbers, so that saved orders keep their places).
    { "pages_order", 0 | 1 << 3 | 4 << 6 | 5 << 9 | 2 << 12 | 3 << 15, 0, {}, 0, 16777215 },
    // Torch, calculator, unit converter, spirit level, counter, random (ToolPages::Part: the parts
    // added later have the higher numbers), then the metronome and the emergency card (PartOrder
    // puts them last); the metronome hidden, the card hidden by Display while it is empty. A mask
    // saved before keeps its value, the metronome shown.
    { "tools_order", 0 | 3 << 3 | 4 << 6 | 5 << 9 | 1 << 12 | 2 << 15, 0, {}, 0, 16777215 },
    { "tools_hidden", 1 << 6, 0, {}, 0, 255 },
    // Polar alignment, Milky Way window, deep-sky finder, eyepieces, night photo, nautical
    // almanac, astrolabe, aiming (ObservingPages::Part); the almanac and aiming hidden. Both
    // defaults changed when those two parts came: an order or mask saved before keeps its
    // values (PartOrder puts the new parts last, shown), the defaults were never saved.
    { "obs_order", 1 | 2 << 3 | 3 << 6 | 4 << 9 | 5 << 12 | 6 << 15 | 0 << 18 | 7 << 21, 0, {}, 0, 16777215 },
    { "obs_hidden", 1 << 6 | 1 << 7, 0, {}, 0, 255 },
    { "counter", 0, 0, {}, 0, 999999 },
    { "counter_target", 0, 0, {}, 0, 4 },
    { "counter_start", 0, 0, {}, 0, 70000000 },
    // A 1000 mm f/8 telescope with a 25 and a 10 mm eyepiece of 52 degrees (Plossls), a 24 mm f/2.8
    // lens on APS-C: shown before the portal sets them.
    { "scope_focal", 1000, 0, {}, 100, 6000 },
    { "scope_apert", 125, 0, {}, 30, 600 },
    { "ep1_focal", 250, 0, {}, 0, 1000 },
    { "ep1_field", 52, 0, {}, 30, 120 },
    { "ep2_focal", 100, 0, {}, 0, 1000 },
    { "ep2_field", 52, 0, {}, 30, 120 },
    { "ep3_focal", 0, 0, {}, 0, 1000 },
    { "ep3_field", 52, 0, {}, 30, 120 },
    { "ep4_focal", 0, 0, {}, 0, 1000 },
    { "ep4_field", 52, 0, {}, 30, 120 },
    { "cam_focal", 24, 0, {}, 4, 800 },
    { "cam_fnumber", 28, 0, {}, 10, 220 },
    { "cam_sensor", 1, 0, {}, 0, 4 },
    { "base_shutter", 7, 0, {}, 0, 18 },
    { "nd_stops", 10, 0, {}, 0, 20 },
    { "trail_min", 60, 0, {}, 1, 600 },
    { "trail_int", 30, 0, {}, 1, 600 },
    // Türksat 42.0 degrees east, one of the presets: no own entry until the portal sets another.
    { "sat_lon", 420, 0, {}, -1800, 1800 },
    // The cycle page (CyclePages::Part): every part hidden, so that the page stays out of the
    // launcher until a woman shows it in the portal; the Hayız part is chosen there too.
    { "cycle_order", 0, 0, {}, 0, 16777215 },
    { "cycle_hidden", 0x7F, 0, {}, 0, 255 },
    // Off until the portal sets the place's lunitidal interval (up to half a lunar day).
    { "tide_min", -1, 0, {}, -1, 745 },
};

static int indexOf( const ItemInfo &info, int value )
{
    for ( int i = 0; i < info.count; ++i ) {
        if ( info.values[i] == value ) {
            return i;
        }
    }
    return -1;
}

static bool isValid( const ItemInfo &info, int value )
{
    if ( info.count > 0 ) {
        return indexOf( info, value ) >= 0;
    }
    return value >= info.minimum && value <= info.maximum;
}

void Settings::load()
{
    for ( int i = 0; i < ItemCount; ++i ) {
        m_values[i] = s_items[i].defaultValue;
    }
    // Also in test builds: the Wi-Fi driver and the portal password need NVS.
    esp_err_t err = nvs_flash_init();
    if ( err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND ) {
        // The partition holds nothing but settings; starting over keeps the defaults.
        nvs_flash_erase();
        err = nvs_flash_init();
    }
#if !CONFIG_WATCH_TEST_SETTINGS
    nvs_handle_t handle = 0;
    if ( err == ESP_OK ) {
        err = nvs_open( s_namespace, NVS_READONLY, &handle );
    }
    if ( err == ESP_OK ) {
        for ( int i = 0; i < ItemCount; ++i ) {
            int32_t saved = 0;
            if ( nvs_get_i32( handle, s_items[i].key, &saved ) == ESP_OK && isValid( s_items[i], saved ) ) {
                m_values[i] = saved;
            }
        }
        nvs_close( handle );
    } else if ( err != ESP_ERR_NVS_NOT_FOUND ) {
        // NOT_FOUND: nothing saved yet.
        ESP_LOGE( TAG, "Settings read failed: %s", esp_err_to_name( err ) );
    }
#endif
    for ( int i = 0; i < ItemCount; ++i ) {
        m_savedValues[i] = m_values[i];
    }
    ESP_LOGI( TAG, "Settings: timeout %d s, always-on %d, brightness %d %%, auto off %d min, face page %d, UTC "
              "offset %d min, location %d, %d (micro-degrees), Hijri method %d, always-on brightness %d %% by "
              "day, %d %% by night, development mode %d",
              m_values[DisplayTimeout], m_values[AlwaysOnFace], m_values[Brightness],
              m_values[AutoOff], m_values[FaceStyle], m_values[UtcOffset], m_values[Latitude],
              m_values[Longitude], m_values[HijriMethod], m_values[AodDayBrightness], m_values[AodNightBrightness],
              m_values[DevMode] );
}

void Settings::factoryReset()
{
#if !CONFIG_WATCH_TEST_SETTINGS
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_erase_all( handle );
        if ( err == ESP_OK ) {
            err = nvs_set_i32( handle, s_items[RtcOffset].key, m_values[RtcOffset] );
        }
        if ( err == ESP_OK ) {
            err = nvs_set_i32( handle, s_items[RtcSetMinute].key, m_values[RtcSetMinute] );
        }
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    ESP_LOGW( TAG, "Settings reset, RTC correction kept: %s", esp_err_to_name( err ) );
#endif
}

void Settings::save()
{
#if !CONFIG_WATCH_TEST_SETTINGS
    int changed = 0;
    for ( int i = 0; i < ItemCount; ++i ) {
        if ( m_values[i] != m_savedValues[i] ) {
            ++changed;
        }
    }
    if ( changed == 0 ) {
        return;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Settings save failed: %s", esp_err_to_name( err ) );
        return;
    }
    for ( int i = 0; i < ItemCount && err == ESP_OK; ++i ) {
        const int value = m_values[i];
        if ( value != m_savedValues[i] ) {
            err = nvs_set_i32( handle, s_items[i].key, value );
            m_savedValues[i] = value;
        }
    }
    if ( err == ESP_OK ) {
        err = nvs_commit( handle );
    }
    nvs_close( handle );
    ESP_LOGI( TAG, "Settings saved (%d changed): %s", changed, esp_err_to_name( err ) );
#endif
}

void Settings::next( Item item )
{
    const ItemInfo &info = s_items[item];
    // A test-build value outside the list steps to the first value.
    const int index = indexOf( info, m_values[item] ) + 1;
    m_values[item] = info.values[index < info.count ? index : 0];
}

bool Settings::setValue( Item item, int value )
{
    if ( !isValid( s_items[item], value ) ) {
        return false;
    }
    m_values[item] = value;
    return true;
}

int Settings::value( Item item ) const
{
    return m_values[item];
}
