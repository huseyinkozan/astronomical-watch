#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Minimal PCF85063A access: the time in UTC and the oscillator-stop flag.
// Register meanings: NXP PCF85063A datasheet rev. 7.3, sections 7.2 to 7.4.
class Rtc
{
public:
    struct DateTime
    {
        uint16_t year;    // 2000 to 2099
        uint8_t month;    // 1 to 12
        uint8_t day;      // 1 to 31
        uint8_t hour;     // 0 to 23
        uint8_t minute;
        uint8_t second;
    };

    esp_err_t init( i2c_master_bus_handle_t bus );

    // Reads seconds through years in one access, as the datasheet requires. oscillatorStopped is
    // the OS flag: the clock stopped since the time was last set, so the time is not valid.
    esp_err_t read( DateTime &time, bool &oscillatorStopped );

    // Writes the time with the clock stopped, then starts it: the next second follows 0.508 s
    // after the start (datasheet 7.2.1.2). Clears the OS flag. The weekday register is not used.
    esp_err_t set( const DateTime &time );

    // Waits for the next second (at most 1.1 s) and logs the time at that moment, so that the
    // host timestamp of the log line shows the clock error to within a few milliseconds.
    // Also logs Control_1 and Offset, which set the crystal load and the rate correction.
    void logAtTick();

private:
    esp_err_t readRegisters( uint8_t reg, uint8_t *values, int count );
    esp_err_t writeRegister( uint8_t reg, uint8_t value );

    i2c_master_dev_handle_t m_device = nullptr;
};
