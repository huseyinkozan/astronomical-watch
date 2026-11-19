# Planets page

## Intent

The owner wants the five planets seen with the naked eye on the watch: where they are in the
solar system, where they are in the sky now, when they rise and set today, and how far they are.
The watch already computes the Sun and the Moon on the device (`main/Astronomy.*`, host-tested
against Skyfield with the JPL DE421 ephemeris); the planets follow the same rules: computed on
the watch from the date, time and location, no network.

## Goals

- A new page between the information and settings pages:
  watch face · clock · information · **planets** · settings.
- Four parts, stepped with a vertical swipe like the other stacked pages (`main/StackPage.*`),
  the page name ("Planets" / "Gezegenler") at the top:
  1. **Solar system**: the Sun in the middle, the orbits of Mercury, Venus, Earth, Mars, Jupiter
     and Saturn as thin dim circles, each body at its heliocentric ecliptic longitude. Radii on a
     square-root scale of the mean distance, Saturn's orbit 180 px (Mercury 36, Venus 50, Earth 58,
     Mars 72, Jupiter 133). Seen from the ecliptic's north: anticlockwise, the vernal equinox to
     the right. Each planet a disc in its colour (Earth blue), Saturn with a thin ring; sizes only
     to tell them apart, not to scale. No text.
  2. **Sky**: the celestial sphere seen from the south as a circle of 160 px radius: the horizon
     a horizontal line through the centre, east ("E"/"D") at its left end, west ("W"/"B") at its
     right end, the zenith on top. A body at altitude h and azimuth A (from north, clockwise)
     sits at x = cx + R cos h sin(A − 180°), y = cy − R sin h. The upper half circle is a thin
     line; the lower half (below the horizon) dotted and dim, as the Full arcs face draws the
     Sun's and the Moon's orbits; bodies below the horizon in a dim tone of their colour. The Sun
     and the Moon are drawn too, for orientation.
  3. **Rise and set**: a table (`main/TablePart.*`) with the header "Rise" / "Set" and a row per
     planet, today's local rise and set times ("--:--" when none falls on the day). The planet
     names are written in their colours: the table is the key to the colours of parts 1 and 2.
  4. **Distance**: a table with "AU" / "AB" and "Light" / "Işık": each planet's distance from
     Earth in astronomical units (two decimals) and the light time in minutes ("51 dk").
- Positions within a few arcminutes of Skyfield (DE421) for 2000–2050; rise and set times within
  two minutes; host tests like `tests/AstronomyTest.cpp` with reference values made by Skyfield.
- Colours: Mercury grey (0xAAAAAA), Venus pale yellow (0xF0E1AA), Earth blue (0x5A8CFF),
  Mars red (0xE65A3C), Jupiter beige (0xE1BE8C), Saturn gold (0xDCBE6E); the Sun and the Moon as
  on the faces.

## Non-goals and anti-goals

- No Uranus or Neptune (the inner orbits would crowd: Mercury ~20 px).
- No "visible now" highlight (the rise and set times say it), no conjunction or event search
  (later, if wanted).
- Nothing on the watch faces or the always-on face; no new periodic wake; no change to the
  minute wake.
- No label text in the drawings (no room); no zoom or touch interaction beyond the swipes.
- The page must not hold heap when far from the shown page (as the information and settings
  pages, `Display::keepPages`).

## Approaches

- **Computation (builder's call):** the JPL "Keplerian elements for approximate positions of the
  major planets" (Standish; valid 1800–2050, arcminute-level for these planets) with Kepler's
  equation, then geocentric equatorial coordinates and altitude/azimuth with the existing
  sidereal time and observer code. Rejected: truncated VSOP87 (more accurate than a watch needs,
  large tables); Meeus' per-planet series (more code for the same result here).
- **Rise and set:** the existing crossing search of the Sun (`Astronomy::sunDay`: samples, then
  bisection), with the planet's altitude and the standard −0.5667° (refraction, no
  semi-diameter). Computed once per local day, when the page is built or a new day begins, not
  every second; positions for parts 1 and 2 once a minute.
- **Drawing:** parts 1 and 2 are each one LVGL object drawing on `LV_EVENT_DRAW_MAIN` (as the
  calendars and the arcs faces); parts 3 and 4 are `TablePart`s with coloured names.

## Constraints

- CLAUDE.md style and rules; every text in `main/Language.*` in English and Turkish.
- Astronomy code plain C++ and host-tested (a new `main/Planets.*` beside `Astronomy.*`).
- The visible area: content inside the corner gauges' quarter circles (docs/hardware.md).
- Heap measured with the page built and with the portal on; stack margins from the event log;
  screenshots (fixed scene) for the published pages.

## Result (2026-11-19)

Built on branch `planets` (not pushed). Positions against Skyfield (DE421) over 2000–2050:
Mercury 0.6′, Venus 0.5′, Mars 1.7′ (seen near opposition), Jupiter 0.9′, Saturn 1.7′ and
0.018 AU; Jupiter and Saturn needed Schlyter's elements with the great-inequality terms (JPL's
Table 1 alone: 5′ and 9′). Rise and set within 5 s at middle latitudes and 9 s at Tromsø and
Reykjavik, the days without a rise or set matching. Interpolating the places through the day
brought the day's computation from 1.7 s to 0.31 s on the watch. The page takes 3.3 KB of heap.
The Turkish page name is "Gezegen": "Gezegenler" did not fit between the header line's bends.

## Decided by the builder (2026-11-19, for the owner to check)

- **Order and hiding:** the planets page's parts get an order and hidden parts set in the portal,
  as the clock, information and settings pages (the owner's earlier rule for stacked pages): two
  new settings at the end of `Settings::Item`, a block in the portal and its help page. The page
  itself cannot be hidden.
- **Accuracy:** JPL's Table 1 elements are within about 1′ for Mercury to Mars but about 7′
  (Jupiter) and 10′ (Saturn). The host tests measure the errors against Skyfield first; when
  Jupiter's or Saturn's are above about 2′, the main terms of their mutual perturbation (the
  "great inequality", a few sine terms) are added. Rise and set times within 2 minutes at the
  mid-latitude reference places; at Tromsø a wider tolerance with its reason in the test (a planet
  low in the sky there changes altitude slowly, so an arcminute moves the crossing by minutes).
  The distance part's second decimal may differ by one for Saturn.
- The planets page is freed far from the shown page like the information page, and also while the
  portal runs unless it is shown.

## Left to the builder

The exact sample step of the rise and set search, the dot sizes and the dim tone, the placement of
the "E"/"W" letters, where the per-day computation runs (main task or LVGL task) as long as the
first view of the page does not freeze the swipe noticeably.
