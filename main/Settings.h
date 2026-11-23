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
        NightMode,       // 1: the red night mode from civil dusk to civil dawn (NightRed), 0: off
        BootCounter,     // 1: a BOOT press counts on the Tools page's counter (ToolPages), 0: back
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
        PlanetsOrder,    // the planets page's parts (PartOrder)
        PlanetsHidden,
        PagesOrder,      // the order of the launcher's pages (PartOrder; Display.cpp, Page)
        ToolsOrder,      // the tools page's parts (PartOrder)
        ToolsHidden,
        ObservingOrder,  // the observing page's parts (PartOrder)
        ObservingHidden,
        // The tools page's counter (Tally), set by the watch: saved with the other settings when
        // active mode ends, not at each count.
        CounterValue,
        CounterTarget,   // Tally::Target
        CounterStart,    // the first count's minute since 1970 UTC, 0: none
        // The observing page's equipment (ObservingPages, Optics), from the setup portal.
        ScopeFocal,      // the telescope's focal length, mm
        ScopeAperture,   // mm
        Eyepiece1Focal,  // tenths of a mm, 0: none
        Eyepiece1Field,  // the apparent field, degrees
        Eyepiece2Focal,
        Eyepiece2Field,
        Eyepiece3Focal,
        Eyepiece3Field,
        Eyepiece4Focal,
        Eyepiece4Field,
        CameraFocal,     // the lens's focal length, mm
        CameraFNumber,   // tenths
        CameraSensor,    // an index of Optics::Sensors
        BaseShutter,     // the ND calculator's shutter speed without the filter, Optics::shutterSeconds
        NdStops,
        TrailMinutes,    // a star-trail series' length
        TrailInterval,   // seconds from one frame to the next
        SatelliteLongitude,  // the aiming part's own satellite, tenths of a degree east (NavParts)
        CycleOrder,      // the cycle page's parts (PartOrder)
        CycleHidden,     // all hidden by default: the page stays out of the launcher until shown
        TideInterval,    // minutes from the Moon's transit to high water at the place (Tide); -1: off
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

    // Erases every saved setting but the RTC's rate correction and the time it was last set (a
    // property of this board's crystal, measured over days); the watch restarts afterwards, so
    // that the others come back as their defaults. Test builds erase nothing.
    void factoryReset();

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
