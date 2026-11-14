#include "DayCounters.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "days";
static const char *s_namespace = "days";
static const char *s_key = "counters";

void DayCounters::load()
{
    m_state = {};
    nvs_handle_t handle = 0;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) != ESP_OK ) {
        return;
    }
    State state = {};
    size_t size = sizeof( state );
    if ( nvs_get_blob( handle, s_key, &state, &size ) == ESP_OK && size == sizeof( state ) && state.count >= 0 &&
         state.count <= MaxCounters ) {
        m_state = state;
    }
    nvs_close( handle );
}

int DayCounters::count() const
{
    return m_state.count;
}

const DayCounters::Counter &DayCounters::counter( int index ) const
{
    return m_state.counters[index];
}

void DayCounters::set( const Counter *counters, int count )
{
    m_state = {};
    m_state.count = count > MaxCounters ? MaxCounters : count;
    for ( int i = 0; i < m_state.count; ++i ) {
        m_state.counters[i] = counters[i];
        m_state.counters[i].name[NameLength - 1] = '\0';
    }
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
