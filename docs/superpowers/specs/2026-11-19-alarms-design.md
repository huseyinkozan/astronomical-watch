# Alarms and the countdown's signal

## Intent

The board has no vibration motor (docs/hardware.md, section 1), but its speaker is loud: the
owner wants the watch to ring for

1. the countdown of the clock page when it reaches zero,
2. an alarm at a sky event with an offset ("10 min before sunset"),
3. an alarm at a clock time on chosen weekdays ("07:00 Mon–Fri").

Alarms are defined in the setup portal; on the watch each one is switched on or off. The
measurement of the battery currents with the meter is postponed; the sound is chosen from the
loudness measurements (docs/hardware.md, "Speaker and alarm sound").

## Goals

- **Sound** (`main/Sound.*`, measured): beeps alternating 1000 Hz and 1500 Hz (the speaker's
  two loudest regions, about 12 dB above 2.7 kHz for the same signal), 100 ms tone and 150 ms
  silence (four beeps a second), 5 ms ramps. The level starts at −18 dB and rises 3 dB every
  2 s to −6 dB (full scale gives only +3 dB for four times the power, and rang in the owner's
  ears). The codec's ADC side stays off while ringing (0x0E `0x6A`, ADC bias and reference off
  in 0x0D); the DAC's own low-power bits (0x0F) stay off until the meter shows that they save
  more than their 2.9 dB costs. The amplifier is switched on once per ringing (120 ms start-up)
  and silence is written between beeps.
- **Ringing:** the watch shows the alarm's name and time over the current page (or wakes into
  active mode for it) and rings until a tap anywhere on the screen or a PWR press, or by
  itself after 60 s. Then active mode goes on as after a PWR wake (display timeout, always-on
  or sleep). Two alarms due in the same minute ring once, the first one's name shown.
- **Alarms:** at most 4, kept in NVS as definitions, never as computed times:
  - kind: clock time (hour, minute) or event (one of the list below) with an offset of
    −120 to +120 minutes;
  - weekdays: a bit per day (Monday first); no day set means once: the alarm switches itself
    off after it has rung;
  - on or off (switched on the watch or in the portal);
  - its time is the local time of the watch's own zone (`Settings::UtcOffset` and the
    zone's DST changes that the watch already follows, `main/Zones.*`).
- **Events** (all already computed by `main/Astronomy.*` and `main/Sky.*`): sunrise, sunset;
  civil, nautical and astronomical dawn and dusk; solar noon; moonrise, moonset; the prayer
  times imsak, güneş, öğle, ikindi, akşam, yatsı (Diyanet's method, `Sky::prayerTimes`, the
  watch's isha angle). An event that does not occur on a day (no moonrise, no astronomical
  dusk at high latitude) moves to the next day it occurs (CLAUDE.md); with none within 400
  days the alarm shows "--:--" and never rings.
- **Next occurrence:** computed from the definition, the location, the UTC offset and the
  zone, in a module without ESP-IDF or LVGL (`main/Alarms.*`), host-tested like the astronomy
  code (`tests/AlarmsTest.cpp`): weekday rules, the offset across midnight, an event missing on
  a day, polar day and night, the "once" alarm, DST changes. Computed again after each ringing,
  after any change of the definitions, the location, the offset, the zone or the clock, and
  kept between wakes in RTC memory with the inputs it was computed from (a minute wake must not
  pay 0.1–0.5 s of astronomy).
- **Late due times** (*builder's call*): a due time found in the past at a wake or a start
  (a reset or crash in the alarm minute, a portal save that moved the clock, the watch powered
  on after the 2 % limit) rings when it is at most 10 minutes late, and is skipped (logged,
  not rung) when it is later: a sunrise alarm ringing at noon after a charge would be the loud
  surprise the anti-goals name. A skipped "once" alarm switches itself off like a rung one; the
  countdown that ended while the watch was off shows 00:00 and does not ring later than that.
- **Weekdays and offsets** (*builder's call*): the weekday boxes select the day on which the
  alarm **rings** (local date of the due time), whatever the event's own date: "Mon–Fri,
  moonrise −60" rings Monday 23:30 for Tuesday's 00:30 moonrise, never Sunday night. An event
  that does not occur, or whose due time falls on an unselected day, moves to the next
  occurrence that rings on a selected day.
- **Countdown:** its end (`Timers::Countdown::endMs`) is a due time like an alarm's, named
  "Countdown"; it rings once and the countdown shows 00:00 as today.
- **Alarm pending** (CLAUDE.md, power rules): while any alarm is on or the countdown runs, the
  watch never powers off. Every place that powers off today (display timeout with always-on
  disabled, no motion, always-on failed, the battery limit) deep-sleeps instead, the panel in
  its sleep state as in `sleepNow()`, with a timer wake for the next due time. In always-on,
  the minute wake also checks the due times, and its sleep ends at the next due time when that
  comes before the next minute. The ESP32's RC timer drifts (seconds per day): the watch wakes a
  little early, reads the PCF85063 and sleeps or waits the rest, so that it rings within a
  second of the due minute.
- **Battery limit** (the owner's decision of 2026-11-14): with an alarm pending, the watch goes
  on below the 10 % limit, without the always-on face, down to about 2 %, then powers off as
  today, so that the RTC keeps running.
  - *Builder's call:* while an alarm is pending and the always-on face is off, nothing would
    read the battery until the next due time, which may be days away (a weekly alarm, a
    99-minute countdown on a day of 1 mA sleep). So that sleep wakes **once an hour** too: it
    reads the battery and sleeps again (about 0.3 s, a few µAh; a necessary function, so an
    allowed periodic wake). The 2 % limit pairs the gauge with the voltage as the 10 % one does
    (`batteryEmpty`: percent and VBAT, the gauge reads low after a power cycle): power off at
    2 % or less and VBAT 3.50 V or less, or at VBAT 3.40 V or less whatever the gauge says.
- **On the watch:** a new part "Alarms" on the clock page (after the countdown), with a row per
  defined alarm: its short name ("07:00 Pzt–Cum", "Gün batımı −10", "Yatsı") and on/off; a tap
  switches it and saves. No alarm defined: a line saying that alarms are set in the portal. The
  part takes part in the clock page's order and hiding in the portal like the others. Below the
  rows a note (the owner's request): an alarm that is on makes the battery last shorter.
- **Three new corner items** for the face pages' inner corners (the owner's request, 2026-11-19),
  chosen per page in the portal like the weekday ring, each with its "always-on" box:
  - **Alarm days:** a bell in the middle of a ring of seven day notches laid out as the
    weekday ring's (`WeekdayIcon`); the notch of each day on which the alarm rings is lit, the
    others dim; the whole icon dim while the alarm is off. *Builder's call:* the slot's
    parameter chooses the alarm (1–4), as the day counter's does; a "once" alarm lights the day
    it will ring.
  - **Alarm on:** a bell, lit while at least one alarm is on, dim otherwise (the owner
    preferred a bell to an hourglass for alarms).
  - **Timer running:** an hourglass, lit while the stopwatch or the countdown runs, dim
    otherwise (the owner's choice).
  - Both drawn on the active face and the always-on face (idle-mode colours, the always-on
    bitmaps or shapes); the default pages do not use them.
- **In the portal:** a box "Alarms" in the Time tab, after the day counters: four entries, each
  with on/off, kind (time or event), the time or the event and the offset, and seven weekday
  boxes; saved with the page's main Save. The box says, as the watch does, that an alarm that is
  on makes the battery last shorter; the help page explains why (the alarm pending sleep).
- **Event log:** a ringing is logged (which alarm, how it ended: tap, PWR, after 60 s).

## Non-goals

- Snooze, custom sounds or melodies, volume setting, an hourly chime.
- Defining alarms on the watch (only on/off there).
- Ringing while the watch is powered off: nothing on the board can power the AXP2101 on at a
  time (docs/hardware.md); hence the alarm pending mode.
- The meter measurement (postponed); the sound's final level and the DAC's low-power bits may
  change after it.

## Anti-goals

- An alarm that is on and does not ring (the worst failure): a reset, a portal save or a new day
  must not lose or skip it; a wake early by the RC drift must not ring early or skip.
- A loud surprise: no full-scale sound, never louder than −6 dB.
- Cost while no alarm is on and the countdown is stopped: no extra wakes, no change to the
  minute wake beyond reading a flag, power-off as today.
- A sleep current raised after ringing (docs/hardware.md, section 4: ES8311 0x0D PDN_VREF;
  runs C and L found the drain about tenfold higher after that write, and the raised state
  outlasted the register value). An alarm pending sleep that drains the cell in hours would
  make the next alarm fail.
  - *Builder's call, a gate before the branch is merged:* the sleep current after a ringing is
    measured, with the owner's meter (always-on and panel-off sleep, against 3.76 mA and the
    floor) or, without the meter, a battery-log run after a ringing against one without.
  - *Plan B if it is raised:* after each ringing, ALDO1 (ES8311 AVDD and the ES7210) is
    switched off for about 100 ms and on again, which resets the codec's analog side; the
    I2C bus must answer afterwards (an unpowered ES7210 holds the bus only while it is off),
    and the sleep preparation's ES7210 state is restored. Measured the same way.

## Constraints

- CLAUDE.md style and rules; every text in `main/Language.*` in English and Turkish, short
  enough for the rows; the portal's Turkish in `data-tr`.
- `main/Alarms.*` plain C++ and host-tested; `tests/run_host_tests.sh` stays green.
- Heap: I2S, the codec driver and a sound task exist only while ringing; the stacks stay inside
  their margins (event log).
- The PWR key keeps its meaning outside ringing; during ringing it only stops the sound.

## Approaches

- **Chosen: the watch stays asleep, never off, while an alarm is pending, and wakes by its
  timer** (CLAUDE.md's design, the owner agreed on 2026-11-19). The next due time is computed at
  sleep entry from the cached occurrences; the wake comes a little early and the PCF85063 gives
  the exact time.
- Rejected: powering the watch on with the PCF85063's alarm — its INT reaches only GPIO39,
  which can neither wake the ESP32 from deep sleep nor power on the AXP2101.
- Rejected: alarms only while the watch is on — the watch powers itself off after 30 min
  without motion, so a night alarm would not ring.
- Cost told to the owner: with an alarm on the watch sleeps at about 1 mA (estimate, the
  board's floor with the panel off, not yet measured) instead of 0.05 mA off; a daily alarm left
  on means days, not weeks, on a charge unless worn and charged.

## Left to the builder

- The wake margin against the RC drift (it must keep "within a second" with the measured drift
  in mind), the RTC memory layout of the cache, the sound task's priority and stack, the exact
  short names, the portal layout inside its box, the event log entry's fields.
- Whether an alarm wake starts the full active mode (display, pages) or a lighter ringing
  screen first, as long as the name shows within about a second and the face follows after.

## Result (2026-11-19)

Built on branch `alarm-sound` (plan `docs/superpowers/plans/2026-11-19-alarms.md`).
- `main/Alarms.*` (45 host checks: weekdays, once, the events against the astronomy and prayer
  functions, the ringing-day rule across midnight, a day without a moonrise, Tromsø's polar
  night and white nights, a DST change, the late rule, the cache key, the portal text, the icons'
  symmetry). Events are searched 60 days ahead (not 400: up to about 40 s of astronomy at polar
  latitudes on the watch); none within them: "--:--", searched again the next day.
- On the board (USB, development build): an alarm from always-on rang at its second (0 s late),
  one from the sleep without the face at its second with the panel starting again, the countdown
  1 s after its end; a daily alarm reset while ringing did not ring again. A tap and PWR stop the
  ringing (guided test `alarm`, by the owner), active mode going on after PWR. A ringing with the
  setup portal on left 75 KB of internal heap at its lowest; main stack margin about 3.9 KB.
  Planning the four sample alarms took 0.33 s once (sky events), 0 ms while nothing changed.
- The Alarms part is the clock page's last part (inserting it would renumber the parts in the
  saved orders); `PartOrder` keeps an order saved before a part was added.
- The corner icons follow the owner's preview (the inner bell enlarged at the owner's request).
- Not yet measured: the battery current while ringing, the sleep current after a ringing (the
  gate before merging), the hourly battery wake and the 2 % floor on a long run.
- After the meter (2026-11-19): -6 dB drew about 0.17 A and full scale browned the board out
  (docs/hardware.md); the owner chose -12 dB as the ringing's top (about 0.08 A at 2700 Hz; the
  1000/1500 Hz tones sound louder than 2700 Hz at -6 dB). The sleep current after a ringing was
  not measured (the owner's clip leads were unreliable, and hours of beeping for a battery-log run
  are a nuisance): the owner chose to merge without it; it stays an open risk to watch in the
  event log's battery readings.
