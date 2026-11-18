#include "DayCounters.h"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "days";
static const char *s_namespace = "days";
static const char *s_key = "counters";
static const char *s_savedKey = "saved";

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
    int saved[MaxCounters] = {};
    size = sizeof( saved );
    if ( nvs_get_blob( handle, s_savedKey, saved, &size ) == ESP_OK && size == sizeof( saved ) ) {
        for ( int i = 0; i < MaxCounters; ++i ) {
            m_savedDays[i] = saved[i];
        }
    } else {
        for ( int i = 0; i < MaxCounters; ++i ) {
            m_savedDays[i] = 0;
        }
    }
    nvs_close( handle );
}

int DayCounters::savedDay( int index ) const
{
    return index >= 0 && index < MaxCounters ? m_savedDays[index] : 0;
}

int DayCounters::count() const
{
    return m_state.count;
}

const DayCounters::Counter &DayCounters::counter( int index ) const
{
    return m_state.counters[index];
}

void DayCounters::set( const Counter *counters, int count, int today )
{
    const State old = m_state;
    int oldSaved[MaxCounters] = {};
    for ( int i = 0; i < MaxCounters; ++i ) {
        oldSaved[i] = m_savedDays[i];
    }
    m_state = {};
    m_state.count = count > MaxCounters ? MaxCounters : count;
    for ( int i = 0; i < m_state.count; ++i ) {
        m_state.counters[i] = counters[i];
        m_state.counters[i].name[NameLength - 1] = '\0';
        m_savedDays[i] = today;
        for ( int k = 0; k < old.count; ++k ) {
            if ( old.counters[k].day == m_state.counters[i].day &&
                 strcmp( old.counters[k].name, m_state.counters[i].name ) == 0 ) {
                m_savedDays[i] = oldSaved[k];
            }
        }
    }
    for ( int i = m_state.count; i < MaxCounters; ++i ) {
        m_savedDays[i] = 0;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, &m_state, sizeof( m_state ) );
        if ( err == ESP_OK ) {
            err = nvs_set_blob( handle, s_savedKey, m_savedDays, sizeof( m_savedDays ) );
        }
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}
