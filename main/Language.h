#pragma once

#include <cstddef>

// The watch's texts in English and Turkish (M11; Settings::Language). Every page takes its words
// from here; a change of language restarts the watch, so the pages are built in it once. The
// always-on face keeps the language in its look (minute wakes read no settings). The fonts hold
// ASCII, the degree sign and the Turkish letters (main/Fonts.h, main/AodGlyphs.c). Plain C++.
class Language
{
public:
    enum Id
    {
        English,
        Turkish,
        LanguageCount
    };

    static void set( int language );
    static int current();

    // Weekday 0 is Monday (Dates::weekday); months 1 to 12.
    static const char *dayName( int weekday );       // Monday, Pazartesi
    static const char *shortDay( int weekday );      // Mon, Pzt
    static const char *dayInitials( int weekday );   // Mo, Pt: the calendar's header
    static const char *monthName( int month );       // June, Haziran
    static const char *shortMonth( int month );      // Jun, Haz
    static const char *hijriMonth( int month );      // Muharram, Muharrem

    // Dates in the order of the language: "Sunday 21 June" / "21 Haziran Pazar"
    // (weekday < 0: none), "21 June" / "21 Haziran" and "Sun 21 Jun 2026" / "21 Haz 2026 Paz".
    static void longDate( char *text, size_t size, int weekday, int day, int month );
    static void shortDate( char *text, size_t size, int weekday, int day, int month, int year );

    enum Text
    {
        // Watch face.
        Sunset,
        Sunrise,
        InTime,  // "in %d:%02d"
        // Settings page.
        ScreenOff,
        AlwaysOnRow,
        DevMode,
        AutoOff,
        OnUsb,
        LanguageRow,
        On,
        Off,
        StayOn,
        TimeoutValue,
        Never,
        Seconds,  // "%d s"
        Minutes,  // "%d min"
        LanguageName,
        // Setup page.
        WifiSetup,
        Network,
        Password,
        NewPassword,
        TimeLeft,  // "%s  %d:%02d left"
        NotStarted,
        // What the portal set.
        SetupTitle,
        Place,
        LatLon,
        Zone,
        Offset,
        Change,
        Hijri,
        ClockSet,
        Fixed,
        None,
        NeverSet,
        Crescent,
        // Battery and about.
        Battery,
        OnBattery,
        ChargeTrickle,
        ChargePre,
        ChargeCurrent,
        ChargeVoltage,
        ChargeDone,
        ChargeNone,
        ChargeUnknown,
        License,
        // Brightness box.
        AodNight,
        AodDay,
        BrightnessNight,
        BrightnessDay,
        // Clock page.
        WorldClock,
        Here,
        CitiesHint,
        SameTime,
        Hours,  // "%c%d h"
        Stopwatch,
        RunningSince,  // "running since %s"
        Stopped,
        LapNumber,  // "Lap %d"
        Lap,
        Reset,
        Start,
        Stop,
        Pause,
        Timer,
        AddMinutes,
        TimerEnds,    // "of %d:%02d, ends %s"
        TimerDone,    // "done at %s"
        TimerPaused,  // "of %d:%02d, paused"
        WeekDay,      // "Week %d, day %d"
        DiyanetMethod,
        LocalCrescent,
        Days,
        DaysHint,
        Today,
        InDays,   // "in %d d"
        DaysAgo,  // "%d d ago"
        // Information pages.
        Sun,
        Rise,
        Set,
        Noon,
        Day,
        Now,
        SunsetIn,
        SunriseIn,
        Twilight,
        Dawn,
        Dusk,
        Astronomical,
        Nautical,
        Civil,
        BlueHour,
        Until,
        GoldenHour,
        Equinox,
        Solstice,
        PrayerTimes,
        Imsak,
        PrayerSunrise,
        Dhuhr,
        Asr,
        Maghrib,
        Isha,
        PrayerIn,      // "%s in %s"
        Diyanet,       // "Diyanet, %s"
        DiyanetApprox, // "Diyanet (approx.), %s"
        Moon,
        Lit,
        Tides,
        DarkSky,
        CrescentRow,
        TideSpring,
        TideNeap,
        TideMiddle,
        PhaseNew,
        PhaseWaxingCrescent,
        PhaseFirstQuarter,
        PhaseWaxingGibbous,
        PhaseFull,
        PhaseWaningGibbous,
        PhaseLastQuarter,
        PhaseWaningCrescent,
        CrescentNone,
        CrescentEye,
        CrescentBinoculars,
        CrescentTelescope,
        CrescentHidden,
        SunCompass,
        North,
        CompassHint,  // "Top towards the Sun\nSun %.0f° high, %.0f°"
        SunBelow,
        SunTooHigh,
        Time,
        Local,
        Solar,
        EquationOfTime,
        Ezani,
        Qibla,
        Sidereal,
        WeekAndDay,
        MoonAge,  // "%.1f d"
        // Page names over the stacked pages (StackPage).
        ClockPageName,
        InfoPageName,
        SettingsPageName,
        // Battery part: the time since the charger was unplugged.
        Unplugged,          // "Unplugged %s"
        DurationDayHours,   // "%d d %d h"
        DurationHourMins,   // "%d h %d min"
        JoinCodeHint,       // the QR code part while the setup portal is off
        TextCount
    };

    static const char *text( Text id );
};
