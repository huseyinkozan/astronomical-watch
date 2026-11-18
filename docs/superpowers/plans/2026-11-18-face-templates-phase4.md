# Face Templates Phase 4 (Corner Gauges) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Each of the four corner gauges shows an item chosen in the portal (global, the same on
every page), from the spec's catalogue, on the active and the always-on face.

**Architecture:** `FaceConfig::Pages` gains the four gauge choices and a parameter each (encoded
after the pages). `Corners::compute` takes them with a context (time, offset, battery, the Sun's
altitude, the counters) and fills each gauge from host-tested rules; `Corners::Input` gains
today's prayer times and the Hijri day. New gauge kinds get colours in `CornerView` and
`AlwaysOn::gaugeColour`.

**Spec:** `docs/superpowers/specs/2026-11-18-face-templates-design.md` (Corner gauges, Phase 4).

## Global Constraints

- Catalogue: empty; day or night; battery; the Moon above or below the horizon; the Moon's phase; the next prayer (from the previous prayer, all six intervals, across midnight); the next sunrise, sunset, moonrise, moonset (from the previous one to the next); the week (Monday 00:00 to the next); the month, Gregorian; the month, Hijri; the year (Gregorian); a day counter (one of four; since the day it was saved, yearly from the last anniversary, the last 30 or the last 100 days before its date); the Sun's altitude (0–90°, empty at night); the hour (24 or 12 hours a turn); the minute; the second (active face only, empty on the always-on face).
- Defaults keep today's four: day or night, battery, the Moon's pass, the Moon's phase.
- Older saved page strings without gauges decode with the default gauges.
- Minute wake within about 0.35 s of app time; host tests green; CLAUDE.md style.

## Review Focus

1. The prayer gauge between the night prayer and the next Imsak (across midnight) and before the first prayer of the day.
2. A day counter whose date has passed, whose saved day is unknown (saved by an older version), or yearly on 29 February.
3. Event countdowns in the polar day or night (no sunrise), and the Moon without a rise on a day.
4. The second gauge on the always-on face (must stay empty) and the hour gauge at 12 and 24 hours.
5. A gauge string from the portal with unknown items or parameters: clamped, never a crash.

---

### Task 1: The model (host-tested)

**Files:** `main/FaceConfig.h/.cpp` (`GaugeItem` enum, `Pages::gauges[4]`, `Pages::gaugeParameters[4]`, defaults, `valid`/`clamp`, encode after the pages as `/` and eight numbers, decode with or without them), `tests/FaceConfigTest.cpp`.

### Task 2: The rules (host-tested)

**Files:** `main/Corners.h/.cpp` (`Input` gains the prayer times and the Hijri day; `Context`; `compute( input, context, items, parameters, gauges )`; new `Kind`s), `tests/CornersTest.cpp` (every item at chosen moments, Review Focus 1–4), callers.

### Task 3: Data and drawing

**Files:** `main/DayCounters.*` (the day each counter was saved, its own NVS key; unknown for older counters), `main/Portal.cpp` (keeps or sets it), `main/FaceTable.*` (the counters' saved days), `main/main.cpp`, `main/AlwaysOn.*` (Look gains the gauges), `main/CornerView.cpp` (colours), development command "C <corner> <item> <parameter>".

- [ ] Pictures of every item on both faces with the fixed scene; minute wake with the busiest gauges.

### Task 4: Portal, documents, review

**Files:** `main/portal/index.html` (a "Corner gauges" block in the Faces tab), `tools/portal_preview.py`, `CLAUDE.md`, `README.md`, the spec (phase 4 result).

- [ ] node --check; preview; release build; final review; fixes with tests; commit.
