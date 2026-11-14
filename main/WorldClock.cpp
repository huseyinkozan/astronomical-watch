#include "WorldClock.h"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "world";
static const char *s_namespace = "world";
static const char *s_key = "cities";
static const char *s_placeKey = "place";

void WorldClock::load()
{
    m_state = {};
    nvs_handle_t handle = 0;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) != ESP_OK ) {
        return;
    }
    State state = {};
    size_t size = sizeof( state );
    if ( nvs_get_blob( handle, s_key, &state, &size ) == ESP_OK && size == sizeof( state ) && state.count >= 0 &&
         state.count <= MaxCities ) {
        m_state = state;
    }
    size = sizeof( m_place );
    if ( nvs_get_str( handle, s_placeKey, m_place, &size ) != ESP_OK ) {
        m_place[0] = '\0';
    }
    nvs_close( handle );
}

void WorldClock::save()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, &m_state, sizeof( m_state ) );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}

int WorldClock::count() const
{
    return m_state.count;
}

const WorldClock::City &WorldClock::city( int index ) const
{
    return m_state.cities[index];
}

const char *WorldClock::homeZoneName() const
{
    return m_state.homeZoneName;
}

const char *WorldClock::homePlace() const
{
    return m_place;
}

void WorldClock::setHomePlace( const char *place )
{
    strlcpy( m_place, place, sizeof( m_place ) );
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_str( handle, s_placeKey, m_place );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}

void WorldClock::set( const City *cities, int count, const char *homeZoneName )
{
    m_state = {};
    m_state.count = count > MaxCities ? MaxCities : count;
    for ( int i = 0; i < m_state.count; ++i ) {
        m_state.cities[i] = cities[i];
        m_state.cities[i].name[NameLength - 1] = '\0';
        m_state.cities[i].zoneName[ZoneNameLength - 1] = '\0';
    }
    strncpy( m_state.homeZoneName, homeZoneName, ZoneNameLength - 1 );
    save();
}

void WorldClock::advance( int64_t utcSeconds )
{
    bool changed = false;
    for ( int i = 0; i < m_state.count; ++i ) {
        changed = Zones::advance( m_state.cities[i].zone, utcSeconds ) || changed;
    }
    if ( changed ) {
        save();
    }
}
