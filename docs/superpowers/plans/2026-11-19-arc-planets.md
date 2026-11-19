# Planets on the Arc Faces Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** The seven planets on the Full arcs and Half arcs faces, active and always-on, as an
option per face page, on by default on the arc pages.

**Spec:** `docs/superpowers/specs/2026-11-19-planets-design.md` ("Planets on the arc faces").

## Global Constraints

- Radii: Full arcs 136, Half arcs 127; dot sizes and colours as the spec; drawn largest first.
- Below the horizon: Full arcs dim (active) / every second pixel (always-on); Half arcs hidden.
- The minute wake measured with the option on; nothing changes with it off.

## Review Focus

1. A planet that never rises or never sets at the place (high latitude): not drawn, no NaN.
2. Southern hemisphere: the Half arcs' direction mirrors as for the Sun (`south`).
3. A page without a location: no planets drawn.
4. Saved face pages of earlier versions: the flag off (only the defaults have it on).
5. The always-on shape budget with the Milky Way band, both orbits and seven planets.

### Task 1: Placement (host-tested)

`Corners::hourAnglePlace( double hourAngle, double semiArc, bool orbit, bool south )` (the
`arcPlace` rule from angles) and `Planets::arcPlace( Body, int64_t time, const Astronomy::Observer &, bool orbit )`;
reference angles from Skyfield's rise and set (`tools/make_planet_arcs.py` →
`tests/planet_arcs.csv`), within 2°. Commit.

### Task 2: The option

`FaceConfig::PlanetsFlag = 0x04`; defaults on the Full arcs and Half arcs pages
(`tests/FaceConfigTest.cpp`); the portal's checkbox beside "Milky Way"; help page. Commit.

### Task 3: Drawing

`FacePage` (active) and `AlwaysOn` (a disc with every second pixel); screenshots of both faces
in the fixed scene against the preview; minute wake measured; heap and stack margins. Commit.

### Task 4: Documents and review

CLAUDE.md, README, spec result; final review (opus); fixes with tests; commit.
