# Astronomical Watch Development Guide

## Project

This is a personal low-power astronomical watch based on the
Waveshare ESP32-S3-Touch-AMOLED-2.06.

## Technology

- ESP-IDF
- C
- LVGL
- Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP
- Waveshare PCF85063A component
- AXP2101 power management
- Manual latitude/longitude configuration
- Manual date/time and UTC offset configuration

## Primary Goal

Battery life has priority over functionality and visual complexity.

The ESP32 should remain in deep sleep whenever the display is not required.

Do not add background activity, periodic wakeups, or hardware features unless
they provide a necessary function for the watch.

## Language

All source code, comments, identifiers, commit messages, and documentation
must be written in English.

## Dependency Selection

Use this priority order when selecting a driver or component:

1. ESP-IDF built-in driver
2. Official Espressif component
3. Waveshare component
4. Well-maintained third-party component
5. Small custom driver

Always use the highest-priority suitable option.

Before introducing a new dependency:

1. Check whether ESP-IDF already provides the required functionality.
2. Check the official Espressif Component Registry.
3. Check Waveshare components.
4. Evaluate well-maintained third-party alternatives.
5. Only then consider implementing a custom driver.

Keep external dependencies to a minimum.

Use the installed board BSP where it provides suitable board-level
initialization or hardware integration. Do not duplicate BSP functionality
without a clear reason.

Do not manually modify files under `managed_components/`; they are managed by
ESP-IDF Component Manager.

## Hardware Integration

Prefer a single shared I2C bus for board peripherals that use I2C.

The PCF85063A RTC should reuse the board's I2C bus when the BSP provides a
suitable bus handle/API.

Do not assume a generic example's GPIO configuration applies to this board.
Use the actual board/BSP configuration.

For display, touch, and power management, inspect the installed BSP APIs and
examples before implementing custom initialization or drivers.

## Power Management

- Wi-Fi must remain disabled during normal operation.
- Bluetooth must remain disabled.
- GPS is not used.
- The display should remain off when not needed.
- A touch event wakes the device.
- The default display timeout is 10 seconds after interaction.
- The device should return to deep sleep afterwards.
- Avoid periodic wakeups unless they provide a necessary function.

Display behavior must be user-configurable.

The intended display settings are:

- Display timeout: selectable values such as 5, 10, 20, 30, and 60 seconds.
- Always-on display: optional mode that disables the normal inactivity timeout.

The 10-second timeout remains the default because battery life is the primary
design goal.

Always-on display must not be implemented by simply keeping the ESP32 awake
unless that is necessary. Prefer the lowest-power practical implementation
supported by the display and RTC hardware.

Do not claim that an always-on mode is low-power without measuring the actual
power consumption on the target hardware.

Settings should be persistent, simple, and limited to meaningful user-facing
trade-offs.

## Architecture

Keep the project structure simple.

Prefer small, focused C modules over complex architectural layers.

The intended runtime flow is:

1. Wake from reset/deep sleep.
2. Initialize required hardware.
3. Read the current time from the RTC.
4. Update astronomical data as required.
5. Render the current screen.
6. Handle touch and user interaction.
7. Apply the configured display timeout.
8. Turn the display off.
9. Enter deep sleep when appropriate.

Avoid periodic tasks when the same result can be achieved during wakeup or
through an RTC alarm.

Astronomical calculations should remain modular and independent from the UI
where practical.

Settings should be kept separate from UI code and should persist across
reboots/deep-sleep cycles.

Do not create abstractions or components unless they provide a clear benefit
to the project.

## Development Workflow

Make changes incrementally.

After each significant hardware/software milestone:

1. Build the project.
2. Flash the device.
3. Verify the behavior on hardware.
4. Confirm that existing functionality still works.
5. Commit the working state to Git.

Never assume hardware functionality without testing it on the actual device.

For power-related changes, test both functionality and measured power
consumption.

## Documentation

Keep `README.md` human-facing and concise. It should describe the project's
purpose, features, hardware, power behavior, architecture at a high level,
build/flash procedure, dependencies, and current status.

Keep durable development constraints and agent-specific instructions in
`AGENTS.md`.

When an architectural decision changes, update the appropriate documentation
in the same change so that the documented design and implementation remain
consistent.
