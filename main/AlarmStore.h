#pragma once

#include "Alarms.h"

// The alarms' definitions in NVS: one blob, written when the setup portal saves them or an alarm
// is switched on the watch.
class AlarmStore
{
public:
    struct List
    {
        Alarms::Definition alarms[Alarms::MaxAlarms];
        int count;
    };

    // The saved list; none, a blob of another size (an earlier layout) or a count out of range:
    // no alarms.
    static void load( List &list );
    static void save( const List &list );
};
