#include "Motion.h"

#include <cinttypes>
#include <cstdlib>
#include <sys/time.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "sdkconfig.h"

[[maybe_unused]] static const char *TAG = "motion";

static const uint8_t s_address = 0x6B;
// Short so that a stuck bus cannot keep the watch awake.
static const int s_timeoutMs = 50;

static const uint8_t REG_CTRL1 = 0x02;
static const uint8_t REG_CTRL2 = 0x03;
static const uint8_t REG_CTRL7 = 0x08;
static const uint8_t REG_TIMESTAMP_L = 0x30;

// CTRL1: address auto-increment, little-endian data, INT pins high-Z, oscillator on (bit0 = 0).
static const uint8_t CTRL1_ON = 0x40;
static const uint8_t CTRL1_SENSOR_DISABLE = 0x01;
// CTRL2: +-2 g, ODR code 0110 (112 Hz from the internal oscillator when only the accelerometer runs).
static const uint8_t CTRL2_2G_112HZ = 0x06;
static const uint8_t CTRL7_ACCEL_ON = 0x01;
static const int s_lsbPerG = 16384;

// The first samples are dropped while the filter settles (t5 = 3/ODR, datasheet 7.3).
static const int s_settleSamples = 3;
static const int s_averageSamples = 8;
static const int s_maxWaitMs = 200;

// No-motion threshold, the largest change of one axis between two checks. Measured on this
// board 10 min apart (docs/hardware.md, QMI8658A section): at most 3 mg on a desk, at least
// 21 mg on the wrist.
static const int s_thresholdMg = 10;

#if CONFIG_WATCH_MOTION_OFF_MIN > 0
// Kept in RTC memory: cleared on power-on and reset.
RTC_DATA_ATTR static bool s_hasReference;
RTC_DATA_ATTR static uint32_t s_lastCheckSeconds;
RTC_DATA_ATTR static int16_t s_referenceMg[3];
RTC_DATA_ATTR static int s_stillChecks;
#endif

esp_err_t Motion::init( i2c_master_bus_handle_t bus )
{
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = s_address;
    config.scl_speed_hz = 400000;
    return i2c_master_bus_add_device( bus, &config, &m_device );
}

esp_err_t Motion::readRegisters( uint8_t reg, uint8_t *values, int count )
{
    return i2c_master_transmit_receive( m_device, &reg, 1, values, count, s_timeoutMs );
}

esp_err_t Motion::writeRegister( uint8_t reg, uint8_t value )
{
    const uint8_t data[2] = { reg, value };
    return i2c_master_transmit( m_device, data, sizeof( data ), s_timeoutMs );
}

esp_err_t Motion::sample( Reading &reading )
{
    reading = {};
    reading.readyMs = 255;
    esp_err_t err = writeRegister( REG_CTRL1, CTRL1_ON );
    if ( err == ESP_OK ) {
        // System turn-on time after power-down: at most 15 ms (datasheet 7.3).
        esp_rom_delay_us( 15000 );
        err = writeRegister( REG_CTRL2, CTRL2_2G_112HZ );
    }
    if ( err == ESP_OK ) {
        err = writeRegister( REG_CTRL7, CTRL7_ACCEL_ON );
    }
    if ( err == ESP_OK ) {
        err = readUntilSamples( s_settleSamples + s_averageSamples, reading );
    }
    // Back to power-down (about 6 uA) whatever happened above.
    const esp_err_t accelOff = writeRegister( REG_CTRL7, 0x00 );
    const esp_err_t oscillatorOff = writeRegister( REG_CTRL1, CTRL1_ON | CTRL1_SENSOR_DISABLE );
    if ( err == ESP_OK ) {
        err = accelOff != ESP_OK ? accelOff : oscillatorOff;
    }
    return err;
}

void Motion::restart()
{
#if CONFIG_WATCH_MOTION_OFF_MIN > 0
    s_hasReference = false;
    s_stillChecks = 0;
#endif
}

bool Motion::stillTooLong()
{
#if CONFIG_WATCH_MOTION_OFF_MIN > 0
    // System time starts at power-on and keeps running in deep sleep.
    timeval now = {};
    gettimeofday( &now, nullptr );
    const uint32_t seconds = static_cast<uint32_t>( now.tv_sec );
    // 30 s early still counts: the minute wakes do not land on exact seconds.
    const uint32_t intervalS = CONFIG_WATCH_MOTION_CHECK_MIN * 60 - 30;
    if ( s_hasReference && seconds - s_lastCheckSeconds < intervalS ) {
        return false;
    }
    s_lastCheckSeconds = seconds;

    Reading reading = {};
    const esp_err_t err = sample( reading );
    if ( err != ESP_OK ) {
        // Counted as no motion, so that a broken sensor cannot keep the watch out of off mode.
        ++s_stillChecks;
        ESP_LOGE( TAG, "Motion check failed: %s", esp_err_to_name( err ) );
    } else if ( !s_hasReference ) {
        s_stillChecks = 0;
        ESP_LOGI( TAG, "Motion reference x=%d y=%d z=%d mg", reading.mg[0], reading.mg[1], reading.mg[2] );
    } else {
        int change = 0;
        for ( int axis = 0; axis < 3; ++axis ) {
            const int axisChange = abs( reading.mg[axis] - s_referenceMg[axis] );
            if ( axisChange > change ) {
                change = axisChange;
            }
        }
        s_stillChecks = change > s_thresholdMg ? 0 : s_stillChecks + 1;
        ESP_LOGI( TAG, "Motion check: change %d mg, still for %d checks", change, s_stillChecks );
    }
    if ( err == ESP_OK ) {
        s_hasReference = true;
        for ( int axis = 0; axis < 3; ++axis ) {
            s_referenceMg[axis] = reading.mg[axis];
        }
    }
    return s_stillChecks * CONFIG_WATCH_MOTION_CHECK_MIN >= CONFIG_WATCH_MOTION_OFF_MIN;
#else
    return false;
#endif
}

esp_err_t Motion::readUntilSamples( int samples, Reading &reading )
{
    // Power-down keeps the output registers, so a new sample shows only as a new time stamp.
    // One read covers TIMESTAMP 0x30-0x32, TEMP 0x33-0x34 and AX, AY, AZ 0x35-0x3A.
    uint8_t data[11] = {};
    const int64_t start = esp_timer_get_time();
    esp_err_t err = readRegisters( REG_TIMESTAMP_L, data, sizeof( data ) );
    uint32_t lastStamp = data[0] | ( data[1] << 8 ) | ( data[2] << 16 );
    int count = 0;
    int32_t sum[3] = { 0, 0, 0 };
    int16_t low[3] = { 0, 0, 0 };
    int16_t high[3] = { 0, 0, 0 };
    while ( err == ESP_OK && count < samples ) {
        const int64_t elapsedUs = esp_timer_get_time() - start;
        if ( elapsedUs > s_maxWaitMs * 1000 ) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us( 1000 );
        err = readRegisters( REG_TIMESTAMP_L, data, sizeof( data ) );
        const uint32_t stamp = data[0] | ( data[1] << 8 ) | ( data[2] << 16 );
        if ( err != ESP_OK || stamp == lastStamp ) {
            continue;
        }
        lastStamp = stamp;
        ++count;
        if ( count == 1 ) {
            reading.readyMs = static_cast<uint8_t>( ( esp_timer_get_time() - start ) / 1000 );
        }
        if ( count <= s_settleSamples ) {
            continue;
        }
        for ( int axis = 0; axis < 3; ++axis ) {
            const int16_t value = static_cast<int16_t>( data[5 + 2 * axis] | ( data[6 + 2 * axis] << 8 ) );
            sum[axis] += value;
            if ( count == s_settleSamples + 1 || value < low[axis] ) {
                low[axis] = value;
            }
            if ( count == s_settleSamples + 1 || value > high[axis] ) {
                high[axis] = value;
            }
        }
    }
    if ( err != ESP_OK ) {
        return err;
    }
    const int averaged = samples - s_settleSamples;
    int spread = 0;
    for ( int axis = 0; axis < 3; ++axis ) {
        reading.mg[axis] = static_cast<int16_t>( sum[axis] / averaged * 1000 / s_lsbPerG );
        const int axisSpread = ( high[axis] - low[axis] ) * 1000 / s_lsbPerG;
        if ( axisSpread > spread ) {
            spread = axisSpread;
        }
    }
    reading.spreadMg = static_cast<uint8_t>( spread > 255 ? 255 : spread );
    return ESP_OK;
}

#if CONFIG_WATCH_MOTION_LOG
struct MotionLogEntry
{
    uint32_t seconds;
    Motion::Reading reading;
};

static const int s_logCapacity = 160;

// Kept in RTC memory: cleared on power-on and reset.
RTC_DATA_ATTR static MotionLogEntry s_log[s_logCapacity];
RTC_DATA_ATTR static uint32_t s_logCount;
#endif

void Motion::recordLog()
{
#if CONFIG_WATCH_MOTION_LOG
    if ( s_logCount >= s_logCapacity ) {
        return;
    }
    const int64_t start = esp_timer_get_time();
    Reading reading = {};
    const esp_err_t err = sample( reading );
    const int awakeMs = static_cast<int>( ( esp_timer_get_time() - start ) / 1000 );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Motion sample failed: %s", esp_err_to_name( err ) );
        return;
    }
    // System time starts at power-on and keeps running in deep sleep.
    timeval now = {};
    gettimeofday( &now, nullptr );
    s_log[s_logCount].seconds = static_cast<uint32_t>( now.tv_sec );
    s_log[s_logCount].reading = reading;
    ++s_logCount;
    ESP_LOGI( TAG, "Motion x=%d y=%d z=%d mg, spread %u mg, ready %u ms, %d ms in all", reading.mg[0],
              reading.mg[1], reading.mg[2], reading.spreadMg, reading.readyMs, awakeMs );
#endif
}

void Motion::printLog()
{
#if CONFIG_WATCH_MOTION_LOG
    ESP_LOGI( TAG, "Motion log: %" PRIu32 " of %d samples", s_logCount, s_logCapacity );
    for ( uint32_t i = 0; i < s_logCount; ++i ) {
        const MotionLogEntry &entry = s_log[i];
        ESP_LOGI( TAG, "Motion sample %" PRIu32 ": t=%" PRIu32 " s, x=%d y=%d z=%d mg, spread=%u mg, ready=%u ms",
                  i, entry.seconds, entry.reading.mg[0], entry.reading.mg[1], entry.reading.mg[2],
                  entry.reading.spreadMg, entry.reading.readyMs );
    }
#endif
}
