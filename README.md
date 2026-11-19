# Astronomical Watch

A personal low-power astronomical watch based on the **Waveshare ESP32-S3-Touch-AMOLED-2.06**.

It shows the time, date and astronomical information computed on the device. Location, date/time,
UTC offset and the other settings are entered on a setup page that the watch serves over its own
Wi-Fi for a few minutes (started on the watch), from any phone or computer browser; no internet,
GPS or online service is used. Battery life has priority over features: the processor sleeps most
of the time. Between interactions a dim
always-on face shows the time; when the watch has not moved for a while, it switches itself off
and the PWR key turns it on again.

## Features

- Time and date; a watch face of pages composed in the setup page: a digital clock (JetBrains
  Mono or seven-segment digits) or an analog one in five styles, large or among the Sun's and
  the Moon's orbits or their paths over the horizon (with the Milky Way and the planets), with up
  to four lines and four corners of information around it; a vertical swipe steps the pages
- Sunrise, sunset, solar noon, civil/nautical/astronomical twilight, golden and blue hours, day
  length, a sun compass
- Moon phase, illumination, age, moonrise and moonset, the new crescent, the Hijri date (by local
  crescent visibility or Diyanet's rule)
- Prayer times by Diyanet's method; solar, ezani and sidereal time, the qibla time
- Planets: Mercury to Neptune in the solar system and in the sky, their rise and set, their
  distance and light time
- A clock page: world clock (three cities that follow their daylight saving time), stopwatch
  with laps, countdown, Gregorian and Hijri calendars, day counters
- Gauges in the four screen corners, chosen in the setup portal: the battery, the day or night,
  the Moon's time above or below the horizon, its phase, the time to the next prayer, sunrise,
  sunset, moonrise or moonset, the week, month, Hijri month or year, a day counter, the Sun's
  altitude, the hour, minute or second; while charging, the battery gauge fills up in white
- Alarms at a clock time on chosen weekdays, or relative to a sun or moon event or a prayer time
  (sunrise, twilights, solar noon, sunset, moonrise, moonset), with an offset; the countdown
  rings too; the board has no vibration motor, so they beep through the speaker
- PWR key opens the interactive mode (touch, pages); after a configurable timeout the watch
  returns to a dim always-on face, updated once a minute, or switches off (setting)
- Switches off after a long time without motion (checked at wide intervals); the PWR key turns it on
- Switches off at 10 % battery, so that the clock keeps time for days; the PWR key still shows the
  watch until the display timeout
- Setup page over the watch's own Wi-Fi, started with a button on the watch's Setup page and
  stopped by the button, a save or a timeout (captive portal; a phone joins by scanning a QR code
  on the watch, or with the network name and password shown there): time and UTC offset from the
  browser, location from a country/city list or coordinates pasted from a map app, the face
  pages, the alarms and every other setting
- English and Turkish; software updates from a phone through the setup page, which also hands
  over the watch's event log and crash dump for a report
- The brightness set with a long press and a drag on the face; the picture mirrored for the
  other wrist

## Screenshots

| Always-on faces | Face pages |
|---|---|
| ![Always-on faces](docs/screenshots/en/always-on.gif) | ![Face pages](docs/screenshots/en/faces.gif) |

Every page and part, the default face pages, and the setup portal: **[English](docs/screenshots/en/README.md)** ·
**[Türkçe](docs/screenshots/tr/README.md)**. Taken from the watch in a fixed scene (Istanbul,
21 June 2026, 19:19); the always-on face is shown at full brightness, the watch dims it.

## Install and Update

Releases are on the project's release page (GitLab and GitHub), each with two files:

- `astronomical_watch-<version>-full.bin`: the first install over USB, one command
  (Python's esptool; if no port appears, hold BOOT while pressing PWR for the download mode).
  It also clears the settings, so it is meant for the first install only:

  ```bash
  pip install esptool
  esptool --chip esp32s3 -p /dev/ttyACM0 write-flash 0x0 astronomical_watch-<version>-full.bin
  ```

- `astronomical_watch-<version>.bin`: later updates from a phone, no computer needed: start the
  setup portal on the watch (Settings, Wi-Fi setup) and follow its "Software update" page.

## Hardware

Waveshare ESP32-S3-Touch-AMOLED-2.06: ESP32-S3R8, 410 × 502 AMOLED (CO5300), FT3168 touch,
QMI8658 IMU, PCF85063 RTC, AXP2101 PMIC, battery connector.
Pins, power tree and device notes are in [docs/hardware.md](docs/hardware.md).

## Software

ESP-IDF v6.1, C++17, LVGL, Waveshare board support package.

## Build and Flash

For development (see [CLAUDE.md](CLAUDE.md) for the release and battery-test builds):

```bash
source ~/.espressif/v6.1/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Host Tests

The astronomy code (`main/Astronomy.*`, `main/Hijri.*`) does not depend on ESP-IDF and is tested
on the computer against reference values from [Skyfield](https://rhodesmill.org/skyfield/) with
NASA JPL's DE421 ephemeris, and against the Hijri month starts published by Turkey's Diyanet:

```bash
sh tests/run_host_tests.sh          # needs only g++
```

The reference values are in the repository. To make them again (Python, about 20 MB):

```bash
sh tools/setup_reference.sh         # Skyfield and the ephemeris in tools/.venv
tools/.venv/bin/python tools/make_astronomy_reference.py > tests/astronomy_reference.csv
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
├── partitions.csv        flash layout (two 4 MB application slots for updates)
├── docs/screenshots/     pictures of the watch pages, in English (en/) and Turkish (tr/)
├── tools/guided_test.py  guided hardware tests
├── tools/screenshot.py   screenshots from the watch (development build)
├── tools/read_events.py  the watch's event log over USB (stability on the wrist)
├── tools/make_cities.py  city list of the setup page, from GeoNames
├── tools/setup_reference.sh  Python environment for the astronomy reference values
├── tools/make_fonts.sh   watch fonts (main/fonts/) from tools/fonts/, needs Node.js
├── tools/make_aod_glyphs.py  always-on bitmaps (main/AodGlyphs.c), needs Pillow (tools/.venv)
├── tools/make_gifs.py    README animations from the screenshots, needs Pillow
├── tools/portal_preview.py  the setup portal's pages on localhost with a fake watch; pictures
├── tools/make_corner_table.cpp  corner gauge pixel table (main/CornerTable.c)
├── tests/                host tests of the astronomy code and their reference data
├── main/                 application
└── archive/              earlier QMI8658 validator (reference only)
```

## License

MIT, see [LICENSE](LICENSE). Dependencies (ESP-IDF, the Waveshare BSP, LVGL) keep their own
licenses. The setup page's city list comes from [GeoNames](https://www.geonames.org/) (CC BY 4.0);
`main/dns_server.*` is ESP-IDF example code (CC0); `main/LandMask.h` is made from
[Natural Earth](https://www.naturalearthdata.com/) land (public domain). The watch font is
[JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) (SIL Open Font License 1.1,
`tools/fonts/OFL.txt`).
