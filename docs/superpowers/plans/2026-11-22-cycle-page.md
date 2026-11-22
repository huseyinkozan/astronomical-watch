# Cycle Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The locked Cycle page (spiral, calendar, history, Moon rows, pregnancy, hayız,
settings), its day entry, the corner gauge and icon, and the portal's delete button, as the
spec describes.

**Architecture:** A one-year log of a byte per day (`CycleLog`, host-tested, stored by
`CycleStore` in NVS namespace `cycle`) is the only data; host-tested rules (`Cycle`, `Hayiz`)
derive everything shown. `CyclePages` is a stacked page like `ToolPages`, drawing its parts
with custom draw callbacks (`CycleDraw` holds the shared symbols). The corner gauge and icon
take a small summary through `Corners::Context` and `CornerView::setCycle`.

**Tech Stack:** ESP-IDF 6.1, C++17, LVGL 9; host tests with g++ (`sh tests/run_host_tests.sh`).

**Spec:** `docs/superpowers/specs/2026-11-22-cycle-page-design.md` (mock-ups beside it).

## Global Constraints

- CLAUDE.md code style: PascalCase classes and files, camelCase, `m_` members, `s_` file
  statics, `{` on a new line for functions, spaces inside parentheses, index `for` loops, no
  exceptions/RTTI, no heap after start-up unless needed, `esp_err_t` for hardware, no aborts.
- Every visible text in `main/Language.*`, English and Turkish, appended before `TextCount`.
- New `Page`, `Part`, `GaugeItem`, `CornerItem`, `FaceTap::Target`, `Settings::Item` values are
  appended (saved numbers stay).
- Meaning never by colour alone (red night mode): shapes and fills carry it.
- Never call a day "safe"; never imply the Moon sets the cycle; nothing but the cycle day and
  phase colours outside the locked page.
- No cycle data in the portal; the scene for screenshots uses a sample log, never the owner's.
- The page is built only while shown (`Display::ensureApp`/`freeApp`); drawn parts are single
  objects.

## Review Focus

1. A log spanning the ring's wrap (day number mod 366) and days without entries after a long
   gap (watch off for weeks): old bytes must not reappear as new days. (Task 1 tests.)
2. A period split by two or more clean days, spotting 3–9 days after a period, and a first
   cycle in the log that started before the log: no false cycle starts, no negative lengths.
   (Task 2 tests.)
3. The time not set (`timeValid` false) or before the newest entry (clock moved back): the page
   shows a message and writes nothing. (Task 5 step.)
4. The adolescent mode in auto without a date, and switching modes with an open part: no
   fertile window or intercourse key anywhere. (Task 2 tests, Task 7 step.)
5. PIN wrong three times, then active mode ends during the 30 s wait: the wait survives only
   while active (RAM), the page stays locked. (Task 5 step.)

---

### Task 1: Small fonts and the log

**Files:**
- Modify: `tools/make_fonts.sh` (sizes 14 16 18 beside 26 48), `main/Fonts.h`, `main/CMakeLists.txt`
  (the new font files), CLAUDE.md (the font rule: 14, 16, 18, 26, 48 px)
- Create: `main/fonts/jetbrains_mono_{14,16,18}.c`, `main/CycleLog.h/.cpp`, `main/CycleStore.h/.cpp`,
  `tests/CycleLogTest.cpp`; Modify: `tests/run_host_tests.sh`

**Interfaces – Produces:**
```cpp
struct CycleLog
{
    static const int Days = 366;
    enum Bits : uint8_t { FlowMask = 0x03, Intercourse = 0x04, MadeUpFast = 0x08 };
    uint8_t days[Days];
    int32_t newestDay;        // local day number of the newest day the ring covers, 0: empty
    int32_t ramadanAdded;     // hayız days of ended Ramadans added to the make-up balance
    int32_t lastRamadanYear;  // the Hijri year last added
    int32_t madeUp;           // make-up fasts kept (bit 3 set/cleared changes it)
    int32_t earlierBalance;   // entered on the Settings part
    uint32_t pinHash;         // 0: no PIN
    uint32_t pinSalt;
    uint8_t teenMode;         // 0 off, 1 on, 2 auto
    int32_t firstPeriodDay;   // 0: unknown
    void clear();
    void advance( int today );            // clears the days between newestDay and today
    uint8_t get( int day ) const;          // 0 outside [today - 365, newestDay]
    void set( int day, uint8_t value );    // ignores days outside the ring; keeps madeUp in step
    int flow( int day ) const;             // 0..3
};
class CycleStore { public: static void load( CycleLog &log ); static esp_err_t save( const CycleLog &log ); static esp_err_t erase(); };
```

- [ ] Add 14, 16, 18 to `make_fonts.sh`, run it (`sh tools/make_fonts.sh`, needs Node), declare
  `jetbrains_mono_14/16/18` in `Fonts.h`, add to CMake; `idf.py build` succeeds.
- [ ] Write `tests/CycleLogTest.cpp`: `advance` across the mod-366 wrap clears skipped days;
  a 400-day gap clears everything; `get` of a day older than 365 days is 0; `set` of bit 3
  increments `madeUp`, clearing it decrements, setting it twice counts once; `set` in the
  future (day > newestDay) is ignored. Run: `sh tests/run_host_tests.sh` – FAIL (no CycleLog).
- [ ] Implement `CycleLog` (index `day % Days`, positive), `CycleStore` (one blob `log` in
  namespace `cycle`, size checked on load: a different size clears). Tests PASS.
- [ ] Commit "Add the small fonts and the cycle log".

### Task 2: Cycle rules

**Files:** Create `main/Cycle.h/.cpp`, `tests/CycleTest.cpp`; Modify `tests/run_host_tests.sh`.

**Interfaces – Consumes:** `CycleLog`. **Produces:**
```cpp
class Cycle
{
public:
    static const int MaxCycles = 14;
    enum Warning : uint16_t { Short = 1, Long = 2, LongBleeding = 4, NoPeriod90 = 8,
                              ManyHeavy = 16, BetweenPeriods = 32 };
    struct Period { int start; int length; int bleedingDays; int heavyDays; uint16_t warnings; int cycleLength; }; // cycleLength 0: current
    struct State
    {
        Period periods[MaxCycles]; int count;       // oldest first; the last is the current
        int today; int cycleDay;                    // 0: no period logged
        bool teen; bool irregular;
        int complete;                               // complete cycles used (<= 6)
        int meanLength; double spread;              // of the last <= 6
        int nextEarly, nextLate, nextMean;          // days after the current start; 0: none
        int fertileFirst, fertileLast;              // cycle days; 0: none
        int ovulationDay;                           // cycle day; 0: none
        bool carryPad; uint16_t warnings;           // the current period/cycle's warnings
    };
    static State analyse( const CycleLog &log, int today );
    static bool teenActive( const CycleLog &log, int today );   // auto: 3 years from firstPeriodDay, on without it
};
```

- [ ] Write tests (the spec's rules, exact values): start after ≥10 clean days, one clean day
  inside a period keeps it, two clean days end it; bleeding 3 days after a period →
  `BetweenPeriods`, no new cycle; 6 cycles 26,29,27,28,31,26 → mean 28, spread ≈1.94,
  next 24..32 (±2 SD, min ±2), Ogino 8..20, ovulation 15; 4 cycles 28 each → fertile 8..19;
  spread 12 days → irregular, fertile = union(Ogino, 6..21); <3 cycles → 6..21 and no
  ovulation; 1 cycle of 30 → next 26..34; teen on → no fertile window, prediction 21..45 with
  <3 cycles, shortest..longest ±2 with ≥3; auto teen without date → teen; adult cycle 23 →
  `Short`, 40 → `Long`, 9 bleeding days → `LongBleeding`, teen 22 days → no warning, teen 46 →
  `Long`; 90 days since start → `NoPeriod90`; 3 heavy days → `ManyHeavy`; carryPad from
  nextEarly until a bleeding day; empty log → cycleDay 0, no prediction, fertile 6..21 hidden?
  (no: with no period logged `fertileFirst` = 0). Run – FAIL.
- [ ] Implement `Cycle::analyse` (walk the ring from today − 365, collect periods, then the
  rules in the spec's order). Tests PASS.
- [ ] Commit "Add the cycle rules".

### Task 3: Hayız rules and make-up fasts

**Files:** Create `main/Hayiz.h/.cpp`, `tests/HayizTest.cpp`.

**Interfaces – Consumes:** `CycleLog`. **Produces:**
```cpp
class Hayiz
{
public:
    enum Kind { Purity, Hayz, Istihaze };
    struct Today { Kind kind; int day; int habit; };       // day: of the hayız, istihaze or purity
    static Kind kindOn( const CycleLog &log, int day );     // the ruling of one day
    static Today today( const CycleLog &log, int day );
    // Hayız days among [first, first + length) (a Ramadan), for the balance.
    static int hayzDays( const CycleLog &log, int first, int length );
    static int balance( const CycleLog &log );              // earlier + ramadanAdded − madeUp, >= 0
};
```

- [ ] Tests (Diyanet, Hanafi): bleeding 2 days → istihaze; 5 days → hayız 1..5, then purity;
  bleeding 1–3, clean 4–5, bleeding 6–7 → hayız 1–7; 12 days with a previous 6-day hayız →
  hayız 1–6, istihaze 7–12; 12 days without a habit → hayız 1–10; bleeding 10 days after a
  hayız ended → istihaze; Ramadan of 30 days with hayız on 5 of them → `hayzDays` 5;
  `balance` with earlier 3, added 5, madeUp 2 → 6; never below 0. Run – FAIL.
- [ ] Implement. Tests PASS. Commit "Add the hayız rules".

### Task 4: Page skeleton, settings, texts, lock

**Files:** Create `main/CyclePages.h/.cpp`; Modify `main/Display.h/.cpp` (Page `CyclePage`
appended, `s_cycleParts`, `s_pageEntries`, `stack()`, `ensureApp`, `freeApp`, `press`, the shot
table entries `cycle`, `cycle-calendar`, `cycle-history`, `cycle-moon`, `cycle-pregnancy`,
`cycle-hayiz`, `cycle-settings` appended last), `main/Settings.h/.cpp` (`CycleOrder`,
`CycleHidden` appended; `CycleHidden` default 0x7F: the page out of the launcher), `main/Portal.cpp`
and `main/portal/index.html` (the page in the part lists), `main/Language.h/.cpp`.

**Interfaces – Produces:**
```cpp
class CyclePages
{
public:
    enum Part { CyclePart, CalendarPart, HistoryPart, MoonPart, PregnancyPart, HayizPart, SettingsPart, PartCount };
    void build( lv_obj_t *tile, CycleLog &log, Settings &settings );
    void forget();
    StackPage &stack();
    void update( int today, bool timeValid, int64_t utcNow );   // each second; recomputes at a new day or after an edit
    void lock();                                  // active mode ended
    bool unlocked() const;                        // also true without a PIN
    bool press( const char *name );               // dev: "entry <flow> <intercourse>", "pin <digits>", "mode", "flip"
};
```

- [ ] Parts as empty `StackPage` parts with their titles; the lock: while `pinHash != 0` and
  locked, a full-part overlay with `Keypad` (Calculator digits; C clears, = checks), "PIN"
  title and four dots; right PIN → unlock until `lock()`; three wrong → 30 s wait shown as
  seconds; first opening with `pinHash == 0` and a never-asked flag (bit in `teenMode`'s
  byte or a separate field) → "Set a PIN" twice or "continue without". Hash: FNV-1a 32 of
  salt bytes then the digits; salt from `esp_random()`.
- [ ] Time invalid or before `log.newestDay`: every part shows "Set the time first" and taps
  write nothing.
- [ ] Build, flash, open the page from the launcher after showing it with a dev command
  ("P cycle" or the portal); PIN set, wrong PIN wait, unlock, `lock()` at active end. Commit
  "Add the cycle page's skeleton and lock".

### Task 5: Symbols, spiral, legend, day entry

**Files:** Create `main/CycleDraw.h/.cpp`; Modify `main/CyclePages.cpp`.

**Interfaces – Produces (`CycleDraw`, LVGL draw helpers, `night` selects the red look):**
```cpp
namespace CycleDraw {
void drop( lv_layer_t *layer, int x, int y, float size, lv_color_t colour, bool half );
void egg( lv_layer_t *layer, int x, int y, bool big, lv_color_t colour );
void spermEgg( lv_layer_t *layer, int x, int y, float outAngle, lv_color_t colour );
void dayBox( lv_layer_t *layer, const lv_point_t corners[4], bool filled, lv_color_t colour );
lv_color_t bleeding( bool night ); lv_color_t fertile( bool night, bool estimated );   // and the dim tones
}
```
- [ ] Spiral part per the spec (turn = 29.530589 days, `Astronomy::moonPhase` for today's
  angle, 30 phase discs, arm −18..+24 days, fading ends, boxes/drops/rings/intercourse marks,
  arrow from the number to today's box, cycle day in `jetbrains_mono_48`, "day", "1 Ara",
  the amber warning in 14 px, the drop and range in 26 px at the bottom; carry-pad and
  no-period texts); arm geometry computed once per day into a small array.
- [ ] Taps: today's box → entry overlay; the number → legend overlay (until the next tap); the
  range line → "what is normal" for 4 s.
- [ ] Entry overlay: title (day and month, "Today" for today), four flow keys, intercourse key
  (hidden in teen mode), made-up fast key (shown while the Hayız part is not hidden), OK saves
  (`CycleLog::set`, `CycleStore::save`), swipe/BOOT/back closes. Night mode looks per spec.
- [ ] Flash; screenshots of the spiral (scene log) day and night ("I 1"); compare with
  `pages.png`. Commit "Draw the cycle spiral and the day entry".

### Task 6: Calendar, history, Moon rows

**Files:** Modify `main/CyclePages.cpp` (or a second file `main/CycleParts.cpp` if `CyclePages.cpp`
passes ~1500 lines).

- [ ] Calendar: the clock page's grid geometry (7 columns 48 px, rows 56 px), today boxed, marks
  under the number (drops sized by flow, rings, sperm mark, crescent for bit 3, estimated dim),
  sideways swipe steps the month (`ObservingPages::swipesSideways`-like flag so the page swipe
  stops), "<" ">" at the bottom; a tap on a past day or today opens the entry for it.
- [ ] History: rows per the spec from `State.periods`, the current row's arrow and estimated
  cells, mean/range/regularity, warning notes in amber with "talk to a doctor" (teen: "or a
  trusted adult").
- [ ] Moon rows: 8 synodic months ending with the current one, from new moon
  (`Astronomy::moonPhase` ageDays to find each new moon), starts as red dots, phase discs above,
  the two text lines and the amber note.
- [ ] Flash, screenshots of the three; commit "Add the cycle calendar, history and Moon rows".

### Task 7: Pregnancy, hayız, settings parts

- [ ] Pregnancy: day row with symbols, three rows (fertile window dates, ovulation date, next
  period range), amber warning with "12-24 of 100"; teen mode: the one-line text instead of
  the window rows.
- [ ] Hayız: today's state, prayer/fasting, limits, make-up fasts left (`Hayiz::balance`),
  "in doubt, ask a scholar or a doctor"; at a new day after a Ramadan ended (`Hijri::month`
  for month 9 of the year), add its hayız days once (`lastRamadanYear`).
- [ ] Settings: rows PIN (on/off; change asks the old PIN), teen mode (off/on/auto), first
  period date (calendar overlay; auto only), earlier make-up balance (keypad; while Hayız
  shown), "Delete cycle data" (two taps in 3 s: `CycleStore::erase`, `log.clear()`).
- [ ] Switching teen mode while a part is open redraws every part (Review Focus 4).
- [ ] Flash, screenshots; commit "Add the pregnancy, hayız and cycle settings parts".

### Task 8: Corner gauge and icon

**Files:** Modify `main/FaceConfig.h` (`CycleGauge`, `CycleIcon` appended), `main/Corners.h/.cpp`
(`Kind` `CycleKind`; `Context` gets `int cycleDay, cycleLength; bool cycleColour`),
`main/CornerView.h/.cpp` (`static void setCycle( int length, int periodDays, int fertileFirst,
int fertileLast, bool colour )`, per-pixel phase colours for a `CycleKind` gauge, white notch
at today), `main/FacePage.cpp` (icon), `main/AlwaysOn.cpp` (gauge 2 px idle colours, neutral;
icon drawn from rings and the small digits), `main/FaceTap.cpp` (both → `CyclePart`, new
target appended), `main/portal/index.html` (gauge and corner lists), `tests/CornersTest.cpp`,
`tests/FaceTapTest.cpp`.

- [ ] Tests: `CycleGauge` with cycleDay 12, length 28 → `CycleKind` 0..428; cycleDay 0 → Empty;
  always-on → the same share (neutral look is CornerView/AlwaysOn's); FaceTap targets. FAIL,
  implement, PASS.
- [ ] CornerView/FacePage/AlwaysOn drawing per the spec (neutral grey while locked or always-on;
  colours when unlocked or no PIN). Flash, screenshots of a face with both ("C 2 <item>", "G
  0 6 <item>") and its always-on face. Commit "Add the cycle corner gauge and icon".

### Task 9: Main task, portal, docs

**Files:** Modify `main/main.cpp` (load `CycleLog` at active start, hand it to Display, the
summary for corners in `updateDisplay` and in `s_look` for always-on at a new day only,
`factoryReset` list + `cycle`, dev scene log for "D" (never saved), `Display::lockCycle()` at
active end), `main/Portal.h/.cpp` (`/api/cycle/delete` POST → `takeCycleDelete()`),
`main/portal/index.html` (button with confirm on the update tab), `main/portal/help.html` (a
Cycle section: symbols, limits, no contraception), CLAUDE.md (the page in the stacked pages
paragraph, the corner items, the dev commands), `tools/screenshot.py` doc numbers.

- [ ] Build, host tests, flash; the full screenshot set of the page in both languages
  (scene), the portal preview with the delete button; commit "Wire the cycle page to the main
  task, the portal and the docs".
- [ ] Final whole-branch review (fresh reviewer), fix Critical/Important with tests, commit.
