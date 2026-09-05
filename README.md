# Astronomical Watch

A personal low-power astronomical watch based on the **Waveshare ESP32-S3-Touch-AMOLED-2.06** development board.

The project is designed for personal use rather than as a general-purpose smartwatch. Its primary goal is to provide a simple watch interface with astronomical information while keeping power consumption as low as possible.

## Features

Planned features:

* Digital date and time display
* Manual date and time configuration
* Manual latitude and longitude configuration
* Sunrise and sunset calculations
* Astronomical, nautical, and civil twilight calculations
* Solar noon calculation
* Moon phase display
* Moon illumination percentage
* Moon age
* Moonrise and moonset times
* Battery level and power information
* Low-power operation with deep sleep
* Touch-to-wake display
* Automatic display timeout

The device does not require a permanent network connection.

## Hardware

Target hardware:

**Waveshare ESP32-S3-Touch-AMOLED-2.06**

The board provides the main hardware required by the project, including:

* ESP32-S3
* AMOLED display
* Touch controller
* PCF85063 RTC
* AXP2101 power management IC
* Battery support

## Software

The project is built with:

* ESP-IDF
* C
* LVGL
* ESP-IDF Component Manager

All source code, comments, identifiers, and documentation are written in English.

## Power Management

Power consumption is a primary design requirement.

During normal operation:

* Wi-Fi is disabled.
* Bluetooth is disabled.
* GPS is not used.
* The AMOLED display is turned off when not needed.
* The ESP32 enters deep sleep when the display is inactive.
* A touch event wakes the device.
* The display remains active for approximately 10 seconds after interaction.

The firmware should avoid unnecessary periodic wakeups and background processing.

## Location

The user enters the geographic location manually using:

```text
Latitude
Longitude
```

For example:

```text
Latitude:  41.0082
Longitude: 28.9784
```

No GPS hardware is required.

## Astronomical Calculations

Astronomical calculations are performed locally on the ESP32.

The project does not depend on an online astronomy service.

Solar calculations will include:

* Astronomical dawn
* Nautical dawn
* Civil dawn
* Sunrise
* Solar noon
* Sunset
* Civil dusk
* Nautical dusk
* Astronomical dusk

Lunar calculations will include:

* Moon phase
* Moon illumination
* Moon age
* Moonrise
* Moonset

## Project Structure

The project intentionally keeps a simple structure.

```text
astronomical-watch/
├── AGENTS.md
├── README.md
├── .gitignore
├── CMakeLists.txt
├── sdkconfig
│
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

The structure may evolve during development, but unnecessary abstractions and components should be avoided.

See `AGENTS.md` for the project's development guidelines.

## Building

Activate the ESP-IDF environment before building:

```bash
get_idf
```

Build the project:

```bash
idf.py build
```

If the target needs to be configured:

```bash
idf.py set-target esp32s3
```

## Flashing

The development board is currently connected through:

```text
/dev/ttyACM0
```

Flash the firmware with:

```bash
idf.py -p /dev/ttyACM0 flash
```

Open the serial monitor:

```bash
idf.py -p /dev/ttyACM0 monitor
```

Or flash and monitor in one command:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

The serial device may change depending on the host system.

## Clean Build

For a normal rebuild:

```bash
idf.py build
```

For a complete clean build:

```bash
idf.py fullclean
idf.py build
```

## Development Workflow

Development is performed incrementally.

After each significant milestone:

1. Build the firmware.
2. Flash the device.
3. Test the functionality on the actual hardware.
4. Confirm that existing functionality still works.
5. Commit the working state to Git.

The Git history should therefore contain stable, hardware-tested checkpoints.

## Component Selection

When a new driver or dependency is required, the following priority order is used:

1. ESP-IDF built-in driver
2. Official Espressif component
3. Waveshare component
4. Well-maintained third-party component
5. Small custom driver

See `AGENTS.md` for the complete dependency selection guidelines.

## Current Status

The initial ESP-IDF project has been successfully built, flashed, and tested on the target hardware.

Current development focus:

* PCF85063 RTC integration
* Reading date and time from the RTC
* Displaying the current date and time
* Low-power display and sleep management

## License

This project is intended for personal use.
