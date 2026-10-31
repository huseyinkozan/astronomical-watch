# Astronomical Watch

A personal low-power astronomical watch based on the **Waveshare ESP32-S3-Touch-AMOLED-2.06**.

It shows the time, date and astronomical information computed on the device. Location, date/time,
UTC offset and the other settings are entered on a setup page that the watch serves over its own
Wi-Fi for a few minutes (started on the watch), from any phone or computer browser; no internet,
GPS or online service is used. Battery life has priority over features: the processor sleeps most
of the time. Between interactions a dim
always-on face shows the time; when the watch has not moved for a while, it switches itself off
and the PWR key turns it on again.

## Planned Features

- Time and date
- Sunrise, sunset, solar noon, civil/nautical/astronomical twilight, day length
- Moon phase, illumination, age, moonrise and moonset
- Battery level
- Alarms at a clock time or relative to a sun or moon event (sunrise, twilights, solar noon,
  sunset, moonrise, moonset), with an offset; a countdown timer; signalled by vibration
- PWR key opens the interactive mode (touch, pages); after a configurable timeout the watch
  returns to a dim always-on face, updated once a minute, or switches off (setting)
- Switches off after a long time without motion (checked at wide intervals); the PWR key turns it on
- Setup page over the watch's own Wi-Fi, started with a button on the watch's settings page and
  stopped by the button or a timeout (captive portal, password shown on the watch): time and UTC offset from the browser, location from a country/city list, a world map or
  coordinates, and every other setting

## Hardware

Waveshare ESP32-S3-Touch-AMOLED-2.06: ESP32-S3R8, 410 × 502 AMOLED (CO5300), FT3168 touch,
QMI8658 IMU, PCF85063 RTC, AXP2101 PMIC, battery connector.
Pins, power tree and device notes are in [docs/hardware.md](docs/hardware.md).

## Software

ESP-IDF v6.1, C++17, LVGL, Waveshare board support package.

## Build and Flash

```bash
source ~/.espressif/v6.1/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Project Structure

```text
astronomical-watch/
├── CLAUDE.md             development rules
├── README.md
├── CMakeLists.txt
├── sdkconfig.defaults    project configuration (sdkconfig is generated)
├── dependencies.lock     locked component versions
├── docs/hardware.md      hardware fact base
├── tools/guided_test.py  guided hardware tests
├── main/                 application
└── archive/              earlier QMI8658 validator (reference only)
```

## Status

Restarted on ESP-IDF with C++17. The watch puts the unused peripherals into their lowest-power
state, shows the UTC time, date and battery state on the display during a short awake window
and then switches itself off through the power-management IC; a PWR press of at least half a
second turns it on again. The always-on face is next.
The earlier Wake-on-Motion validation results are kept in `docs/hardware.md`, and the validator
itself in `archive/wom-validator/` (git tag `wom-validator`).

## Roadmap

Each step is built, verified on the board and documented before the next one starts.

1. **M1** (done): PWR key wake from deep sleep, wake statistics kept across sleep.
2. **M2** (done): unused peripherals in their lowest-power state, battery drain runs (about 4 mA
   remain in deep sleep; the search continues in M5).
3. Off mode (done): software power-off through the AXP2101 and power-on with the PWR key; the
   battery charges while the watch is off.
4. **M3** (done; crystal drift measurement running): RTC (PCF85063) time in UTC, set from the
   host, kept through off mode.
5. **M4** (done): display basics (time, date and battery through LVGL; panel power on ALDO2).
6. **M5**: always-on face with a minute-aligned wake, validated with a battery run; wake time
   measured, with and without `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP`.
7. **M6**: motion check at wide intervals.
8. **M7**: active mode (touch, pages, display timeout).
9. **M8**: settings in NVS and the settings page on the watch (setup button, password and IP
   address).
10. **M9**: setup portal over Wi-Fi (time, UTC offset, location, all settings); wake time measured
    again with the larger image.
11. **M10+**: astronomy, battery display.
12. Vibration motor.
13. Alarms (clock time and sun/moon events) and countdown timer, also set through the portal.

## License

Personal use.
