# Cycle page: menstrual cycle tracking

Mock-ups: `2026-11-22-cycle-page/pages.png` (every part, day and night), `entry.png` (the day's
entry), `corner.png` (the corner item, unlocked and locked); drawn by the scripts beside them
(`mock2.py` … `mock9.py`, Pillow, the watch font).

## Intent

A page for women who wear the watch: the menstrual cycle tracked on the watch alone, detailed
enough to show what they need (where they are in the cycle, when the next period is likely, the
fertile days as an estimate, how their cycles vary, warning signs, and for Muslim women the
days of hayız), not a bare counter. Girls who have just started menstruating get a mode of
their own. Everything is entered and read on the watch; nothing leaves it (no network), and the
page is locked against someone who picks up the watch.

The owner chose every visual and rule below in the design conversation of 2026-11-22, with web
research on how apps and watches show the cycle, prediction methods and their failure rates,
the Moon, variable cycles, Diyanet's rulings and adolescent norms (sources at the end).

## Goals

### The page and its parts

A new stacked page **Cycle** ("Döngü"), a new `Page` value appended after `ObservingPage` (the
saved launcher order keeps its numbers). Its parts, in this default order:

1. **Cycle** (the spiral, below) – the first part.
2. **Calendar** – the month grid of the clock page's Gregorian calendar (sideways swipe steps
   the month, "<" ">" at the bottom), today in a box, under each day's number its marks
   (drops, rings). A tap on a day opens the day's entry for that day (past days and today; no
   future days).
3. **History** – one row per cycle, aligned at day 1 (about 7 px a day, a ruler 1, 8, 15, 22,
   29, 36), bleeding days red, the cycle's length at the row's end; the current cycle last,
   filled to today with an arrow under today, then its next start's likely days as dim red
   outlined cells. Below: the mean length, the range and spread ("26-31 (±1.9)"), "regular" or
   "irregular", and the warning notes (below) on the rows they concern.
4. **Moon** ("Ay ile") – rows are synodic months (29.53 days) from new moon to new moon, the
   Moon's phases above the first row (new, first quarter, full, last quarter, new); each cycle
   start a red dot in its row. A vertical line of dots would mean the cycle follows the Moon,
   a drift or scatter that it does not. Text: "row: one Moon month (new moon at the left)",
   "dot: the day a period started", and in amber "For information; the Moon does not set the
   cycle (studies)".
5. **Pregnancy** ("Gebelik") – the current cycle's days in a row (boxes, rings above, drops
   below, an arrow under today), then three rows: fertile window (dates), ovulation (date),
   next period (date range); below in amber: "Estimate: not for contraception or for getting
   pregnant. With calendar methods 12-24 of 100 women a year become pregnant (typical use)."
6. **Hayız** – Diyanet's rules (below); hidden by default.
7. **Settings** – the page's own settings (below).

The page is hidden in the launcher by default (Claude's call: the published firmware is used by
men too; a woman shows it in the portal's page list or on the watch's settings).

### Symbols (all parts and the corner icon)

- **Box = a day.** Filled: a day that passed (today included); outlined: a day to come.
- **Drop = bleeding** (a disc with a triangle on top, tip up), always filled; its size shows the
  logged flow (light 0.75, medium 1.0, heavy 1.3 of the base drop, about 9 x 12 px). Estimated
  bleeding (the next period's likely days) is a dim drop; the likely middle (the mean ±1.5
  days) a little brighter than the range's ends.
- **Ring = a fertile day** (an egg cell, open middle). Days that passed: blue; estimated: dim
  blue. The estimated ovulation: a larger, thicker ring.
- **Intercourse:** the ring open towards the outside of the spiral (a gap of about 60°), a
  filled dot in its middle whose straight tail leaves through the gap (a sperm in the egg).
  Drawn instead of the day's ring (on any day, fertile or not; outside the fertile window the
  ring is grey).
- Colours: bleeding red, fertile blue, days grey; warnings amber.
- **Red night mode** (`Settings::NightMode`, NightRed): one red, two levels (bright, dim). The
  shapes carry the meaning: filled/outlined boxes, drops (estimated drops half filled: the
  outline with the lower half filled), rings bright (passed) or dim (estimated). No meaning may
  rest on colour alone.

### The Cycle part (the spiral)

- A piece of an Archimedean spiral; **one turn is one synodic month** (29.53 days), so the
  Moon's phases stand as a fixed ring of 30 small phase discs around it (new moon at the top,
  the phase of each angle). Today sits on the arm at its own Moon phase's angle.
- The arm runs from about 18 days before today (inner end) to about 24 days after (outer
  end), moving out about 52 px a turn (today at about 112 px from the centre). Both ends fade
  out (dimmer boxes), so that the arm reads as part of an endless path.
- Each day: its box on the arm, its drop just inside the arm, its ring (or intercourse mark)
  just outside it.
- In the middle: the cycle day large ("12.", the light 48 px font or the nearest existing one),
  "day" under it, today's date under that as day and short month ("1 Ara"). An arrow with a
  shaft runs from the number straight to today's box.
- At the bottom, above the lowest line, in amber: "Estimate; not for contraception or
  pregnancy". The lowest line: a large drop and the next period's range ("14-22 Ara") in red.
- A tap on today's box opens the day's entry for today. (Other taps do nothing; the page swipe
  stays as on the other stacked pages.)
- No logged period yet: the spiral without drops and predictions, the middle text "no period
  logged", the lowest line "tap today's box to log".

### The day's entry

An overlay over the part, as the unit converter's keypad (`entry.png`): the day as the title
("Today, 1 Ara" or "29 Kas Pazar"), "Bleeding" with four keys (none, light, medium, heavy:
a dash and drops of three sizes), "Intercourse" as a key that toggles ("yes"/"no", with the
mark), and "OK", which saves; a part swipe, BOOT or the back chevron closes it without saving.
In the adolescent mode the intercourse key is hidden.

There is no separate "period started" entry: a logged bleeding day starts a cycle by itself
(rules below).

### The corner item and gauge

- **Gauge** (`FaceConfig::GaugeItem`, appended): the cycle from its start to its estimated end
  (the mean length), the passed part bright in the phase colours (red bleeding, blue fertile,
  grey otherwise), the rest dim in the same colours, a white notch across the gauge at today;
  from the gauge's lower end like the others. Always-on face: 2 px in idle colours, notches at
  both ends and the middle, as the other always-on gauges.
- **Icon** (`FaceConfig::CornerItem`, appended): a ring of the cycle's days (the mean length),
  passed days bright in the phase colours, the rest dim, a small white triangle outside the
  ring pointing at today, the cycle day's number inside. Always-on face: a one-bit bitmap.
- **Lock:** while the page is locked (always on the always-on face) both are neutral: one grey,
  no phase colours (a progress ring and a number, which reads like a counter or a date). After
  the PIN, until active mode ends, they show their colours.
- With the PIN switched off they show their colours whenever the display is active (nothing to
  unlock); the always-on face stays neutral.
- They never show intercourse, flow, hints or warnings. A tap opens the Cycle part (through the
  PIN when locked).
- No cycle logged: the gauge empty, the icon an empty ring without a number.

### Data

- **The log:** one byte per day for the last 366 days, a ring indexed by the local day number
  modulo 366, with the day of the newest entry kept beside it so that days that passed without
  an entry are cleared before use. Bits: 0–1 the flow (0 none, 1 light, 2 medium, 3 heavy),
  2 intercourse, 3 a make-up (kaza) fast kept, 4–7 free (always written 0, ignored when read). In NVS as one blob in a new
  namespace `cycle` (with the settings below), written when the entry's OK saves (not at
  every minute wake).
- **The page's settings** (namespace `cycle`, not `Settings`, so that the factory reset list
  and the "delete cycle data" button clear them together): the PIN (4 digits, stored as a
  salted hash or at least not in clear text; Claude's call: a 32-bit FNV-1a of a random salt
  and the digits is enough against someone holding the watch), PIN on/off, the adolescent mode
  (off, on, or automatic from the first period's date), the first period's date (optional).
- Factory reset (`factoryReset` in `main.cpp`) adds `cycle` to its list.

### Rules (host-tested, `main/Cycle.*`, no ESP-IDF or LVGL)

- **Cycle start:** a bleeding day (flow ≥ 1) after at least 10 days without bleeding (or the
  first bleeding day in the log). **Period length:** the start and the bleeding days that
  follow it with at most one day without bleeding between them. **Cycle length:** from one
  start to the next.
- **Prediction (next start):** the mean of the last 6 complete cycles (fewer if fewer exist) ±
  2 standard deviations, at least ±2 days, rounded to days. One complete cycle: its length ±4
  days. No complete cycle: no prediction (the spiral says so).
- **Adolescent mode:** normal range 21–45 days. Prediction: with 3 or more complete cycles
  from the shortest to the longest recorded cycle (±2 days), with fewer 21 to 45 days after the
  start. Automatic mode: on for 3 years after the first period's date; without a date it acts
  as on (the safe side: no fertile window, no intercourse key) and the Settings part asks for
  the date. The setting's default is off.
- **Regularity:** "irregular" when the shortest-to-longest spread of the last 6 cycles exceeds
  9 days (FIGO; 7 days from age 26 is not used, as the age is not known) or fewer than 3
  complete cycles exist; the text then says "irregular".
- **Fertile window**, the first rule that applies (Claude's call; never narrower than the
  wider candidate, as too narrow a window is the unsafe error):
  1. adolescent mode: none; the Pregnancy part and the spiral show one line instead, "Cycles
     are often irregular in these years; pregnancy is possible on any day";
  2. fewer than 3 complete cycles (no logged period at all included): days 6 to 21;
  3. 3 to 5 cycles: days 8 to 19 (Standard Days Method), days 6 to 21 when irregular;
  4. 6 cycles: Ogino-Knaus over the last 6 (day shortest − 18 to day longest − 11); when
     irregular the wider of that and days 6 to 21 (their union).
  **Ovulation estimate:** the predicted mean start − 13 days, drawn only in cases 3 and 4.
- **Warning notes** (on the History part's rows, amber, with "talk to a doctor" and in the
  adolescent mode "talk to a doctor or a trusted adult"):
  - adults (FIGO): a cycle under 24 or over 38 days; bleeding over 8 days;
  - adolescents (ACOG): a cycle under 21 or over 45 days; bleeding over 7 days;
  - both: 90 days or more since the last start (shown also on the spiral); three or more heavy
    days in one period; bleeding logged outside a period (a bleeding day that does not start a
    cycle and is not part of a period's bleeding) as "bleeding between periods".
- **Carry-a-pad hint:** from the first day of the predicted range until a bleeding day is
  logged, the spiral's middle shows "carry a pad" under the date (page only).
- **"What is normal":** a tap on the lowest line (the range) shows three short lines over the
  part for a few seconds: the normal cycle range (adults 24–38, adolescents 21–45 days),
  bleeding up to 8 (7) days, and in the adolescent mode "irregular in the first years".
- **Hayız (Diyanet, Hanafi):** counted on its own, apart from the cycle start above (which
  needs 10 bleeding-free days):
  - hayız lasts at least 3 and at most 10 days; at least 15 days of purity (tuhr) lie between
    two;
  - clean days between two bleeding days within the 10 days count as hayız (the common Hanafi
    teaching, "fâsid tuhr"; checked 2026-11-23: ilmihal sources agree, Diyanet's board says it for
    spotting after the usual days within ten days and for lohusalık; not found in these words for
    hayız on Diyanet's site, kept as built);
  - bleeding of fewer than 3 days, or starting within 15 days of the last hayız's end, is
    istihaze;
  - bleeding beyond 10 days: the habit (âdet), the length in days of the last valid hayız,
    counts as hayız, the rest istihaze; without a habit in the log, 10 days;
  - during hayız prayer and fasting are not done; in istihaze both go on;
  - missed prayers are not made up; missed Ramadan fasts are (kaza).
- **Kaza fasts** (the owner's request): the day's entry, while the Hayız part is shown, gets a
  third key "made up a fast" (bit 3 of the day). The calendar marks such a day with a small
  crescent under its number. The Hayız part shows "Make-up fasts left: N", where N = the
  Ramadan days that fell in hayız (each Ramadan added once, when it has ended, from `Hijri`;
  the Hijri year added is stored) + an earlier balance (entered on the Settings part with the
  keypad: other years, illness, travel) − the made-up fasts. The added Ramadan days and the
  made-up count are kept as counters in the `cycle` namespace (setting or clearing bit 3
  changes the count), so that the balance outlives the one-year log; never below 0.
  - The part shows today's state ("Hayız: day 3" / "Istihaze" / "Purity: day 12"), prayer and
    fasting, the limits, the make-up fasts left, and "in doubt, ask a scholar or a doctor".

### PIN lock

- When on, opening the page (any part, from the launcher, the corner item or a face tap)
  shows the keypad (`main/Keypad.*`) for 4 digits; the page appears after the right PIN and
  stays open until active mode ends.
- 3 wrong PINs: 30 s wait before the next try (the keypad shows the seconds).
- First opening: a part asks to set a PIN (twice) or "continue without PIN"; later the
  Settings part switches it on or off and changes it (the old PIN first).
- Forgotten PIN: "delete cycle data" (portal) or the factory reset clears the PIN with the data.

### The legend

The "Meanings" panel of `pages.png` is an overlay, not a part: a tap on the spiral's middle
(the number) shows it over the part until the next tap (Claude's call); the help page shows
the same symbols.

### The Settings part

Rows: PIN (on/off, change), adolescent mode (off/on/auto), the first period's date (set with
the calendar overlay; shown only for auto), the earlier make-up fast balance (keypad; shown
while the Hayız part is shown), "delete cycle data" (two taps within 3 s, as the factory
reset; clears the namespace).

### Portal

No cycle data in the portal (not shown, not sent). The portal's page list shows the page and
its parts (show, hide, order) like the others, and the update tab gets a "delete cycle data"
button with the browser's confirmation (POST, as `/api/reset`). The help pages (help.html,
tools.html or a new section) explain the page, its symbols and its limits.

## Non-goals

- Mood or symptom tracking, or any link drawn between mood and the cycle (the owner decided
  against it).
- Contraception or conception guidance beyond the estimate and its stated failure rates;
  temperature, mucus or other fertility signs.
- Pain logging, pad counts, the PBAC chart.
- Notifications, alarms or always-on hints for the cycle.
- Protection beyond the page's PIN (no flash or NVS encryption; the threat is someone holding
  the watch).
- Cycle data in the portal.

## Anti-goals

- Calling any day "safe".
- Implying that the Moon sets or predicts the cycle.
- Showing intercourse, flow, warnings or hints outside the locked page.
- Meaning carried by colour alone (it must survive the red night mode).

## Constraints

- CLAUDE.md: C++17 on ESP-IDF, the code style, `esp_err_t` for hardware, no aborts; every text
  in `main/Language.*` in English and Turkish, short enough for the rows; LVGL's printf
  without floats.
- Fonts: JetBrains Mono Regular at 14, 16 and 18 px are added (the owner's choice: the small
  notes, rulers, legends and warnings of the mock-ups), with the 26 and 48 px fonts' range,
  through `tools/make_fonts.sh` and `main/Fonts.h` (the application slot holds 4 MB, the image
  about 1.8 MB). The large cycle day uses the 48 px font. CLAUDE.md's font rule is updated in
  the same change.
- Memory: the page is built only while shown (`Display::keepPages`, `ensureApp`); drawn parts
  are one object each (as the deep-sky list and the astrolabe), so that the internal heap stays
  free for the portal's Wi-Fi.
- Rules in `main/Cycle.*`, host-tested in `tests/` (cycle detection with gaps, predictions,
  Ogino windows and fallbacks, regularity, every warning, adolescent mode, hayız cases from
  Diyanet's fetvas, the log ring's day changes); the Moon's phase from `main/Astronomy.*`.
- Always-on: the icon's bitmap and the gauge from the same rules; the minute wake reads the
  log only at a new local day (as `FaceTable::follow`), never at every wake.
- Battery: no periodic wakes or background work for the page.

## Approaches

- **A. A daily log, one page module, a host-tested rules module** – picked. One byte a day
  keeps every edit a one-byte change; all derived values come from one place that the host
  tests cover; the UI follows the tools and observing pages' structure.
- **B. Events only** (starts and bleeding days) – smaller, but correcting a day means
  rebuilding events; no need on this flash.
- **C. In two stages** (the spiral, the calendar and the entry first) – the owner chose to
  build everything designed at once.
- The log keeps one year (the owner's choice): enough for 6 cycles of Ogino, 7 history rows and
  8 Moon months.

## Decided with the owner

The parts and their contents; the spiral (a turn per Moon month, past inside, future outside,
fading ends, the arrow from the number to today's box, today's date as day and month, the
range with a drop at the bottom, the warning above it); boxes for days (filled passed,
outlined to come), drops for bleeding sized by flow (estimated dim, half filled at night),
rings for fertile days (estimated dim, ovulation larger), the intercourse mark as described;
the night mode by shape; the calendar, history rows and Moon rows as drawn; the pregnancy
part with the warning and the failure rates; the hayız part; the corner gauge and icon with
today's mark and the neutral locked look; flow entered per day, no separate start entry; the
PIN for this page only, against someone holding the watch; no cycle data in the portal; the
adolescent features (mode with 21–45 days and wider predictions, no fertile window or
intercourse, warning notes with "a trusted adult", the carry-a-pad hint, "what is normal"),
shown only on the page; a one-year log, a byte per day with bit masks; make-up fasts marked
per day with a balance on the Hayız part; small fonts of 14, 16 and 18 px; everything in one
build.

## Left to the builder

The exact pixel geometry and spacing (the mock-ups give the layout); the drawing method for
drops, rings and the spiral (custom draw callbacks, a cached arm geometry per day); the
module split of the UI (`CyclePages.*` and, if large, a second file as the tools page did);
the dev commands for screenshots and tests ("B …" presses, a fixed scene with a sample log
that never shows the owner's data); the Language texts' exact wording within the meanings
above; the hash of the PIN; the help page's text.

## Sources

- Apple Cycle Tracking: https://support.apple.com/en-us/120356 ;
  Clue symbols: https://support.helloclue.com/hc/en-us/articles/39305317739037 ;
  Flo: https://flo.health/newsroom/10-tips-for-best-flo-experience
- Failure rates (Trussell, Contraceptive Technology 2018):
  https://www.datocms-assets.com/21281/1662539230-contraceptive-technologies_failure-rates-table-2018.pdf ;
  calendar methods: https://fphandbook.org/explaining-how-use-calendar-based-methods
- Cycle variation (Bull et al. 2019): https://pmc.ncbi.nlm.nih.gov/articles/PMC6710244/ ;
  FIGO 2018 normal limits: https://obgyn.onlinelibrary.wiley.com/doi/10.1002/ijgo.12666
- Fertile window timing (Wilcox et al., BMJ 2000):
  https://www.aafp.org/afp/2001/0501/p1829a
- The Moon: Clue's analysis https://helloclue.com/articles/cycle-a-z/myth-moon-phases-menstruation ;
  Helfrich-Förster et al. 2021 https://pmc.ncbi.nlm.nih.gov/articles/PMC7840133/
- Adolescents (ACOG CO 651):
  https://acog.org/clinical/clinical-guidance/committee-opinion/articles/2015/12/menstruation-in-girls-and-adolescents-using-the-menstrual-cycle-as-a-vital-sign ;
  NHS: https://www.nhs.uk/conditions/periods/starting-periods/
- Diyanet: https://kurul.diyanet.gov.tr/Cevap-Ara/112/adet-kanamasi-on-gunden-fazla-suren-bir-kadin-ibadetlerini-yerine-getirmede-nasil-hareket-etmelidir ;
  https://kurul.diyanet.gov.tr/tr/fetva/adeti-bittikten-sonra-on-bes-gun-gecmeden-yeniden-kanamasi/0193c42d-ad31-747e-15de-67f04a8fe914 ;
  https://fetva.diyanet.gov.tr/tr/fetva/kadinlarin-adet-veya-lohusalik-hallerinde-yapamayacaklari-seyler-nelerdir/0193c42d-4b21-7774-1a09-738831304296
