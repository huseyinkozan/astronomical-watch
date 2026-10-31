# Astronomical Watch

A personal, low-power astronomical watch on the Waveshare ESP32-S3-Touch-AMOLED-2.06.
It shows time, date, sun and moon data computed on the device from a location, date/time and UTC
offset entered by the user. No Bluetooth, GPS, internet or network time; Wi-Fi only for the setup
portal, started by the user.
The goal is a guide, not a fixed specification; it may be refined during development.

**Battery life has priority over functionality and visual complexity.** The ESP32 stays in
deep sleep, or is powered off, whenever the display is not needed.

## Technology

- ESP-IDF v6.1 (CMake via `idf.py`), C++17, LVGL (through the board BSP)
- Waveshare `esp32_s3_touch_amoled_2_06` BSP; small custom classes for the AXP2101 and the PCF85063
- Dependencies are declared in `main/idf_component.yml`; versions are locked in `dependencies.lock`

## Build

```bash
source ~/.espressif/v6.1/esp-idf/export.sh   # same as the `esp-idf` zsh function
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

`sdkconfig` is generated from `sdkconfig.defaults` and committed. Change options in
`sdkconfig.defaults` (or with `idf.py menuconfig` and copy the decision there).

## Repository Layout

| Path | Content |
|---|---|
| `main/` | Application component (`main.cpp` and project modules) |
| `docs/hardware.md` | Hardware fact base: pins, power tree, register semantics, sources, unknowns |
| `tools/guided_test.py` | Guided hardware tests: prompts the tester and checks the serial log; logs go to `test-logs/` (ignored) |
| `archive/wom-validator/` | Earlier QMI8658 Wake-on-Motion validator and its docs (git tag `wom-validator`). Read-only reference; do not build or extend it |
| `managed_components/`, `build/` | Generated; never edit |

## Code Style (C++17 on ESP-IDF)

- Classes and class files: `PascalCase` (`PowerManager.h/.cpp`). Functions and variables: `camelCase`.
  Members: `m_` prefix. File-local statics: `s_` prefix.
- Function bodies open `{` on a new line; conditions and loops open `{` on the same line.
- Spaces inside condition and call parentheses: `if ( i == 0 ) { doWork( value ); }`.
- Plain index `for` loops instead of range-based loops.
- Keep C++ simple: small classes, no exceptions, no RTTI, no templates beyond the standard library,
  no heap allocation after start-up unless needed. ESP-IDF C APIs are used directly.
- `app_main` is `extern "C"`.
- Return `esp_err_t` for hardware operations. Product code must not abort on a peripheral error and
  must use bounded I2C timeouts, so a failure can still end in deep sleep.
- Comment only what the code cannot say: the why, a hardware constraint, a workaround.
- All code, comments, identifiers, commit messages and documentation are in English.

## Power Management Rules

- Bluetooth stays disabled. GPS is not used. Wi-Fi runs only for the setup portal: started by
  the user on the watch, on battery or USB power, for a limited time (a soft AP draws roughly
  100–150 mA, so a 5 min setup costs about 2–3 % of the 400 mAh cell; an estimate).
- The watch has three modes, and a variant of off while an alarm is pending:
  - **Active**: display on, touch and pages. The PWR key enters it from the other two modes. After
    the display timeout without interaction (default 10 s; user-selectable 5, 10, 20, 30, 60 s)
    the watch goes to always-on, or straight to off when always-on is disabled in the settings.
  - **Always-on**: the panel keeps showing a dim, minimal face from its own memory while the ESP32
    is in deep sleep; a timer wake once per minute updates it. At wide intervals (default every
    10 min) the accelerometer is sampled once; when the watch has not moved for a while (default
    30 min) it powers off. Shift the face by a few pixels each minute against AMOLED burn-in.
  - **Off**: the ESP32 powers the AXP2101 off (REG10 bit0). Every rail except RTCLDO is off
    (about 40 µA; the PCF85063 keeps time on VCC-RTC) and ESP32 RTC memory is lost, so settings
    live in NVS. The AXP2101 watches the PWR key and powers the watch on into active mode.
    The press must last at least 512 ms (REG27 ONLEVEL, kept while a battery is present), so a
    brief touch does not switch the watch on. Validated on the board, the PCF85063 keeping time
    and charging in the off state included (docs/hardware.md).
  - **Alarm pending**: while an alarm or countdown is set, the watch does not power off: nothing
    on the board can power the AXP2101 on at a set time (the PCF85063 INT reaches only GPIO39,
    which cannot wake from deep sleep either). Instead the ESP32 deep-sleeps with one timer wake
    for the next alarm, waking a little early to re-read the PCF85063 because the internal RC
    timer drifts. In always-on the minute wake checks the alarms.
- Always-on must use the lowest-power approach the panel allows (low brightness, few lit pixels,
  idle or partial mode where they help), never a CPU that stays awake. Do not call any mode
  low-power without evidence from the board (see "Power evaluation without external equipment").
- Deep-sleep wake sources: the PWR key (GPIO10, active high, EXT1), the minute timer in
  always-on and the alarm timer while an alarm is pending. In off mode the AXP2101 itself powers
  the watch on from the PWR key. Motion does not wake the watch: the QMI8658 is powered down
  between motion checks and its INT1 is not used. Touch (GPIO38) and the RTC interrupt (GPIO39)
  cannot wake from deep sleep.
- Avoid periodic wakeups and background tasks unless they provide a necessary function.
- Settings are persistent, simple and limited to meaningful user-facing trade-offs.

## Architecture

Intended runtime flow: wake → initialise only what is needed → read time from the RTC →
by wake source:
- PWR key: active mode → update astronomy data when required → render → handle touch → after the
  timeout draw the always-on face and dim the panel.
- Minute timer: update the always-on face; when due, check motion and power off if the watch
  has not moved.
- Alarm timer: re-read the time; when an alarm is due, vibrate and show it, otherwise sleep the
  remaining seconds.

Then deep sleep with the minute timer (always-on) or the alarm timer (alarm pending), or power
off through the AXP2101 (off mode).

Always-on wakes never start the BSP display (it resets the panel and would blank the image);
they open the panel directly, redraw only what changed and hold the panel pins again
(`main/Panel.*`, `main/AlwaysOn.*`).

- Small, focused modules; no layers or abstractions without a clear benefit.
- Astronomy code is independent of UI and ESP-IDF so it can be tested on the host.
- Settings are separate from UI code and persist across reboots, deep sleep and off mode (NVS).
- Setup portal (M9): a button on the watch's settings page starts it (not every charge); the
  same button stops it, and it stops by itself after a timeout or when the settings are saved.
  The watch opens a WPA2 soft AP with a captive portal (DNS answers every name with the watch's
  address, so phones and computers open the page by themselves; otherwise the IP address shown
  on the watch is typed in). The password is set on the watch and shown on its settings page.
  The page sends the browser's UTC time (timed like the guided test, with the round trip
  measured) and UTC offset, a location (country/city list, a simple world map or coordinates;
  browsers give no geolocation to a plain HTTP page) and every setting. If the Wi-Fi stack
  lengthens the deep-sleep wakes too much even with
  `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP`, a build option (`CONFIG_WATCH_SETUP_PORTAL`)
  removes it and the watch's own settings page remains.
- Alarms are stored as definitions (clock time, or sun/moon event plus offset; repeat) in NVS,
  never as a computed time. The next occurrence is computed from the definition, location and
  UTC offset, and again after each firing and after any change to them. An event that does not
  occur on a day (no moonrise, no astronomical dusk at high latitude) moves to the next day it
  occurs.

## Dependency Selection

Use the highest-priority suitable option, and keep external dependencies to a minimum:

1. ESP-IDF built-in driver
2. Official Espressif component
3. Waveshare component
4. Well-maintained third-party component
5. Small custom driver

Use the BSP where it provides suitable board-level initialisation (shared I2C bus via
`bsp_i2c_get_handle()`, display, touch). Do not duplicate BSP functionality without a reason.
Never modify files under `managed_components/`.

## Hardware Integration (documentation first)

**Read `docs/hardware.md` before any hardware work.** It records what is proven by the schematic,
the datasheets, reference software and measurements on this board, and what is still unknown.
Local copies of the datasheets, the Waveshare repositories, SensorLib and XPowersLib may be available
through the additional directories in `.claude/settings.local.json`; prefer them over web fetches.

Before implementing or debugging a peripheral or low-power feature:

1. Check the schematic: pins, interrupts, buses, pull-ups, power rails, shared signals.
2. Read the relevant datasheet sections: register semantics, state transitions, interrupt
   acknowledgement, timing, low-power behaviour.
3. Inspect the BSP, installed components and reference code for this exact board.
4. Compare the three views; record facts, assumptions, contradictions and open questions.
5. Write the smallest possible validator, only for the open questions.
6. Test on the board and record the measured result.
7. Update `docs/hardware.md` (and this file if a rule changes).

Do not use trial-and-error register changes as the first debugging method. Do not copy register
values from examples without decoding them for this device, mode and wiring. When the datasheet and
the observed behaviour disagree, keep both and design a focused test.

Do not tune the motion-check threshold or interval without a hardware measurement that justifies
the change.

## Development Workflow

Work incrementally. After each significant milestone: build, flash, verify on the board, confirm
existing functions still work, update the docs, then commit with a message that states what was
verified. Never assume hardware behaviour without testing it on the device.

Tests that need the tester to act (press keys, move the watch) are written as a step list in
`tools/guided_test.py` and run by the user in a terminal (`python tools/guided_test.py <test>`),
then the saved log is analysed. Wake/sleep logging relies on `CONFIG_WATCH_DEV_LOGGING`; disable it
for battery-life runs.

### Power evaluation without external equipment

No current meter or power profiler is available, and the AXP2101 cannot measure battery current.
Power-related changes are evaluated with on-board data instead:

- Wake statistics kept in RTC memory: wakes per source, awake time per wake
  (`esp_timer`), refused sleep entries.
- Battery voltage and gauge percentage logged over long runs on battery only (USB charges the battery).
- An energy budget estimated from datasheet currents and the measured awake time.

Report such results as estimates, never as measured current.

Battery drain runs use separate builds, so the committed `sdkconfig` stays unchanged
(`sdkconfig.battery` enables the timer log; add `sdkconfig.noquiesce` for a baseline run):

```bash
idf.py -B build-battery -D SDKCONFIG=build-battery/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.battery" build flash
```

Power-cycle the watch (hold PWR 6 s, then press) so that the log and all chips start from their
power-on state, unplug USB for the run, then plug it in and read the log with
`python tools/guided_test.py battery` (this test does not reset the board).

## Documentation

- `README.md`: human-facing and concise (purpose, features, hardware, build, status).
- `CLAUDE.md`: durable development rules and agent instructions.
- `docs/hardware.md`: hardware facts with sources.

Update the matching document in the same change when a design decision changes.
