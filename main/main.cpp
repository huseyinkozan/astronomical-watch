#include <cstdio>

#include "Axp2101.h"
#include "BatteryLog.h"
#include "DeepSleep.h"
#include "Peripherals.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "main";

// Development aid: keeps USB connected long enough to read logs and to flash again.
static const int s_awakeWindowMs = CONFIG_WATCH_AWAKE_WINDOW_MS;
static const gpio_num_t s_powerKeyGpio = GPIO_NUM_10;

static Axp2101 s_pmu;
static DeepSleep s_deepSleep;
static Peripherals s_peripherals;
[[maybe_unused]] static BatteryLog s_batteryLog;

static void logPowerKeyIrq()
{
    bool shortPress = false;
    bool longPress = false;
    if ( s_pmu.clearPowerKeyIrq( shortPress, longPress ) == ESP_OK ) {
        ESP_LOGI( TAG, "PWR key IRQ: short %d, long %d", shortPress, longPress );
    } else {
        ESP_LOGE( TAG, "PWR key IRQ read failed" );
    }
}

// Logs every level change of the PWR key line to confirm its polarity.
// Each press restarts the window, as the display timeout will later do.
static void watchPowerKey( int windowMs )
{
    int lastLevel = gpio_get_level( s_powerKeyGpio );
    ESP_LOGI( TAG, "GPIO10 level %d; awake for %d ms after the last press", lastLevel, windowMs );
    int idleMs = 0;
    while ( idleMs < windowMs ) {
        const int level = gpio_get_level( s_powerKeyGpio );
        if ( level != lastLevel ) {
            ESP_LOGI( TAG, "GPIO10 -> %d", level );
            if ( level == 1 ) {
                s_deepSleep.addEvent( DeepSleep::Event::PowerKeyPressed );
                ESP_LOGI( TAG, "Awake window restarted" );
            }
            lastLevel = level;
            idleMs = 0;
        }
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
        idleMs += 20;
    }
}

static void sleepNow()
{
    s_peripherals.holdResetLines();
    s_deepSleep.enter();
}

// Display-off state (CLAUDE.md, off mode): the AXP2101 cuts every rail except RTCLDO and the PWR
// key powers the watch on again. Battery-log builds keep deep sleep for their timer wake, and a
// failed power-off also ends in deep sleep.
static void powerOff( [[maybe_unused]] bool pmuReady )
{
#if !CONFIG_WATCH_BATTERY_LOG
    if ( pmuReady ) {
        uint16_t millivolts = 0;
        uint8_t percent = 0;
        bool vbusGood = false;
        s_pmu.readBattery( millivolts, percent, vbusGood );
        ESP_LOGI( TAG, "Powering off: battery %u mV, %u %%, VBUS %d", millivolts, percent, vbusGood );
        // The USB console loses its last lines unless they are flushed before the rails fall.
        fflush( stdout );
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
        s_pmu.powerOff();
        ESP_LOGE( TAG, "Power-off failed; entering deep sleep" );
    }
#endif
    sleepNow();
}

extern "C" void app_main( void )
{
    [[maybe_unused]] const DeepSleep::Event wake = s_deepSleep.onBoot();

    gpio_config_t keyConfig = {};
    keyConfig.pin_bit_mask = 1ULL << s_powerKeyGpio;
    keyConfig.mode = GPIO_MODE_INPUT;
    gpio_config( &keyConfig );

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    const bool pmuReady = s_pmu.init( bus ) == ESP_OK;
    if ( !pmuReady ) {
        ESP_LOGE( TAG, "AXP2101 init failed" );
    }

#if CONFIG_WATCH_BATTERY_LOG
    if ( wake == DeepSleep::Event::WakeTimer ) {
        if ( pmuReady ) {
            s_batteryLog.record( s_pmu );
        }
        sleepNow();
    }
#endif

    s_peripherals.quiesceOnce( bus, s_pmu );
    if ( pmuReady ) {
        s_pmu.setPowerOnPress();
        s_pmu.logStatus();
        logPowerKeyIrq();
#if CONFIG_WATCH_BATTERY_LOG
        s_batteryLog.record( s_pmu );
        s_batteryLog.print();
#endif
    }

    watchPowerKey( s_awakeWindowMs );
    logPowerKeyIrq();

    powerOff( pmuReady );
}
