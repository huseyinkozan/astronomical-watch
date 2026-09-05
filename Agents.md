# Astronomical Watch Development Guide

## Project

This is a personal low-power astronomical watch based on the
Waveshare ESP32-S3-Touch-AMOLED-2.06.

## Technology

- ESP-IDF
- C
- LVGL
- PCF85063 RTC
- AXP2101 power management
- Manual latitude/longitude configuration

## Primary Goal

Battery life has priority over functionality and visual complexity.

The ESP32 should remain in deep sleep whenever the display is not required.

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

## Power Management

- Wi-Fi must remain disabled during normal operation.
- Bluetooth must remain disabled.
- GPS is not used.
- The display should remain off when not needed.
- A touch event wakes the device.
- The display remains active for 10 seconds after interaction.
- The device should return to deep sleep afterwards.
- Avoid periodic wakeups unless they provide a necessary function.

## Architecture

Keep the project structure simple.

Do not create abstractions or components unless they provide a clear
benefit to the project.

Prefer small, focused C modules over complex architectural layers.

## Development Workflow

Make changes incrementally.

After each significant hardware/software milestone:

1. Build the project.
2. Flash the device.
3. Verify the behavior on hardware.
4. Commit the working state to Git.

Never assume hardware functionality without testing it on the actual device.