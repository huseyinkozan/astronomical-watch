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
        DevMode,         // 1: development mode, more entries in the event log (EventLog)
        AutoOff,         // minutes without motion in always-on until power-off, 0: never
        UsbStayOn,       // 1: no display timeout while USB power is present
        Language,        // Language::Id: 0 English, 1 Turkish
        UtcOffset,       // minutes added to the RTC's UTC for the displayed time
        Latitude,        // micro-degrees, north positive
        Longitude,       // micro-degrees, east positive
        HijriMethod,     // Hijri::Method: 0 local crescent visibility, 1 Diyanet
        IshaAngle,       // the night prayer's Sun depression: 17 (Turkey) or 16 degrees
        FaceStyle,       // the face page shown, an index of the pages (FaceConfig, FaceStore)
        Brightness,      // active-mode panel brightness, percent (a long press and a drag)
        AodDayBrightness,    // always-on panel brightness from sunrise to sunset, percent
        AodNightBrightness,  // and from sunset to sunrise
        HomeChange1,     // the own time zone's next clock change: minutes since 1970 UTC, 0: none
        HomeAfter1,      // the UTC offset from then on (minutes)
        HomeChange2,     // and the change after it (WorldClock, Zones)
        HomeAfter2,
        RtcOffset,       // the PCF85063 Offset register's correction steps (Rtc::applyCalibration)
        RtcSetMinute,    // when the setup portal last set the clock: minutes since 1970 UTC, 0: never
        // The order of the stacked pages' parts and of the styles, and the hidden ones (PartOrder;
        // 0: the natural order, nothing hidden); the settings page keeps its hidden parts in
        // SettingsHidden, added later.
        ClockOrder,
        ClockHidden,
        InfoOrder,
        InfoHidden,
        SettingsOrder,
        PowerKeyPullUp,  // 1: the ESP32's pull-up on the PWR key line, set by the watch when the
                         // board's resistor is missing (runActiveMode, DeepSleep)
        UnpluggedMinute, // when USB power last went away: minutes since 1970 UTC, 0 while present
        SettingsHidden,  // the settings page's hidden parts (PartOrder); the setup part always shows
        Mirror,          // the picture mirrored: bit 0 left-right, bit 1 top-bottom (both: turned
                         // around); the portal, or PWR while a brightness drag is held (Display)
        ItemCount
    };
    // The items before this one are on the watch's settings page; FaceStyle is chosen by a
    // vertical swipe on the face; all come from the setup portal too.
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
