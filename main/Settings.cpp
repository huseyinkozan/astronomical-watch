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
    { "brightness", CONFIG_WATCH_DISPLAY_BRIGHTNESS, 3, { 20, 50, 100 }, 0, 0 },
    { "auto_off_min", CONFIG_WATCH_MOTION_OFF_MIN, 4, { 20, 30, 60, 0 }, 0, 0 },
    { "utc_offset_min", 0, 0, {}, -720, 840 },
    { "latitude_ud", 0, 0, {}, -90000000, 90000000 },
    { "longitude_ud", 0, 0, {}, -180000000, 180000000 },
    { "hijri_method", 0, 2, { 0, 1 }, 0, 0 },
    { "face_style", 0, 0, {}, 0, 2 },
    { "aod_style", 0, 0, {}, 0, 13 },
    { "face_items", 15, 0, {}, 0, 15 },
    { "aod_items", 7, 0, {}, 0, 7 },
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
    ESP_LOGI( TAG, "Settings: timeout %d s, always-on %d, brightness %d %%, auto off %d min, face %d, always-on "
              "style %d, UTC offset %d min, location %d, %d (micro-degrees), Hijri method %d, face items %d, "
              "always-on items %d", m_values[DisplayTimeout], m_values[AlwaysOnFace], m_values[Brightness],
              m_values[AutoOff], m_values[FaceStyle], m_values[AodStyle], m_values[UtcOffset], m_values[Latitude],
              m_values[Longitude], m_values[HijriMethod], m_values[FaceItems], m_values[AodItems] );
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
