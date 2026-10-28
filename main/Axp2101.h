#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Minimal AXP2101 PMIC access: only the registers this project uses.
// Register meanings: X-Powers AXP2101 datasheet V1.0, section 6.13.
class Axp2101
{
public:
    esp_err_t init( i2c_master_bus_handle_t bus );

    esp_err_t readRegister( uint8_t reg, uint8_t &value );
    esp_err_t writeRegister( uint8_t reg, uint8_t value );

    // 14-bit ADC result: the high byte must be read first (datasheet 6.10).
    esp_err_t readAdc14( uint8_t highReg, uint16_t &value );

    void logStatus();

    // Reads and clears the PWRON short/long press IRQ flags (REG49, write 1 to clear).
    esp_err_t clearPowerKeyIrq( bool &shortPress, bool &longPress );

private:
    i2c_master_dev_handle_t m_device = nullptr;
};
