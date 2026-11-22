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

For use on the wrist, a release build skips the development logging (no wait for the USB host at
each PWR wake, about 1.5 s on battery) and optimises for speed; the event log and the core dump
work as in the development build:

```bash
idf.py -B build-release -D SDKCONFIG=build-release/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;configs/release" build flash
```

Versions are annotated git tags `vX.Y.Z` (from v0.0.1): ESP-IDF names the image with
`git describe`, which takes only annotated tags, so a release shows `v0.0.1` and later
development builds `v0.0.1-3-gabc1234-dirty`. A new release is a new tag, pushed with approval.

`sdkconfig` is generated from `sdkconfig.defaults` and committed. Change options in
`sdkconfig.defaults` (or with `idf.py menuconfig` and copy the decision there).

## Repository Layout

| Path | Content |
|---|---|
| `main/` | Application component (`main.cpp` and project modules) |
| `docs/hardware.md` | Hardware fact base: pins, power tree, register semantics, sources, unknowns |
| `configs/` | Build variants added to `sdkconfig.defaults` with `-D SDKCONFIG_DEFAULTS="sdkconfig.defaults;configs/<name>"`: `release`, battery runs (`battery` and its add-ons), validators (`sound`, `sweep`) |
| `tools/guided_test.py` | Guided hardware tests: prompts the tester and checks the serial log; logs go to `test-logs/` (ignored) |
| `tools/screenshot.py` | Screenshots of the watch pages from the development build |
| `tools/read_events.py` | Reads the watch's event log (stability on the wrist) |
| `tools/portal_preview.py` | Serves the setup portal's pages with a fake watch on localhost; pictures of them |
| `tools/make_corner_table.cpp` | The corner gauges' pixel table `main/CornerTable.c` (built by the host tests, which check it is current) |
| `tools/make_fonts.sh`, `tools/fonts/` | The watch fonts in `main/fonts/` (JetBrains Mono, ASCII, ° and the Turkish letters; needs Node.js for `npx lv_font_conv`) |
| `tests/` | Host tests of the astronomy code (`sh tests/run_host_tests.sh`, needs only g++) and their reference data |
| `tools/setup_reference.sh`, `tools/make_*.py` | Python environment (Skyfield, Pillow in `tools/.venv`, ignored) and generators of reference data, tables, the always-on bitmaps (`main/AodGlyphs.c`) and the README animations |
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
- Text uses the monospaced fonts of `main/Fonts.h` (26 and 48 px), so that numbers line up in
  columns; a new character or size goes into `tools/make_fonts.sh`, and the C files are made again.
- Every text the user sees comes from `main/Language.*` in English and Turkish (the portal page
  keeps its Turkish beside the English in `data-tr`); a new text gets both, short enough for the
  rows in both languages. A change of language restarts the watch, so the pages are built in it.
- LVGL's own printf has no floating point (`lv_label_set_text_fmt` with `%f` crashes): format
  with `snprintf` and set the text.
- No call with an effect inside an `ESP_LOGx` argument: the release build (log level WARN)
  drops `ESP_LOGI/D/V` with their arguments. Call first, then log the result (an update was
  never confirmed in release builds up to v0.0.8 this way).
- All code, comments, identifiers, commit messages and documentation are in English.

## Power Management Rules

- Bluetooth stays disabled. GPS is not used. Wi-Fi runs only for the setup portal: started by
  the user on the watch, on battery or USB power, for a limited time (a soft AP draws roughly
  100–150 mA, so a 5 min setup costs about 6–8 % of the cell, which is labelled 400 mAh but
  behaves like about 150 mAh (docs/hardware.md, run R); an estimate).
- The watch has three modes, and a variant of off while an alarm is pending:
  - **Active**: display on, touch and pages. The PWR key enters it from the other two modes. After
    the display timeout without interaction (default 10 s; user-selectable 5, 10, 20, 30, 60 s)
    the watch goes to always-on, or straight to off when always-on is disabled in the settings.
    A PWR press in active mode does the same at once. Touch cannot wake the watch (GPIO38).
    A touch reported as pressed without a break for 60 s ends active mode too (a stuck touch
    controller would keep the display on until the battery is flat). While the torch (tools
    page) shows, its own 2 min auto-off replaces the display timeout (a tap restarts it), and at
    0:00 active mode ends as at a timeout; so does the running metronome, up to 10 min after the
    last tap on its part (`Display::toolSecondsLeft`).
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
    for the next alarm, waking a little early (2 s and 0.2 % of the sleep) to re-read the
    PCF85063 because the internal RC timer drifts, and at least once an hour to read the
    battery. Every power-off but the battery floor becomes this sleep without the face: a wake
    0.5 s later puts the panel to deep standby (`Panel::sleep`) and cuts ALDO2. In always-on the
    minute wake checks the due times and ends always-on for them. The due times are planned
    in active mode and kept in RTC memory (`main.cpp`, `s_plan`); the ones rung or skipped in
    RTC_NOINIT memory, so that a reset in the alarm minute neither loses nor repeats it.
  - **Battery limit**: at 10 % or less on battery the watch powers off instead of entering or
    keeping always-on, so that the cell keeps the RTC running for days (off state about 40 µA,
    about 1 mAh a day; an estimate). VBAT must also be at most 3.65 V, because the gauge reads
    too low for a while after a power cycle; VBAT at most 3.45 V alone is enough, for a gauge that
    stays high (`main/BatteryLimit.*`, host-tested). Below that the AXP2101 powers itself off at
    VSYS 3.0 V (REG24, set at each active start) should the firmware never get there. The PWR key
    still starts active mode, and the watch
    powers off again when it ends. The battery gauge shows red at the limit. Battery-log builds
    ignore the limit. With an alarm pending the watch goes on below the limit without the face,
    down to 2 % with VBAT at most 3.50 V (or VBAT at most 3.40 V), then powers off (the owner,
    2026-11-14: the RTC must keep its time).
- Always-on must use the lowest-power approach the panel allows (low brightness, few lit pixels,
  idle or partial mode where they help), never a CPU that stays awake. Do not call any mode
  low-power without evidence from the board (see "Power evaluation").
- Deep-sleep wake sources: the PWR key (GPIO10, active high, EXT1), the minute timer in
  always-on and the alarm timer while an alarm is pending. In off mode the AXP2101 itself powers
  the watch on from the PWR key. Motion does not wake the watch: the QMI8658 is powered down
  between motion checks (and in active mode except while the spirit level or the alidade shows)
  and its INT1 is not used. Touch (GPIO38) and the RTC interrupt (GPIO39)
  cannot wake from deep sleep. A board whose PWR line lacks its pull-up resistor (R11) is found
  by the AXP2101's key flags in active mode and gets the internal pull-up from then on
  (docs/hardware.md). A PWR wake from always-on or the alarm sleep that the AXP2101 did not
  latch as a press (REG49 bit1) while the key is up is a pulse on GPIO10 alone: the watch goes
  back to that sleep without the display (seen on another project's board, not on ours).
- Avoid periodic wakeups and background tasks unless they provide a necessary function.
- Settings are persistent, simple and limited to meaningful user-facing trade-offs.

## Architecture

Intended runtime flow: wake → initialise only what is needed → read time from the RTC →
by wake source:
- PWR key: active mode → update astronomy data when required → render → handle touch → after the
  timeout draw the always-on face and dim the panel.
- Minute timer: update the always-on face; when due, check motion and power off if the watch
  has not moved.
- Alarm timer: re-read the time; when an alarm is due, ring and show it, otherwise sleep the
  remaining seconds.

Then deep sleep with the minute timer (always-on) or the alarm timer (alarm pending), or power
off through the AXP2101 (off mode).

Always-on wakes never start the BSP display (it resets the panel and would blank the image);
they open the panel directly, redraw only what changed and hold the panel pins again
(`main/Panel.*`, `main/AlwaysOn.*`). Active mode starts LVGL in `main/Display.cpp`, not with
`bsp_display_start()`: the BSP adds the QSPI panel as an RGB panel, so LVGL draws into the
buffer while it is still being sent (docs/hardware.md, Display). It opens the panel with
`Panel::openForLvgl`, not `bsp_display_new()`, whose last init command sets full brightness and
flashed the old always-on image; the panel stays dark until LVGL's first frame. LVGL renders
only in its own task (the main task's stack overflows), and allocates from the ESP-IDF heap.

The watch face is a list of face pages (`main/FaceConfig.*`, at most eight, kept in NVS by
`main/FaceStore.*`, composed in the portal; design: `docs/superpowers/specs/2026-11-18-face-templates-design.md`).
A page is a template (Large; Full arcs: the Sun's and the Moon's orbits around a small clock;
Half arcs: their paths over the horizon, a small clock below and the Milky Way band, turned by
the galactic centre's azimuth, `main/MilkyWay.*`; on both, an option shows the seven planets
between the Sun's and the Moon's arcs, `Planets::arcPlace`; the places of the bodies in
`Corners::arcPlace`, each template's clock in `FaceConfig::clockPlace`) with a clock (digital in JetBrains Mono or in seven segments
of our own geometry, `main/SevenSegment.*`; or analog in five styles, `main/AnalogClock.*`, the
seconds hand only on the active face) and eight slots in fixed places: four
text lines (top edge, top, bottom, bottom edge) and the four inner corners, filled from
catalogues (the corners also icons: the Moon's phase, the weekday ring and the time icon: the
hourglass while a timer runs, else while an alarm is on the bell inside the ring of the next due
alarm's days, else the bell dim; `main/FaceText.*`; today's and tomorrow's events, the counters and cities in
`main/FaceTable.*`), each marked whether the always-on face shows it too. A text longer than its
slot (`FaceConfig::textWidth`) is cut to whole characters, a name before its number.
`main/FacePage.*` draws a page in active mode. A short tap on it opens a part as the launcher
does (`main/FaceTap.*`, host-tested; not after a swipe or a long press): the Sun or the Moon on
the arcs, a corner icon, else the corner's gauge (an L along both edges), a text slot, the ring
between the arcs (the planets' sky), the clock (the world clock); each item opens its own part
(the time icon the running countdown or stopwatch, else the alarms), a hidden part the page's
first one. The always-on face (`AlwaysOn`) draws the page
shown when active mode ended, from 1-bit bitmaps made on the host (`tools/make_aod_glyphs.py` →
`main/AodGlyphs.c`: both digit sets at the Large size and the watch font's characters at 26 px);
the first minute wake of a new local day computes the slots' events of today and tomorrow
again and moves the Hijri date on (`FaceTable::follow`). After a change, regenerate the
file and compare both faces with `tools/screenshot.py` ("F <page>", "A <page>").

The four screen corners are gauges on every page and on the always-on face, each showing an item
chosen in the portal, the same on every page (`FaceConfig::GaugeItem`, kept after the pages: the
day or night, the battery, the Moon's pass or phase, the next prayer time, sunrise, sunset,
moonrise or moonset from the previous one, the week, the month, the Hijri month, the year, a day
counter (since it was saved, yearly, the last 30 or 100 days), the Sun's altitude, the hour, the
minute or the second, which stays empty on the always-on face). `main/Corners.*` computes them
(host-tested rules), `main/CornerView.*` draws them on LVGL's top layer, `AlwaysOn` with its own
bitmaps. The defaults: top left the day (yellow, sunrise to sunset) or the night (blue), top right the
battery (green, orange above 10 %, red at the power-off limit; while charging, the part above the
value is white: in active mode it grows step by step to the full end and starts again, on the
always-on face it is drawn solid from the next minute wake), bottom left the Moon above the
horizon (bluish white, rise to set) or below it (dark grey, set to rise), bottom right the
Moon's phase (the lit part grows from the lower end and shrinks towards the upper end). Each is
a quarter circle 3 px inside the visible edge, starting at its lower end, with a notch in the
middle as long as the gauge is wide. Active mode draws them 5 px wide with round ends in a
channel of a dim tone of their own colour. The always-on face draws them 2 px wide without the
channel in idle-mode colours, with notches at both ends too, outside the burn-in shift, and redraws a corner only when its
gauge moved by a pixel. In active mode the
gauges are no LVGL objects (four arcs cost a quarter of each frame while swiping): `CornerView`
writes the ring's pixels into each rendered band, their colours computed from a table of the
ring's pixels in flash (`main/CornerTable.c`, made by `tools/make_corner_table.cpp`; the host
tests fail when it is stale), which the always-on face reads too. The Moon's phase icon is a corner item; the default pages
put it in the bottom-left corner, inside the Moon's gauge (the top-left corner stands for the Sun).

Brightness: active mode has one level, the always-on face one by day and one by night, switched
at sunrise and sunset (`Corners::night`; no light sensor on the board). A long press on the watch
face shows both levels it can set, the active one with a vertical bar and the always-on one of
the moment with a horizontal bar; the first 20 px of the drag choose one and hide the other. The
portal sets all three. A PWR press while that drag is held (its direction chosen) mirrors the
picture along it instead of ending active mode (Settings::Mirror, also in the portal; both
directions turn it around): the panel's own MADCTL bits, the touch points mirrored to match
(docs/hardware.md, Display).

Red night mode (Settings::NightMode, Auto by default, on the settings page and in the portal):
from civil dusk to civil dawn (the Sun below -6 degrees, `Corners::dark`, its events kept with the
always-on face) everything is shown in red of the same brightness, so that the eye stays used to
the dark; never without a location. Active mode converts each band just before it is sent
(`NightRed`, host-tested, after the night palette of mkvesala/ESP32-Waveshare-SkippersWatch, MIT),
except while the Wi-Fi QR code shows; the always-on face draws every lit pixel in idle-mode red.
Development command "I 1" forces it on, "I 0" back to the setting.

Event log (`main/EventLog.*`, NVS): boots with their reset reason, the end of each active session
(touches, touch controller state), the end of always-on and power-offs, with battery, smallest
heap and stack margins. Written once at the event, never at every minute wake; the development
mode setting (settings page) adds an always-on summary every 30 min. A crash leaves a core dump
in its flash partition; so does a task that starves the idle task for 5 s (the task watchdog
resets the watch instead of only warning; development command "W" tests it). Read both over USB: `python tools/read_events.py`, then
`idf.py -p /dev/ttyACM0 coredump-info`.

- Small, focused modules; no layers or abstractions without a clear benefit.
- Astronomy code is independent of UI and ESP-IDF so it can be tested on the host
  (`main/Astronomy.*`, `main/Hijri.*`, `main/Sky.*`, `main/Corners.*`, `main/Planets.*`;
  `sh tests/run_host_tests.sh`). Its reference values come from Skyfield with the JPL DE421 ephemeris
  (`tools/make_astronomy_reference.py`, `tools/make_planet_reference.py`,
  `tools/make_planet_risings.py`); the Diyanet Hijri method is checked against Diyanet's
  published month starts (`tests/diyanet_month_starts.csv`). Change the code only with the tests
  passing; a new tolerance or known difference needs a reason in the test.
- Hijri months start by local crescent visibility (Odeh's criterion at the watch's location) or
  by Diyanet's 1978 Istanbul rule (anywhere on land), chosen in the portal.
- Settings are separate from UI code and persist across reboots, deep sleep and off mode (NVS,
  `main/Settings.*`); the Kconfig values are their defaults. Test builds that need other values
  set `CONFIG_WATCH_TEST_SETTINGS`, which ignores the saved settings.
- Touch input: rows of 7.8 x 26 mm inside the corner gauges, one tap steps a value; no vertical scrolling
  inside the pages but the launcher's list; sliding off a row cancels the tap. Three tiles side by side
  (`main/Display.*`): the watch face, the launcher and the page last opened from it (a trial, 2026-11-21).
  A wake starts at the face; a swipe shows the launcher, a second one the opened page. A PWR wake
  within 60 s after active mode ended (RTC memory kept: always-on or the alarm sleep;
  `s_resumeSeconds` in `main.cpp`) shows the tile and part shown then. The launcher
  shows "Pages" at the top like the stacked pages, under it a list that scrolls with momentum: each
  stacked page's name in grey, then its parts as rows (68 px high, six fit, one width, a faint
  1 px outline), the pages and their parts in the portal's order (`Settings::PagesOrder` and the
  part orders) without the hidden parts; a tap opens that part on the third tile and slides there.
  Off screen, the list is scrolled so that the last opened part's row is the second from the top,
  its outline brighter; a mark at the right edge, like the part marks, shows the scroll position.
  Near the corners the rows on screen narrow, centred, and the page names move in, so that
  they stay a few pixels clear of the gauges, dimming as they do (at the top as they slide under
  the page name, their own hidden top edge deciding; colours, not LVGL opacity;
  `Display::narrowLauncherRows`, in the list's scroll event; the owner's choice of 2026-11-21). The stacked pages: clock (world clock, stopwatch, countdown,
  Gregorian and Hijri calendars, day counters, alarms; `main/ClockPages.*`), information (sun,
  twilight, prayer times, moon, compass, time, battery, about; the Sun part shows the time to
  sunset or sunrise and, while the Sun is up, a turnaround time, now plus half the daylight left,
  `Sky::turnaround`; the Time part's UTC row names its date when it is not the local one),
  planets (the seven planets and the
  Earth: the solar system on a logarithmic scale, the sky seen from the south, the day's rise and
  set, distances; `main/PlanetPages.*`, the day's rise and set computed in the main task on the
  page's request, 0.4 s; design: `docs/superpowers/specs/2026-11-19-planets-design.md`) and
  settings (settings, more settings, what the portal set, setup portal, its QR code; the setup
  part always shown), tools (torch, calculator, unit converter, spirit level, counter, random,
  metronome, emergency card; the metronome hidden by default, the card while it is empty;
  `main/ToolPages.*`, the last two in `main/ToolParts.*`, the counter's rules in `main/Tally.*`, the SOS timing in `main/Morse.*`, the
  calculator's expressions in `main/Calculator.*`, the conversions in `main/Units.*` and the
  level's angles in `main/Tilt.*`, all host-tested; plan:
  `docs/superpowers/plans/2026-11-21-tools.md`) and observing (polar alignment, Milky Way window,
  deep-sky finder, eyepieces, night photo, nautical almanac, astrolabe, aiming; the almanac and
  aiming hidden by default; `main/ObservingPages.*` with the
  astrolabe, `main/SkyParts.*`, `main/OpticsParts.*` and `main/NavParts.*` with the others; the astrolabe's geometry
  and the alidade's answers in `main/Astrolabe.*`, the pole star, darkness, Milky Way window and
  Messier list in `main/Observing.*` and `main/Messier.*`, the formulas in `main/Optics.*`, the
  almanac in `main/Almanac.*`, the dish, the Sun's direction and the panel in `main/Aiming.*`, the
  magnetic variation in `main/Magnetic.*` (WMM2025, NOAA's coefficients, `tools/make_wmm_table.py`), all
  host-tested, the sky against Skyfield (`tools/make_astrolabe_reference.py`,
  `tools/make_observing_reference.py`, `tools/make_navigation_reference.py`), WMM against NOAA's
  test values; a page whose part list is empty stays out of the launcher
  and the portal's lists by itself). The `Page` numbers are kept in
  the saved launcher order, so new pages come last in the enum; the default order puts tools and
  observing after information. The torch fills the screen (the page name hides): red, amber,
  white, SOS, SOS with beeps (`Ringer::startSos`, the alarm's lowest level, on the light's clock;
  an alarm stops them); a long press, then a drag: sideways the colour (round the list), up or down the panel's
  brightness while it shows (kept in RTC memory; `Display::updateBrightness` also dims the QR
  code); the night-red mode leaves it as it is. The counter's count, target and first minute are
  settings written when active mode ends, not at each count; a long press on the count resets it.
  The calculator types an expression on a keypad drawn by one object (`main/Keypad.*`, 5 rows of
  4 keys of 78 x 58 px, C and = in amber), its live result in grey above it, the result after "=";
  fn steps digits, functions, inverse functions (fn keeps its place); degrees by default, DEG/RAD
  on the functions; a function key brings the digits back; parentheses left open close at the
  end; errors show "Error". Its text and modes stay in RTC memory. The unit converter shows one
  kind (angle and time, distance, temperature), stepped by a tap on its title; a tap on a value
  opens the same keypad over the part (h:m:s: the point key writes ":"; C or "=" on nothing goes
  back, a part swipe too). No speed-time-distance solver (the calculator does it in one line).
  The spirit level reads the accelerometer 25 times a second only while its part shows
  (`Motion::setStreaming`, about 56 Hz; powered down when the part, the page or active mode ends;
  `Display::updateBrightness` decides it with the torch), filtered (`Tilt`, a quarter per sample)
  and in the picture's directions (the sensor's axes, docs/hardware.md, flipped with
  Settings::Mirror). Its modes, stepped by a tap on the title: the level (a bubble on rings of 1,
  5, 15, 30 and 45 degrees, square-root scale, the 30-45 degree band in dark amber; pitch, roll
  and the slope, amber in the band) and sighting along the top edge (the edge's altitude, the
  same from either end): a height from a distance or a distance from a height, entered with the
  keypad over the part (metres, from eye level, kept in RTC memory); a tap on the angle holds the
  value from the moment the finger touched.
  The metronome (30-240 bpm; - and +, a long press steps by 5; a CPR key sets 110; a tap on the
  dots steps the beats in a bar: none, 2, 3, 4, the first beat at 1500 Hz) beats through
  `Ringer::startBeats` at the alarm's lowest level (the sound task plays each beat by its time,
  a new tempo only moves the clock; the codec runs only while it plays). It stops when its part
  is left or active mode ends. The emergency card (`main/EmergencyCard.*`, an NVS blob of seven
  31-byte texts in the watch font's letters) is a centred `TablePart` under a red title; the
  portal reads and writes it with its own request (`/api/card`, before the save: the save's form
  and the state answer are near the 4 KB buffer). Development commands "B beat", "B bar",
  "B cpr", "B tempo <bpm>".
  The astrolabe has five modes, stepped by a tap on the title: the dial (a northern astrolabe's
  stereographic projection: the plate for the location with the horizon, almucantars every 15
  degrees, the equator and the tropics, cut at the far tropic; the rete with the ecliptic, the
  Sun and 20 bright stars of a J2000 table precessed to the date, white above the horizon, grey
  below; south of the equator the signs change; no star names), where a sideways drag turns the
  time by 4 min a pixel up to a day either way and a tap returns to now (the page swipe stops in
  this mode: `ObservingPages::swipesSideways`, asked again after a mode change through the tile's
  LV_EVENT_VALUE_CHANGED); the alidade on the Sun (the top edge's altitude, aimed by its shadow,
  gives the local time, the morning or afternoon solution nearest the clock; geometric, no
  refraction) and on Polaris (the latitude, corrected by its hour angle); both read the
  accelerometer like the level and only in these modes, a tap holds the angle; a horizontal
  sundial (hour lines 6 to 18, the shadow of now, solar and clock time); the analemma (the year's
  declination against the equation of time, today marked, the noon altitude). The plate's lines
  are computed once per latitude into a buffer allocated while the page is built.
  Polar alignment draws a polar scope's reticle as a 12-hour clock with the pole star (Polaris,
  sigma Octantis south of the equator; precessed, no nutation or aberration: within 0.6 degree
  of hour angle), inverted by default as most polar scopes show it; a tap flips it upright (RTC
  memory). The Milky Way window: astronomical darkness from the Sun's hour angle, the galactic
  centre 10 degrees up, the Moon's centre down or under 10 % lit (sampled every 5 min), at least
  30 min; on a 16:00 to 08:00 bar, with the centre's highest, the Moon and the next night with a
  window (up to 120 nights; from late November at 41 degrees north the next is in March). It is
  computed only while its part shows, asked for 0.4 s after it slid in (`lv_timer`), once a night
  and place, in the main task like the planets' day (131 ms measured for a June night at 41 degrees; the
  longest searches about ten times that, by the host's ratio). The deep-sky finder lists the 110 Messier objects (NASA HEASARC, public domain; `tools/make_messier_table.py`,
  packed in 660 bytes; common names in `Language::messierName`) 10 degrees up, highest first,
  with the altitude and the best time in tonight's darkness, again each minute while shown. Its
  list is the pages' second vertical scroll after the launcher's: one drawn object in a scrolling
  container without elastic ends; a drag of 60 px past either end steps the part
  (`StackPage::step`), and a list that fits leaves its swipes to the page. The eyepiece and photo
  parts (`TablePart`) read the equipment from the settings each time they show (the portal's
  "Observing equipment" box: telescope focal and aperture, four eyepieces in tenths of a mm with
  their apparent fields, the lens's focal and f-number, a sensor preset with its crop and pixel
  pitch, the ND calculator's base shutter and stops, a star-trail series' length and interval);
  a tap steps the eyepiece, a tap on the photo part's title its calculator (no star trails: 500
  and NPF rules; ND filter; star trails: frames and arc), both kept in RTC memory. The nautical
  almanac (`TablePart`, updated each second while shown) shows one body, a tap steps it (the
  Sun, the Moon, Venus, Mars, Jupiter, Saturn: the navigational planets): GHA and declination at
  the UTC second as an almanac writes them ("112° 34.1'", "N 23° 26.2'", Turkish K/G), the Sun's
  semi-diameter, local apparent noon and noon altitude, the Moon's semi-diameter, horizontal
  parallax and meridian passage, a planet's SHA and meridian passage (within 0.4' for the Sun,
  2.6' for the Moon, 1.1' for the planets). Aiming (`TablePart`, once a minute while shown): a tap
  on the title steps dish and panel; the dish's satellite (a tap on its row: Türksat 42° E and
  31° E, Astra 19.2° E, Hotbird 13° E, Astra 28.2° E, Nilesat 7° W, and the portal's own
  longitude first when it is not one of them, `Settings::SatelliteLongitude`): elevation, true
  azimuth, the magnetic variation, the compass bearing (true minus variation) and the time the
  Sun stands in the dish's direction today; the panel facing the equator: the latitude as the
  year's tilt, 90 minus the noon altitude as today's (negative when the Sun passes on the other
  side of the zenith), its azimuth and compass bearing. The variation and the Sun's time are
  computed again for a new day, place or satellite; the choices are kept in RTC memory.
  Development commands "B flip", "B scroll <px>", "B eyepiece", "B calc", "B body", "B aim",
  "B satellite".
  The page and part last opened stay in RTC memory over the always-on wakes (lost at power-off:
  then the clock page's first part). The BOOT key (GPIO0, polled in `runActiveMode`, never a
  wake source) steps back: opened page to launcher, launcher to face, on the face it closes the
  brightness control; on the tools page's counter it counts instead (`Settings::BootCounter`,
  "BOOT counter" on the settings page and in the portal; the back chevron still goes back); it stops a ringing alarm like PWR; a press of 1 s or more is reserved (a
  log line). A back button does the same on the launcher and the opened page: a grey chevron at
  the left edge's middle between the left corner gauges, a 40 x 120 px tap zone that takes taps
  only (swipes from it still scroll the list, step the parts and the calendars, swipe the tiles). A vertical swipe steps the face page on the watch face and the part on
  the stacked pages (`main/StackPage.*`); the pages keep `LV_EVENT_GESTURE` and do not scroll, and
  the new page or part slides in. The stacked pages show their name at the top between the
  upper corner gauges, over a line bending up to them; their parts start `StackPage::HeaderPad`
  lower. On a calendar a sideways swipe steps the month, so the page
  swipe stops there ("<" ">" at the bottom say so; BOOT still goes back); a tap chooses a day, marked on both calendars
  until the next wake (the Hijri month of a day chosen on the Gregorian one is computed then).
  The Hijri calendar marks Diyanet's religious days with a green dot (`main/HolyDays.*`,
  host-tested against Diyanet's 2026 calendar: the kandils on their evenings, Regaip the
  Thursday before Rajab's first Friday, Ramadan's first day, both bayrams, the Hijri new year,
  Ashura) and names the next one with the days left; below it the Ramadan count in Shaban and
  Ramadan, else the method. main.cpp searches the next one with the watch's Hijri method once a
  day when the calendar asks for a month (up to `HolyDays::MaxMonths` months; RTC memory).
  The countdown's title is a button: a tap switches to the regatta start (`main/Regatta.*`,
  host-tested): 5:00, Sync rounds the time left to the nearest minute, short 1000 Hz beeps at
  the alarm's lowest level at 4:00, 1:00 and the start (`Ringer::signal`), serviced every 20 ms
  in active mode (`ClockPages::serviceRegatta`), which stays on until the start (PWR still ends
  it, and the signals with it); no alarm rings for it. Only the
  watch face and the launcher are built at a PWR wake; the opened page is built when the launcher is
  shown and freed at the face, and only one stacked page is built at a time (`Display::keepPages`,
  `ensureApp`; a page's values, timers and part survive its objects): a glance at the time built
  1.6 s of pages before the lazy build, and the built pages left too little heap for the portal's
  Wi-Fi. Their
  tables are one object per part (`main/TablePart.*`) and repeated looks share their styles
  (`main/Styles.*`), so that the internal heap stays free (docs/hardware.md). Marks along
  the right edge, in line with the corner gauges, show the position (`main/PageMarks.*`; on the
  face only for a moment after a step); marks along the bottom edge show the tile while the
  tiles are swiped sideways. Large dark-grey areas shimmer on
  this panel; use black with outlines.
  The panel hides each corner outside a quarter circle of about 120 px radius (docs/hardware.md,
  Visible area); keep content inside it.
- Time zones (`main/Zones.*`, `main/WorldClock.*`): the portal's browser computes each zone's
  offset now and its next two changes from the IANA name (city list from GeoNames), so the watch
  follows daylight saving time for a year without a zone database; the own zone's numbers are
  settings (the always-on wakes keep a copy in RTC memory). Each portal save that sets the clock
  also corrects the PCF85063's rate (Offset register, `Settings::RtcOffset`) from its error since
  the last set, when at least two days have passed.
- Prayer times (`Sky::prayerTimes`): Diyanet's method, the Sun's values taken once a day like
  Diyanet; checked against its published 2026 tables (`tests/PrayerTest.cpp`, all within a
  minute). The prayer part shows the current or next kerahat interval (`Sky::kerahat`, Diyanet's
  fetva on its table's times: sunrise to +45 min, dhuhr -10 min, maghrib -45 min; after maghrib
  tomorrow's morning as today's a day later). Research notes on this and other topics live in `research/` (excluded from git).
- Setup portal (M9, `main/Portal.*`, `main/portal/`): a row on the watch's Setup page starts it
  (not every charge); the same row stops it, and it stops by itself 2 min after the last browser
  request (the pages ping it every 10 s while they are visible; a page switched away has 2 min to
  come back) or when the page saves (not after a face page's "show on the watch", which saves the
  faces, shows that page and keeps the portal on for the next change). The watch opens a WPA2 soft AP with a captive portal (DNS
  answers every name with the watch's address, so phones and computers open the page by
  themselves; otherwise the IP address shown on the watch is typed in). The password is four
  random digits, each written twice (e.g. 33556688, easy to type), made on the watch, kept in
  NVS and shown on the Setup page; a "New password" button replaces it. The part below it shows
  the network as a Wi-Fi QR code (`WIFI:T:WPA;...`) while the portal runs, for a phone's camera:
  297 px on a mid-grey ground with the panel at 40 % while it shows (a white ground at the
  face's brightness glared, and a phone's Wi-Fi QR reader failed), drawn by one object from
  qrcodegen's module table (LVGL's qrcode widget kept a 13 KB bitmap the portal's Wi-Fi needs). The page sends the browser's UTC time (the shortest of five round trips
  measured, the RTC started on the true second) and UTC offset (fixed; daylight saving time
  changes are set again through the portal), a location (country/city list from GeoNames, or
  coordinates pasted as map apps copy them; no map; browsers give no geolocation to a plain HTTP
  page) with a name for it (shown on the Sun and prayer pages) and every setting; the world clock shows three of the up to four cities NVS keeps.
  The faces show local time. The page also takes a new application image (on USB or at least
  30 % battery; only this project's images): two application slots (`partitions.csv`; nvs and
  phy_init keep their offsets, so settings survive a change of the table), and the bootloader's
  rollback: a new image is confirmed once active mode has drawn its pages, a reset before that
  returns to the old one, and the update ends with a full restart because deep-sleep wakes skip
  that check. The page offers the event log and the core dump for download
  (`tools/read_events.py --file`). Besides the setup page, help.html explains the watch and
  tools.html (`/tools`, "?tr" for Turkish) each tool of the Tools and Observing pages, in the
  launcher's order; a new tool gets its section there. The pages go into the image gzipped at
  build time (`main/CMakeLists.txt`), sent with `Content-Encoding: gzip`: 51 KB with the city
  list. The
  Wi-Fi stack stays out of IRAM (sdkconfig.defaults) so that the deep-sleep wakes load little more; if they still grow too much, a build option
  (`CONFIG_WATCH_SETUP_PORTAL`) removes the portal and the watch's own settings page remains.
- Alarms are stored as definitions (clock time, or sun/moon event plus offset; repeat) in NVS,
  never as a computed time. The next occurrence is computed from the definition, location and
  UTC offset, and again after each firing and after any change to them. An event that does not
  occur on a day (no moonrise, no astronomical dusk at high latitude) moves to the next day it
  occurs (searched 60 days ahead). `main/Alarms.*` (host-tested) holds these rules: at most four
  alarms, a clock time or one of 17 sky events (the Sun's and the Moon's events and the prayer
  times) with up to ±120 min, the weekdays it rings on (none: once, then it switches itself
  off), a due time rung up to 10 min late and skipped later. They are defined in the portal
  (`main/AlarmStore.*`) and switched on the clock page's last part, Alarms. There is no
  vibration motor on the board: `main/Ringer.*` beeps 1000/1500 Hz (the speaker's loudest
  region, docs/hardware.md, "Speaker and alarm sound"), from -18 dB to at most -12 dB (the meter: -6 dB drew 0.17 A, full scale browned the board out), through
  `main/Sound.*`, for at most 60 s; a tap, PWR or BOOT stops it. Design:
  `docs/superpowers/specs/2026-11-19-alarms-design.md`. Development commands: "B <name>" presses a button of the opened page (clock page:
  `ClockPages::press`, with "regatta" and "sync" for the countdown's mode; tools page: count, undo, target, reset, roll, dice, coin, number, torch, "keys <keys>" as
  `Calculator::keyForChar`, kind, "unit <n>", level, hold, input, beat, bar, cpr, "tempo <bpm>"; observing page: mode, "drag <min>", now, hold,
  flip, "scroll <px>", eyepiece, calc, body, aim, satellite),
  "R <s>" a test
  alarm, "K <n> <text>" defines an alarm, "Q" ends active mode as without the always-on face,
  "H" makes the next PWR wake a phantom (it must go back to sleep), "W" starves the idle task
  (the task watchdog must reset the watch). "W <tile>" swipes to a tile (0 to 2), "W 3" and
  "W 4" scroll the launcher down and up; "n <mode>" sets the launcher rows' behaviour until the
  next reset, for comparisons (2 narrowing and dimming, the default; 1 narrowing only; 0 one width).

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

### Power evaluation

The AXP2101 cannot measure battery current. The owner has a meter (Mastech MY64, DC ranges 2,
20 and 200 mA) that measures it in series with the battery's + lead, with a clip lead bridging
the meter while the watch boots or wakes (docs/hardware.md, "Battery current measured with a
meter"). The owner connects it; ask for a reading of a steady state: off, always-on between
the minute wakes, active mode. It cannot follow the wakes themselves (a few hundred ms at tens
of milliamps), so on-board data stay in use for averages over time:

- Wake statistics kept in RTC memory: wakes per source, awake time per wake
  (`esp_timer`), refused sleep entries.
- Battery voltage and gauge percentage logged over long runs on battery only (USB charges the battery).
- An energy budget from the measured currents, datasheet values and the measured awake time.

Report meter readings as measured currents; slopes and budgets as estimates.

Battery drain runs use separate builds, so the committed `sdkconfig` stays unchanged
(`configs/battery` enables the timer log; add `configs/noquiesce` for a baseline run):

```bash
idf.py -B build-battery -D SDKCONFIG=build-battery/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;configs/battery" build flash
```

Power-cycle the watch (hold PWR 6 s, then press) so that the log and all chips start from their
power-on state, unplug USB for the run, then plug it in and read the log with
`python tools/guided_test.py battery` (this test does not reset the board).

## Documentation

- `README.md`: human-facing and concise (purpose, features, screenshots, install, hardware, build);
  no status or roadmap (the release notes and the git history keep them).
- `CLAUDE.md`: durable development rules and agent instructions.
- `docs/hardware.md`: hardware facts with sources.

Update the matching document in the same change when a design decision changes.

Before a push that changes what the watch shows, refresh `docs/screenshots/en/` and `tr/` from
the board with the development build, once per language (host command `L 0` or `L 1` switches
the watch's language; put back the one it had):
`python tools/screenshot.py --out docs/screenshots/<en|tr> --send P --after P --after N` (all
pages, every face page and its always-on face; the watch's saved face pages and corner gauges
are what it shows, so the defaults; the setup portal on for its page, then off
with a new password, so that the published one is not in use), then
`tools/.venv/bin/python tools/make_gifs.py --dir docs/screenshots/<en|tr>` and the portal
pictures `tools/portal_preview.py --shot docs/screenshots/<en|tr>/<portal|portal-faces|update>.png
--language <0|1> [--page /#faces | /update]`, their empty lower part cut off. Each language has its page of them
(`docs/screenshots/<en|tr>/README.md`, the watch's pages as columns, their parts below each
other); the README shows only the animations. The pages show a fixed scene (Istanbul,
2026-06-21 19:19), so that the published pictures stay comparable and never show the watch's
real location.

A release: the commits pushed, an annotated tag `vX.Y.Z` on the last one (pushed to both
remotes), the About page and the portal pictures captured again from a clean build of the tag
(they show the version), then the release build of the tag (`build-release`, as above) gives
`astronomical_watch-vX.Y.Z.bin` (the update through the portal) and, with
`esptool --chip esp32s3 merge-bin -o astronomical_watch-vX.Y.Z-full.bin @flash_args` in the
build directory, the full image for a first install over USB. Both go on the release pages of
GitLab and GitHub with the notes.
`tools/screenshot.py` can also be used during development to check a layout without the tester
(`--live` for the watch's own time and settings).
