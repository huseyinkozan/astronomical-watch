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

    // VBAT (REG34/35, mV), fuel gauge (REGA4, %) and VBUS good (REG00 bit5).
    esp_err_t readBattery( uint16_t &millivolts, uint8_t &percent, bool &vbusGood );

    // Reads and clears the PWRON short/long press IRQ flags (REG49, write 1 to clear).
    esp_err_t clearPowerKeyIrq( bool &shortPress, bool &longPress );

    // Switches off every rail without a load on this board; keeps DCDC1 (VCC3V3) and
    // ALDO1 (A3V3: an unpowered ES7210 would hang the shared I2C bus). See docs/hardware.md.
    esp_err_t disableUnusedRails();

    // ALDO2 feeds the only pull-up of the AMOLED power enable DSI_PWR_EN (R10, docs/hardware.md).
    esp_err_t setPanelPower( bool on );

    // Enables only the PWR key short/long press IRQs and clears every pending IRQ flag. A
    // pending enabled IRQ holds the IRQ line low through its 10k pull-up to VCC-RTC.
    esp_err_t limitIrqs();

    // Sets the PWR press that powers the watch on from off (REG27 ONLEVEL) to 512 ms, so that a
    // brief touch of the key does not. Logs the value found first: REG27 resets only at POR.
    esp_err_t setPowerOnPress();

    // Battery test: switches every ADC channel (REG30) and the fuel gauge (REG18 bit3) on or off.
    esp_err_t setMeasurement( bool on );

    // Battery test: sets the DCDC1 (VCC3V3) output voltage, 1500-3400 mV in 100 mV steps (REG82).
    esp_err_t setDcdc1Voltage( int millivolts );

    // Software power-off (REG10 bit0): every rail except RTCLDO goes off. Returns only on failure.
    esp_err_t powerOff();

private:
    i2c_master_dev_handle_t m_device = nullptr;
};
