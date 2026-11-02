#include "BatteryLog.h"

#include <cinttypes>
#include <sys/time.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "battery";

static const int s_capacity = 48;

struct BatterySample
{
    uint32_t seconds;
    uint16_t millivolts;
    uint8_t percent;
    uint8_t vbus;
};

RTC_DATA_ATTR static BatterySample s_samples[s_capacity];
RTC_DATA_ATTR static uint32_t s_count;

void BatteryLog::record( Axp2101 &pmu )
{
    if ( s_count >= s_capacity ) {
        return;
    }
    uint16_t millivolts = 0;
    uint8_t percent = 0;
    bool vbus = false;
    if ( pmu.readBattery( millivolts, percent, vbus ) != ESP_OK ) {
        return;
    }
    // System time starts at power-on and keeps running in deep sleep.
    timeval now = {};
    gettimeofday( &now, nullptr );

    BatterySample &sample = s_samples[s_count];
    sample.seconds = static_cast<uint32_t>( now.tv_sec );
    sample.millivolts = millivolts;
    sample.percent = percent;
    sample.vbus = vbus ? 1 : 0;
    ++s_count;
}

void BatteryLog::recordIfDue( [[maybe_unused]] Axp2101 &pmu )
{
#if CONFIG_WATCH_BATTERY_LOG
    timeval now = {};
    gettimeofday( &now, nullptr );
    // 30 s early still counts: the minute wakes do not land on exact seconds.
    const uint32_t intervalS = CONFIG_WATCH_BATTERY_LOG_INTERVAL_MIN * 60 - 30;
    if ( s_count > 0 && static_cast<uint32_t>( now.tv_sec ) - s_samples[s_count - 1].seconds < intervalS ) {
        return;
    }
    record( pmu );
#endif
}

void BatteryLog::print()
{
    ESP_LOGI( TAG, "Battery log: %" PRIu32 " of %d samples", s_count, s_capacity );
    for ( uint32_t i = 0; i < s_count; ++i ) {
        const BatterySample &sample = s_samples[i];
        ESP_LOGI( TAG, "Battery sample %" PRIu32 ": t=%" PRIu32 " s, vbat=%u mV, gauge=%u %%, vbus=%u", i,
                  sample.seconds, sample.millivolts, sample.percent, sample.vbus );
    }
}
