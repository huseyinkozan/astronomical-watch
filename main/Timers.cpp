#include "Timers.h"

#include "Regatta.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "timers";
static const char *s_namespace = "timers";
static const char *s_key = "state";

void Timers::load()
{
    m_state = {};
    nvs_handle_t handle = 0;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) != ESP_OK ) {
        return;
    }
    State state = {};
    size_t size = sizeof( state );
    // A blob of another size comes from another layout of State: start at zero.
    if ( nvs_get_blob( handle, s_key, &state, &size ) == ESP_OK && size == sizeof( state ) ) {
        m_state = state;
    }
    nvs_close( handle );
}

void Timers::save()
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

const Timers::Stopwatch &Timers::stopwatch() const
{
    return m_state.stopwatch;
}

int64_t Timers::elapsedMs( int64_t nowMs ) const
{
    const Stopwatch &watch = m_state.stopwatch;
    int64_t elapsed = watch.accumulatedMs;
    if ( watch.running && nowMs > watch.startMs ) {
        elapsed += nowMs - watch.startMs;
    }
    return elapsed;
}

void Timers::startStopStopwatch( int64_t nowMs )
{
    Stopwatch &watch = m_state.stopwatch;
    if ( watch.running ) {
        watch.accumulatedMs = elapsedMs( nowMs );
        watch.running = false;
    } else {
        watch.startMs = nowMs;
        watch.running = true;
    }
    save();
}

void Timers::lap( int64_t nowMs )
{
    Stopwatch &watch = m_state.stopwatch;
    if ( !watch.running ) {
        return;
    }
    const int64_t elapsed = elapsedMs( nowMs );
    for ( int i = LapCount - 1; i > 0; --i ) {
        watch.lapMs[i] = watch.lapMs[i - 1];
    }
    watch.lapMs[0] = elapsed - watch.lastLapMs;
    watch.lastLapMs = elapsed;
    ++watch.laps;
    save();
}

void Timers::resetStopwatch()
{
    m_state.stopwatch = {};
    save();
}

const Timers::Countdown &Timers::countdown() const
{
    return m_state.countdown;
}

int64_t Timers::remainingMs( int64_t nowMs ) const
{
    const Countdown &timer = m_state.countdown;
    if ( !timer.running ) {
        return timer.remainingMs;
    }
    return timer.endMs > nowMs ? timer.endMs - nowMs : 0;
}

void Timers::addMinutes( int minutes )
{
    Countdown &timer = m_state.countdown;
    int added = minutes * 60;
    if ( timer.durationS + added > MaxMinutes * 60 ) {
        added = MaxMinutes * 60 - timer.durationS;
    }
    timer.durationS += added;
    if ( timer.running ) {
        timer.endMs += static_cast<int64_t>( added ) * 1000;
    } else {
        timer.remainingMs += static_cast<int64_t>( added ) * 1000;
    }
    save();
}

void Timers::startPauseCountdown( int64_t nowMs )
{
    Countdown &timer = m_state.countdown;
    if ( timer.running && remainingMs( nowMs ) > 0 ) {
        timer.remainingMs = remainingMs( nowMs );
        timer.running = false;
    } else if ( timer.durationS > 0 ) {
        // A countdown that has run out starts again from its duration.
        const int64_t left = ( timer.running || timer.remainingMs <= 0 ) ? timer.durationS * 1000LL : timer.remainingMs;
        timer.endMs = nowMs + left;
        timer.running = true;
    }
    save();
}

void Timers::resetCountdown()
{
    if ( regatta() ) {
        setRegatta( true );
        return;
    }
    m_state.countdown = {};
    save();
}

void Timers::finishCountdown()
{
    m_state.countdown.running = false;
    m_state.countdown.remainingMs = 0;
    save();
}

bool Timers::regatta() const
{
    return m_state.countdown.regatta == 1;
}

void Timers::setRegatta( bool on )
{
    Countdown &timer = m_state.countdown;
    timer = {};
    if ( on ) {
        timer.regatta = 1;
        timer.durationS = Regatta::DurationS;
        timer.remainingMs = Regatta::DurationS * 1000LL;
    }
    save();
}

void Timers::syncRegatta( int64_t nowMs )
{
    Countdown &timer = m_state.countdown;
    const int64_t left = remainingMs( nowMs );
    if ( !regatta() || !timer.running || left <= 0 ) {
        return;
    }
    timer.endMs = nowMs + Regatta::sync( left );
    save();
}
