#include "Ringer.h"

#include "Sound.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ringer";

static const int s_durationMs = 60000;
static const int s_toneMs = 100;
static const int s_gapMs = 150;
static const int s_frequencies[2] = { 1000, 1500 };
// ES8311 register 0x32: 0x9B -18 dB, 6 steps of 0.5 dB a 3 dB rise, at most 0xA7 -12 dB (the
// owner's choice after the meter: -6 dB drew 0.17 A and full scale browned the board out; at
// 1000/1500 Hz -12 dB still sounds louder than 2700 Hz at -6 dB, docs/hardware.md).
static const int s_firstVolume = 0x9B;
static const int s_lastVolume = 0xA7;
static const int s_volumeStep = 6;
static const int s_riseMs = 2000;

static volatile bool s_running = false;
static volatile bool s_stopRequested = false;
static volatile bool s_finished = false;

static void ringTask( void * )
{
    Sound sound;
    if ( sound.start() == ESP_OK ) {
        sound.setVolume( s_firstVolume );
        sound.setAmplifier( true );
        // The amplifier's start-up (NS4150B: 120 ms) passes in silence.
        sound.silence( 150 );
        const int64_t startUs = esp_timer_get_time();
        int beep = 0;
        int volume = s_firstVolume;
        while ( !s_stopRequested ) {
            const int elapsedMs = static_cast<int>( ( esp_timer_get_time() - startUs ) / 1000 );
            if ( elapsedMs >= s_durationMs ) {
                s_finished = true;
                break;
            }
            const int wanted = s_firstVolume + s_volumeStep * ( elapsedMs / s_riseMs );
            if ( wanted != volume && volume < s_lastVolume ) {
                volume = wanted < s_lastVolume ? wanted : s_lastVolume;
                sound.setVolume( volume );
            }
            sound.tone( s_frequencies[beep % 2], s_toneMs, false );
            sound.silence( s_gapMs );
            ++beep;
        }
    } else {
        ESP_LOGE( TAG, "Sound did not start" );
    }
    sound.stop();
    s_running = false;
    vTaskDelete( nullptr );
}

bool Ringer::start()
{
    if ( s_running ) {
        return true;
    }
    s_stopRequested = false;
    s_finished = false;
    s_running = true;
    // Below LVGL's priority, so that the overlay and the touches stay responsive.
    if ( xTaskCreate( ringTask, "ringer", 4096, nullptr, 2, nullptr ) != pdPASS ) {
        s_running = false;
        ESP_LOGE( TAG, "Task not created" );
        return false;
    }
    return true;
}

void Ringer::stop()
{
    s_stopRequested = true;
    // A beep and a gap at most (250 ms), then the codec's suspend sequence.
    for ( int i = 0; i < 100 && s_running; ++i ) {
        vTaskDelay( pdMS_TO_TICKS( 10 ) );
    }
}

bool Ringer::ringing() const
{
    return s_running;
}

bool Ringer::finished() const
{
    return s_finished;
}
