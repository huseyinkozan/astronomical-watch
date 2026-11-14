#pragma once

#include <cstdint>

// A persistent log of notable events in NVS, for stability analysis on the wrist (USB cannot
// stay connected): boots with their reset reason, the end of each active session (touches,
// touch controller state), the end of always-on, power-offs. Each entry is written once, at the
// event, never at every minute wake; with the development mode setting on, always-on also adds a
// summary every 30 min. Read with tools/read_events.py (host command "E",
// CONFIG_WATCH_DEV_LOGGING).
class EventLog
{
public:
    enum Type : uint8_t
    {
        Boot = 1,           // power-on or reset; code: esp_reset_reason_t
        ActiveEnd = 2,      // code: 1 display timeout, 2 PWR press
        AlwaysOnEnd = 3,    // code: 1 PWR press
        PowerOff = 4,       // code: PowerOffReason
        AlwaysOnStatus = 5  // development mode: every 30 min of always-on
    };

    enum PowerOffReason : uint8_t
    {
        AlwaysOnDisabled = 1,  // the setting, or the display did not start
        NoMotion = 2,
        BatteryLimit = 3,
        FaceFailed = 4  // the panel or the RTC failed in always-on
    };

    // Flags.
    enum Flag : uint8_t
    {
        DisplayStarted = 1,
        TouchStarted = 2,
        PortalUsed = 4,
        Usb = 8,          // VBUS present
        CoreDump = 16,    // Boot: a core dump is stored in flash
        TimeInvalid = 32  // the RTC time was not read
    };

    struct Entry
    {
        uint32_t time;  // UTC, Unix seconds
        uint8_t type;
        uint8_t code;
        uint8_t flags;
        uint8_t batteryPercent;
        uint16_t batteryMillivolts;
        uint16_t duration;  // ActiveEnd: seconds; always-on: minutes
        uint16_t touches;   // ActiveEnd: touch presses
        uint8_t touchModeStart;  // FT3168 power mode register at the start (0xFF: no answer)
        uint8_t touchModeEnd;    // and at the end
        uint16_t heapMinimumKb;  // smallest free internal heap since boot
        uint16_t mainStackFree;  // bytes never used of the main task's stack
        uint16_t lvglStackFree;  // of the LVGL task's stack (0: not running)
        uint16_t wakes;          // always-on: timer wakes; Boot: boot count
        uint16_t longestWakeMs;  // always-on: the longest timer wake
        uint8_t reserved[6];
    };

    // Fills the fields every entry shares (time, battery, heap, stacks) and writes the entry.
    // `utcTime` 0: the RTC time was not read.
    static void add( Entry entry, uint32_t utcTime );

    // Prints every entry, oldest first, as "EVENT ..." lines between "EVENTS BEGIN" and
    // "EVENTS END"; times in local time by `utcOffsetMinutes`.
    static void print( int utcOffsetMinutes );
};
