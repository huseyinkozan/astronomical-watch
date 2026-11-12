#include "Settings.h"

#include <cstdint>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *s_namespace = "settings";

#if CONFIG_WATCH_ALWAYS_ON
static const int s_alwaysOnDefault = 1;
#else
static const int s_alwaysOnDefault = 0;
#endif

struct ItemInfo
{
    const char *key;  // NVS key, at most 15 characters
    int defaultValue;
    int count;
    int values[5];
};

// The auto-off times are multiples of the motion check interval (10 min by default), which
// decides when the time is reached.
static const ItemInfo s_items[Settings::ItemCount] = {
    { "timeout_s", CONFIG_WATCH_DISPLAY_TIMEOUT_S, 5, { 5, 10, 20, 30, 60 } },
    { "always_on", s_alwaysOnDefault, 2, { 1, 0 } },
    { "brightness", CONFIG_WATCH_DISPLAY_BRIGHTNESS, 3, { 20, 50, 100 } },
    { "auto_off_min", CONFIG_WATCH_MOTION_OFF_MIN, 4, { 20, 30, 60, 0 } },
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

void Settings::load()
{
    for ( int i = 0; i < ItemCount; ++i ) {
        m_values[i] = s_items[i].defaultValue;
    }
#if !CONFIG_WATCH_TEST_SETTINGS
    esp_err_t err = nvs_flash_init();
    if ( err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND ) {
        // The partition holds nothing but these settings; starting over keeps the defaults.
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    nvs_handle_t handle = 0;
    if ( err == ESP_OK ) {
        err = nvs_open( s_namespace, NVS_READONLY, &handle );
    }
    if ( err == ESP_OK ) {
        for ( int i = 0; i < ItemCount; ++i ) {
            int32_t saved = 0;
            if ( nvs_get_i32( handle, s_items[i].key, &saved ) == ESP_OK && indexOf( s_items[i], saved ) >= 0 ) {
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
    ESP_LOGI( TAG, "Settings: timeout %d s, always-on %d, brightness %d %%, auto off %d min", m_values[DisplayTimeout],
              m_values[AlwaysOnFace], m_values[Brightness], m_values[AutoOff] );
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

int Settings::value( Item item ) const
{
    return m_values[item];
}
