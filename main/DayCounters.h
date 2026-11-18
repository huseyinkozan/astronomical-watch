#pragma once

// Day counters of the clock page: up to four named dates set in the setup portal ("Trip home",
// "Wedding"), shown as the days until or since them, and the day each was saved (a corner gauge
// fills from it to the date). Kept in NVS; the saved days under their own key, so that counters
// saved by older versions still load (their saved day unknown).
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

    // The day counter `index` was saved (days since 1970-01-01), 0 when unknown.
    int savedDay( int index ) const;

    // A counter with the same name and date as one kept keeps its saved day; others take `today`
    // (0 when the clock is not set).
    void set( const Counter *counters, int count, int today );

private:
    struct State
    {
        int count;
        Counter counters[MaxCounters];
    };
    State m_state = {};
    int m_savedDays[MaxCounters] = {};
};
