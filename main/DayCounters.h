#pragma once

// Day counters of the clock page: up to four named dates set in the setup portal ("Trip home",
// "Wedding"), shown as the days until or since them. Kept in NVS.
class DayCounters
{
public:
    static const int MaxCounters = 4;
    static const int NameLength = 24;

    struct Counter
    {
        char name[NameLength];  // ASCII and the Turkish letters (the watch font)
        int day;                // days since 1970-01-01
    };

    void load();

    int count() const;
    const Counter &counter( int index ) const;

    void set( const Counter *counters, int count );

private:
    struct State
    {
        int count;
        Counter counters[MaxCounters];
    };
    State m_state = {};
};
