# Astronomical Watch

A personal low-power astronomical watch based on the **Waveshare ESP32-S3-Touch-AMOLED-2.06**.

It shows the time, date and astronomical information computed on the device. Location, date/time
and UTC offset are entered manually; no network, GPS or online service is used. Battery life has
priority over features: the watch sleeps most of the time and wakes on wrist motion or the PWR key.

## Planned Features

- Time and date
- Sunrise, sunset, solar noon, civil/nautical/astronomical twilight, day length
- Moon phase, illumination, age, moonrise and moonset
- Battery level
- Wake on wrist motion or PWR key, configurable display timeout, optional always-on display

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
├── main/                 application
└── archive/              earlier QMI8658 validator (reference only)
```

## Status

Restarted on ESP-IDF with C++17. The project skeleton builds and runs on the board; it logs the
boot count and wake cause.
The earlier Wake-on-Motion validation results are kept in `docs/hardware.md`, and the validator
itself in `archive/wom-validator/` (git tag `wom-validator`).

## License

Personal use.
