#pragma once

#include "Axp2101.h"

// Battery drain log kept in RTC memory, used to estimate the average current without a meter
// (CONFIG_WATCH_BATTERY_LOG). Starts empty after power-on or reset; stops when full.
class BatteryLog
{
public:
    void record( Axp2101 &pmu );

    // Records when the logging interval has passed since the last sample (always-on minute
    // wakes); does nothing without CONFIG_WATCH_BATTERY_LOG.
    void recordIfDue( Axp2101 &pmu );
    void print();
};
