#include <cinttypes>

#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";

// Survives deep sleep; cleared on power-on and reset.
RTC_DATA_ATTR static uint32_t s_bootCount = 0;

static const char *wakeCauseName( esp_sleep_wakeup_cause_t cause )
{
    switch ( cause ) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "reset/power-on";
    case ESP_SLEEP_WAKEUP_EXT1:      return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER:     return "timer";
    default:                         return "other";
    }
}

extern "C" void app_main( void )
{
    ++s_bootCount;
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI( TAG, "Boot #%" PRIu32 ", wake cause: %s (%d)", s_bootCount, wakeCauseName( cause ), static_cast<int>( cause ) );
}
