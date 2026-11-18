# Face Templates Phase 3 (Arcs, Milky Way) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Full arcs and Half arcs templates (the old Dial and Sky arcs rebuilt on the face
pages) with either clock type, and the Milky Way band on the half arcs, on both faces.

**Architecture:** Plain C++ for what can be tested on the host: the galactic centre's place
(`Astronomy`), the band's geometry and the templates' clock places (`FaceConfig`). `FacePage`
draws the arcs in one object; `AlwaysOn` draws them as dots and shapes. The old drawing code is
in git (`0dab4d0:main/WatchFace.cpp`, `0dab4d0:main/AlwaysOn.cpp`, layoutArcs and layoutDial).

**Spec:** `docs/superpowers/specs/2026-11-18-face-templates-design.md` (Templates, Milky Way,
Phase 3); mock-up `templates.png` pages 1 and 2, `mock.py` `fullArcs`, `halfArcs`.

## Global Constraints

- Full arcs: the Sun's orbit r 150 and the Moon's r 122 around (205, 251), each dense where the body is up and sparse below, with rise/set marks (the old Dial); the clock inside: digits 64 px or an analog dial of r 100.
- Half arcs: the horizon at y 232 (a line from x 40 to 370), the Sun's path r 150 and the Moon's r 105 above it, each body placed by the time between its rise and set (the old Sky arcs); the clock below: digits 64 px centred at y 310, or an analog dial of r 70.
- Milky Way (Half arcs option, `MilkyWayFlag`): a circle of r 260 through the observer (the horizon's middle), its centre turned by the galactic centre's azimuth (east at the left end, south up, west at the right end; mirrored south of the equator), clipped to the half disc of r 160; 16 px wide; bright (45 % of (150, 170, 255)) when the centre is up and the Sun at least 18° down, dim (20 %) when the centre is up in a lighter sky, hidden when it is down. Always-on: the band's pixels on the dot grid (every second pixel of every second row), blue.
- The galactic centre: RA 17h45m40.04s, Dec −29°00′28.1″ (J2000); host-tested against Skyfield.
- One medium digit size, 64 px, for both arc templates (JetBrains Mono Light 64 and seven segment 64; active fonts and always-on bitmaps).
- Defaults: the spec's four pages first (Full arcs, Half arcs with the Milky Way, Large seven segment, Large analog Classic), then Large JetBrains Mono.
- Minute wake within 0.35 s of app time; host tests green; CLAUDE.md style.

## Review Focus

1. Polar day or night (no sunrise or moonrise in the table): the paths draw without a body, no division by zero.
2. The Milky Way at a southern location (the arcs and the band mirror) and with the centre exactly at the horizon.
3. The arcs and the clock stay clear of the slots and the corner gauges, also with the always-on face's ±8 px shift.
4. The always-on shapes fit MaxShapes with both orbits' dots, an analog clock and a weekday ring.
5. A page switched to an arc template without a location (sky invalid): no bodies, no band, no garbage.

---

### Task 1: Geometry and data (host-tested)

**Files:** `main/Astronomy.h/.cpp` (`static Position starPosition( double raHours, double decDegrees, int64_t time, const Observer &observer )`), create `main/MilkyWay.h/.cpp` (`struct Band { float cx, cy, radius; bool shown; bool bright; }`, `static Band band( double centreAltitude, double centreAzimuth, double sunAltitude, bool south, float observerX, float observerY )`), `main/FaceConfig.h/.cpp` (`Template { Large, FullArcs, HalfArcs }`, `struct ClockPlace { int centreY; int digitHeight; int analogRadius; }`, `static ClockPlace clockPlace( int layout )`, defaults), `tools/make_astronomy_reference.py` (galactic centre rows), tests `tests/AstronomyTest.cpp` (or a new `tests/MilkyWayTest.cpp`), `tests/FaceConfigTest.cpp`, `tests/FaceTextTest.cpp` (defaults fit).

- [ ] Tests first: the centre's altitude and azimuth for Istanbul and a southern place at three times within 0.1° of Skyfield; the band hidden below the horizon, bright and dim by the Sun; the band's centre direction at azimuth 90°, 180°, 270° and mirrored south; clock places; five default pages. Run: fail.
- [ ] Implement; run `sh tests/run_host_tests.sh`: pass; commit.

### Task 2: Assets

**Files:** `tools/make_fonts.sh` (Light 64; Light 90 removed if unused), `main/fonts/`, `main/Fonts.h`, `tools/make_aod_glyphs.py` and `main/AodGlyphs.*` (`aodStyles` by digital style and size: Large and Medium), `main/CMakeLists.txt`.

- [ ] Regenerate; build without warnings; host tests (AodSmallTest) pass; commit.

### Task 3: The active face

**Files:** `main/FacePage.h/.cpp` (clock by `clockPlace`; an arcs object: dotted paths or orbits, the Sun, the Moon on its path, the band; `setSky( const Corners::Input &, bool located, double centreAltitude, double centreAzimuth, double sunAltitude )`), `main/Display.cpp` (feeds it each minute), `main/main.cpp` (dev "H" also sets the template: "J <page> <template>").

- [ ] Capture both arc templates with both clock types and the band (fixed scene); compare with the mock-up; nothing over the slots; commit.

### Task 4: The always-on face

**Files:** `main/AlwaysOn.h/.cpp` (`addDottedArc` back, the orbits and paths, the Sun, a small moon, a `BandShape` for the Milky Way's dots; MaxShapes as needed), Look gains the galactic centre is computed in the wake (`Astronomy::starPosition`).

- [ ] Capture both templates' always-on forms; measure the minute wake (Full arcs with an analog clock); commit with the numbers.

### Task 5: Portal, documents, review

**Files:** `main/portal/index.html` (templates Large, Full arcs, Half arcs; "Milky Way" box for Half arcs), `tools/portal_preview.py`, `CLAUDE.md`, `README.md`, the spec (phase 3 result).

- [ ] node --check; preview; release build flashed; final whole-branch review by a fresh reviewer; fixes with tests; commit; report.
