# Face Templates Phase 2 (Analog) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The five analog styles in the Large template, on the active and the always-on face.

**Architecture:** A plain C++ module `AnalogClock` turns a style, a radius and a time into line
segments (marks and hands) with a role each; `FacePage` draws them with LVGL lines in one
object's draw event, `AlwaysOn` turns them into `LineShape`s. The portal already stores the
analog style and the seconds hand flag (phase 1).

**Tech Stack:** ESP-IDF v6.1, C++17, LVGL 9, host tests with g++.

**Spec:** `docs/superpowers/specs/2026-11-18-face-templates-design.md` (The clock element, Phases 2),
mock-ups `docs/superpowers/specs/2026-11-18-face-templates/details.png`, `templates.png` page 4,
`mock.py` `analog()`.

## Global Constraints

- Analog styles: Classic (12 marks), Bold hands (12 marks), Fine scale (60 marks), One hand (hour hand only, read on the fine scale), Plain (hands only, no marks).
- Seconds hand: an option per page on the active face; on by default with Classic; never on the always-on face.
- Large: radius 140 around (205, 251) (mock-up); nothing over the corner gauges or the slots.
- Mock-up geometry (`mock.py`): marks from 0.86 r (major) or 0.93 r (minor) to r; hour hand 0.55 r, minute hand 0.85 r; widths classic 6/4, bold 12/8, fine 5/3, one 6 (0.92 r, amber), plain 7/4; centre disc radius 6.
- Minute wake stays within about 0.35 s of app time; host tests stay green; CLAUDE.md code style.

## Review Focus

1. Hands at 12:00, 3:00, 6:00, 9:00 and 23:59 point where a reader expects (angle origin and direction).
2. One hand: at 19:19 it points between 7 and 8, a quarter of the way (the minutes read on the fine scale).
3. The always-on face never shows a seconds hand, also when the page's flag is on.
4. The always-on shapes fit `MaxShapes` with 60 marks, two hands, the centre and a weekday ring.
5. A page switched to analog while the clock is invalid (no time set): no hands drawn, no garbage.

---

### Task 1: `AnalogClock` geometry and the default analog page

**Files:** Create `main/AnalogClock.h/.cpp`, `tests/AnalogClockTest.cpp`; modify `tests/run_host_tests.sh`, `main/FaceConfig.cpp` (defaults: a third page, Large analog Classic with the seconds hand, the mock-up's slots), `tests/FaceConfigTest.cpp`.

**Interfaces:** Produces `AnalogClock::Segment { float x0, y0, x1, y1; float width; Role role; }`, `enum Role { MajorMark, MinorMark, HourHand, MinuteHand, SecondHand, OneHand }`, `static int segments( int style, float radius, int hour, int minute, int second, bool secondsHand, Segment *out, int capacity )` (centre at 0, 0; y down; returns the count), `static const int MaxSegments = 64`.

- [ ] Step 1: tests: mark counts per style (12, 12, 60, 60, 0), hand ends at 3:00 / 6:00 / 12:00, one hand at 19:19, the seconds hand only when asked, capacity respected. Run: fail (no module).
- [ ] Step 2: implement; run `sh tests/run_host_tests.sh`: pass. Defaults test: three pages, page 3 analog Classic with the seconds hand; defaults still fit (FaceTextTest).
- [ ] Step 3: commit.

### Task 2: The active face

**Files:** `main/FacePage.h/.cpp` (an analog object of 2 r + margin, draw event from `AnalogClock`; marks white or grey, hands white, one hand and seconds hand amber, centre disc; invalidated when the minute changes, or each second with the seconds hand), `tools/screenshot.py` (FACE_STYLES 3).

- [ ] Step 1: build, flash, capture page 3 and the five styles (`G 2 ...` is for slots; a development command `H <page> <style>` sets the analog style, not saved). Expected: as `details.png`, nothing over the slots; seconds hand on Classic.
- [ ] Step 2: commit with the findings.

### Task 3: The always-on face

**Files:** `main/AlwaysOn.h/.cpp` (`MaxShapes` 80; `addAnalog` from `AnalogClock` without the seconds hand, all white, minor marks 1 px, centre disc).

- [ ] Step 1: capture the always-on form of the five styles; measure the minute wake with the Classic page (`O`, logger) as in phase 1. Expected ≤ 0.35 s app time on average.
- [ ] Step 2: commit with the numbers.

### Task 4: Portal, documents, hand-over

**Files:** `main/portal/index.html` ("Analog" without "(coming)"; choosing analog Classic for a page turns its seconds hand on), `CLAUDE.md`, `README.md`, the spec (phase 2 result).

- [ ] Step 1: `node --check`; preview pictures; release build without warnings, flashed back.
- [ ] Step 2: final whole-branch review of phase 2 by a fresh reviewer; fixes with tests; commit; report.
