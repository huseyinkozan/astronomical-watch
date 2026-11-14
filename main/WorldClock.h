#pragma once

#include <cstdint>

#include "Zones.h"

// The world clock's cities, set in the setup portal and kept in NVS, and the IANA name of the
// watch's own time zone, so that the portal can compute the next clock changes again at each
// visit. The own zone's offsets themselves are settings (Settings::UtcOffset and the Home*
// items), which the always-on wakes need without reading this.
class WorldClock
{
public:
    static const int MaxCities = 4;
    static const int NameLength = 24;
    static const int ZoneNameLength = 36;

    struct City
    {
        char name[NameLength];      // as shown: ASCII and the Turkish letters (the watch font)
        char zoneName[ZoneNameLength];  // IANA, e.g. "Europe/London"
        Zone zone;
    };

    void load();
    void save();

    int count() const;
    const City &city( int index ) const;
    const char *homeZoneName() const;

    // The name of the watch's location as chosen in the portal ("" for typed coordinates).
    const char *homePlace() const;
    void setHomePlace( const char *place );

    void set( const City *cities, int count, const char *homeZoneName );

    // Moves the clock changes that have passed; saves when one did.
    void advance( int64_t utcSeconds );

private:
    struct State
    {
        int count;
        City cities[MaxCities];
        char homeZoneName[ZoneNameLength];
    };
    State m_state = {};
    char m_place[NameLength] = {};
};
