#include "Rtc.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rtc";

static const uint8_t s_address = 0x51;
// Short so that a stuck bus cannot keep the watch awake.
static const int s_timeoutMs = 50;

static const uint8_t REG_CONTROL1 = 0x00;
static const uint8_t REG_OFFSET = 0x02;
static const uint8_t REG_SECONDS = 0x04;

static const uint8_t CONTROL1_STOP = 1 << 5;
static const uint8_t CONTROL1_12_24 = 1 << 1;
static const uint8_t CONTROL1_CAP_SEL = 1 << 0;
static const uint8_t SECONDS_OS = 1 << 7;

static uint8_t fromBcd( uint8_t value )
{
    return static_cast<uint8_t>( ( value >> 4 ) * 10 + ( value & 0x0F ) );
}

static uint8_t toBcd( int value )
{
    return static_cast<uint8_t>( ( ( value / 10 ) << 4 ) | ( value % 10 ) );
}

esp_err_t Rtc::init( i2c_master_bus_handle_t bus )
{
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = s_address;
    config.scl_speed_hz = 400000;
    return i2c_master_bus_add_device( bus, &config, &m_device );
}

esp_err_t Rtc::readRegisters( uint8_t reg, uint8_t *values, int count )
{
    return i2c_master_transmit_receive( m_device, &reg, 1, values, count, s_timeoutMs );
}

esp_err_t Rtc::writeRegister( uint8_t reg, uint8_t value )
{
    const uint8_t data[2] = { reg, value };
    return i2c_master_transmit( m_device, data, sizeof( data ), s_timeoutMs );
}

esp_err_t Rtc::read( DateTime &time, bool &oscillatorStopped )
{
    // Seconds, minutes, hours, days, weekdays, months, years (24-hour mode).
    uint8_t raw[7] = {};
    const esp_err_t err = readRegisters( REG_SECONDS, raw, 7 );
    oscillatorStopped = ( raw[0] & SECONDS_OS ) != 0;
    time.second = fromBcd( raw[0] & 0x7F );
    time.minute = fromBcd( raw[1] & 0x7F );
    time.hour = fromBcd( raw[2] & 0x3F );
    time.day = fromBcd( raw[3] & 0x3F );
    time.month = fromBcd( raw[5] & 0x1F );
    time.year = static_cast<uint16_t>( 2000 + fromBcd( raw[6] ) );
    return err;
}

esp_err_t Rtc::set( const DateTime &time )
{
    if ( time.year < 2000 || time.year > 2099 || time.month < 1 || time.month > 12 || time.day < 1 ||
         time.day > 31 || time.hour > 23 || time.minute > 59 || time.second > 59 ) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t control1 = 0;
    esp_err_t err = readRegisters( REG_CONTROL1, &control1, 1 );
    if ( err == ESP_OK ) {
        control1 &= static_cast<uint8_t>( ~CONTROL1_12_24 );
        err = writeRegister( REG_CONTROL1, control1 | CONTROL1_STOP );
    }
    if ( err == ESP_OK ) {
        // OS = 0 in the seconds byte clears the flag; the weekday (0) is not used.
        const uint8_t data[8] = { REG_SECONDS, toBcd( time.second ), toBcd( time.minute ), toBcd( time.hour ),
                                  toBcd( time.day ), 0, toBcd( time.month ), toBcd( time.year - 2000 ) };
        err = i2c_master_transmit( m_device, data, sizeof( data ), s_timeoutMs );
    }
    // Always try to restart the clock, even after a failed write.
    const esp_err_t startErr = writeRegister( REG_CONTROL1, control1 & static_cast<uint8_t>( ~CONTROL1_STOP ) );
    return err != ESP_OK ? err : startErr;
}

void Rtc::logAtTick()
{
    uint8_t control[3] = {};
    if ( readRegisters( REG_CONTROL1, control, 3 ) == ESP_OK ) {
        ESP_LOGI( TAG, "Control_1=0x%02X (CAP_SEL %s, %s), Offset=0x%02X", control[0],
                  ( control[0] & CONTROL1_CAP_SEL ) ? "12.5 pF" : "7 pF",
                  ( control[0] & CONTROL1_STOP ) ? "STOPPED" : "running", control[2] );
    }

    uint8_t first = 0;
    if ( readRegisters( REG_SECONDS, &first, 1 ) != ESP_OK ) {
        ESP_LOGE( TAG, "Time read failed" );
        return;
    }
    const int64_t startUs = esp_timer_get_time();
    uint8_t seconds = first;
    while ( seconds == first && esp_timer_get_time() - startUs < 1100 * 1000 ) {
        vTaskDelay( pdMS_TO_TICKS( 2 ) );
        if ( readRegisters( REG_SECONDS, &seconds, 1 ) != ESP_OK ) {
            break;
        }
    }
    DateTime time = {};
    bool oscillatorStopped = false;
    const esp_err_t err = read( time, oscillatorStopped );
    ESP_LOGI( TAG, "RTC %04u-%02u-%02u %02u:%02u:%02u UTC, OS %d%s %s", time.year, time.month, time.day, time.hour,
              time.minute, time.second, oscillatorStopped, seconds == first ? " (no tick seen)" : "",
              esp_err_to_name( err ) );
}
