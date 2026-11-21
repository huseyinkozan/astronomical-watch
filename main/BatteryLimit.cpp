#include "BatteryLimit.h"

bool BatteryLimit::empty( int millivolts, int percent )
{
    return ( percent <= PowerOffPercent && millivolts <= PowerOffMillivolts ) || millivolts <= BackstopMillivolts;
}

bool BatteryLimit::atFloor( int millivolts, int percent )
{
    return ( percent <= FloorPercent && millivolts <= FloorPairedMillivolts ) || millivolts <= FloorMillivolts;
}
