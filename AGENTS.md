# Astronomical Watch Development Guide

## Project

This is a personal low-power astronomical watch based on the Waveshare
ESP32-S3-Touch-AMOLED-2.06.

## Technology

-   ESP-IDF
-   C
-   LVGL
-   Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP
-   Waveshare PCF85063A component
-   AXP2101 power management
-   Manual latitude/longitude configuration
-   Manual date/time and UTC offset configuration

## Primary Goal

Battery life has priority over functionality and visual complexity.

The ESP32 should remain in deep sleep whenever the display is not
required.

Do not add background activity, periodic wakeups, or hardware features
unless they provide a necessary function for the watch.

## Language

All source code, comments, identifiers, commit messages, and
documentation must be written in English.

## Dependency Selection

Use this priority order when selecting a driver or component:

1.  ESP-IDF built-in driver
2.  Official Espressif component
3.  Waveshare component
4.  Well-maintained third-party component
5.  Small custom driver

Always use the highest-priority suitable option.

Before introducing a new dependency:

1.  Check whether ESP-IDF already provides the required functionality.
2.  Check the official Espressif Component Registry.
3.  Check Waveshare components.
4.  Evaluate well-maintained third-party alternatives.
5.  Only then consider implementing a custom driver.

Keep external dependencies to a minimum.

Use the installed board BSP where it provides suitable board-level
initialization or hardware integration. Do not duplicate BSP
functionality without a clear reason.

Do not manually modify files under `managed_components/`; they are
managed by ESP-IDF Component Manager.

## Hardware Integration

Prefer a single shared I2C bus for board peripherals that use I2C.

The PCF85063A RTC should reuse the board's I2C bus when the BSP provides
a suitable bus handle/API.

Do not assume a generic example's GPIO configuration applies to this
board. Use the actual board/BSP configuration.

For display, touch, and power management, inspect the installed BSP APIs
and examples before implementing custom initialization or drivers.

## Power Management

-   Wi-Fi must remain disabled during normal operation.
-   Bluetooth must remain disabled.
-   GPS is not used.
-   The display should remain off when not needed.
-   QMI8658 INT1 on GPIO21 is the intended deep-sleep motion-wake
    source.
-   Touch interrupt GPIO38 is suitable for light-sleep wake, but not
    ESP32-S3 deep-sleep wake.
-   The default display timeout is 10 seconds after interaction.
-   The device should return to deep sleep afterwards when appropriate.
-   Avoid periodic wakeups unless they provide a necessary function.

Display behavior must be user-configurable.

The intended display settings are:

-   Display timeout: selectable values such as 5, 10, 20, 30, and 60
    seconds.
-   Always-on display: optional mode that disables the normal inactivity
    timeout.

The 10-second timeout remains the default because battery life is the
primary design goal.

Always-on display must not be implemented by simply keeping the ESP32
awake unless that is necessary. Prefer the lowest-power practical
implementation supported by the display and RTC hardware.

Do not claim that an always-on mode is low-power without measuring the
actual power consumption on the target hardware.

Settings should be persistent, simple, and limited to meaningful
user-facing trade-offs.

## Architecture

Keep the project structure simple.

Prefer small, focused C modules over complex architectural layers.

The intended runtime flow is:

1.  Wake from reset/deep sleep.
2.  Initialize required hardware.
3.  Read the current time from the RTC.
4.  Update astronomical data as required.
5.  Render the current screen.
6.  Handle touch and user interaction.
7.  Apply the configured display timeout.
8.  Turn the display off.
9.  Enter deep sleep when appropriate.

Avoid periodic tasks when the same result can be achieved during wakeup
or through an RTC alarm.

Astronomical calculations should remain modular and independent from the
UI where practical.

Settings should be kept separate from UI code and should persist across
reboots/deep-sleep cycles.

Do not create abstractions or components unless they provide a clear
benefit to the project.

### QMI8658 Motion and Wake Constraints

The board's QMI8658 IMU uses I2C address `0x6B`. Its INT1 output is
connected to GPIO21, which is an ESP32-S3 RTC-capable GPIO and is the
current candidate for deep-sleep motion wake.

The following behavior has been validated on the target hardware:

-   Normal accelerometer-only output is valid at 125 Hz, 62.5 Hz, and
    31.25 Hz with the gyroscope disabled and the accelerometer range set
    to ±2 g.
-   31.25 Hz is the lowest normal-output ODR tested so far; do not treat
    it as the absolute hardware minimum without further validation.
-   `STATUS0.aDA` remains clear even while valid acceleration samples
    are available. Do not gate accelerometer reads on `STATUS0.aDA` on
    this board.
-   For diagnostic sampling, use a microsecond-based schedule such as
    `esp_timer_get_time()` when the requested ODR is faster than the
    FreeRTOS tick rate.
-   The validated low-power 21 Hz Wake-on-Motion configuration uses
    `CTRL2=0x0D`. Do not substitute `CTRL2=0x0A`; that invalid
    configuration previously caused continuous false WoM interrupts.
-   In the validated WoM configuration, stationary behavior can remain
    quiet and intentional wrist motion produces `STATUS1.WoM` with
    INT1/GPIO21.
-   Reading `STATUS1` acknowledges a genuine WoM event and returns
    GPIO21 low.
-   A physical movement may produce a finite burst of additional genuine
    WoM events for several seconds after the initial event. This is not
    continuous stationary noise.
-   After acknowledging a genuine WoM event, INT1 may temporarily
    reassert high while `STATUS1` contains no pending motion status.
    This observed `INT1_TOGGLE_EDGE_CASE` clears spontaneously and must
    not be counted as a new WoM event solely from the GPIO level.
-   Before entering deep sleep, require the INT1 line to be stably low
    and use `STATUS1.WoM` to distinguish genuine motion events from the
    observed status-less INT1 edge behavior.

Do not tune WoM threshold, blanking, or related motion parameters
without a hardware measurement that justifies the change.

The next wake/sleep milestone is to validate GPIO21 as an ESP32-S3 EXT1
deep-sleep wake source while preserving the known-good QMI8658
configuration.

### Documentation-First Hardware Integration

Before implementing, tuning, or experimentally debugging a hardware
peripheral or low-power feature, perform a documentation-first
integration pass. This is the default priority for both human and
AI-assisted development.

Use this order:

1.  Inspect the exact board schematic and identify the peripheral's
    pins, interrupt lines, buses, pull-ups/pull-downs, power rails,
    reset lines, and any shared or multiplexed signals.
2.  Read the relevant sections of the peripheral manufacturer's
    datasheet, application notes, and errata. Focus on the feature being
    implemented, including register semantics, state transitions,
    interrupt acknowledgement, timing, and low-power behavior.
3.  Inspect the target board BSP, installed components, official vendor
    drivers, and official/reference examples. Prefer examples for the
    exact board or device over generic examples.
4.  Compare the three views explicitly:
    -   what the schematic proves about this board,
    -   what the datasheet specifies,
    -   what the reference software actually configures.
5.  Record known facts, assumptions, contradictions, and unresolved
    questions before writing experimental code.
6.  Write the smallest possible validator only for the unresolved
    questions.
7.  Test on the actual board and record the measured result before
    treating a behavior as validated.
8.  Update `README.md` and/or `AGENTS.md` when the result changes the
    durable understanding of the hardware or architecture.

Do not use trial-and-error register changes as the first debugging
method when the relevant documentation or reference implementation has
not yet been reviewed.

Do not copy register values from an example without decoding and
verifying that they apply to the exact device variant, operating mode,
and board wiring.

When documentation and observed hardware behavior disagree, preserve
both facts in the investigation. Do not silently force the observation
to match the documentation. Design a focused test to resolve the
discrepancy.

For each new peripheral or substantial hardware feature, maintain a
compact integration record when useful:

  -----------------------------------------------------------------------
  Area                                What to capture
  ----------------------------------- -----------------------------------
  Board wiring                        GPIOs, buses, IRQs, power/reset,
                                      shared signals

  Datasheet                           Required mode, registers, timing,
                                      interrupt semantics

  Reference software                  Initialization sequence and encoded
                                      register values

  Known-good state                    Configuration already validated on
                                      hardware

  Unknowns                            Questions that still require
                                      measurement

  Validator                           Minimal test designed only for
                                      those unknowns

  Result                              Measured behavior and resulting
                                      design decision
  -----------------------------------------------------------------------

This process is intended to reduce exploratory debugging while
preserving the project rule that hardware behavior must ultimately be
verified on the target board.

## Development Workflow

Make changes incrementally.

After each significant hardware/software milestone:

1.  Build the project.
2.  Flash the device.
3.  Verify the behavior on hardware.
4.  Confirm that existing functionality still works.
5.  Commit the working state to Git.

Never assume hardware functionality without testing it on the actual
device.

For power-related changes, test both functionality and measured power
consumption.

## Documentation

Keep `README.md` human-facing and concise. It should describe the
project's purpose, features, hardware, power behavior, architecture at a
high level, build/flash procedure, dependencies, and current status.

Keep durable development constraints and agent-specific instructions in
`AGENTS.md`.

When an architectural decision changes, update the appropriate
documentation in the same change so that the documented design and
implementation remain consistent.
