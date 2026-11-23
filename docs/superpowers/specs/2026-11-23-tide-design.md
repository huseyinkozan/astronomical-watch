# Tide: design (2026-11-23)

## Intent

The owner's Casio shows the tide; the watch should too, from what it already knows (the Moon)
and one setting of the place. It is an approximation for ocean coasts: Turkey's seas are
microtidal (a few cm to some tens of cm), so the help says where it is of use. Research:
`research/tide-gauge.md` (not in git); mock-ups: `2026-11-23-tide/mock.png` (A-E) and
`2026-11-23-tide/mock2.png` (the icon's three shapes).

## Decisions (the owner, 2026-11-23)

- **A, a corner gauge** (FaceConfig::GaugeItem, appended): it fills with the level, low water
  empty, high water full; cyan while the water rises, blue while it falls; the notch in the
  middle as on every gauge. A tap opens D.
- **B, a corner icon** (FaceConfig::CornerItem, appended): a wave whose crest reaches the
  icon's top at high water; at low water no wave, a flat blue sea line at 20 % of the icon's
  height with the part below it blue; between them the crest grows with the level. Shape: B3
  of `mock2.png`, a crest leaning to the shore (a slow rise to 68 % of the width, a steep
  front), the owner's pick. A tap opens D.
- **D, an information part** on the Info page after the Moon (see the regrouping below): the day's curve (0-24 h, cyan
  rising, blue falling, now marked), today's high and low waters, the state now (rising or
  falling, the level in %), spring or neap by the Moon's age, the interval set, and
  "approximate: the Moon only".
- **E, the portal's setting**: needed, because the delay between the Moon's transit and high
  water (the lunitidal interval) changes by hours from coast to coast and nothing on the watch
  can know it (a global tide model would be large and coarse near coasts). Without it nothing
  shows the tide.
- C (a text line) is left out.

## The pages regrouped (the owner, 2026-11-23)

A page holds at most eight parts (`PartOrder`'s 3-bit fields) and the Info page had eight. The
owner let the parts be regrouped by meaning: the Info page keeps the Sun, the twilights, the
prayer times, the Moon, the tide (new, after the Moon), the sun compass and the time (seven);
the battery and the about parts move to the settings page, before the factory reset (eight).
The Info page's saved order and hidden mask fall back to the defaults once.

## The model (host-tested, `main/Tide.*`)

Casio's: high water at the Moon's upper and lower transits plus the interval; the level a
cosine of half the time between transits (about 12 h 25 min): level = (1 + cos(2π (t − high)
/ period)) / 2; rising where its slope is positive. Spring and neap from the Moon's age
(strongest at new and full Moon). The Moon's transits come from the code that already gives
the almanac's meridian passage. Tests: the level is 1 at a high water and 0 half a period
later, the next high and low times, rising and falling, the day's list across midnight, a
transit missing on a day (the Moon transits about 50 min later each day).

## My calls (for the owner to check)

- The setting: `Settings::TideInterval` in minutes, 0 to 745 (12 h 25 min), "off" by default;
  in the portal's location box, as hours and minutes, with a short help: how to find it (a high
  water time from the port's table minus the Moon's transit before it, which the Moon part
  shows) and where the tide is of use.
- D is hidden by default (as the almanac and aiming): the portal shows it; without the setting
  it says to set the interval.
- The icon is blue whatever the direction (the owner's words); the gauge alone tells rising
  from falling by colour.
- The always-on face: the gauge as the others (2 px, idle-mode colours); the icon as its
  surface line only, in white (the always-on face's bitmaps are 1-bit).
- The night-red mode converts them as everything else.
- The portal's help page, the site's face section (its option tables and icon pictures) and
  the screenshots get the new items.

## Not in this

The tide's height in metres or centimetres (it needs the port's range), tide tables of ports,
the weather's effect.
