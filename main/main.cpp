#include "Axp2101.h"
#include "DeepSleep.h"

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
            }
            lastLevel = level;
            idleMs = 0;
        }
        vTaskDelay( pdMS_TO_TICKS( 20 ) );
        idleMs += 20;
    }
}

extern "C" void app_main( void )
{
    s_deepSleep.onBoot();

    gpio_config_t keyConfig = {};
    keyConfig.pin_bit_mask = 1ULL << s_powerKeyGpio;
    keyConfig.mode = GPIO_MODE_INPUT;
    gpio_config( &keyConfig );

    if ( s_pmu.init( bsp_i2c_get_handle() ) == ESP_OK ) {
        s_pmu.logStatus();
        logPowerKeyIrq();
    } else {
        ESP_LOGE( TAG, "AXP2101 init failed" );
    }

    watchPowerKey( s_awakeWindowMs );
    logPowerKeyIrq();

    s_deepSleep.enter();
}
