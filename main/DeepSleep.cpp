#include "DeepSleep.h"

#include <cinttypes>
#include <cstdio>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "sleep";

// PWR key through the BSS138 inverter: LOW when idle, HIGH while pressed (docs/hardware.md).
static const gpio_num_t s_powerKeyGpio = GPIO_NUM_10;
static const int s_keyReleaseTimeoutMs = 3000;
static const uint64_t s_fallbackWakeUs = 60ULL * 1000 * 1000;
static const int s_eventCapacity = 16;

struct WakeStatistics
{
    uint32_t bootCount;
    uint32_t powerOnWakes;
    uint32_t powerKeyWakes;
    uint32_t timerWakes;
    uint32_t otherWakes;
    uint32_t fallbackSleeps;
    uint32_t lastAwakeMs;
    uint64_t totalAwakeMs;
    // Last timer wake: from chip reset to app_main (bootloader included) and app run time.
    uint32_t lastTimerBootMs;
    uint32_t lastTimerAwakeMs;
};

struct EventRecord
{
    uint32_t boot;
    uint32_t timeMs;
    DeepSleep::Event event;
};

// Kept in RTC memory: survives deep sleep, cleared on power-on and reset.
RTC_DATA_ATTR static WakeStatistics s_stats;
RTC_DATA_ATTR static EventRecord s_events[s_eventCapacity];
RTC_DATA_ATTR static uint32_t s_eventCount;

static DeepSleep::Event s_currentWake = DeepSleep::Event::PowerOn;

static const char *eventName( DeepSleep::Event event )
{
    switch ( event ) {
    case DeepSleep::Event::PowerOn:            return "power-on/reset";
    case DeepSleep::Event::WakePowerKey:       return "wake: PWR key";
    case DeepSleep::Event::WakeTimer:          return "wake: timer";
    case DeepSleep::Event::WakeOther:          return "wake: other";
    case DeepSleep::Event::PowerKeyPressed:    return "PWR key pressed";
    case DeepSleep::Event::SleepPowerKey:      return "sleep (wake on PWR key)";
    case DeepSleep::Event::SleepFallbackTimer: return "sleep (60 s timer, key held)";
    }
    return "?";
}

#if CONFIG_WATCH_DEV_LOGGING
// USB re-enumerates after every wake; lines written before the host reopens the port are lost.
static void waitForUsbHost()
{
    for ( int elapsedMs = 0; elapsedMs < 1500; elapsedMs += 10 ) {
        if ( usb_serial_jtag_is_connected() ) {
            // Connected means the host sends frames; give it time to reopen the serial port too.
            vTaskDelay( pdMS_TO_TICKS( 500 ) );
            return;
        }
        vTaskDelay( pdMS_TO_TICKS( 10 ) );
    }
}

static void flushConsole()
{
    fflush( stdout );
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
}
#endif

void DeepSleep::addEvent( Event event )
{
    EventRecord &record = s_events[s_eventCount % s_eventCapacity];
    record.boot = s_stats.bootCount;
    record.timeMs = static_cast<uint32_t>( esp_timer_get_time() / 1000 );
    record.event = event;
    ++s_eventCount;
}

DeepSleep::Event DeepSleep::onBoot()
{
    const uint32_t causes = esp_sleep_get_wakeup_causes();
    Event wakeEvent = Event::WakeOther;

    if ( causes & ( 1UL << ESP_SLEEP_WAKEUP_EXT1 ) ) {
        const uint64_t pins = esp_sleep_get_ext1_wakeup_status();
        if ( pins & ( 1ULL << s_powerKeyGpio ) ) {
            wakeEvent = Event::WakePowerKey;
            ++s_stats.powerKeyWakes;
        } else {
            ++s_stats.otherWakes;
        }
    } else if ( causes & ( 1UL << ESP_SLEEP_WAKEUP_TIMER ) ) {
        wakeEvent = Event::WakeTimer;
        ++s_stats.timerWakes;
        s_stats.lastTimerBootMs = esp_log_timestamp();
    } else if ( causes == 0 || causes == ( 1UL << ESP_SLEEP_WAKEUP_UNDEFINED ) ) {
        // A power-on or reset clears RTC memory, so the statistics start again here.
        s_stats = WakeStatistics();
        s_eventCount = 0;
        wakeEvent = Event::PowerOn;
        ++s_stats.powerOnWakes;
    } else {
        ++s_stats.otherWakes;
    }
    ++s_stats.bootCount;
    s_currentWake = wakeEvent;

    // Timer wakes are short measurement wakes: no waiting for USB and no event dump.
    const bool timerWake = wakeEvent == Event::WakeTimer;
#if CONFIG_WATCH_DEV_LOGGING
    if ( !timerWake ) {
        waitForUsbHost();
    }
#endif
    ESP_LOGI( TAG, "Boot #%" PRIu32 ", wake source: %s (causes=0x%" PRIx32 ")", s_stats.bootCount,
              eventName( wakeEvent ), causes );
    if ( !timerWake ) {
        ESP_LOGI( TAG, "Wakes: power-on %" PRIu32 ", PWR %" PRIu32 ", timer %" PRIu32 ", other %" PRIu32
                  ", fallback sleeps %" PRIu32,
                  s_stats.powerOnWakes, s_stats.powerKeyWakes, s_stats.timerWakes, s_stats.otherWakes,
                  s_stats.fallbackSleeps );
        ESP_LOGI( TAG, "Awake: last %" PRIu32 " ms, total %" PRIu64 " ms", s_stats.lastAwakeMs,
                  s_stats.totalAwakeMs );
        ESP_LOGI( TAG, "Last timer wake: reset to app %" PRIu32 " ms, app %" PRIu32 " ms",
                  s_stats.lastTimerBootMs, s_stats.lastTimerAwakeMs );

        const uint32_t first = s_eventCount > s_eventCapacity ? s_eventCount - s_eventCapacity : 0;
        for ( uint32_t i = first; i < s_eventCount; ++i ) {
            const EventRecord &record = s_events[i % s_eventCapacity];
            ESP_LOGI( TAG, "Event: boot #%" PRIu32 " at %" PRIu32 " ms: %s", record.boot, record.timeMs,
                      eventName( record.event ) );
        }
    }
    addEvent( wakeEvent );
    return wakeEvent;
}

static bool waitForKeyRelease()
{
    for ( int elapsedMs = 0; elapsedMs < s_keyReleaseTimeoutMs; elapsedMs += 20 ) {
        if ( gpio_get_level( s_powerKeyGpio ) == 0 ) {
            return true;
        }
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
    }
    return false;
}

void DeepSleep::enter( uint64_t timerUs )
{
    // ANY_HIGH would wake again at once while the key is still held.
    const bool keyReleased = waitForKeyRelease();

    if ( keyReleased ) {
        // The 10k pull-up to VCC3V3 stays powered in deep sleep; internal pulls are not needed.
        rtc_gpio_pullup_dis( s_powerKeyGpio );
        rtc_gpio_pulldown_dis( s_powerKeyGpio );
        esp_sleep_enable_ext1_wakeup_io( 1ULL << s_powerKeyGpio, ESP_EXT1_WAKEUP_ANY_HIGH );
        addEvent( Event::SleepPowerKey );
    } else {
        ++s_stats.fallbackSleeps;
        esp_sleep_enable_timer_wakeup( s_fallbackWakeUs );
        addEvent( Event::SleepFallbackTimer );
    }
    if ( timerUs > 0 ) {
        esp_sleep_enable_timer_wakeup( timerUs );
    }
#if CONFIG_WATCH_BATTERY_LOG
    esp_sleep_enable_timer_wakeup( CONFIG_WATCH_BATTERY_LOG_INTERVAL_MIN * 60ULL * 1000 * 1000 );
#endif

    // esp_timer starts after the bootloader, so this misses about 250 ms of boot time.
    s_stats.lastAwakeMs = static_cast<uint32_t>( esp_timer_get_time() / 1000 );
    s_stats.totalAwakeMs += s_stats.lastAwakeMs;
    if ( s_currentWake == Event::WakeTimer ) {
        s_stats.lastTimerAwakeMs = s_stats.lastAwakeMs;
    }

    ESP_LOGI( TAG, "Entering deep sleep after %" PRIu32 " ms (wake: %s)", s_stats.lastAwakeMs,
              keyReleased ? "PWR key" : "60 s timer, key still high" );
#if CONFIG_WATCH_DEV_LOGGING
    if ( s_currentWake != Event::WakeTimer ) {
        flushConsole();
    }
#endif
    esp_deep_sleep_start();
}
