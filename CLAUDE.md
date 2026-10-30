# Astronomical Watch

A personal, low-power astronomical watch on the Waveshare ESP32-S3-Touch-AMOLED-2.06.
It shows time, date, sun and moon data computed on the device from a manually entered
location, date/time and UTC offset. No Wi-Fi, Bluetooth, GPS or network time.
The goal is a guide, not a fixed specification; it may be refined during development.

**Battery life has priority over functionality and visual complexity.** The ESP32 stays in
deep sleep whenever the display is not needed.

## Technology

- ESP-IDF v6.1 (CMake via `idf.py`), C++17, LVGL (through the board BSP)
- Waveshare `esp32_s3_touch_amoled_2_06` BSP, Waveshare `pcf85063a` component
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

- Wi-Fi and Bluetooth stay disabled. GPS is not used.
- The watch has three modes:
  - **Active**: display on, touch and pages. The PWR key enters it from the other two modes. After
    the display timeout without interaction (default 10 s; user-selectable 5, 10, 20, 30, 60 s)
    the watch goes to always-on.
  - **Always-on**: the panel keeps showing a dim, minimal face from its own memory while the ESP32
    is in deep sleep; a timer wake once per minute updates it. At wide intervals (default every
    10 min) the accelerometer is sampled once; when the watch has not moved for a while (default
    30 min) it goes to sleep. Shift the face by a few pixels each minute against AMOLED burn-in.
  - **Sleep**: panel in deep standby, ESP32 in deep sleep, no timer wake. Only the PWR key wakes
    the watch, into active mode.
- Always-on must use the lowest-power approach the panel allows (low brightness, few lit pixels,
  idle or partial mode where they help), never a CPU that stays awake. Do not call any mode
  low-power without evidence from the board (see "Power evaluation without external equipment").
- Deep-sleep wake sources: the PWR key (GPIO10, active high, EXT1) and, in always-on only, the
  minute timer. Motion does not wake the watch: the QMI8658 is powered down between motion checks
  and its INT1 is not used. Touch (GPIO38) and the RTC interrupt (GPIO39) cannot wake from deep sleep.
- Avoid periodic wakeups and background tasks unless they provide a necessary function.
- Settings are persistent, simple and limited to meaningful user-facing trade-offs.

## Architecture

Intended runtime flow: wake → initialise only what is needed → read time from the RTC →
by wake source:
- PWR key: active mode → update astronomy data when required → render → handle touch → after the
  timeout draw the always-on face and dim the panel.
- Minute timer: update the always-on face; when due, check motion and go to sleep mode if the
  watch has not moved.

Then deep sleep (with the minute timer in always-on, without it in sleep mode).

- Small, focused modules; no layers or abstractions without a clear benefit.
- Astronomy code is independent of UI and ESP-IDF so it can be tested on the host.
- Settings are separate from UI code and persist across reboots and deep sleep.

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
