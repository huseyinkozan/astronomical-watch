#pragma once

#include "Axp2101.h"
#include "driver/i2c_master.h"

// Puts the board peripherals that the watch does not use yet into their lowest-power state.
// Sources and expected currents: docs/hardware.md. These steps will move into their own
// modules (touch, display, motion, RTC) when those are implemented.
class Peripherals
{
public:
    // Runs once after power-on or reset (RTC flag); after a deep-sleep wake it only checks
    // that the I2C devices still respond. The touch step must be the first I2C traffic.
    void quiesceOnce( i2c_master_bus_handle_t bus, Axp2101 &pmu );

    // Gives every line a defined state through deep sleep. The touch and panel reset lines are
    // held HIGH (a floating low level would wake both chips). The I2S lines to the codecs are
    // held low (GPIO45 low is also its strap default), the codec data output gets a pull-down,
    // the unused RTC inputs are isolated, and ESP-IDF isolates every other digital pad once
    // the deep-sleep hold is enabled.
    void holdSleepLines();

    // Releases those holds (reset lines stay HIGH), so that the BSP can reset the panel and
    // wake the touch controller from its sleep.
    void releaseSleepLines();

    // Puts the touch controller to sleep (1.5 mA active, 10 uA asleep) after the BSP started it.
    void sleepTouch( i2c_master_bus_handle_t bus );
};
