# Configurable face templates — design

Date: 2026-11-18. Status: agreed with the owner in conversation (questions of a builder check
answered the same day); to be built in phases.
Mock-ups: `2026-11-18-face-templates/templates.png` (active and always-on of each template),
`details.png` (weekday icon, digital and analog styles), `seven-segment-proportions.png`;
`mock.py` draws them (run from the repository root with `tools/.venv/bin/python`).

## Intent

Today the watch has four fixed watch faces (Classic, Day arc, Sky arcs, Dial) and five fixed
always-on styles, each with a few on/off items. The owner wants to compose the faces instead:
a small set of templates whose clock and surrounding information are chosen in the setup
portal. One choice drives both the active face and the always-on face, so they always match;
the always-on face is the chosen page drawn the 1-bit, dim way.

## Goals

- Face pages built from templates; the vertical swipe on the face steps through them, the
  portal orders and hides them (as today with the styles).
- The same clock choices in every template: digital or analog, and the same style lists; the
  size follows the room the template gives the clock.
- Information slots around the clock, chosen from catalogues; each slot can also be shown on the
  always-on face or only on the active face.
- The four corner gauges become configurable too, from a catalogue of cyclic quantities.
- New elements: seven-segment digits of our own geometry, a weekday icon, the Milky Way on the
  half-arc template, five analog styles.

## Non-goals and anti-goals

- No free placement (x, y) of components: slots have fixed places, so nothing can run into the
  corner gauges, the panel's hidden corners or the burn-in shift.
- No fonts from third parties for the seven segments (our own geometry, no licence question).
- No italic digits.
- The always-on minute wake must not grow noticeably (today about 0.33 s from reset); battery
  life keeps its priority (CLAUDE.md). Measure it with the wake statistics after each phase.
- No new periodic wakes; nothing animated on the always-on face.

## Templates

Each face page is one template with its own settings. Four pages by default:

1. **Full arcs** — the Sun's and the Moon's orbits around the centre (today's Dial); the clock
   inside the inner orbit (small).
2. **Half arcs** — the Sun's and the Moon's paths over the horizon (today's Sky arcs), the clock
   below the horizon (medium). Option: the Milky Way band (below).
3. **Large, digital** — the clock alone in the middle (large).
4. **Large, analog** — the same template with an analog clock.

Pages 3 and 4 are the same template ("Large") with a different clock type: every template takes
either clock type. The number of pages stays small (at most six; the marks along the right
edge show the position).

## The clock element

- **Type**: digital, analog or none, per page (none: the time only in the corners, see the
  slots, or not at all).
- **Digital styles** (one list in every template): JetBrains Mono (today's font), seven segment;
  more can be added later. Seconds are a text slot, not part of the clock.
- **Seven segment**: hexagonal segments (pointed ends), digit width = 0.62 × height (≈ 1/φ),
  segment thickness = height / 11, joint gap ≈ height / 40, upright. On the always-on face the
  segments are filled with the 1-bit dot grid of today's Dense style. Sizes: about 122 px high in
  Large, about 64 px in Full arcs (mock-ups). Basis for the proportions: MIL-STD-1472 gives 1/7–1/8
  for light characters on dark as the upper end, thinner for night use; DSEG7 uses 1/16, 1/11 and
  1/8 in its three weights; the owner chose 1/11 with the 0.62 width.
- **Analog styles** (one list in every template): Classic (today's Dial hands and marks), Bold
  hands, Fine scale (60 minute marks), One hand (hour hand only, read on the fine scale), Plain
  (hands only, no marks).
- **Seconds hand**: an option per page for the analog clock on the active face (the owner
  agreed); on by default with Classic, as today's Dial. Never on the always-on face.
- **Size by template**: Large ≈ full width between the gauges; Half arcs ≈ 70 px digits or a
  ≈ 90 px radius dial; Full arcs ≈ 64 px digits or a ≈ 100 px radius dial (inside the Moon orbit).

## Slots

Eight slots, in the same places in every template (pixel places in the mock-ups):

- **Text slots** (centred, the watch font 26 px): top edge (between the two upper gauges, centre
  y ≈ 25, about 10 characters), top inner (y ≈ 60), bottom inner (y ≈ 442), bottom edge (y ≈ 478,
  about 10 characters). A text that does not fit is cut, not wrapped.
- **Corner slots**: the four inner corners inside the gauges (centres about (66, 70), (344, 70),
  (66, 432), (344, 432)); today's moon icon sits in the bottom-left one.

Text slot catalogue: empty; date long ("21 Haziran Pazar"); date short ("21 Haz"); Hijri date;
next sunrise/sunset with the time left; next prayer with the time left; seconds (active face
only); battery percent; the first world clock city's time; a day counter, any of the Days part ("Ramazan 232");
the Sun's altitude; the Moon's altitude.

Corner slot catalogue: empty; moon phase icon; weekday icon; weekday short ("Paz"); day of the
month, Gregorian or Hijri ("21"); month short, Gregorian or Hijri ("Haz"); the Sun's altitude
("☀ 13°", a drawn sun, the font has no symbol); the Moon's altitude; the hour, the minute or the
second on its own ("19"; the owner's example: the hour top left, the minute top right; the second
on the active face only).

Each slot has an "also on always-on" flag. The always-on face shows the slot only when it is set
and the content makes sense there (seconds never).

### Weekday icon

The day's first letter in a ring of seven notches, about 44 px across (a little larger than the
moon icon); the first `n` notches lit for the n-th day of the week (Monday one, Sunday seven), so
the lit count also tells Monday from Sunday (both "P" in Turkish). Lit notches amber on the active
face, unlit ones dim grey; on the always-on face only the lit notches, in white.

## Corner gauges

Each of the four corner gauges takes one item of this catalogue (global, the same on every page):
day or night (today's top left), battery (today's top right), the Moon above or below the horizon
(today's bottom left), the Moon's phase (today's bottom right), the time between two prayers,
the week, the month (Gregorian or Hijri), the year, a day counter (any counter of the clock page's Days part; the owner wants all three fills
selectable per gauge: from the day the counter was saved in the portal to its date; yearly, from
the date's last anniversary to the next; a fixed span, the last 30 or 100 days before the date;
the counters therefore keep the day they were saved), the
Sun's altitude (0–90°, empty at night), the hour (12 or 24 hours a turn, selectable), the minute,
the second (the second on the
active face only: its corner is drawn again every second; empty on the always-on face), empty.
The owner's rule: any cyclic time unit can be a gauge. Defaults keep today's four.

Countdowns: any coming event can be a gauge as well (the owner's addition): the next sunrise,
sunset, prayer time, moonrise or moonset. The gauge fills from the event's previous occurrence to
its next one, is full at the event and starts again; the time between two prayers is this rule for
"the next prayer".

Decided by the builder: the prayer gauge runs through all six intervals, Isha to the next Fajr
across midnight included; the month gauge is two catalogue items (Gregorian, Hijri); the year is
the Gregorian year.

## Milky Way (Half arcs option)

Drawing (decided by the builder): the band is the mock-up's circle of fixed curvature through the
observer, turned around the observer like a hand by the centre's azimuth; it does not follow the
galactic plane's real tilt. South of the equator the arcs' directions mirror (north up), so the
band's direction mirrors too.


The galactic centre (Sagittarius A*, RA 17h45m40s, Dec −29°00′28″, J2000) is a fixed point of the
sky; its altitude and azimuth come from the same sidereal time and observer code as the Sun and
the Moon (`main/Astronomy.*`). On the half arcs it is drawn as a wide, faint band (a large circle
through the observer, the middle of the horizon line), turned like a clock hand: its direction
follows the centre's azimuth (east at the left end, south up, west at the right end, as the arcs
run). Bright when the centre is above the horizon and the Sun is at least 18° below it (a dark
sky); dim otherwise; hidden while the centre is below the horizon. The host tests check the
centre's altitude and azimuth against Skyfield.

## Always-on face

The always-on face draws the page the active face shows (the last one seen before always-on), with
`AlwaysOn::layout` extended from the same page settings: the clock in its 1-bit form (the
digital styles as host-generated glyph sets per size, `tools/make_aod_glyphs.py`; analog hands
and marks as shapes), the arcs as dots, the slots marked "also on always-on" with the small
1-bit glyphs, the weekday icon as shapes. The small glyph set grows to the letters the slot texts
need in both languages (ASCII and the Turkish letters, digits, ':', '°', '%'). The always-on page
of active mode keeps showing exactly what the minute wakes draw.

Keeping the always-on texts true for hours (decided by the builder, 2026-11-18): when active mode
ends, today's and tomorrow's event times (sunrise, sunset, the prayer times, the Hijri day change)
go into `AlwaysOn::Look`; a minute wake picks the next one and its time left from that table. The
Sun's and the Moon's altitudes are computed in the minute wake (a few trigonometric terms; to be
measured). When the always-on face lasts beyond the table (48 h), one wake recomputes it.
Budget: the average minute wake stays at about 0.35 s from reset; the rare recomputing wake may
take longer.

The always-on preview page (the leftmost tile) goes (the owner's decision): the always-on face
follows the face page; its brightness is set from the face (see Transition). The page in use is
saved in `FaceStyle`.

## Settings and portal

- A new module `FaceConfig` keeps the page list in one NVS blob (like `WorldClock`): per page the
  template, clock type, digital style, analog style, Milky Way flag, eight slot choices with
  their always-on flags; plus the four corner gauge choices. About 16 bytes per page.
- The old settings `FaceStyle`, `AodStyle`, `FaceItems`, `AodItems`, `FaceOrder`, `FaceHidden`,
  `AodOrder`, `AodHidden` go without being carried over: after the update the watch starts from
  the default pages (the owner's choice).
- The portal gets a "Faces" section: one block per page (template, clock type, styles, the eight
  slots each with an "also on always-on" box) and the four corner gauges; order and hide as today.
- The watch's own settings page keeps no face settings; the vertical swipe chooses the page.

## Transition (the owner's decision, 2026-11-18)

The old four faces, the five always-on styles, the always-on page (the leftmost tile) and their
settings go in phase 1; nothing is carried over. Until phase 3 the watch has only Large pages.
The always-on brightness moves into the face's brightness control: after the long press a
vertical drag sets the active level, a horizontal one the always-on level of the moment.

## Phases

Each phase ends built, flashed, checked on the board with screenshots of every affected page and
its always-on face (fixed scene), the minute-wake time from the wake statistics, the heap with
the portal on, and the host tests; the owner reviews at the end.

1. **Framework and Large digital**: `FaceConfig` with defaults (the old faces, always-on styles,
   always-on tile and their settings removed); the Large template with both digital styles; the eight slots with the text and corner
   catalogues including the weekday icon; the always-on face from the same page; the portal
   section.
2. **Analog**: the five analog styles in Large, active and always-on.
3. **Arcs**: Full arcs and Half arcs as templates (today's Dial and Sky arcs rebuilt on them),
   either clock type; the Milky Way band with its host tests.
4. **Corner gauges**: the gauge catalogue, generalised cyclic quantities, portal choices.
5. **Clean-up and release**: what is left of the old code removed, CLAUDE.md,
   README and the screenshots per language, release v0.0.3 with the earlier batch (commit
   776b14e).

### Phase 1 result (2026-11-18)

Done on branch `face-templates`, unreleased. What the board showed and what changed from the text
above:

- Slot widths: 176 px (11 characters) at the edges; an inner slot shares its row with the corners,
  so it takes 208 px beside a used corner and 320 px otherwise. The default pages follow the
  approved mock-up (page 1: short date, next prayer, day counter, weekday ring, Sun altitude,
  moon, day of the month; page 2 in JetBrains Mono: long date and Hijri date, no corners), and a
  host test checks that their texts fit in both languages.
- A name-and-number text cut to its slot loses letters of the name, never digits of the number
  ("Eve dönü 12", not "Eve dönüş 1").
- The altitude corners show the value in the body's colour (amber Sun, pale Moon); the font has
  no sun sign.
- The always-on seven-segment digits are bitmaps made on the host from the same geometry, dotted;
  the always-on texts use the watch font's characters at 26 px, all white; the weekday ring shows
  its lit notches as chords.
- Minute wake with the full-page always-on face: 243 ms of app time on average over 12 minutes
  (225 to 336 ms; the first, which clears the screen, 422 ms). Portal on: 121 KB of internal
  heap free.

## Left to the builder

The exact pixel places within the mock-ups' layout, colours of new elements (following today's:
white values, grey captions, amber for the Sun and highlights), the glyph sizes the always-on
generator emits, the data layout of the blob, and how the portal block is laid out, as long as
it stays usable on a phone.
