# Face templates, phase 1 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One configurable list of face pages (`FaceConfig`), the Large template with a JetBrains Mono or seven-segment digital clock, eight slots (four texts, four corners) with an always-on flag each, the always-on face drawn from the same page, and the portal section that edits the pages.

**Architecture:** Pure C++ modules hold the model and the content (`FaceConfig`, `FaceText`, `FaceTable`, `SevenSegment`), host-tested like the astronomy code. `FacePage` draws a Large page in LVGL and replaces `WatchFace`; today's four faces, five always-on styles, the always-on page (the leftmost tile) and their settings are removed in this phase (the owner's decision, 2026-11-18): until phase 3 the watch has only Large pages. `AlwaysOn` lays out the always-on form from the page kept in its `Look`; event times for the always-on texts come from a small table taken when active mode ends. The brightness control of the face takes both levels: a vertical drag the active brightness, a horizontal one the always-on brightness of the moment (day or night), the panel showing it while dragging.

**Tech Stack:** ESP-IDF v6.1, C++17, LVGL 9 (BSP), NVS; host tests with g++ (`sh tests/run_host_tests.sh`); Python/Pillow glyph generator (`tools/make_aod_glyphs.py`); the portal page in plain JavaScript.

**Spec:** `docs/superpowers/specs/2026-11-18-face-templates-design.md`

## Global Constraints

- Code style of CLAUDE.md: PascalCase classes and files, camelCase, `m_` members, `s_` file statics, function `{` on a new line, spaces inside parentheses, plain index `for` loops, no exceptions, RTTI or templates beyond the standard library, no heap allocation after start-up unless needed, English code and comments, comments only for the why.
- Every text the user sees comes from `main/Language.*` in English and Turkish; the portal keeps its Turkish in `data-tr`.
- Fonts: `main/Fonts.h` (26 and 48 px, light 90 and 110 px); a new size goes through `tools/make_fonts.sh`.
- Seven segment: digit width = 0.62 × height, segment thickness = height / 11, joint gap = height / 40, hexagonal segments with pointed ends, upright. Large template: height 122 px.
- Slot places (screen pixels, centres): text slots y = 25 (top edge), 60 (top inner), 442 (bottom inner), 478 (bottom edge), centred on x = 205; edge slots at most 160 px wide, inner ones at most 240 px; corner slots (66, 70), (344, 70), (66, 432), (344, 432). A text too wide is clipped, never wrapped.
- Weekday icon: about 44 px across, seven notches, the first n lit for the n-th day (Monday = 1, Sunday = 7), lit amber and unlit dim grey on the active face, only lit ones in white on the always-on face; the day's first letter in the middle.
- At most 8 pages (`PageMarks::MaxCount`). Nothing of the old face settings is carried over: the watch starts from the default pages. The old faces, always-on styles, the always-on tile and the settings `FaceItems`, `AodStyle`, `AodItems`, `FaceOrder`, `FaceHidden`, `AodOrder`, `AodHidden` are removed; `FaceStyle` stays as the index of the page shown.
- Tiles after this phase: face, clock, information, settings (four).
- The always-on minute wake stays at about 0.35 s from reset on average (today about 0.33 s); measured with the wake statistics.
- The always-on face shows a slot only when its always-on flag is set and the item makes sense there (seconds never).

## Review Focus

1. A slot text wider than its slot (a long Turkish date in an edge slot): clipped at the slot width on both faces, never wrapping into the gauges. Pinned in Task 2 (`FaceText::fit`) and checked on screenshots in Tasks 5 and 6.
2. A character of a slot text missing from the always-on small glyph set (Turkish letters, °, %): the always-on face must not drop letters silently. Pinned in Task 6 by a host test that renders every slot string of both languages through the glyph table.
3. No location or no clock set yet (latitude and longitude 0, sky invalid, RTC month 0), battery unknown: the slot stays empty, no "0:00" or garbage. Pinned in Task 2.
4. A saved page blob from another version or damaged (wrong size, values out of range): defaults or clamped values, never a crash or an empty list. Pinned in Task 1.
5. An always-on face that runs past midnight or longer than the event table: the next event and the date still right after midnight; past the table the always-on texts recompute it. Pinned in Task 2 (`FaceTable`) and Task 6.

---

### Task 1: The page model (`FaceConfig`)

**Files:**
- Create: `main/FaceConfig.h`, `main/FaceConfig.cpp`
- Create: `tests/FaceConfigTest.cpp`
- Modify: `tests/run_host_tests.sh` (build and run `build-host/face_config_test`)

**Interfaces:**
- Produces:
  - `class FaceConfig` with `static const int MaxPages = 8; static const int SlotCount = 8;`
  - `enum Template : uint8_t { Large, TemplateCount };` (phase 3 adds `FullArcs`, `HalfArcs`)
  - `enum ClockType : uint8_t { DigitalClock, AnalogClock, NoClock, ClockTypeCount };`
  - `enum DigitalStyle : uint8_t { MonoDigits, SevenSegment, DigitalStyleCount };`
  - `enum AnalogStyle : uint8_t { ClassicHands, BoldHands, FineScale, OneHand, PlainHands, AnalogStyleCount };` (phase 2 draws them; phase 1 stores them)
  - `enum Slot { TopEdge, TopInner, BottomInner, BottomEdge, TopLeft, TopRight, BottomLeft, BottomRight };` (the first four take `TextItem`, the last four `CornerItem`)
  - `enum TextItem : uint8_t { NoText, LongDate, ShortDate, HijriDate, NextSunEvent, NextPrayer, Seconds, BatteryPercent, CityTime, DayCounter, SunAltitude, MoonAltitude, TextItemCount };`
  - `enum CornerItem : uint8_t { NoCorner, MoonIcon, WeekdayIcon, WeekdayShort, DayOfMonth, HijriDayOfMonth, MonthShort, HijriMonthShort, SunAltitudeCorner, MoonAltitudeCorner, HourCorner, MinuteCorner, SecondCorner, CornerItemCount };`
  - `struct Page { uint8_t layout; uint8_t clock; uint8_t digital; uint8_t analog; uint8_t flags; uint8_t items[SlotCount]; uint8_t parameters[SlotCount]; uint8_t alwaysOn; };` — `flags` bit 0 seconds hand, bit 1 Milky Way, bit 7 hidden; `parameters` the city (0–2) or counter (0–3) index of a slot; `alwaysOn` one bit per slot.
  - `struct Pages { uint8_t count; Page pages[MaxPages]; };`
  - `static void defaults( Pages &pages );` — two pages: (0) Large seven segment: TopEdge `ShortDate`, BottomInner `NextSunEvent`, BottomEdge `HijriDate`; TopLeft `WeekdayIcon`, BottomLeft `MoonIcon`, BottomRight `DayOfMonth`; always-on: TopEdge, TopLeft, BottomLeft, BottomRight. (1) Large Mono, like today's Classic: TopInner `LongDate`, BottomInner `HijriDate`, BottomEdge `NextSunEvent`; BottomLeft `MoonIcon`; always-on: TopInner, BottomLeft.
  - `static bool valid( const Pages &pages );` — false for count 0 or > MaxPages, or any value out of its enum range or a parameter out of range.
  - `static void clamp( Pages &pages );` — out-of-range values to 0 (NoText, NoCorner, MonoDigits…), count to 1..MaxPages.
  - `static int encode( const Pages &pages, char *text, int size );` — pages separated by `;`, each `layout,clock,digital,analog,flags,i0..i7,p0..p7,alwaysOn` in decimal; returns the length or -1 when it does not fit.
  - `static bool decode( const char *text, Pages &pages );` — the inverse; false (and `pages` untouched) on a malformed text; out-of-range numbers are clamped.
  - `static int shown( const Pages &pages, int list[MaxPages] );` — indices of the pages without the hidden flag, in order; at least one (the first page when all are hidden).
  - `static bool alwaysOnSlot( const Page &page, int slot );` — the flag, and false for `TextItem::Seconds` and `CornerItem::SecondCorner`.

- [ ] **Step 1: Write the failing test** `tests/FaceConfigTest.cpp` in the style of `tests/PartOrderTest.cpp` (a `check( name, condition )` counter, a final "Face config: N checks, M failures" line, exit 1 on failure):
  - `defaults` gives 2 pages, `valid` true, page 0 layout `Large`, clock `DigitalClock`, digital `SevenSegment`, `items[TopLeft] == WeekdayIcon`.
  - `encode` then `decode` gives the same bytes (`memcmp` of the structs).
  - `decode( "0,0,9,0,0,1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0,255" )` succeeds with `digital` clamped to 0 and `items[4]` (a corner) = 5 kept, `items[0]` = 1.
  - `decode( "garbage" )` and `decode( "" )` return false and leave the struct unchanged.
  - A page with `flags = 0x80` on every page: `shown` returns 1 page (index 0).
  - `alwaysOnSlot` false for a `Seconds` slot with its bit set, true for `LongDate` with its bit set.
  - Fewer than 22 numbers in a page (a truncated text): `decode` false.

- [ ] **Step 2: Run it to see it fail:** `sh tests/run_host_tests.sh` — Expected: the build of `face_config_test` fails (no `FaceConfig.h`).
- [ ] **Step 3: Implement** `main/FaceConfig.h/.cpp` with the interfaces above (no ESP-IDF includes: the host tests build it).
- [ ] **Step 4: Run:** `sh tests/run_host_tests.sh` — Expected: "Face config: … 0 failures" and every earlier line unchanged.
- [ ] **Step 5: Commit** `git add main/FaceConfig.* tests/FaceConfigTest.cpp tests/run_host_tests.sh` with a message stating the tests pass.

### Task 2: The slot contents (`FaceText`, `FaceTable`)

**Files:**
- Create: `main/FaceText.h`, `main/FaceText.cpp`, `main/FaceTable.h`, `main/FaceTable.cpp`
- Create: `tests/FaceTextTest.cpp`
- Modify: `tests/run_host_tests.sh` (build with `main/Language.cpp`, `main/Sky.cpp`, `main/Dates.cpp`, `main/Hijri.cpp`, `main/Astronomy.cpp`)

**Interfaces:**
- Consumes: `FaceConfig::TextItem`, `FaceConfig::CornerItem` (Task 1); `Sky::Day`, `Sky::prayerTimes`, `Astronomy::sunDay`, `Astronomy::sunPosition`, `Astronomy::moonPosition`, `Language`.
- Produces:
  - `class FaceTable` (fits in RTC memory; no pointers):
    - `struct Event { int64_t time; uint8_t kind; };` kinds `Sunrise, Sunset, Prayer0..Prayer5` (`Sky::PrayerCount` = 6).
    - `static const int MaxEvents = 16;` `Event events[MaxEvents]; int count; int64_t validUntil; int hijriDay, hijriMonth, hijriYear; int localDay;` plus the counters (`int16_t counterDays[4]` as days since 1970, `char counterNames[4][20]`) and the cities (`char cityNames[3][24]`, their `Zones::Zone` copies).
    - `void build( const Sky::Day &today, int utcOffsetMinutes, const Astronomy::Observer &observer, int ishaAngle );` — today's and tomorrow's sunrise, sunset and six prayer times (tomorrow: `Astronomy::sunDay` and `Sky::prayerTimes` of the next day, not a full `Sky::computeDay`, which takes about 0.9 s); `validUntil` = the end of tomorrow.
    - `bool next( int64_t now, bool prayer, Event &event ) const;` — the first sun event (prayer false) or prayer (true) after `now`; false past `validUntil`.
  - `class FaceText`:
    - `struct Inputs { bool timeValid; int year, month, day, hour, minute, second, weekday; bool skyValid; int64_t now; const FaceTable *table; int batteryPercent; double sunAltitude, moonAltitude; bool altitudesValid; };` (weekday 0 Monday, as `Language`)
    - `static void text( FaceConfig::TextItem item, int parameter, const Inputs &in, char *out, int size );` — empty string when the data is missing (Review Focus 3). Formats: `LongDate` = `Language::longDate`; `ShortDate` "21 Haz" / "Jun 21"; `HijriDate` "5 Muharrem 1448"; `NextSunEvent` "Gün batımı 1:21" (name and time left, `Language::InTime` style); `NextPrayer` "Akşam 1:28"; `Seconds` ":42"; `BatteryPercent` "68 %"; `CityTime` "London 17:19"; `DayCounter` "Ramazan 232"; `SunAltitude` "13°", `MoonAltitude` "42°" (rounded; empty below the horizon).
    - `static void corner( FaceConfig::CornerItem item, const Inputs &in, char *out, int size );` — texts for the text corners ("Paz", "21", "5" Hijri, "Haz", "Muh", "13°", "19", "19", "42"); empty for the icon items (drawn by the caller).
    - `static int fit( const char *text, int advance, int maxWidth );` — the number of bytes of `text` (UTF-8, whole characters) that fit `maxWidth` at `advance` px per character (Review Focus 1).

- [ ] **Step 1: Write the failing test** `tests/FaceTextTest.cpp` with the fixed scene (Istanbul 41.0082, 28.9784; 2026-06-21 19:19:00, UTC+3; isha 17; `Sky::computeDay` for that day):
  - Turkish: `LongDate` "21 Haziran Pazar"; `HijriDate` "5 Muharrem 1448"; `NextSunEvent` "Gün batımı 1:21"; `NextPrayer` "Akşam 1:28"; `SunAltitude` "13°"; corner `WeekdayShort` "Paz"; corner `DayOfMonth` "21".
  - English: `LongDate` "Sunday 21 June"; `NextSunEvent` "Sunset 1:21".
  - `timeValid` false: every item gives "".
  - `table.next` at 2026-06-21 23:59 (prayer) gives tomorrow's Imsak; at 2026-06-23 00:01 it gives false (past `validUntil`, Review Focus 5).
  - `fit( "21 Haziran Pazar", 16, 160 )` = the bytes of "21 Hazira" (10 characters × 16 px; "ı" two bytes counted as one character).
- [ ] **Step 2: Run** `sh tests/run_host_tests.sh` — Expected: build fails (no `FaceText.h`).
- [ ] **Step 3: Implement** both modules; scene values above come from the published screenshots (sun part: sunset 20:40, "Şimdi 13°"; prayer part: Akşam 20:47, "Akşam 1:28 sonra").
- [ ] **Step 4: Run** the host tests — Expected: "Face text: … 0 failures", earlier lines unchanged.
- [ ] **Step 5: Commit.**

### Task 3: Seven-segment geometry (`SevenSegment`)

**Files:**
- Create: `main/SevenSegment.h`, `main/SevenSegment.cpp`, `tests/SevenSegmentTest.cpp`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Produces:
  - `struct SevenSegment::Point { float x, y; };` and `struct Hexagon { Point p[6]; };`
  - `static float digitWidth( float height );` = 0.62 × height.
  - `static int segments( int digit, float height, Hexagon out[7] );` — the lit segments of a digit (0–9) with its top-left at (0, 0), using thickness = height / 11 and gap = height / 40; returns how many.
  - `static float colonSize( float height );` = height / 11 (a square), at 0.3 and 0.7 of the height.
  - `static float timeWidth( float height );` — four digits, two digit gaps of 0.16 × digit width, the colon with 0.18 × digit width on each side.
  - `static void layout( float height, float x[4], float &colonX );` — the digits' left edges and the colon's centre for a time block starting at 0.
- [ ] **Step 1: Test:** for height 122: `digitWidth` 75.64; `segments( 8, … )` = 7 and every point inside [0, width] × [0, height]; `segments( 1, … )` = 2; `timeWidth( 122 )` < 330 (fits between the corner gauges at the clock's height); the horizontal segment's thickness (top hexagon height) equals 122 / 11 within 0.01.
- [ ] **Step 2–4:** fail, implement (the hexagon of `tools/make_aod_glyphs.py` `hexagon()` and `segmentDigit()` boxes, in floats), pass.
- [ ] **Step 5: Commit.**

### Task 4: Pages on the watch (storage, page list, swipe; the old faces out)

**Files:**
- Create: `main/FaceStore.h`, `main/FaceStore.cpp` (NVS, target only); `main/FacePage.h/.cpp` as an empty tile owner (Task 5 fills it)
- Delete: `main/WatchFace.*`, `main/AodPage.*` (move `WatchFace`'s `paintMoon` into `FacePage` first)
- Modify: `main/Settings.h/.cpp` (remove the old items listed in Global Constraints, `face_style` range 0..7), `main/Portal.cpp` (`s_fieldNames` follows `Settings::Item`), `main/Display.h/.cpp` (tiles without the always-on page; `shownStyles`, `onGesture`, `setScene`, `showFaceStyle`, `refreshAlwaysOnPage` reworked or removed; the brightness drag: after the long press the first movement over 20 px decides the axis, vertical sets `Brightness`, horizontal `AodDayBrightness` or `AodNightBrightness` by the day gauge, the panel at that level while dragging, the box shows which), `main/AlwaysOn.*` (only the Large layout stays; the arcs, dial and digit-set styles go, Task 6 adds the page layout), `main/main.cpp` (load the pages at start; the development screenshot list), `tools/make_aod_glyphs.py` and `main/AodGlyphs.c` (the digit sets go except what Task 6 needs), `tools/screenshot.py` (no always-on tile)

**Interfaces:**
- Consumes: Task 1.
- Produces:
  - `FaceStore::load( FaceConfig::Pages &pages )` — the NVS blob "faces" in namespace "watch" (as `WorldClock`), `FaceConfig::defaults` when missing, wrong size or `!valid` after `clamp`; `FaceStore::save( const FaceConfig::Pages & )`.
  - `Display::setPages( const FaceConfig::Pages & )` — keeps a copy; `Settings::FaceStyle` is now the index into it.
  - The vertical swipe on the face steps `FaceConfig::shown` (replacing `FaceOrder`/`FaceHidden`).
- [ ] **Step 1:** Build (`idf.py build`) — no new warnings; host tests pass.
- [ ] **Step 2:** Flash the development build to the owner's watch (serial check `3C:DC:75:6E:F4:54`, the only device): the watch starts on the (empty) face tile, swipes sideways through four tiles, the brightness drag changes the active level vertically and the always-on level horizontally (log lines "Brightness setting …"), the minute wakes still draw the always-on corner gauges. Record the result.
- [ ] **Step 3: Commit.**

### Task 5: The Large page on the active face (`FacePage`)

**Files:**
- Create: `main/FacePage.h`, `main/FacePage.cpp`
- Modify: `main/Display.h/.cpp` (feed time, sky, battery each second), `main/Language.*` (any new text both languages)

**Interfaces:**
- Consumes: Tasks 1–3; `FaceText::Inputs` built in `Display` from `m_day`, `m_now`, `m_battery`, a `FaceTable` built when the sky day is computed.
- Produces: `FacePage::build( lv_obj_t *tile )`, `FacePage::show( const FaceConfig::Page &page )`, `FacePage::update( const FaceText::Inputs &inputs )` (once a second), `FacePage::setMoon( int percent, bool waxing )`.
- Clock types: `NoClock` draws no clock; `AnalogClock` draws nothing until phase 2 (the portal may already store it).
- Drawing decisions: Mono digits `jetbrains_mono_light_110`, white, centred at y 251; seven segment height 122 drawn in one object's `LV_EVENT_DRAW_MAIN` from `SevenSegment::segments` as two triangles + two end triangles per hexagon (`lv_draw_triangle`), white; text slots `jetbrains_mono_26`, edge slots white, inner slots grey 0xA0A0A0, `LV_LABEL_LONG_MODE_CLIP` at the slot width; text corners white 26 px; `MoonIcon` the 37 px moon canvas; `WeekdayIcon` one object drawing seven `lv_draw_arc` notches (radius 20, width 4, 51.4° each with 8° gaps, starting at 12 o'clock clockwise) and the letter label.
- [ ] **Step 1:** Build, flash (development build), capture the face page with the fixed scene: `python tools/screenshot.py --port <by-id> --out <scratchpad>/p5 1` with the page shown both as seven segment and as Mono (portal-free: a development command `F <page>` added to `main.cpp` that shows a page, as `S` shows a screen). Expected, compared with `docs/superpowers/specs/2026-11-18-face-templates/templates.png` (page 3): digits 122 px high, nothing over the gauges, "21 Haz" at the top edge, "Gün batımı 1:21" bottom inner, "5 Muharrem 1448" bottom edge, weekday icon with seven lit notches (Sunday) top left, moon bottom left, "21" bottom right.
- [ ] **Step 2:** Long text check (Review Focus 1): a development command sets TopEdge to `LongDate`; the capture shows it clipped inside 160 px.
- [ ] **Step 3: Commit** with the screenshots' findings in the message.

### Task 6: The always-on face from the page

**Files:**
- Modify: `main/AlwaysOn.h/.cpp` (`Look` loses `style`/`items` and gains `FaceConfig::Page page; FaceTable table; Astronomy::Observer observer; int utcOffsetMinutes; int ishaAngle;`), `main/main.cpp` (fill the new `Look` fields when active mode ends), `tools/make_aod_glyphs.py` and the generated `main/AodGlyphs.c` (the Mono digit set at the Large size, JetBrains Mono Light like `jetbrains_mono_light_110`; small glyph set: every character of the slot strings in both languages, Review Focus 2), `tools/screenshot.py` (the "A" capture draws the always-on form of page n)
- Create: `tests/AodSmallTest.cpp` (host: every string `FaceText` can produce for the scene in both languages, plus the `Language` names it uses, maps to `aodSmallCodes`)

**Interfaces:**
- Consumes: Tasks 1–3, 5's decisions on places.
- Produces: `static void layoutLarge( AlwaysOn::Face &face, const AlwaysOn::Look &look, const Rtc::DateTime &time, int64_t utcNow )` in `AlwaysOn.cpp`; a new `ShapeKind` `SegmentShape` (a hexagon of `SevenSegment`, filled with the 1-bit dot grid, every second pixel of every second row) drawn by `drawShape`; the weekday icon as dotted arcs (`addDottedArc`) with the small glyph letter; text slots with `addSmallText` cut by `FaceText::fit` with `aodSmallAdvance`; the sun and moon altitudes computed in the minute wake with `Astronomy::sunPosition`/`moonPosition`; the table rebuilt in the wake when `utcNow` passes `table.validUntil` (prayer times and sun days only).
- [ ] **Step 1: Test** `AodSmallTest` fails before the generator change (missing letters), passes after: `tools/.venv/bin/python tools/make_aod_glyphs.py` regenerates `main/AodGlyphs.c`.
- [ ] **Step 2:** Build, flash, capture the always-on form of both default pages (the "A" capture). Expected: matches the always-on row of `templates.png` for page 3 (dotted seven segment, "21 Haz" edge, weekday ring with white lit notches, moon, "21"; the non-always-on slots absent); the Mono page with solid 1-bit digits.
- [ ] **Step 3: Minute wake time:** leave the watch in always-on on USB for 10 minutes with a dev build that logs the wake statistics; expected the timer wake time stays at about 0.35 s or less on average (`Last timer wake: … app … ms`). Record the numbers.
- [ ] **Step 4: Commit.**

### Task 7: The portal's "Faces" section

**Files:**
- Modify: `main/Portal.cpp/.h` (state JSON: `"faces":"<FaceConfig::encode>"`, catalogue sizes; save: field `faces` → `FaceConfig::decode` → `FaceStore::save` → `Display::setPages`), `main/portal/index.html` (one block per page: template, clock type, digital style, analog style (stored for phase 2), seconds hand, four text slots and four corners each with an always-on checkbox and, for `CityTime`/`DayCounter`, the city or counter; order with ↑/↓, hide with a checkbox; remove the old face/always-on style selects and the face/always-on order lists), `tools/portal_preview.py` (state `faces` from defaults)
- [ ] **Step 1:** `node --check` on the page's script (as done before); `tools/portal_preview.py --shot … --height 5000` in both languages: the section readable on a phone width.
- [ ] **Step 2 (end to end):** flash, start the portal (development command `P`), join its network from the laptop's Wi-Fi (`nmcli dev wifi connect <ssid> password <pw> ifname wlp0s20f3`; the ethernet keeps the laptop online), `curl` the state, POST a save that changes page 0's TopEdge to `LongDate`, then capture the face: the change shows. Disconnect the laptop from the watch's network afterwards.
- [ ] **Step 3: Commit.**

### Task 8: Verification, documents, hand-over

**Files:** `CLAUDE.md` (faces paragraph: pages and slots, `FaceConfig`, the always-on face follows the page), `README.md` (features: configurable faces), `docs/superpowers/specs/2026-11-18-face-templates-design.md` (phase 1 done, anything learned)
- [ ] **Step 1:** Host tests, release and development builds without warnings.
- [ ] **Step 2:** On the owner's watch: every page and its always-on form captured with the fixed scene; heap with the portal on (the "Portal on" line, expect ≥ 40 KB); the minute wake numbers of Task 6.
- [ ] **Step 3:** Flash the release build back; update the documents; commit; report to the owner with the pictures (no push, no release: phase 5 releases).
