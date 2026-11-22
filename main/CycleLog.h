#pragma once

#include <cstdint>

// The cycle page's data (docs/superpowers/specs/2026-11-22-cycle-page-design.md): a byte per day
// for the last year, in a ring indexed by the local day number, and the page's own settings.
// Plain C++ so that the host tests can use it; CycleStore keeps it in NVS.
struct CycleLog
{
    static const int Days = 366;
    // A day's bits; the others stay 0.
    enum Bits : uint8_t
    {
        FlowMask = 0x03,  // 0 none, 1 light, 2 medium, 3 heavy
        Intercourse = 0x04,
        MadeUpFast = 0x08  // a make-up (kaza) fast kept that day
    };
    enum TeenMode : uint8_t
    {
        TeenOff,
        TeenOn,
        TeenAuto  // on for three years from firstPeriodDay (on without it)
    };

    // Saved as one blob (CycleStore): a different version or size starts empty; later fields take
    // reserved bytes, so that an update keeps the log and the PIN.
    static const uint8_t Version = 1;
    uint8_t version;
    uint8_t days[Days];
    int32_t newestDay;        // local day number (days since 1970-01-01) of the ring's newest day, 0: empty
    int32_t ramadanAdded;     // the hayız days of ended Ramadans, added to the make-up balance
    int32_t lastRamadanYear;  // the Hijri year whose Ramadan was added last
    int32_t madeUp;           // make-up fasts kept; outlives the days that leave the ring
    int32_t earlierBalance;   // make-up fasts owed from before the log, entered on the watch
    uint32_t pinHash;         // 0: no PIN
    uint32_t pinSalt;
    uint8_t pinAsked;         // 1 once the page offered to set a PIN
    uint8_t teenMode;         // TeenMode
    int32_t firstPeriodDay;   // 0: not entered
    uint8_t reserved[32];

    void clear();

    // Moves the ring on to `today`, clearing the days that passed without an entry. A day before
    // the newest one (the clock moved back) changes nothing.
    void advance( int today );

    // A day's byte; 0 for a day the ring does not cover (older than a year, or after newestDay).
    uint8_t get( int day ) const;
    int flow( int day ) const;

    // A day the ring covers; others are ignored. Keeps madeUp in step with bit 3.
    void set( int day, uint8_t value );

private:
    bool covers( int day ) const;
};
