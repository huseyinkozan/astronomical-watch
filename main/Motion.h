#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

// QMI8658A accelerometer, sampled once at wide intervals (CLAUDE.md, always-on mode): it is
// switched on, a few samples are averaged and it is powered down again (about 6 uA).
// Register meanings: QMI8658A datasheet Rev A; board notes in docs/hardware.md.
class Motion
{
public:
    struct Reading
    {
        int16_t mg[3];      // average of the samples, x, y, z in mg
        uint8_t spreadMg;   // largest max - min of one axis over the samples (noise or motion)
        uint8_t readyMs;    // from enabling the accelerometer to its first new sample, 255 if none
    };

    esp_err_t init( i2c_master_bus_handle_t bus );

    // Takes one reading and powers the accelerometer down again, also after an error.
    esp_err_t sample( Reading &reading );

    // A new always-on period (after a PWR wake): the next check takes a new reference.
    void restart();

    // Always-on minute wake: every CONFIG_WATCH_MOTION_CHECK_MIN it takes a reading and compares
    // it with the one before. True once CONFIG_WATCH_MOTION_OFF_MIN have passed without a change
    // above the no-motion threshold; always false when that option is 0.
    bool stillTooLong();

    // Measurement build (CONFIG_WATCH_MOTION_LOG): one reading per call kept in RTC memory,
    // printed on a PWR wake. Used to choose the no-motion threshold (docs/hardware.md).
    void recordLog();
    void printLog();

private:
    esp_err_t readRegisters( uint8_t reg, uint8_t *values, int count );
    esp_err_t writeRegister( uint8_t reg, uint8_t value );
    esp_err_t readUntilSamples( int samples, Reading &reading );

    i2c_master_dev_handle_t m_device = nullptr;
};
