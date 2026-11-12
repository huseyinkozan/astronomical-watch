#pragma once

// User settings (CLAUDE.md): kept in NVS, so they survive deep sleep and off mode. Each setting
// has a short list of values, and the settings page steps through it with one tap per value.
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
        ItemCount
    };

    // Reads the saved values. A missing value, or one that is not in the item's list, keeps the
    // default. Test builds (CONFIG_WATCH_TEST_SETTINGS) ignore the saved values.
    void load();

    // Writes the values that changed since load(); test builds write nothing.
    void save();

    // Steps the item to the next value of its list; after the last comes the first.
    void next( Item item );

    int value( Item item ) const;

private:
    // Written by next() in the LVGL task and read by the main task; an int access is atomic.
    int m_values[ItemCount] = {};
    int m_savedValues[ItemCount] = {};
};
