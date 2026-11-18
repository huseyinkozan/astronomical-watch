# Memory: table template, shared styles, freeing far pages

## Intent

v0.0.3 crashed while the face pages were swiped (the LVGL task's stack overflowed; fixed in
2727935 and 5b9f500 with larger stacks and a stack-end watchpoint). The owner wants the internal
heap kept free as far as possible, so that the watch has room for what comes next (a planet page,
alarms) and the setup portal never runs short again (once it had 6.9 KB left and crashed).

Measured with the development build (2026-11-18, internal heap):

| State | Free | Largest block |
|---|---|---|
| Face and clock pages built (active mode start) | 163 KB | 116 KB |
| + information page | 121 KB (−43 KB) | 76 KB |
| + settings page | 106 KB (−15 KB) | 64 KB |
| + setup portal on | 49 KB (−57 KB) | 31 KB |

The information page's 43 KB are about 150 LVGL labels and row containers (two or three labels a
row, each with its own local styles). It is built when its neighbour is shown and never freed.

## Goals

- The information page's tables cost a few KB instead of 43 KB.
- The objects that stay share their styles instead of each keeping local copies.
- Pages that are not the shown page or its neighbour do not hold heap; with the portal on, the
  internal heap keeps clearly more than today's 49 KB free.
- Nothing changes on the screen: the same texts, colours, places and animations (screenshots of
  every page compared before and after each step).

## Non-goals and anti-goals

- No PSRAM now (the owner agreed to postpone it: its wake cost and the Wi-Fi buffers in PSRAM need
  a validator; revisit if the heap gets tight again).
- No new rule for the portal (it still runs on battery or USB).
- No slower page swipes: a page or part must still slide in without a visible pause beyond
  today's on-demand build (about 0.3 s for a page built the first time).
- No longer minute wakes (always-on does not use these pages).

## Approaches (decided with the owner, 2026-11-19)

- **Table template per part (chosen):** a table part is one LVGL object that draws its title and
  rows itself (as the calendars already do), from a small text array of our own. About 43 KB →
  4–5 KB. The slide animation stays as it is.
- One shared template for every table part, refilled when the part changes (rejected): a few KB
  less, but two copies would have to alternate during the slide; clearly more complex.
- Building the information page's parts one by one on demand (the owner's item 2; dropped):
  with one object per part, building all of them costs little; not worth the extra logic.

## Design

Three steps, in this order, each measured (heap free and largest block at each state of the
table above, the LVGL and main stack margins from the event log) and compared with screenshots.

### Step 1: table template

A small module `main/TablePart.*`: one LVGL object per part, full size of the part, drawing on
`LV_EVENT_DRAW_MAIN`:

- a title line (grey, as today's heading), then rows; a row is a name (left, grey) and one or two
  values (right aligned, white), or one full-width centred line; per row a colour override (the
  next prayer in amber: name and value) and a "grey value" flag (the twilight header, the place
  lines);
- the same columns and font as today (name 160 px, values 105 px each, JetBrains Mono 26 px, row
  pitch as today's flex layout gives: measure it on today's screenshots);
- the texts live in a fixed array inside the object's C++ owner (rows × cells × a short string),
  not in LVGL; setting a text that did not change does not invalidate.

Converted parts: Sun, Twilight, Prayer times, Moon, Time (`SkyPages`) and the settings page's
"Setup portal set" part (`Display`). The compass, battery and about parts and the clock page stay
as they are (few objects, or drawn already).

### Step 2: shared styles

The objects that remain (settings rows and buttons, clock page buttons and labels, the stacked
pages' headers, the face page's labels, the brightness control) take their font, colours, text
alignment and paddings from a few static `lv_style_t` shared by all of them, instead of
`lv_obj_set_style_*` calls that give each object its own copy. Styles that change at run time on
one object (a colour that follows a state) may stay local.

### Step 3: freeing far pages

- The information and settings pages are freed when neither they nor a neighbour is shown, and
  built again when a neighbour is shown (today's `Display::ensureBuilt`), so their state must be
  rebuildable from what `Display` keeps (texts, settings, the shown part).
- While the setup portal runs (started on the settings page), the information page is freed
  too, although it is the settings page's neighbour; a swipe towards it builds it again (a short
  pause on that swipe is accepted while the portal runs).
- The face and clock pages stay built (shown at every wake).

## Constraints

- CLAUDE.md style; product code must not abort on an allocation failure (LVGL object creation can
  fail: no crash, the page stays empty).
- LVGL is used only in its own task, with the display lock (as today).
- Screenshots (`tools/screenshot.py`, fixed scene) of every page before step 1 are the reference.

## Decided by the builder (2026-11-19, to be checked by the owner)

- **The heap goal's measure:** with the portal on and idle (right after it starts), at least about
  90 KB of internal heap free (steps 1 and 3 give back about 40 KB); reported as free, largest
  block and the minimum. The portal under load (a phone loading the page, a save, an update) is
  checked once by the owner at the end, judged by the event log's heap minimum; freeing and
  rebuilding pages must not leave the largest block clearly smaller than before.
- **Pages while the portal runs:** the information page is held only while it is itself the
  shown page; the settings page follows the general rule (freed when neither it nor a neighbour is
  shown; its portal state is rebuilt from what `Display` keeps).
- The setup-info part keeps its own geometry (names at x 30, values in 220 px with dots, 36 px
  pitch), since nothing may change on the screen; a freed page comes back on the part it showed;
  rebuilding runs in the LVGL task; screenshots are compared pixel by pixel.

## Left to the builder

The exact text buffer sizes, how the row pitch is matched to today's layout, which run-time
colour changes stay local styles, the order of freeing and rebuilding within the LVGL task.
