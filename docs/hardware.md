# Hardware Fact Base — Waveshare ESP32-S3-Touch-AMOLED-2.06

This file records what is known about the board and how it is known. Read it before hardware work;
update it when a validator or measurement changes the picture.

Source tags: **[SCH]** schematic V1.0, **[DS]** datasheet, **[REF]** reference code (BSP, Waveshare
examples, SensorLib/XPowersLib, Rust watch firmwares), **[HW]** validated on this board,
**[?]** unknown / to be tested.

## 1. Pin Map

| GPIO | Function | Notes | RTC GPIO (deep-sleep wake) |
|---|---|---|---|
| 0 | BOOT key | 10k pull-up, active LOW [SCH] | yes |
| 1 / 2 / 3 / 17 | SD MOSI(CMD) / SCK / MISO(D0) / CS | 10k pull-ups to VCC3V3 [SCH] | – |
| 4–7 | QSPI SIO0–3 (display) | [SCH][REF] | – |
| 8 | LCD_RESET | [SCH] | yes (can be held) |
| 9 | TP_RESET (touch) | [SCH] | yes (can be held) |
| 10 | PWR key state (SYS_OUT) | BSS138 inverter, 10k pull-up (R11) to VCC3V3, **HIGH while pressed**; the MOSFET conducts while the key is idle, so R11 draws ~0.33 mA all the time [SCH] | **yes** |
| 11 / 12 | QSPI SCLK / LCD_CS | [SCH] | – |
| 13 | LCD_TE | [SCH] | – |
| 14 / 15 | I2C SCL / SDA (shared) | 2.2k pull-ups (R49 / R23) to **VCC3V3**; a line held low would draw ~1.5 mA; BSP: port 1, 400 kHz [SCH][REF] | – |
| 16 / 40 / 41 / 42 / 45 | I2S MCLK / DSDIN / SCLK / ASDOUT / LRCK | [SCH] | – |
| 18 | Vibration motor | NPN low-side, motor supply = ALDO3 [SCH] | – |
| 19 / 20 | USB D− / D+ | native USB-Serial-JTAG [SCH] | – |
| **21** | **QMI8658 INT1** | push-pull when enabled [SCH][DS] | **yes** |
| 38 | Touch INT | [SCH] | **no** (light sleep only) |
| 39 | PCF85063 INT | open-drain [SCH][DS] | **no** (light sleep only) |
| 43 / 44 | UART0 TX / RX | [SCH] | – |
| 46 | Speaker amplifier enable (PA_CTRL) | [SCH] | – |

I2C devices on the shared bus [SCH][REF]: FT3168 touch `0x38`, PCF85063 `0x51`, QMI8658 `0x6B`,
AXP2101 `0x34`, ES8311 codec `0x18` (CE low through R50), ES7210 ADC `0x40` (AD0/AD1 to AGND).
Speaker amplifier NS4150B: CTRL (GPIO46) has a 10k pull-down, so it is off unless driven. The
microphones are biased from ES7210 MICBIAS12.

Not connected to the ESP32 [SCH]: QMI8658 INT2 (test point TP15 only), AXP2101 IRQ (RP5 10k pull-up
to VCC-RTC; official example uses `CONFIG_PMU_INTERRUPT_PIN=-1`), PCF85063 CLKOUT.

## 2. Parts and Power Tree

- MCU: **ESP32-S3R8** (8 MB octal PSRAM in package). No 32 kHz crystal on the ESP32 [SCH].
- Chip revision v0.2 [HW].
- Flash: **GD25Q256, 32 MB** [SCH]; detected at boot as GigaDevice, 32768 KiB, running in DIO mode
  [HW]. The project configures 32 MB (the Waveshare examples use 16 MB). ESP-IDF supports 32-bit
  addressing for the GD25Q256: the area above 16 MB can hold data; executing code from it needs the
  experimental `CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH`, so application partitions stay
  below 16 MB [ESP-IDF docs, SPI flash optional features].
- AXP2101 PMIC rails [SCH]:

| Rail | Net | Loads | Rule |
|---|---|---|---|
| DCDC1 | VCC3V3 | ESP32, flash, QMI8658, AMOLED VCI/VDDIO, touch, SD card, ES8311 digital, I2C pull-ups | always on |
| ALDO1 | A3V3 | ES8311 AVDD and **all of ES7210** (which sits on the shared I2C bus) | **keep on**: an unpowered ES7210 hangs the I2C bus |
| ALDO2 | — | pull-up of DSI_PWR_EN (AMOLED panel power enable) | **on while the display shows anything**: with ALDO2 off the image goes dark at once [HW] |
| ALDO3 | — | vibration motor | off unless haptics |
| RTCLDO | VCC-RTC | PCF85063 VDD, AXP IRQ pull-up | RTC keeps time while a battery is present, even with the system off |
| others | — | no load found | – |

- AXP PWROK → ESP32 CHIP_PU. PWR key → AXP PWRON (short/long press) and GPIO10 [SCH].
- Battery: MX1.25 connector; 10k thermistor RP2 on AXP TS [SCH].

## 3. Device Notes

### QMI8658C IMU (`0x6B`, SA0 = GND)
Two datasheets were compared: QMI8658**A** Rev A (2022) and QMI8658**C** Rev 0.6 (2021, "advance
information"); the board has the QMI8658**C**. The WoM section (C: §9, Table 33, Fig. 10) and the
CTRL2 ODR table are identical in both; codes 1001–1011 are "N/A", which explains why `CTRL2=0x0A`
failed. **They differ on the CTRL9 handshake:**

| | QMI8658A Rev A | QMI8658C Rev 0.6 |
|---|---|---|
| CmdDone bit | STATUSINT `0x2D` bit7 | STATUS1 `0x2F` bit0 |
| Acknowledge | write CTRL9 = `0x00` (CTRL_CMD_ACK) | read STATUS1 |
| INT1 during handshake | raised unless CTRL8.bit7 = 1 | always raised, pulled low by the STATUS1 read |
| CTRL8 | bit7 selects handshake signalling | "Reserved: Not Used" |

The validator saw STATUSINT.bit7 set and completed the handshake by writing CTRL9 = `0x00` [HW],
i.e. the chip behaves at least partly like the A description; SensorLib does the same [REF].
Whether CTRL8.bit7 has any effect on this chip, and whether a STATUS1 read alone also completes the
handshake, is [?]. Both datasheets agree that INT1 is driven during the handshake.

CTRL1 also differs: the C datasheet marks bits 4:1 as reserved, the A datasheet defines bit3 as
INT1_EN and bit4 as INT2_EN. INT1 worked with `CTRL1=0x48` on this board [HW]; whether bit3 is
required on the C chip is [?]. The watch no longer wakes on motion (it samples the accelerometer at
wide intervals instead), so INT1 and WoM are kept here as background only.

The register notes below follow the A datasheet unless marked.

- CTRL1 `0x02`: bit6 ADDR_AI, bit5 BE (big-endian, default 1), bit4 INT2_EN, bit3 INT1_EN,
  bit0 SensorDisable. Enabled INT pins are push-pull, disabled pins are high-Z [DS Table 22, §6].
- CTRL2 `0x03`: bits 6:4 range (000 = ±2 g), bits 3:0 ODR; low-power codes 1100 = 128 Hz,
  **1101 = 21 Hz**, 1110 = 11 Hz, 1111 = 3 Hz, gyro must be off [DS Table 22].
- CTRL8 `0x09` bit7: 0 = the CTRL9 command handshake is also signalled on **INT1**;
  1 = only on STATUSINT.bit7 [DS §5.10].
- CTRL9 `0x0A` protocol: write parameters → write command → wait STATUSINT(0x2D).bit7 CmdDone →
  write `0x00` (ACK) → CmdDone clears. WoM command = `0x08` [DS §5.10, Table 28].
- WoM [DS §12, Table 39, Fig. 25]: CAL1_L `0x0B` = threshold in mg (0 disables WoM);
  CAL1_H `0x0C` bits 7:6 = 00 INT1/initial 0, 10 INT1/initial 1, 01 INT2/initial 0,
  11 INT2/initial 1; bits 5:0 = blanking (samples ignored after enable).
  Sequence: CTRL7 = 0 → CTRL2 → CAL1_L/H → CTRL9 `0x08` handshake → CTRL7.aEN = 1; the pin must be
  enabled in CTRL1.
- **Each WoM event toggles the selected INT line. Reading STATUS1 (`0x2F`) clears bit2 (WoM) and
  returns the line to its initial level** [DS §12.4]. The line is not latched.
- Current at 1.8 V: low-power accel 21 Hz ≈ 42 µA, 11 Hz ≈ 35 µA, 3 Hz ≈ 30 µA; power-down ≈ 6 µA
  [DS Tables 15, 31].
- Reference: SensorLib `configWakeOnMotion()` encodes CAL1_H the same way; its default pin is INT2,
  which is not routed on this board [REF].

Validated on this board [HW] (validator in `archive/wom-validator/`, tag `wom-validator`):
- Communication at `0x6B` works.
- Accel-only normal output is valid at 125, 62.5 and 31.25 Hz (±2 g, gyro off). 31.25 Hz is the
  lowest normal ODR tested, not a proven minimum.
- `STATUS0.aDA` stays clear even while valid samples are available; do not gate reads on it.
- Low-power 21 Hz WoM works with `CTRL1=0x48`, `CTRL2=0x0D`, `CAL1_L=100` (mg), `CAL1_H=0x08`
  (INT1, initial LOW, blanking 8). `CTRL2=0x0A` was invalid and caused continuous false WoM events.
- Stationary: quiet. Intentional wrist motion: `STATUS1.WoM` with INT1/GPIO21 high.
- Reading STATUS1 acknowledges a genuine event and returns GPIO21 low.
- One movement can cause a finite burst of further genuine events for several seconds.
- After an acknowledged event, INT1 may reassert high while STATUS1 shows no event, then clear by
  itself (`INT1_TOGGLE_EDGE_CASE`). Do not count it as motion from the GPIO level alone.
  Possible explanations from the datasheet (not yet tested): the toggle semantics above, or the CTRL9
  handshake on INT1 while CTRL8.bit7 = 0.
- Before deep sleep, require INT1 stably low and use STATUS1.WoM to tell real events from this case.
- Deep-sleep wake via GPIO21 (EXT1) is **not validated yet**.

### PCF85063A RTC (`0x51`)
- INT is open-drain [DS Table 3]; on this board it goes to GPIO39 (no deep-sleep wake).
- Control_1 `0x00` bit0 CAP_SEL: 0 = 7 pF, 1 = 12.5 pF. The Waveshare component writes `0x01`
  (12.5 pF); the correct value for Y1 + C22/C23 is [?] → compare drift over several days (the
  watch currently runs with the reset value, 7 pF).
- Control_2 `0x01`: AIE(7) AF(6) MI(5) HMI(4) TF(3) COF[2:0]. Flags clear by writing 0.
  **CLKOUT runs at 32.768 kHz from power-on (COF = 000); disable with COF = 111** [DS §7.2.2.4].
  The Waveshare component never disables it, and `enable_alarm` rewrites the whole register [REF].
- Timer mode `0x11`: when unused, keep TCF = 1/60 Hz for lowest power [DS].
- Supply current ≈ 220 nA typical with CLKOUT off [DS Table 40].
- Time access [DS §7.4]: seconds through years must be read or written in one I2C access (the
  counters are frozen during it, for at most 1 s). Seconds bit7 is the OS flag: set while the
  oscillator is stopped or starting after power-on, cleared only by writing the seconds.
- Setting to the exact second [DS §7.2.1.2]: STOP = 1 holds the prescaler in reset; after the
  time is written and STOP is cleared, the first increment follows 0.507 8 s later.
- Accessed through a small custom class (`main/Rtc.*`): the Waveshare component writes Control_1
  (CAP_SEL 12.5 pF) and Offset on every init, masks the OS flag when reading, sets the time
  without STOP and uses a 1 s I2C timeout. The class keeps the time in UTC, 24-hour mode.

Validated on this board [HW] (guided test `rtc`, 2026-10-31):
- First read: Control_1 `0x00` (CAP_SEL 7 pF, running), Offset `0x00` (the reset values), OS = 1
  and the time 2000-01-03 20:07:21, i.e. counting since a power-on reset about 2 days 20 h earlier
  and never set.
- Set with STOP from the host clock (sent at the host's second + 0.48 s); OS then reads 0.
- After 72 s in off mode the RTC read OS = 0 and 0.10 s behind the host timestamp, which then
  still included up to 0.1 s of host read latency (since fixed in `tools/guided_test.py`). The
  PCF85063 keeps time while the AXP2101 is off, as expected from RTCLDO.
- Drift and the right CAP_SEL: repeat the guided test `clock` over hours and days; each RTC line
  in its log carries the RTC − host difference. First two readings after the reader fix, 14.5 min
  apart: −0.128 s → −0.164 s, i.e. about −40 ppm (3.5 s/day slow), on a desk at room
  temperature. Two more readings (`clock`, `display`) over 33 min in all: **−41.8 ± 0.2 ppm**
  (3.6 s/day slow), residuals under 0.5 ms. A higher load capacitance (CAP_SEL 12.5 pF) would slow
  it further, so CAP_SEL stays 7 pF and the Offset register corrects the rate: −41.8 / 4.34 ≈ −10
  in MODE 0 (`0x76`; a slow clock takes a negative value, datasheet Figure 11), leaving about
  +1.6 ppm. MODE 0 corrects once every two hours, one pulse per minute in minutes 00 to 09 of
  that hour (about 31 ms each, 0.31 s in all; datasheet Table 13), so the check needs readings
  several hours apart. Offset `0x76` was written at 18:11 UTC; one hour later the readings still
  followed −42 ppm, as expected before the first correction cycle.

### AXP2101 PMIC (`0x34`)
- **The BSP never initialises the AXP2101**; rails are at their eFuse defaults unless configured [REF].
- Rail enables: REG80 (DCDC1–4, bits 0–3), REG90 (ALDO1–4 bits 0–3, BLDO1/2 bits 4/5, CPUSLDO bit6,
  DLDO1 bit7). Defaults come from eFuse → read them back. Always read-modify-write [DS §6.13].
- PWRON timing REG27: long-press power-off default 6 s. IRQ enable 40h–42h, status 48h–4Ah
  (write 1 to clear). PWRON short press = REG49 bit3 [DS].
- Battery %: REGA4. VBAT: REG34[5:0] + REG35. ADC channel enable: REG30. Software power-off: REG10 bit0.
- The 14-bit ADC measures only VBAT, VBUS, VSYS, TS and die temperature; **there is no battery
  current measurement** [DS §6.10]. On-board power data is therefore indirect (see CLAUDE.md).
- Quiescent current ≈ 40 µA [DS §6.5.4].
- Power-on sources from the off state [DS §6.5.4.2]: PWRON key longer than ONLEVEL, and, only if
  enabled "by customization" (factory eFuse, not a register), VBUS or VBAT rising, IRQ pin low
  for more than 16 ms, or the battery charged above 3.3 V. On this board the IRQ pin has only its
  pull-up (net label EXIO5/AXP_IRQ, not routed) and the PCF85063 INT goes only to GPIO39 [SCH],
  so an RTC alarm cannot power the board on.
- Accessed through a small custom class (`main/Axp2101.*`). XPowersLib was reviewed as a reference
  but not added: only a few registers are needed, and a custom class keeps the register meaning
  visible and the I2C timeout short (50 ms).

Power-on defaults read on this board [HW] (eFuse values; nothing was written except IRQ clears):

| Register | Value | Meaning |
|---|---|---|
| REG80 / REG82 | `0x0F` / 3300 mV | DCDC1–4 on, DCDC1 = 3.3 V |
| REG90 / REG91 | `0xFF` / `0x01` | **all LDOs on**: ALDO1–4, BLDO1–2, CPUSLDO, DLDO1, DLDO2 |
| REG92–95 | 3.3 / 3.3 / 3.0 / 1.8 V | ALDO1 / ALDO2 / ALDO3 (motor) / ALDO4 |
| REG27 | `0x14` | long-press IRQ 1.5 s, power off 6 s, power on 128 ms |
| REG30 | `0x03` | ADC: VBAT and TS channels on |
| REG41 | `0xFC` | VBUS/battery insert/remove and PWRON short/long press IRQs enabled |
| REG26 | `0x08` | sleep/wakeup at reset value |
| REG81 | `0x00` | DCDC1 automatic PWM/PFM (no forced PWM) |
| REG20 | `0x04` / `0x01` | power-on source: VBUS insert / PWRON key |

With USB connected: STATUS1 `0x38` (VBUS good, battery present), charging CC then CV, battery
3.87–4.20 V. ALDO3 (motor supply) and the unused rails are on by default; switching them off is
part of the sleep preparation (M2).

PWR key [HW]:
- GPIO10 reads 0 while idle and 1 while the key is pressed (schematic polarity confirmed).
- EXT1 `ANY_HIGH` on GPIO10 wakes the ESP32 from deep sleep reliably (causes bit EXT1,
  `esp_sleep_get_ext1_wakeup_status()` bit 10). RTC memory survives the sleep.
- The press that wakes the chip also sets the AXP short-press flag (REG49 bit3); so does a
  160 ms press while the chip is awake, seen at the same time as GPIO10 going high.
- Holding the key about 6 s powers the whole board off (AXP OFFLEVEL); a short press powers it on
  again with RTC memory cleared and REG20 = `0x01`.

Off mode [HW] (guided test `off`, 2026-10-31; `main/Axp2101.*`):
- Writing REG10 bit0 powers the board off: the USB connection drops within 40 ms of the last log
  line. A PWR press powers it on again with REG20 = `0x01` (PWR key) and REG21 = `0x02`
  (software power-off), RTC memory cleared; the sleep preparation then runs again (6/6).
- REG27 ONLEVEL set to 512 ms (`0x14` → `0x15`) is still `0x15` after each off/on cycle, as the
  datasheet's POR reset type says; it returns to the eFuse value only after a power-on reset
  (battery removed). Three brief taps left the watch off; a press of about 1 s powered it on.
- With USB plugged in the watch stayed off for 5 min after the power-off: VBUS that is already
  present does not power it on (only a VBUS insert does, REG20 `0x04`).
- Logging starts about 0.8 s after the ESP32 boots, as after a deep-sleep wake; the press itself
  must last at least 512 ms first.
- The battery charges in the off state: after the cell was discharged to 80 % with the display on
  (`sdkconfig.drain`), 5.3 min off with USB plugged in raised the gauge from 80 to 85 % (about
  20 mAh, roughly 0.2 A on average; an estimate), and the charger read constant current at the
  next power-on.

### Display (CO5300 AMOLED, 410 × 502, QSPI)
- The BSP drives it with the `esp_lcd_sh8601` driver: RGB565, x-gap 22, QSPI 40 MHz; it offers
  display on/off (0x29/0x28) and brightness (0x51) only. `bsp_display_start()` always resets the panel [REF].
- CO5300 commands: 0x28 display off, 0x10 sleep in, 0x11 sleep out (wait 120 ms; must precede 0x29),
  0x39 idle mode, 0x4F deep standby, 0x51 brightness [REF].
- CO5300 datasheet currents [DS §6, p.103]: Sleep In ≈ 85–110 µA (VDDI) + 25 µA (VCI); deep
  standby ≈ 2 µA + 1 µA. **No figures are given for normal, idle or partial display**, so the
  always-on panel current must come from a battery run.
- Idle mode (0x39) reduces colours to 8, set by the MSB of R, G and B; 0x38 leaves it. Partial mode
  (0x12) shows only the window set by 0x30/0x31; 0x13 returns to normal mode [DS §7.5]. AMOLED
  current grows with lit pixels and brightness.
- The panel refreshes itself from its GRAM. Whether the image is kept while the ESP32 deep-sleeps
  with the panel powered is [?] (needed for always-on). The QSPI pins (GPIO4–7, 11, 12) are digital
  pads; on the ESP32-S3 they can keep their level through deep sleep with `gpio_hold_en()` plus
  `gpio_deep_sleep_hold_en()` (CS must stay high) [ESP-IDF `gpio.h`].
- Reference: the infinition Rust firmware estimates its always-on mode (HH:MM at ~10 % brightness
  on black, CPU awake between minute updates) at ~8 mA; this is an estimate, not a measurement [REF].

Validated on this board [HW] (guided test `display`, 2026-10-31; `main/Display.*`):
- `bsp_display_start()` (LVGL 9.6, `esp_lvgl_port` 2.9) shows the image after the sleep
  preparation's deep standby: its reset pulse leaves deep standby. Panel start, touch included,
  takes about 0.4 s after ALDO2 is switched on. The LVGL draw buffer is 410 × 100 pixels in
  internal RAM (the BSP ignores its PSRAM flag); PSRAM is not enabled.
- DSI_PWR_EN needs ALDO2: with the time on screen, switching ALDO2 off darkened the panel at once
  while the controller kept running. So the panel's high-voltage supply follows ALDO2, and ALDO2
  off also removes that supply's quiescent current whenever the display is not needed.
- The BSP aborts (`ESP_ERROR_CHECK`) if the QSPI bus cannot be initialised; the sleep
  preparation frees the bus after its deep-standby command, so the BSP finds it free.

### FT3168 Touch (`0x38`)
- Register 0xA5 power mode: 0 active, 1 monitor, 2 standby, 3 hibernate (wake needs TP_RESET) [REF].
- Current: active 1.5 mA, monitor 30 µA, sleep 10 µA [DS Table 3-2].
- In monitor or sleep mode it may stop answering I2C after another device on the bus was accessed
  [DS §2.3]; send its commands right after a reset, before other bus traffic.
- After a reset it answers with power mode 0x00 (active); after `0xA5=0x03` it no longer answers
  I2C, which confirms sleep [HW].

### ESP32-S3
- EXT0/EXT1 wake only on RTC GPIOs 0–21; EXT1 supports ANY_HIGH [DS].
- Deep sleep ≈ 7–8 µA [DS Table 5-10]. Each wake from deep sleep is a full boot.
- Reset to `app_main` takes about 255 ms with the default configuration (log timestamps, 160 MHz,
  DIO flash, INFO logging, image validation on) [HW]. This is the baseline for wake latency.
- The internal RC slow clock drifts seconds per day: use it only as a wake timer; the PCF85063 is the clock.
- The native USB-Serial-JTAG connection drops while the chip is in deep sleep. After a wake the
  host needs about 0.4 s to re-enumerate and reopen the port; earlier log lines are lost, and so is
  a line written just before sleep unless the console is flushed [HW]. Development builds
  (`CONFIG_WATCH_DEV_LOGGING`) therefore wait for the host and flush before sleeping; with it,
  logging starts about 0.77 s after the wake instead of about 0.26 s.

## 4. Sleep Preparation (`main/Peripherals.*`)

Done once after power-on or reset (RTC flag); everything survives deep sleep. Verified with the
guided tests `m2` (until commit `dd54aec`) and `off` after a full power cycle [HW]:

| Step | Read-back on the board | Datasheet current before → after |
|---|---|---|
| FT3168: reset pulse, `0xA5=0x03` | answered before (mode 0x00), no answer after | 1.5 mA (active) → 10 µA |
| CO5300: reset pulse, QSPI `0x4F 0x01` (deep standby) | command accepted; panel stays dark | ~135 µA (Sleep In) → ~3 µA |
| QMI8658: CTRL7 = 0, CTRL1 bit0 = 1 | CTRL1 `0x21`, CTRL7 `0x00`, WHO_AM_I `0x05` | ~15 µA → ~6 µA |
| PCF85063: Control_2 COF = 111 | CONTROL2 `0x07` | CLKOUT 32.768 kHz → off |
| AXP2101: DCDC2–4, ALDO2–4, BLDO1–2, CPUSLDO, DLDO1–2 off | REG80 `0x01`, REG90 `0x01`, REG91 `0x00` | not specified |
| AXP2101: IRQ enables only PWR short/long press, all IRQ flags cleared | REG40–42 `0x00 0x0C 0x00`, REG48–4A `0x00 0x00 0x00` | see below |

- After each later deep-sleep wake the IMU and RTC still answer on I2C: switching the unused rails
  off does not disturb the bus [HW].
- **The codecs are left at their reset state.** After a power cycle they read their datasheet
  reset values [HW]: ES8311 0x14 `0x10`, 0x0E `0x6A`, 0x0D `0xFC`, 0x01 `0x00` (analog blocks
  already powered down); ES7210 0x40 `0x80`, 0x01 `0x20`, 0x06 `0x00`. A power-down step was
  tried in battery run C (ES8311: 0x14 `0x00`, 0x0E `0x6E`, 0x0D `0xF8`, 0x01 `0x00`; ES7210:
  0x40 `0xC0`, 0x01 `0x7F`, 0x06 `0x07`, the ES7210 values being those of Espressif's
  `es7210_stop()`). Every write read back correctly, but the sleep drain rose about tenfold, and
  run D without the step returned to the level of runs A/B; it was removed. Which write caused it
  is not tested. Two observations:
  - ES8311 0x0D, written as `0xF8`, read `0xFC` again after the 5 h run while the drain stayed
    high, so PDN_VREF is probably not the cause (its datasheet polarity, "0 – disable", is the
    opposite of every other PDN bit in the register; Espressif's `es8311_suspend()` ends with
    `0xFC`).
  - Main suspect: ES7210 0x06 bit0 (PUPDN_OFF) switches off the pin pull-ups. The I2S lines
    (MCLK GPIO16, SCLK GPIO41, LRCK GPIO45) go to both codecs and are not driven by the ESP32 in
    deep sleep, so without the pull-ups they float, and floating CMOS inputs can draw current.
  Neither datasheet gives a standby current.
- AXP2101 IRQ: the line has a 10k pull-up to VCC-RTC and is not read by the ESP32. Any pending
  enabled IRQ (the defaults enable charger, gauge and VBUS events) holds it low and draws up to
  ~0.3 mA through the pull-up. Only the PWR key IRQs stay enabled; their flag is cleared on every
  wake. The PWRON edge flags (REG49 bits 1:0) still latch on a press (REG49 `0x0B` after a PWR
  wake [HW]) but, being disabled, do not drive the line.
- GPIO8 (LCD_RESET) and GPIO9 (TP_RESET) have no pull resistor on the board; they are driven HIGH
  and held (`gpio_hold_en`) through deep sleep, because a low level would leave deep standby/sleep.
- The panel QSPI bus is opened with the BSP pin definitions for this one command: the BSP has no
  API that opens the panel IO without switching the display on.
- If the PWR key is still high when the awake window ends, the watch sleeps with a 60 s timer
  instead of the key wake. With the window restarting on every press, this needs a key held for
  more than 10 s, but the AXP powers the board off at 6 s, so it only guards against a stuck line.
- Estimated sleep current after preparation, from datasheet values: ESP32-S3 ~8 µA, AXP2101
  ~40 µA, touch 10 µA, IMU 6 µA, panel 3 µA, RTC <1 µA → **roughly 70 µA**, plus the unknown
  ES8311/ES7210 reset-state current and **~0.33 mA through the PWR key pull-up R11**, which no
  software can switch off (about 0.4 mA in total). Before preparation the touch controller alone
  was ~1.5 mA. These are estimates; see the battery drain runs for evidence from the board.

### Battery drain runs (estimates, not measured current)

Method: the watch sleeps on battery only and wakes every 15 min to log VBAT and the AXP2101 gauge
(`CONFIG_WATCH_BATTERY_LOG`). Slopes are least-squares fits over the samples after the first hour
(charger relaxation excluded). Current = gauge slope × 400 mAh (Waveshare's recommended cell; the
capacity of the fitted cell is not verified). The AXP2101 gauge has no current sense, so its
percentage is a model of the voltage, not an independent measurement. Each logging wake costs
about 0.3 s awake (log: 52 ms after boot), under 0.02 mA on average.

| Run | Firmware state | Duration | VBAT slope | Gauge slope | Estimated average current |
|---|---|---|---|---|---|
| A (2026-10-29/30) | peripherals at power-on defaults (`sdkconfig.noquiesce`) | 9.5 h fitted (10.7 h total), 4.10 → 4.01 V | −8.8 ± 0.6 mV/h | −1.00 ± 0.02 %/h | **≈ 4 mA** |
| B (2026-10-30) | sleep preparation on (section 4) | 9.5 h fitted (10.5 h total), 4.04 → 3.94 V | −8.8 ± 0.5 mV/h | −0.96 ± 0.02 %/h | **≈ 3.9 mA** |
| C (2026-10-31) | B + codec power-down + AXP IRQ step | 4.0 h fitted (5.2 h total), 4.10 → 3.71 V | −69 ± 2 mV/h | −9.4 ± 0.1 %/h | **≈ 35–40 mA** |
| D (2026-10-31) | B + AXP IRQ step (codec step skipped) | 2.0 h fitted (3.1 h total), 3.92 → 3.89 V | −11 ± 5 mV/h | unusable (see below) | **same level as A/B** |

- About 8.8 mV per gauge percent in the 80–98 % range.
- In both runs the slopes of the first and second half agree: the drain is steady, not a
  relaxation tail after charging.
- **The sleep preparation made no visible difference.** A − B = 0.04 ± 0.02 %/h (0.16 ± 0.09 mA)
  on the gauge and 0.0 ± 0.8 mV/h on VBAT, so the saving is below about 0.4 mA. The expected
  1.5 mA from the touch controller is not there; FocalTech controllers usually fall back to
  Monitor mode (30 µA [DS]) by themselves when idle, but the FT3168 datasheet does not state this
  default [?].
- So about 4 mA comes from something the preparation does not touch, or from the method itself.
  Candidates: the ES8311/ES7210 codecs (ALDO1 and DCDC1 stay on), the ESP32 sleep configuration,
  the AXP2101 itself, and the 400 mAh capacity assumption (a smaller cell scales every estimate
  down). The fitted cell is labelled 400 mAh.
- **Off run (2026-10-30/31):** the watch was switched off with the PWR key (6 s) for 11.25 h,
  between 2 h of logging before and 1 h after. The morning boot read PWROFF status (REG21) `0x01`
  (PWR key) and started its wake counters from zero, so the AXP had really cut every rail except
  RTCLDO. VBAT before and after, each projected to the switch moment with the −8.8 mV/h of runs
  A/B: 4072 → 4068 mV, **−0.3 ± 0.6 mV/h** (gauge 94 → 93 %). The AXP2101 datasheet gives about
  40 µA in the off state (≈ 0.1 mV/h).
- So the ~4 mA is not the cell or the method: at least about 90 % of it flows only while the
  rails are on and the ESP32 sleeps, which makes it a load software may be able to switch off.
  Candidates: the ES8311/ES7210 codecs at their power-on state, the AXP2101 in its on state (ADC
  enabled in REG30, sleep mode in REG26 unused), and the ESP32 sleep configuration. REG81 reads
  `0x00` (automatic PWM/PFM), so a forced-PWM DCDC1 is ruled out [HW].
- **Run C** added a codec power-down step and the AXP IRQ step (section 4): the drain rose about
  tenfold, steady over the whole run (first and second half −9.0 and −10.1 %/h).
- **Run D** kept the AXP IRQ step and skipped the codec step: back to the level of runs A/B.
  So the codec step caused the rise and was removed; the AXP IRQ step shows no visible effect
  either way. Run D started at about half charge right after a power cycle: the gauge then rose
  from 57 to 72 % on battery while it re-learned the state of charge, so only VBAT is usable.
  Two dips of 15–25 mV in single samples widen the VBAT fit; without them the slope is about
  −7.5 mV/h. At about 3.9 V the mV-to-mAh relation of runs A/B does not apply exactly.
- The remaining ~4 mA only matters while the watch sleeps with its rails on (always-on and a
  pending alarm), since the display-off state powers the board off. Its search continues with
  the always-on battery run (M5); candidates left: the ESP32 pad states in deep sleep and the
  AXP2101 on state (REG30 ADC, REG26).
- VBAT readings scatter by up to ~20 mV between neighbouring samples (single reading right after
  boot); use fits over many samples, not single values.

## 5. Open Questions

1. Battery thermistor: the schematic has RP2 on TS, but the official AXP example disables TS
   ("no thermistor"). Check the board.
2. QMI8658C CTRL9 handshake: does CTRL8.bit7 = 1 keep CmdDone off INT1 on this chip, and does a
   STATUS1 read alone acknowledge it (C datasheet) as well as CTRL9 = `0x00` (A datasheet)?
3. How much does one accelerometer sample change between motion checks on a desk and on the wrist
   (sets the no-motion threshold), and how long must the accelerometer run for one valid sample?
4. Does the CO5300 keep its image while the ESP32 deep-sleeps with the panel powered and the QSPI
   pins held? What does the always-on mode draw at low brightness, with and without idle mode?
5. Does Offset `0x76` (−10, MODE 0) bring the RTC drift near 0 ppm? Check with the guided test
   `clock` over several hours and days, and on the wrist (temperature).
6. ES8311/ES7210 standby current at their reset state (neither datasheet gives a figure), and
   which write of the run C power-down step raised the drain (suspect: ES7210 pull-ups off).
7. Which on-state load draws the ~4 mA of runs A, B and D in deep sleep (ESP32 pad states,
   AXP2101 on state, codecs at their reset state)?

## 6. Sources

- Schematic: Waveshare repository, `Schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf`
  (https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06)
- Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06
- QMI8658A datasheet Rev A: https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf
- QMI8658C datasheet Rev 0.6: Waveshare wiki, Resources section
- PCF85063A datasheet: https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf
- AXP2101 datasheet: https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf
- CO5300 datasheet: https://files.waveshare.com/wiki/common/Co5300_Datasheet.pdf
- FT3168 datasheet: https://files.waveshare.com/wiki/common/FT3168.pdf
- ES8311 datasheet rev 10.0: https://files.waveshare.com/wiki/common/ES8311.DS.pdf (user guide:
  https://files.waveshare.com/wiki/common/ES8311.user.Guide.pdf)
- ES7210 datasheet rev 21.0: https://files.waveshare.com/wiki/common/ES7210_DS.pdf
- ESP32-S3 sleep modes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html
- SensorLib (QMI8658, PCF85063): https://github.com/lewisxhe/SensorLib
- XPowersLib (AXP2101): https://github.com/lewisxhe/XPowersLib
- Rust watch firmwares for this board (reference only): waveshare-watch-rs (infinition) and its
  fork by bourumir-wyngs
