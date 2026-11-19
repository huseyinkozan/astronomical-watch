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
  1. **Solar system**: the Sun in the middle, the orbits of the eight planets as thin dim circles,
     each body at its heliocentric ecliptic longitude. Radii on a logarithmic scale of the mean
     distance (changed from a square root, see Result): Mercury 30 px to Neptune 180 px. Seen from the ecliptic's north: anticlockwise, the vernal equinox to
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

Built on branch `planets` (not pushed). After a preview the owner chose all eight planets on a
logarithmic scale (Mercury's orbit 30 px, Neptune's 180 px) instead of the square root, and
Uranus and Neptune on every part (small in the sky: the eye does not see them); Neptune violet,
apart from the Earth's blue; distances from 10 AU with one decimal, so that the columns stay
apart. Positions against Skyfield (DE421): the test's 13 moments give Mercury 0.6′, Venus 0.5′,
Mars 1.7′, Jupiter 0.9′, Saturn 1.7′ (0.018 AU), Uranus 1.8′, Neptune 0.8′; the final review's
check every 7 days over 2000–2050 found Venus 1.6′, Mars 3.3′ (near opposition) and Saturn 2.1′
at worst, a few pixels' fraction on the sky circle. Jupiter and Saturn needed Schlyter's elements
with the great-inequality terms (JPL's Table 1 alone: 5′ and 9′). Rise and set within 5 s at
middle latitudes and 9 s at Tromsø and Reykjavik, the days without a rise or set matching.
Interpolating the places through the day brought the day's computation from 1.7 s to 0.31 s for
five planets (0.43 s for seven) on the watch. The page takes 3.3 KB of heap. The Turkish page
name is "Gezegen": "Gezegenler" did not fit between the header line's bends.

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

## Planets on the arc faces (2026-11-19, the owner's decisions after a preview)

- An option per face page, "Planets" beside "Milky Way" in the portal (`FaceConfig::PlanetsFlag`),
  on by default on the default Full arcs and Half arcs pages.
- The seven planets sit between the Sun's and the Moon's arcs (Full arcs r 136, between 150 and
  122; Half arcs r 127, between 150 and 105), placed by the faces' rule for the Sun and the Moon
  (`Corners::arcPlace`): the share of their way from rise to set (and on the Full arcs, below
  the horizon, from set to rise). Computed from the hour angle and the semi-diurnal arc
  (closed form, no search), so that a wake pays a few milliseconds, not the 0.4 s of the
  planets page's rise and set.
- Dots in the planets page's colours and sizes for a face (Jupiter 5 px, Venus and Saturn 4,
  Mercury and Mars 3, Uranus and Neptune 2; Saturn with a short ring line), drawn from the largest
  to the smallest, so that a small one stays visible on a large one.
- Full arcs: below the horizon in the dim tone, as the Sun and the Moon; Half arcs: only above
  the horizon. The always-on face: the idle-mode colours (Mercury white; Venus, Jupiter, Saturn
  yellow; Mars red; Uranus cyan; Neptune blue); below the horizon every second pixel lit.
- Cost to measure: the minute wake with the option on (estimate +20 ms).

Result (2026-11-19): `Planets::arcPlace` gives the place from the hour angle H and the half arc
H0 (cos H0 = (sin −0.5667° − sin φ sin δ) / (cos φ cos δ)); on the Full arcs the faces' rule
reduces to 270° + H. Against Skyfield's rises and sets at Istanbul, Sydney and Quito: 28 rows,
largest error 0.06°. On the board in the fixed scene the four faces match the preview. The
always-on face draws the planets below the horizon as discs with every second pixel lit, Saturn
without its ring there; Uranus got an idle-mode cyan. Minute wake, Half arcs page, measured with
the wake statistics: 307 ms of app time on average with the planets (7 wakes; the Milky Way flag
was on too, its band not drawn by day) against 285 ms without (8 wakes), the last wake 249 against
234 ms: about 20 ms, as estimated. Heap in active mode at least 140 KB; stack margins main
3764 B, LVGL 3664 B. Saved face pages of earlier versions keep the option off ("Load default
faces" in the portal, or the box). This section replaces the non-goal "nothing on the watch faces".
