# Astronomical Watch

A personal low-power astronomical watch based on the **Waveshare
ESP32-S3-Touch-AMOLED-2.06** development board.

The project is designed for personal use rather than as a
general-purpose smartwatch. Its primary goal is to provide a simple
watch interface with astronomical information while keeping power
consumption as low as possible.

## Features

Planned features:

-   Digital date and time display
-   Manual date and time configuration
-   Manual latitude and longitude configuration
-   Manual UTC offset configuration
-   Sunrise and sunset calculations
-   Astronomical, nautical, and civil twilight calculations
-   Solar noon calculation
-   Moon phase display
-   Moon illumination percentage
-   Moon age
-   Moonrise and moonset times
-   Battery level and power information
-   Low-power operation with deep sleep
-   Touch-to-wake display
-   Configurable display timeout
-   Optional always-on display mode

The device does not require a permanent network connection. Astronomy
calculations are performed locally.

## Hardware

Target hardware:

**Waveshare ESP32-S3-Touch-AMOLED-2.06**

The board provides the main hardware required by the project, including:

-   ESP32-S3
-   2.06-inch AMOLED display
-   Touch controller
-   PCF85063 RTC
-   AXP2101 power management IC
-   Battery support

The project uses the board support package (BSP) for the target board
where appropriate.

## Software

The project is built with:

-   ESP-IDF
-   C
-   LVGL
-   ESP-IDF Component Manager
-   Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP
-   Waveshare PCF85063A component

All source code, comments, identifiers, and documentation are written in
English.

## Power Management

Power consumption is a primary design requirement.

During normal operation:

-   Wi-Fi is disabled.
-   Bluetooth is disabled.
-   GPS is not used.
-   The AMOLED display is turned off when not needed.
-   The ESP32 should enter deep sleep when the display is inactive and
    the wake sources are in a safe state.
-   QMI8658 INT1 on GPIO21 is the intended deep-sleep motion-wake
    source.
-   Touch interrupt GPIO38 is intended for touch-ready light-sleep
    operation; it cannot be used as an ESP32-S3 deep-sleep GPIO wake
    source.
-   The default display timeout is approximately 10 seconds after
    interaction.
-   Unnecessary periodic wakeups and background processing should be
    avoided.

### Display Modes

Display behavior is user-configurable.

The intended settings include:

-   **Display timeout** --- selectable values such as 5, 10, 20, 30, and
    60 seconds.
-   **Always-on display** --- optional mode in which the normal
    inactivity timeout is disabled.

The 10-second timeout is the default because battery life is a primary
design goal. Users who prefer continuous display visibility can enable
always-on mode at the cost of higher power consumption.

Always-on operation must not automatically imply that the ESP32 remains
continuously active. The implementation should use the lowest-power
practical approach supported by the display and RTC hardware, and actual
power consumption should be measured on the target hardware before
treating it as a low-power mode.

Settings should be persistent and implemented without introducing
unnecessary architectural complexity.

## Location and Time

The user enters the geographic location manually using:

``` text
Latitude
Longitude
```

For example:

``` text
Latitude:  41.0082
Longitude: 28.9784
```

No GPS hardware is required.

Time and timezone-related settings are also intended to remain simple
and manual:

-   Date and time
-   UTC offset

The project does not require network time synchronization.

## Astronomical Calculations

Astronomical calculations are performed locally on the ESP32.

The project does not depend on an online astronomy service.

Solar calculations will include:

-   Astronomical dawn
-   Nautical dawn
-   Civil dawn
-   Sunrise
-   Solar noon
-   Sunset
-   Civil dusk
-   Nautical dusk
-   Astronomical dusk
-   Day length

Lunar calculations will include:

-   Moon phase
-   Moon illumination
-   Moon age
-   Moonrise
-   Moonset

Astronomical calculation code should remain modular and independent from
the UI where practical.

## Project Structure

The project intentionally keeps a simple structure.

``` text
astronomical-watch/
├── AGENTS.md
├── README.md
├── .gitignore
├── CMakeLists.txt
├── sdkconfig
├── dependencies.lock
└── main/
    ├── CMakeLists.txt
    ├── main.c
    ├── idf_component.yml
    ├── rtc.c
    ├── rtc.h
    ├── display.c
    ├── display.h
    ├── astronomy.c
    ├── astronomy.h
    ├── power.c
    ├── power.h
    ├── settings.c
    └── settings.h
```

The structure may evolve during development, but unnecessary
abstractions and components should be avoided.

See `AGENTS.md` for the project's development guidelines.

## Component Selection

When a new driver or dependency is required, the following priority
order is used:

1.  ESP-IDF built-in driver
2.  Official Espressif component
3.  Waveshare component
4.  Well-maintained third-party component
5.  Small custom driver

Keep external dependencies to a minimum.

The current project uses the Waveshare board BSP and the Waveshare
PCF85063A component. Component Manager resolves dependency versions and
maintains `dependencies.lock`.

Do not manually modify files under `managed_components/`; they are
managed by ESP-IDF Component Manager.

See `AGENTS.md` for the complete dependency selection guidelines.

## Motion Wake Validation

The board's QMI8658 IMU is being validated as the low-power motion-wake
source. INT1 is connected to GPIO21, an ESP32-S3 RTC-capable GPIO
suitable for the planned EXT1 deep-sleep wake path.

Validated hardware findings:

-   QMI8658 communication works at I2C address `0x6B`.
-   Normal accelerometer-only output is valid at 125 Hz, 62.5 Hz, and
    31.25 Hz with ±2 g range and the gyroscope disabled.
-   Stationary acceleration is clean and close to 1 g, while normal
    rotation and deliberate motion are clearly distinguishable.
-   `STATUS0.aDA` remains clear even while valid acceleration data is
    available, so it is not used as a data-ready gate.
-   Low-power 21 Hz Wake-on-Motion is valid with `CTRL2=0x0D`.
-   The earlier continuous stationary WoM interrupts were traced to an
    invalid `CTRL2=0x0A` configuration rather than sensor noise.
-   With the corrected configuration, stationary WoM testing can remain
    quiet and intentional wrist motion produces a genuine `STATUS1.WoM`
    event.
-   Reading `STATUS1` acknowledges genuine WoM events and returns GPIO21
    low.
-   A single physical movement can produce a finite burst of additional
    WoM events for a few seconds before settling.
-   A temporary status-less INT1 reassertion has also been observed
    after genuine WoM activity. GPIO21 may go high while `STATUS1=0x00`,
    then clear by time alone. This is treated as an
    `INT1_TOGGLE_EDGE_CASE`, not as a new motion event based only on
    GPIO level.

Deep-sleep wake has not yet been validated. The next motion/power
checkpoint is to preserve the known-good WoM configuration and verify
GPIO21 as an ESP32-S3 EXT1 deep-sleep wake source.

## Building

Activate the ESP-IDF environment before building:

``` bash
get_idf
```

Build the project:

``` bash
idf.py build
```

If the target needs to be configured:

``` bash
idf.py set-target esp32s3
```

## Flashing

The development board is currently connected through:

``` text
/dev/ttyACM0
```

Flash the firmware with:

``` bash
idf.py -p /dev/ttyACM0 flash
```

Open the serial monitor:

``` bash
idf.py -p /dev/ttyACM0 monitor
```

Or flash and monitor in one command:

``` bash
idf.py -p /dev/ttyACM0 flash monitor
```

The serial device may change depending on the host system.

## Clean Build

For a normal rebuild:

``` bash
idf.py build
```

For a complete clean build:

``` bash
idf.py fullclean
idf.py build
```

## Development Workflow

Development is performed incrementally.

After each significant milestone:

1.  Build the firmware.
2.  Flash the device.
3.  Test the functionality on the actual hardware.
4.  Confirm that existing functionality still works.
5.  Commit the working state to Git.

The Git history should therefore contain stable, hardware-tested
checkpoints.

Hardware behavior, especially power consumption and wake/sleep behavior,
should be measured on the actual board rather than assumed from software
configuration alone.

## Current Status

The ESP-IDF project is configured for the ESP32-S3 target and builds
successfully.

The current dependency set includes:

-   Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP
-   Waveshare PCF85063A
-   LVGL
-   ESP-LVGL port
-   Required Espressif and Waveshare BSP dependencies

Current development focus:

-   Validate QMI8658 INT1/GPIO21 as an EXT1 deep-sleep wake source
-   Define safe post-motion settling and re-entry to deep sleep
-   PCF85063A RTC integration on the shared BSP I2C bus
-   Display/LVGL initialization and display power control
-   Touch-ready light-sleep behavior using GPIO38
-   AXP2101 battery and power telemetry
-   Low-power display and deep-sleep management
-   Configurable display timeout and always-on behavior

## License

This project is intended for personal use.
