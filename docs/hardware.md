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
- Board check against the documents (2026-11-04) [HW]: esptool reads ESP32-S3 (QFN56) v0.2 with
  embedded 8 MB PSRAM (AP, 3.3 V) and flash JEDEC ID `0xC8` / `0x4019` (GigaDevice GD25Q256).
  Package markings in photos of both sides: `063A` (PCF85063A, the variant with alarm
  registers), `GigaDevice 25Q256EYIG`, `ESP32-S3 ... R8...`, `7210` (ES7210), `AXP2101`,
  `NS4150B`, `8658` (QMI8658; the marking does not show A or C) and a 40 MHz crystal. The ES8311
  marking was not legible and the FT3168 sits on the display flex. The silkscreen shows no board
  revision. Identity registers read once after power-on (`logChipIds`, touch step):

  | Part | Register | Read | Datasheet / reference |
  |---|---|---|---|
  | ES8311 | 0xFD / 0xFE / 0xFF | `0x83 0x11 0x01` | `0x83 0x11 0x00`: newer chip version |
  | ES7210 | 0x3D / 0x3E / 0x3F | `0x72 0x10 0x04` | `0x72 0x10 0x00`: newer chip version |
  | QMI8658 | 0x00 / 0x01 | `0x05 0x7C` | QMI8658A (section 3) |
  | AXP2101 | REG03 | `0x4A` | `0x4A` (XPowersLib) |
  | FT3168 | 0xA0 / 0xA3 / 0xA6 / 0xA8 | `0x03 0x64 0x02 0x11` | 0xA0 `0x03` = FT3168 (Waveshare `Arduino_FT3x68`); vendor `0x11` FocalTech; 0xA3 `0x64` is shared with the FT6336U in SensorLib's table |

  So the parts match the schematic and the datasheets used, except that both codecs are a
  later chip version than their datasheets (ES8311 rev 10.0, ES7210 rev 21.0), which may bear on
  the ES8311 0x0D result (section 4). The cell is labelled
  `402728 3.7V 400mAh`, but between 4.11 and 4.05 V it behaves like a cell of about 150 mAh
  (battery run R, section 4). The cell itself measures 28.1 × 23.3 × 4.0 mm and was not swollen
  (2026-11-11); the cavity is about 35 × 5 mm, 21 mm wide at the bottom and widening by
  1–2 mm towards the top. Quality cells of this volume
  (for example 402030, 4 × 20 × 30 mm) are rated about 160–200 mAh, so the cell is about what its
  size allows and only the 400 mAh label is wrong. A replacement that fits the cavity, with room
  for its protection board and for swelling, would gain at most a few tens of percent
  (estimate from typical datasheets); the cell is kept.
- AXP2101 PMIC rails [SCH]:

| Rail | Net | Loads | Rule |
|---|---|---|---|
| DCDC1 | VCC3V3 | ESP32 (the flash and the in-package PSRAM through its VDD_SPI, which ESP-IDF switches off in deep sleep), QMI8658, AMOLED VCI/VDDIO, touch, SD card, ES8311 DVDD and PVDD (R51 0R), NS4150B VDD (shut down by the CTRL pull-down R31), pull-ups (I2C, SD, R8 on GPIO0, R9 on CHIP_PU, R11 on GPIO10) | always on |
| ALDO1 | A3V3 | ES8311 AVDD and **all of ES7210** (which sits on the shared I2C bus) | **keep on**: an unpowered ES7210 hangs the I2C bus |
| ALDO2 | — | pull-up of DSI_PWR_EN (AMOLED panel power enable) | **on while the display shows anything**: with ALDO2 off the image goes dark at once [HW] |
| ALDO3 | — | vibration motor | off unless haptics |
| RTCLDO | VCC-RTC | PCF85063 VDD, AXP IRQ pull-up | RTC keeps time while a battery is present, even with the system off |
| others | — | no load found | – |

- The LDOs are fed from VSYS (ALDOIN "must be VSYS" [AXP DS]), not from DCDC1, so the DCDC1
  voltage does not change the current of the ALDO1 loads.

- AXP PWROK → ESP32 CHIP_PU, with R9 10k to VCC3V3; PWROK only pulls low ("outputs low level,
  which will be pulled up" [AXP DS §6.5.4]), so R9 carries no current while the chip runs.
  PWR key → AXP PWRON (short/long press) and GPIO10 [SCH].
- AXP VBACKUP (pin 27, net VBAT2) has only CP9 2.2 µF; no backup cell is fitted [SCH].
- Battery: MX1.25 connector; 10k thermistor RP2 on AXP TS [SCH].

## 3. Device Notes

### QMI8658A IMU (`0x6B`, SA0 = GND)
Two datasheets were compared: QMI8658**A** Rev A (2022) and QMI8658**C** Rev 0.6 (2021, "advance
information"). **The board has the QMI8658A** [HW, 2026-11-04]: REVISION_ID reads `0x7C`, the
value in the A datasheet text (both register tables say `0x68`, the C text `0x79`), and the
package marking ends in `RB`, the A marking scheme (C: `DA`). The WoM section (C: §9, Table 33, Fig. 10) and the
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
INT1_EN and bit4 as INT2_EN. INT1 worked with `CTRL1=0x48` on this board [HW], as the A
datasheet describes. The watch no longer wakes on motion (it samples the accelerometer at
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

Motion check (M6, `main/Motion.*`) [HW, 2026-11-12]:
- One reading: CTRL1 = `0x40` (oscillator on, address auto-increment, little-endian, INT pins
  high-Z), 15 ms for the system turn-on time t0, CTRL2 = `0x06` (±2 g, 112 Hz from the internal
  oscillator), CTRL7 = `0x01`. Power-down keeps the output registers [DS Table 31], so a new
  sample is recognised by the sample time stamp (`0x30`–`0x32`) advancing, not by the data. The
  first new sample came 11 ms after CTRL7 (21 ms on the first reading after power-on), as
  t2 ≈ 3 ms plus one sample period predicts. Three samples are dropped (t5 = 3/ODR) and eight
  averaged; then CTRL7 = 0 and CTRL1 = `0x41` (power-down). The watch is awake 109 ms longer
  per reading; one reading every 10 min adds under 0.01 mA on average (estimate).
- Measurement (`sdkconfig.motionlog`, one reading per always-on minute, `tools/guided_test.py
  motion` steps): **on a desk** 31 readings over 30 min stayed within 3 mg on every axis; the
  eight samples of one reading spread by 5–12 mg. **On the wrist** for 65 min (sitting, typing,
  walking, a while with the arm still) two readings one minute apart differed by as little as
  2 mg, but **readings 10 min apart always differed by at least 21 mg** on their largest axis.
- Product rule: every 10 min (`CONFIG_WATCH_MOTION_CHECK_MIN`) one reading is compared with the
  previous one; a change of more than **10 mg** on any axis counts as motion. After 30 min
  (`CONFIG_WATCH_MOTION_OFF_MIN`, three checks) without motion the watch powers off. A failed
  reading counts as no motion, so a broken sensor cannot keep the watch out of off mode.
- Validated with `sdkconfig.motionquick` (a check every minute, power-off after 3 min;
  `tools/guided_test.py motioncheck` and `motionhand`): on the desk the changes were 1 mg and
  the watch powered off after the third check; held in the hand the changes were 361 and 539 mg,
  each resetting the count, and back on the desk it powered off three checks later. A PWR wake
  takes a new reference.

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
- Quiescent current ≈ 40 µA [DS Table 5-5], specified only with no VBUS, the BATFET disabled and
  only RTCLDO on, i.e. the off state. The current with DCDC1 and an LDO on is not given.
- Sleep mode (REG26 bit0) only records REG80/90/91 and restores them on wakeup; the datasheet
  names no lower internal current for it [DS §6.5.4.4]. Wakeup (bit1) restores the rails and,
  with REG26 bit3 set (power-on value here, `0x08`), pulls PWROK low, which resets the ESP32.
  Setting only bit0 before deep sleep and clearing it at each wake (battery run T) made no
  difference to the drain, so the firmware does not use it.
- Charger at power-on [HW, 2026-11-10]: REG61–64 = `0x05 0x08 0x15 0x03`: 125 mA precharge,
  200 mA constant current, termination on at 125 mA, 4.2 V. That suits a 400 mAh cell, not this
  one (about 100–150 mAh in use, at most about 220–300 mAh for its size): 200 mA is above 1C, and
  125 mA ends the constant-voltage phase early, so charging stopped with the cell resting at
  about 4.10–4.11 V. The firmware now writes 25 mA precharge, 100 mA constant current and 25 mA
  termination (`0x01 0x04 0x11`) at every power-on and PWR wake (`Axp2101::setChargeCurrents`).
  The values read back, and after a USB re-plug the charger restarted in constant current. A
  lower termination gains only a few percent; 0 mA or termination off would hold the cell at
  4.2 V indefinitely, which ages it. After a full charge with the new setting the cell rested at
  about 4.155 V one hour after unplugging, against 4.10–4.11 V before (run U); the capacity
  itself is still open (question 8).
- Backup charger (REG18 bit2, reset 0): a linear charger of 100 µA into VBACKUP, termination set by
  REG6A [DS §6.12.2]. LilyGo lists deep-sleep currents with "backup power" on and off: 530 / 460 µA
  (T-Watch S3) and 1.1 mA / 840 µA (T-Watch Ultra), and switches it off before sleep [REF]. On this
  board it is off: REG18 = 0x0A (gauge and cell charger on, backup charger and watchdog off),
  REG6A = 0x03 [HW, 2026-11-10]. The firmware only changes bit3 (gauge), read-modify-write.
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
| REG10 | `0x34` | bit5: a rail switched off is discharged (so DSI_PWR_EN is held low through R10 while ALDO2 is off); bit2: holding PWRON 16 s shuts the PMIC enable |
| REG50 | `0x12` | TS pin is a fixed input that does not affect the charger; TS current source off (50 µA when on) |
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

LVGL start without `bsp_display_start()` [REF][HW] (M8, 2026-11-12; `main/Display.cpp`):
- `bsp_display_start()` adds this QSPI panel with `lvgl_port_add_disp_rgb()`. On that path
  `esp_lvgl_port` 2.9 calls `lv_display_flush_ready()` right after `esp_lcd_panel_draw_bitmap()`,
  which for QSPI only queues the DMA transfer; LVGL then draws the next band into the single
  buffer while it is still being sent. On the board this showed as horizontal stripes: a few
  lines on the watch face, the settings rows unreadable. It also passes the SH8601 panel handle
  to `esp_lcd_rgb_panel_register_event_callbacks()`, which writes RGB-panel fields past the end
  of the SH8601 object [REF: `esp_lcd_panel_rgb.c`]. Waveshare's LVGL example uses the same BSP code.
- `Display` repeats the BSP's steps with public calls: `lvgl_port_init()`, `bsp_display_new()`
  (it also sets the handles that `bsp_display_brightness_set()` uses), `lvgl_port_add_disp()`
  (flush done on the QSPI transfer-done callback), the BSP's even/odd area rounder, and
  `bsp_touch_new()` with `lvgl_port_add_touch()`. No stripes since then.
- With `buff_dma = true` and `sw_rotate = false` (the BSP has `false` and `true`) the touch
  controller raised no interrupt at all (TP_INT stayed high during swipes, no LVGL press); with
  the BSP's flags it works. Which of the two flags matters, and why, is [?].
- Large dark-grey areas (0x202020 row backgrounds) shimmer continuously on this panel at every
  brightness; white on black does not. The settings rows are black with a grey outline.

Always-on face [HW] (guided test `aod`, 2026-10-31; `main/Panel.*`, `main/AlwaysOn.*`):
- **The CO5300 keeps its image while the ESP32 deep-sleeps**, with ALDO2 on and the panel pins
  held (CS high, SCLK and SIO0–3 low, LCD_RESET high; all are RTC pads, so `gpio_hold_en` holds
  them through deep sleep). A timer wake opens the QSPI bus and the `esp_lcd_sh8601` panel without
  a reset or init commands and redraws only the digits: the HH:MM changed at each minute with no
  blanking or flicker, over 4 minutes in two runs.
- The BSP is not used on these wakes: `bsp_display_start()` would reset the panel. A PWR wake
  releases the holds and `Display` starts the active face as usual; the sleeping touch controller
  wakes from the BSP's reset pulse.
- Face: seven-segment HH:MM drawn as filled rectangles (no LVGL, no fonts) at brightness 10 %
  in idle mode (39h), moved within ±8 pixels each minute against burn-in. It shows UTC until the
  UTC offset setting exists (M8).
- Wake length per minute (RTC memory statistics): 382 ms from reset to `app_main` plus 112 ms in
  the app with the image check on; **230 + 137 ms** with
  `CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP` (now the default; the check still runs after
  power-on and reset). The remaining 230 ms likely include the bootloader and startup logs
  written to UART0 at 115200 baud [?].
- The wake lands about 1–1.5 s after the minute changes (the sleep is computed from whole RTC
  seconds plus 100 ms); the digits change that much late.

### FT3168 Touch (`0x38`)
- Register 0xA5 power mode: 0 active, 1 monitor, 2 standby, 3 hibernate (wake needs TP_RESET) [REF].
- Current: active 1.5 mA, monitor 30 µA, sleep 10 µA [DS Table 3-2].
- In monitor or sleep mode it may stop answering I2C after another device on the bus was accessed
  [DS §2.3]; send its commands right after a reset, before other bus traffic.
- After a reset it answers with power mode 0x00 (active); after `0xA5=0x03` it no longer answers
  I2C, which confirms sleep [HW].
- The BSP drives it with the `esp_lcd_touch_ft5x06` driver: the FT3x68 point registers (count
  0x02, first point 0x03–0x06, second 0x09–0x0C) match the FT5x06 layout. Neither the BSP nor
  Waveshare's Arduino example (`06_LVGL_Arduino_v9`, raw coordinates to LVGL) applies an offset,
  swap or mirror; the 22-pixel column offset belongs to the panel's GRAM only [REF].

Validated on this board [HW] (guided test `active`, 2026-11-12; `main/Display.*`, M7):
- Touch coordinates need no correction: touches about 1 cm from the corners read (97, 101),
  (328, 102), (347, 415) and (75, 430), top left clockwise, so x grows to the right and y
  downwards within the panel's 410 × 502 (1 cm is about 120 pixels).
- LVGL's tileview moves between the pages with a horizontal swipe. LVGL's inactivity time
  (`lv_display_get_inactive_time`) follows the touches: with a tap every 3 s the display stayed
  on for 23 s and the timeout came 10.0 s after the last tap.
- From a PWR wake the active face appears about 1.1–1.4 s after the boot log (1.7–2.1 s after
  reset), part of it the development logging's wait for the USB host.

Taps on the settings page (M8, `main/Display.cpp`) [REF][HW]:
- LVGL cancels a click once the finger moved more than the indev scroll limit, 10 px by default
  (0.8 mm here), and starts a scroll instead [lvgl issue #2600]. `lv_indev_set_scroll_limit()`
  sets it to 30 px (2.4 mm); the page swipe still works.
- LVGL objects are press-locked by default: a finger that slides onto another row still clicks
  the first one on release (seen on the board). The rows clear the press lock, so sliding off
  a row cancels the tap (checked on the board).
- Rows are 108 px high (8.7 mm), about the Wear OS minimum touch target of 48 dp (~9 mm).

### ESP32-S3
- EXT0/EXT1 wake only on RTC GPIOs 0–21; EXT1 supports ANY_HIGH [DS].
- Deep sleep ≈ 7–8 µA [DS Table 5-10]. Each wake from deep sleep is a full boot.
- VDD_SPI is in 3.3 V mode (VDD3P3_RTC through R_SPI) and feeds the GD25Q256 and the in-package
  PSRAM. ESP-IDF v6.1 hands it to the sleep state machine with `RTC_CNTL_SDIO_PD_EN` set in deep
  sleep, so the switch opens [TRM §10.3.4.3; `rtc_sleep.c`]. `CONFIG_SPIRAM` is off: the PSRAM is
  never initialised. Its data lines GPIO33–37 are not reserved, so ESP-IDF isolates them in deep
  sleep together with the other digital pads once `gpio_deep_sleep_hold_en` is set (always, run H).
- Reports of mA-level deep sleep on ESP32-S3R8 boards (Arduino Nano ESP32: about 1.75 mA) trace
  to flash lines left floating with the flash powered from the main 3.3 V rail; here the flash is
  on VDD_SPI [SCH]. Another report (970 → 14 µA) was an ADC unit left open at sleep entry; this
  firmware opens none.
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
  run D without the step returned to the level of runs A/B; it was removed. Runs I to L traced
  the rise to ES8311 0x0D `0xF8` (PDN_VREF cleared, section 4 below). Earlier observations:
  - Run C read ES8311 0x0D as `0xFC` again after the 5 h run while the drain stayed high, which
    first argued against PDN_VREF; run L shows that this write alone raises the drain, so the
    raised state outlasts the register value (not explained).
  - The suspicion on ES7210 0x06 (pin pull-ups off, floating I2S lines) is ruled out by runs I
    and J.
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
- If the PWR key is still high when active mode ends, the watch sleeps with a 60 s timer
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
(charger relaxation excluded). Run R calibrated the slope with a known load: **about 5.8 mV/h of VBAT per 1 mA** of
battery current (±12 %), valid near full charge (4.05–4.11 V). The current column uses that
factor; runs that went below about 4.0 V (C, E, F, I, K, L) are rough there, since the mV-per-mAh
relation changes with the state of charge. Before run R the column was gauge slope × 400 mAh,
shown in brackets; it overstated every current by about 2.6 times. Durations from a slope (hours
of always-on per charge) did not depend on the capacity assumption and stay valid. The AXP2101 gauge has no current sense, so its
percentage is a model of the voltage, not an independent measurement. Each logging wake costs
about 0.3 s awake (log: 52 ms after boot), under 0.02 mA on average.

| Run | Firmware state | Duration | VBAT slope | Gauge slope | Estimated average current |
|---|---|---|---|---|---|
| A (2026-10-29/30) | peripherals at power-on defaults (`sdkconfig.noquiesce`) | 9.5 h fitted (10.7 h total), 4.10 → 4.01 V | −8.8 ± 0.6 mV/h | −1.00 ± 0.02 %/h | **≈ 1.5 mA** (≈ 4) |
| B (2026-10-30) | sleep preparation on (section 4) | 9.5 h fitted (10.5 h total), 4.04 → 3.94 V | −8.8 ± 0.5 mV/h | −0.96 ± 0.02 %/h | **≈ 1.5 mA** (≈ 3.9) |
| C (2026-10-31) | B + codec power-down + AXP IRQ step | 4.0 h fitted (5.2 h total), 4.10 → 3.71 V | −69 ± 2 mV/h | −9.4 ± 0.1 %/h | **≈ 12 mA**, rough (≈ 35–40) |
| D (2026-10-31) | B + AXP IRQ step (codec step skipped) | 2.0 h fitted (3.1 h total), 3.92 → 3.89 V | −11 ± 5 mV/h | unusable (see below) | **same level as A/B** |
| E (2026-10-31/11-01) | always-on face, brightness 10 %, idle mode off (`sdkconfig.aodlog`) | 10.8 h fitted (11.8 h total), 4.06 → 3.74 V | −28.8 mV/h | −3.96 %/h | **≈ 5 mA**, rough (≈ 13–16) |
| F (2026-11-01/02) | E with idle mode on (39h) | 10.5 h fitted (18.9 h total), 4.09 → 3.87 V | −18.5 mV/h | −2.33 %/h | **≈ 3.2 mA**, rough (≈ 8.5–9.5) |
| G (2026-11-02) | B + idle pads held in every second hour (see below) | 10.75 h fitted (11.75 h total), 4.09 → 4.01 V | −6.7 mV/h overall | −0.79 %/h overall | held vs. free not resolved |
| H (2026-11-02/03) | B + idle pads held through every sleep | 10.5 h fitted (11.5 h total), 4.11 → 4.03 V | −6.2 ± 0.4 mV/h | −0.71 ± 0.02 %/h | **≈ 1.1 mA**; 1.2 mA over 1–4 h (≈ 2.8–2.9) |
| I (2026-11-03) | H + codec power-down of run C, 5 min samples | 2.9 h fitted (3.9 h total), 4.01 → 3.78 V | −68 ± 2 mV/h | −9.0 ± 0.1 %/h | **≈ 12 mA**, rough (≈ 30–36) |
| J (2026-11-03) | H + ES7210 0x06 = `0x07` only (pull-ups, TDMIN pull-down, DLL off) | 1.2 h fitted (1.5 h total), 3.85 → 3.84 V | −7.2 ± 0.4 mV/h without two dips | unusable (re-learning after the power cycle) | **floor level** |
| K (2026-11-03) | H + the four ES8311 writes only | 1.2 h fitted (1.5 h total), 3.87 → 3.80 V | −68 ± 5 mV/h | −5 %/h, still re-learning | **as run I** |
| L (2026-11-03) | H + ES8311 0x0D = `0xF8` only (PDN_VREF cleared) | 1.2 h fitted (1.5 h total), 3.81 → 3.73 V | −68 ± 5 mV/h | −10 %/h | **as run I** |
| M (2026-11-03/04) | H + every codec write except ES8311 0x0D | 10.75 h fitted (11.75 h total), 4.09 → 4.01 V | −6.7 ± 0.4 mV/h | −0.75 ± 0.01 %/h | **≈ 1.15 mA, as run H** (≈ 3) |
| N (2026-11-04) | H + panel QSPI lines held (CS high, SCLK and SIO0–3 low), 5 min samples | 2.9 h fitted (3.9 h total), 4.10 → 4.08 V | −7.0 ± 0.1 mV/h without dips (H over the same 1–4 h: −7.1 ± 0.2) | −0.76 %/h (H: −0.77) | **same as H** |
| O (2026-11-04) | H + AXP2101 ADCs (REG30 = 0) and fuel gauge (REG18 bit3) off in deep sleep, on again at each wake, 5 min samples | 3.9 h, 4.11 → 4.07 V | ≈ −7 mV/h between the steps of the stale readings (see below) | none (gauge off, 100 % throughout) | **same as H within ≈ 0.2 mA** (≈ 0.5) |
| P (2026-11-08) | H with the display FPC unplugged (panel, touch and panel supply removed), 5 min samples | 2.8 h fitted (3.8 h total), 4.11 → 4.06 V | −7.0 ± 0.2 mV/h without dips (H over the same hours: −7.1 ± 0.2, N: −7.0 ± 0.1) | 95 → 93 % in 2.75 h (H, N: 96 → 94 %) | **same as H: the floor is not in the panel module** |
| Q (2026-11-09) | H with DCDC1 (VCC3V3) at 3.0 V in deep sleep, 3.3 V again at each wake (`sdkconfig.dcdc1`), 5 min samples | 2.8 h fitted (3.9 h total), 4.11 → 4.08 V | −6.2 ± 0.1 mV/h without dips (H, N, P over the same hours: −7.0 to −7.1) | 96 → 95 % in 2.9 h | **≈ 11 % lower (≈ 0.14 mA): most of the floor is a load on VCC3V3** |
| R (2026-11-09/10) | H + calibration load: GPIO1, 2, 3, 17 (10k pull-ups to VCC3V3, no SD card) held low in deep sleep (`sdkconfig.sdlow`), 5 min samples | 2.8 h fitted (3.8 h total), 4.11 → 4.06 V | −14.0 ± 0.4 mV/h without dips (P over the same hours: −7.0 ± 0.2) | 96 → 92 % in 2.8 h | **+1.2 mA known load → 5.8 mV/h per mA; floor ≈ 1.2 mA** |
| S (2026-11-10) | H + USB pads GPIO19/20 isolated in deep sleep, given back to USB at each wake (`sdkconfig.usbiso`), 5 min samples | 2.8 h fitted (3.8 h total), 4.10 → 4.08 V | −6.9 ± 0.1 mV/h without dips (N, P over the same hours: −7.0, −7.1) | 96 → 93 % in 2.8 h | **same as H: floating USB pads are not the floor** |
| T (2026-11-10) | H + AXP2101 sleep mode (REG26 bit0) set before deep sleep, cleared at each wake (`sdkconfig.axpsleep`), 5 min samples | 2.8 h fitted (3.9 h total), 4.10 → 4.08 V | −6.9 ± 0.1 mV/h without dips (S, P over the same hours: −6.9, −7.1) | 96 → 94 % in 2.8 h | **same as H: the AXP2101 sleep mode does not lower the floor** |
| U (2026-11-11) | H after a full charge with the new charger setting (100 mA, termination 25 mA), 5 min samples | 2.8 h fitted (4.0 h total), 4.155 → 4.135 V | −7.6 ± 0.1 mV/h without dips | 100 → 99 % in 2.8 h | **new baseline, higher on the cell's curve: not comparable with runs A–T** |

- mA figures in the notes below that were written before run R assume a 400 mAh cell;
  multiply them by about 0.38 (run R).
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
  pending alarm), since the display-off state powers the board off. Runs G and H below lowered
  it to about 2.9 mA.
- **Run E** (always-on face drawn each minute, battery samples taken on every 15th minute wake):
  about four times the deep-sleep drain, so the face adds about 9–12 mA. The minute wakes
  account for only about 0.3 mA (0.37 s × ~50 mA per minute), so nearly all of it is the panel
  in normal display mode at 10 % with about 6 % of its pixels lit. At this rate a 400 mAh cell
  lasts about a day in always-on. The gauge fell 100 → 48 % in 11.8 h.
- **Run F**: idle mode (39h) lowers the always-on drain by about 40 % (5–6 mA), with no visible
  change to the white digits; the face adds about 5 mA over the deep-sleep floor. Both halves
  of the fit agree (−20.0 and −17.4 mV/h). Idle mode is now the default
  (`CONFIG_WATCH_AOD_IDLE_MODE`). A 400 mAh cell lasts about 45 h in always-on.
- Both runs woke once a minute without a miss: 710 timer wakes in about 708 min (E) and 1139 in
  about 1137 min (F). A failed or out-of-range RTC read would have computed a negative sleep;
  `AlwaysOn::update` now retries such a read a minute later, and a PWR wake logs the timer of
  the last sleep.
- **Run G** (idle pads): in deep sleep nothing drives the I2S lines to the codecs (GPIO16, 40,
  41, 45 out, 42 in), and the ESP32 digital pads GPIO38–42 come out of reset with the input
  buffer on and no pull [ESP32-S3 DS Table 2-1]; run C showed that switching the ES7210 pin
  pull-ups off raises the drain about tenfold, so floating I2S inputs are a plausible part of
  the floor. Run G held the I2S outputs low (GPIO45 low is its strap default), pulled GPIO42
  down, isolated GPIO13 and GPIO21 (`rtc_gpio_isolate`) and enabled `gpio_deep_sleep_hold_en`,
  which makes ESP-IDF isolate every other digital pad; this state alternated hourly with the
  plain one. Wakes and the following PWR wake were normal. The overall slope was about 20 %
  below runs A/B, but the hourly difference was not resolved (VBAT +3 ± 9 mV/h, or +7.7 ±
  4.4 mV/h without the single-sample dips; gauge +0.15 ± 0.31 %/h): four samples per hour do
  not beat the VBAT scatter, and the gauge smooths over hours.
- **Run H** held the idle pads through every sleep (`Peripherals::holdSleepLines`, now part of
  every sleep): the drain fell by about 1 mA against run B (gauge −0.25 ± 0.03 %/h, VBAT about
  −2.6 mV/h; both halves of the run agree). Run G's average lies between B and H, as expected
  for half the time held. Which of the held lines carries the saving is not tested. About
  2.5 mA of the floor remain unexplained besides R11 (~0.33 mA).
- **Run I** repeated the codec power-down of run C with the I2S lines held: the same tenfold
  drain. Floating I2S lines are therefore not the cause; one of the register writes is. Run I
  was the first short screening run (a sample every 5 min, `sdkconfig.quick`); an effect of
  this size shows within the first hour. The writes are now selected one group at a time
  (`CONFIG_WATCH_CODEC_WRITE_MASK`) to find the one responsible.
- **Run J** wrote only ES7210 0x06: the drain stayed at the floor, so the pin pull-ups are not
  the cause. A PWR wake read every codec register back unchanged (0x06 `0x07`, the rest at their
  reset values). The gauge rose 57 → 63 % on battery while it re-learned, as in run D.
- **Run K** wrote only the four ES8311 registers: the drain rose as in run I, so the ES8311
  causes it. Its 0x01 write is a no-op (the register reads `0x00` from reset), leaving 0x14
  (input selection off), 0x0E (LPVREFBUF) and 0x0D (PDN_VREF cleared). All four still read
  their written values after 1.5 h.
- **Run L** wrote only ES8311 0x0D = `0xF8`: the same rise. **Clearing PDN_VREF (0x0D bit2)
  raises the sleep drain by about 30 mA**, although the datasheet calls the cleared bit
  "disable internal reference circuits". The register must keep its reset value `0xFC`, which
  is also the value Espressif's `es8311_suspend()` ends with. This explains runs C and I.
- **Run M** applied every other write of the step (ES8311 0x14, 0x0E, 0x01; ES7210 0x40,
  0x01, 0x06), all read back unchanged after 11.75 h: no saving against run H (gauge +0.04 ±
  0.02 %/h, VBAT −0.5 ± 0.6 mV/h, i.e. at most a fraction of a milliamp either way). The codecs
  at their reset state are not the remaining floor, so they stay at reset (the test option
  remains for later checks).
- **Run N**: ESP-IDF isolates only the digital pads (GPIO26–48, `SOC_GPIO_VALID_DIGITAL_IO_PAD_MASK`)
  when a deep-sleep hold is enabled; the RTC pads (GPIO0–21) keep their state, so without a hold
  the panel QSPI lines are undriven at the CO5300 inputs, which stay powered from VCC3V3. Holding
  them as the always-on face does made no difference against run H over the same hours of the
  run (VBAT within 0.1 mV/h, i.e. within about 0.1 mA), so the hold was not kept for plain deep
  sleep. The SD lines (GPIO1–3, 17) are not a candidate: they have 10k pull-ups and the ESP32
  enables no pull on them at reset [ESP32-S3 DS Table 2-1].
- **Run O**: with the ADCs off in sleep, VBAT read at the timer wakes stays stale: a wake of
  about 0.15 s with the ADC switched on 0.1 s earlier rarely updates it, so the log is a
  staircase (4106, 4088, 4083, 4069 mV) and the usual fit does not apply. Between the step
  edges (about 0.8 h to 3.4 h) the drop is 19 mV in about 2.6 h, about −7 mV/h; the uncertainty
  from the 5 min step timing and the 1 mV steps is about ±1 mV/h (±0.5 mA). The measurement
  chain of the AXP2101 is therefore not the remaining floor and stays at its power-on state.
  The ADC conversion time is not in the datasheet; 100 ms after enabling REG30 the reading
  was still the stale value.
- **Run P**: with the display FPC unplugged the drain did not change (VBAT within 0.1 mV/h of
  runs H and N over the same hours), so the panel module as a whole (CO5300, FT3168, the panel
  supply and the lines to them) is not the remaining floor; it lies on the main board. A first
  attempt did not start: after the 6 s power-off the PWR press did not switch the watch on, and
  without the panel nothing showed it; the next USB insertion powered it on (REG20 `0x04`).
- **Run Q**: with DCDC1 lowered from 3.3 V to 3.0 V in deep sleep the VBAT slope fell by
  0.8 ± 0.25 mV/h (about 11 ± 4 %, about 0.14 mA with the factor of run R). The AXP2101's own current and the ALDO1 loads
  (fed from VSYS) do not depend on the DCDC1 voltage; a load on VCC3V3 draws 9 % less battery
  current if it is a constant current and 17 % less if it is resistive (DCDC1 efficiency taken
  as unchanged). So between about 60 % (resistive) and all (constant current) of the ~1.2 mA
  floor is drawn on VCC3V3: the ESP32 and the main-board parts on DCDC1, not the AXP2101 on
  state or the codecs' analog side on ALDO1. The voltage itself was not read back; the REG82
  write returned OK at each sleep entry and the 54 timer wakes ran normally at 3.0 V.
- **Run R** (calibration): the four SD lines, each with a 10k pull-up to VCC3V3 and no card in
  the slot, were driven low and held through deep sleep (the PWR wake next morning read them
  low, so the hold lasted). That adds 4 × 3.3 V / 10 kΩ = 1.32 mA on VCC3V3, about 1.2 ± 0.1 mA
  from the battery (DCDC1 efficiency taken as 85–92 %; the resistor tolerance is not known).
  VBAT fell 7.0 ± 0.45 mV/h faster than in run P over the same hours, so **1 mA ≈ 5.8 mV/h**
  (±12 %) near full charge. With 8.8 mV per gauge percent this is about 1.5 mAh per percent,
  i.e. the cell behaves like about 150 mAh, close to the ~100 mAh two users measured for the cell
  sold with this board (Waveshare issue #6); the gauge agrees (≈ 1.45 against ≈ 0.73 %/h, about
  170 mAh). The deep-sleep floor of runs H, N and P is therefore **about 1.2 mA**, of which
  about 0.3 mA is R11; the rest, about 0.9 mA, is still unexplained. The LilyGo T-Watch S3
  (ESP32-S3R8 and AXP2101) is specified at 0.46–0.53 mA in deep sleep. A full discharge would
  measure the real capacity; it is not needed for comparing runs.
- **Run S**: the USB pads GPIO19/20 have no pull on the board and are RTC pads, which ESP-IDF
  leaves alone in deep sleep, so with USB unplugged they float at the USB PHY inputs. Isolating
  them (`rtc_gpio_isolate`) before deep sleep made no difference against runs N and P (within
  about 0.2 mV/h, about 0.03 mA). Giving them back at the start of each wake (`rtc_gpio_deinit`)
  restored the USB console on the next PWR wake; the log lines printed after the isolation
  ("Entering deep sleep") are lost.
- **Run T**: LilyGo enables the AXP2101 sleep mode (REG26 bit0) before deep sleep on the watches
  whose deep-sleep currents are the comparison (section 6). Set before every deep sleep and
  cleared at every wake, read back each time (`0x08 → 0x09`, and `0x09` again at the next wake),
  it made no difference against runs S and P over the same hours. The wakeup bit was never
  written; all 50 timer wakes and the PWR wake were normal. This was the last run with the old
  charger setting; later runs start from a fuller cell, so their slopes are not comparable with
  runs A–T (run R calibrates 4.05–4.11 V).
- **Run U**: the first run after a full charge with the new charger setting (AXP2101 section).
  At the flashing boot the charger reported "charger done" at 4159 mV with USB in. On battery,
  VBAT read 4161 mV after 5 min, 4154 mV after 55 min and 4135 mV after 3.4 h: the rest voltage
  one hour after unplugging is about 45 mV higher than with the old setting (4.10–4.11 V). The
  gauge stayed at 100 % for 1.9 h. The slope over 1–3.8 h is −7.6 ± 0.1 mV/h, steeper than runs
  S and T (−6.9) at 4.08–4.10 V. The firmware is the same as in H, so the difference is most
  likely the steeper open-circuit curve of the cell above 4.11 V, not a higher drain: if the
  floor is still about 1.2 mA, this range has about 6.3 mV per mAh against 5.8 in run R. On that
  assumption the 45 mV are roughly 7 mAh, a few percent of the cell; an estimate, not a
  capacity measurement. Runs from now on are compared with run U.
- Screening resolution: with 5 min samples and the dips removed (readings more than 3 mV below
  the fit, refitted until none is left), a 3 h window fits VBAT to about ±0.1–0.2 mV/h (about
  ±0.03 mA), provided the run starts near full charge and runs are compared over the same hours.
  Run S had several dips of 5–8 mV; the earlier 10 mV cut-off kept them and gave −6.1 ±
  0.4 mV/h, while runs N, P, Q and R give the same slopes with either cut-off.
- VBAT readings scatter by up to ~20 mV between neighbouring samples (single reading right after
  boot); use fits over many samples, not single values.

## 5. Open Questions

1. Battery thermistor: the schematic has RP2 on TS, but the official AXP example disables TS
   ("no thermistor"). The eFuse value REG50 `0x12` already makes TS a fixed input that does not
   affect the charger, with its current source off [HW]; whether RP2 is fitted is unchecked.
2. QMI8658A CTRL9 handshake: does CTRL8.bit7 = 1 keep CmdDone off INT1 on this chip, as the A
   datasheet states?
3. Answered 2026-11-12 (QMI8658A section, motion check): at most 3 mg on a desk and at least
   21 mg on the wrist between readings 10 min apart; the first new sample comes 11 ms after the
   accelerometer is enabled. Longer wrist runs (sleep included) may still show rarer still phases.
4. Which other CO5300 settings lower the always-on panel current further (frame rate, partial
   mode)? With idle mode it was about 3.2 mA, of which about 1.5 mA was the deep-sleep floor
   (runs E and F, question 7, scaled with run R); with the idle pads held the floor is about
   1.2 mA (run H).
5. Does Offset `0x76` (−10, MODE 0) bring the RTC drift near 0 ppm? Check with the guided test
   `clock` over several hours and days, and on the wrist (temperature).
6. Why does ES8311 0x0D = `0xF8` (PDN_VREF cleared) raise the sleep drain by about 30 mA
   (runs I to L), against the datasheet wording?
7. Which on-state load draws the remaining ~1.2 mA in deep sleep (runs H and R; about 0.4 mA
   of the ~1.5 mA of runs A, B and D came from idle pads; R11 takes about 0.3 mA)? It is on the main board (run P), and most or
   all of it is drawn on VCC3V3 (run Q). The pull-up lines are not held low in sleep: right
   after a PWR wake, before any driver touches them, GPIO0, 1, 2, 3, 14, 15 (I2C), 17, 38
   (touch INT) and 39 (RTC INT) read high, GPIO18 and 46 low [HW, 2026-11-09]. ESP-IDF v6.1
   powers down XTAL, RC_FAST, VDD_SPI, RTC_PERIPH and the digital domain in this configuration
   (EXT1 does not keep RTC_PERIPH on) and leaves only the RTC pads (GPIO0–21) unmanaged
   [ESP-IDF `sleep_modes.c`, `rtc_sleep.c`]. Candidates: the AXP2101 on-state current (not specified), the ESP32 sleep state (domains,
   clocks or the USB PHY kept on), the ES8311 digital supply, the QMI8658 and the KEYS pull-up R11
   (about 0.33 mA, section 1). Unlikely as the main part: the AXP2101 on state and the parts on
   ALDO1 (run Q). Ruled out: the panel module (run P), a card in the SD
   slot (none is fitted), the panel QSPI lines (run N), the panel's high-voltage supply running
   with ALDO2 off (REG10 off-discharge on, so DSI_PWR_EN is low), the AXP2101 ADCs and gauge
   (run O), the TS current source (off at power-on, REG50), the floating USB pads (run S), the
   AXP2101 backup charger (off, REG18 = 0x0A), the AXP2101 sleep mode (run T).
8. What is the real capacity of the cell (about 100–150 mAh in use against the 400 mAh label;
   issue #6)? The early charge termination (125 mA at power-on, now 25 mA; AXP2101 section)
   explains only a few percent: the rest voltage rose from about 4.10–4.11 V to about 4.155 V
   (run U). Measure the capacity with a full discharge at a known load.
9. Why does the touch controller raise no interrupt when LVGL is added with `buff_dma = true`
   and `sw_rotate = false` (Display section, M8)? Not needed while the BSP's flags are used.

## 6. Sources

- Schematic: Waveshare repository, `hardware/schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf`
  (moved from `Schematic/` in October 2026, same file;
  https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06). As of 2026-11-10 neither the
  repository (examples, factory firmware source in `examples/factory`) nor the BSP
  (`waveshare/esp32_s3_touch_amoled_2_06` 2.0.0, the version used here) has deep-sleep or
  low-power code.
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
- Board issue #6 (deep sleep about 2.1 mA measured by a user with an INA228, firmware state not
  given; cell measured at about 100 mAh by two users from the charge energy, under 400 mWh):
  https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/issues/6. A Waveshare collaborator
  promised a low-power example in March 2026 without a date. One user claims that a charge
  termination current of 25–50 mA lets the cell charge fully; with 25 mA the rest voltage rose
  by about 45 mV, a gain of a few percent (run U), so the termination does not explain the small
  capacity. The
  thread also corrects GPIO13 to the panel TE line (not a panel enable) and names ALDO2 as the
  panel supply switch, as in section 1.
- Board issues #7 (PCF85063 INT reaches only GPIO39), #9 (touch cannot wake from deep sleep) and
  #30 (AXP2101 IRQ not connected) agree with the schematic reading in this file; #17 (AXP2101
  IRQ on GPIO35) is disputed by Waveshare and contradicts the schematic.
- Community firmwares for this board (S3Watch and OLEDS3Watch by joaquimorg, bind by xunicatt,
  waveshare-watch-rs) publish no sleep current (checked 2026-11-10).
- LilyGo T-Watch S3 (ESP32-S3R8 + AXP2101), deep sleep 0.46–0.53 mA, `LilyGoWatch2022::sleep()`:
  https://github.com/Xinyuan-LilyGO/LilyGoLib (`src/LilyGoWatchS3.cpp`)
- LVGL click and scroll: https://github.com/lvgl/lvgl/issues/2600 (a scroll cancels CLICKED),
  https://github.com/lvgl/lvgl/issues/9005 (PRESSED/RELEASED during a scroll).
- Wear OS touch targets (48 × 48 dp): https://developer.android.com/training/wearables/accessibility
