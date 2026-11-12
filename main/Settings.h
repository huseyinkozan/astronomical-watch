#pragma once

// User settings (CLAUDE.md): kept in NVS, so they survive deep sleep and off mode. A setting on
// the watch's page has a short list of values, stepped with one tap per value; the setup portal
// sets every setting.
// The Kconfig values are the defaults.
class Settings
{
public:
    enum Item
    {
        DisplayTimeout,  // seconds without touch until active mode ends
        AlwaysOnFace,    // 1: always-on face after active mode, 0: power off
        Brightness,      // active-mode panel brightness, percent
        AutoOff,         // minutes without motion in always-on until power-off, 0: never
        UtcOffset,       // minutes added to the RTC's UTC for the displayed time
        Latitude,        // micro-degrees, north positive
        Longitude,       // micro-degrees, east positive
        HijriMethod,     // Hijri::Method: 0 local crescent visibility, 1 Diyanet
        ItemCount
    };
    // The items before this one are on the watch's settings page; the rest come from the setup
    // portal only.
    static const int PageItemCount = UtcOffset;

    // Reads the saved values. A missing value, or one that is not in the item's list, keeps the
    // default. Test builds (CONFIG_WATCH_TEST_SETTINGS) ignore the saved values.
    void load();

    // Writes the values that changed since load(); test builds write nothing.
    void save();

    // Steps the item to the next value of its list; after the last comes the first.
    void next( Item item );

    // Sets a value from the setup portal; false (and no change) when it is not valid.
    bool setValue( Item item, int value );

    int value( Item item ) const;

private:
    // Written by next() in the LVGL task and read by the main task; an int access is atomic.
    int m_values[ItemCount] = {};
    int m_savedValues[ItemCount] = {};
};
