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
| 10 | PWR key state (SYS_OUT) | BSS138 inverter, 10k pull-up to VCC3V3, **HIGH while pressed** [SCH] | **yes** |
| 11 / 12 | QSPI SCLK / LCD_CS | [SCH] | – |
| 13 | LCD_TE | [SCH] | – |
| 14 / 15 | I2C SCL / SDA (shared) | 2.2k pull-ups to **VCC3V3**; BSP: port 1, 400 kHz [SCH][REF] | – |
| 16 / 40 / 41 / 42 / 45 | I2S MCLK / DSDIN / SCLK / ASDOUT / LRCK | [SCH] | – |
| 18 | Vibration motor | NPN low-side, motor supply = ALDO3 [SCH] | – |
| 19 / 20 | USB D− / D+ | native USB-Serial-JTAG [SCH] | – |
| **21** | **QMI8658 INT1** | push-pull when enabled [SCH][DS] | **yes** |
| 38 | Touch INT | [SCH] | **no** (light sleep only) |
| 39 | PCF85063 INT | open-drain [SCH][DS] | **no** (light sleep only) |
| 43 / 44 | UART0 TX / RX | [SCH] | – |
| 46 | Speaker amplifier enable (PA_CTRL) | [SCH] | – |

I2C devices on the shared bus [SCH][REF]: FT3168 touch `0x38`, PCF85063 `0x51`, QMI8658 `0x6B`,
AXP2101 `0x34`, ES8311 codec, ES7210 ADC.

Not connected to the ESP32 [SCH]: QMI8658 INT2 (test point TP15 only), AXP2101 IRQ (pulled up to
VCC-RTC; official example uses `CONFIG_PMU_INTERRUPT_PIN=-1`), PCF85063 CLKOUT.

## 2. Parts and Power Tree

- MCU: **ESP32-S3R8** (8 MB octal PSRAM in package). No 32 kHz crystal on the ESP32 [SCH].
- Chip revision v0.2 [HW].
- Flash: **GD25Q256, 32 MB** [SCH]; detected at boot as GigaDevice, 32768 KiB, running in DIO mode
  [HW]. **Known deviation:** the project (like the Waveshare examples)
  configures 16 MB, because addresses above 16 MB need 32-bit flash addressing. Revisit only if the
  firmware or data ever needs more than 16 MB.
- AXP2101 PMIC rails [SCH]:

| Rail | Net | Loads | Rule |
|---|---|---|---|
| DCDC1 | VCC3V3 | ESP32, flash, QMI8658, AMOLED VCI/VDDIO, touch, SD card, ES8311 digital, I2C pull-ups | always on |
| ALDO1 | A3V3 | ES8311 AVDD and **all of ES7210** (which sits on the shared I2C bus) | **keep on**: an unpowered ES7210 hangs the I2C bus |
| ALDO2 | — | pull-up of DSI_PWR_EN (AMOLED panel power enable) | off when the panel is off [?] |
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
  (12.5 pF); the correct value for Y1 + C22/C23 is [?] → compare drift over several days.
- Control_2 `0x01`: AIE(7) AF(6) MI(5) HMI(4) TF(3) COF[2:0]. Flags clear by writing 0.
  **CLKOUT runs at 32.768 kHz from power-on (COF = 000); disable with COF = 111** [DS §7.2.2.4].
  The Waveshare component never disables it, and `enable_alarm` rewrites the whole register [REF].
- Timer mode `0x11`: when unused, keep TCF = 1/60 Hz for lowest power [DS].
- Supply current ≈ 220 nA typical with CLKOUT off [DS Table 40].

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

### Display (CO5300 AMOLED, 410 × 502, QSPI)
- The BSP drives it with the `esp_lcd_sh8601` driver: RGB565, x-gap 22, QSPI 40 MHz; it offers
  display on/off (0x29/0x28) and brightness (0x51) only. `bsp_display_start()` always resets the panel [REF].
- CO5300 commands: 0x28 display off, 0x10 sleep in, 0x11 sleep out (wait 120 ms; must precede 0x29),
  0x39 idle mode, 0x4F deep standby, 0x51 brightness [REF].
- Whether the image is kept while the ESP32 deep-sleeps with the panel powered is [?]
  (needed for always-on). The CO5300 datasheet was not reviewed (file too large to fetch).

### FT3168 Touch (`0x38`)
- Register 0xA5 power mode: 0 active, 1 monitor, 2 standby, 3 hibernate (wake needs TP_RESET) [REF].
- Current: active 1.5 mA, monitor 30 µA, sleep 10 µA [DS Table 3-2].

### ESP32-S3
- EXT0/EXT1 wake only on RTC GPIOs 0–21; EXT1 supports ANY_HIGH [DS].
- Deep sleep ≈ 7–8 µA [DS Table 5-10]. Each wake from deep sleep is a full boot.
- Reset to `app_main` takes about 255 ms with the default configuration (log timestamps, 160 MHz,
  DIO flash, INFO logging, image validation on) [HW]. This is the baseline for wake latency.
- The internal RC slow clock drifts seconds per day: use it only as a wake timer; the PCF85063 is the clock.
- The native USB-Serial-JTAG connection drops while the chip is in deep sleep.

## 4. Open Questions

1. Battery thermistor: the schematic has RP2 on TS, but the official AXP example disables TS
   ("no thermistor"). Check the board.
2. Does the display work with ALDO2 off? Read REG90 at boot.
3. QMI8658C CTRL9 handshake: does CTRL8.bit7 = 1 keep CmdDone off INT1 on this chip, and does a
   STATUS1 read alone acknowledge it (C datasheet) as well as CTRL9 = `0x00` (A datasheet)?
4. Can accel samples be read while WoM is active (needed for a wrist-pose check on wake)?
5. Does the CO5300 keep its image while the ESP32 deep-sleeps with the panel powered?
6. Correct PCF85063 CAP_SEL for this crystal.
7. ES8311/ES7210 standby current at power-on defaults.

## 5. Sources

- Schematic: Waveshare repository, `Schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf`
  (https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06)
- Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06
- QMI8658A datasheet Rev A: https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf
- QMI8658C datasheet Rev 0.6: Waveshare wiki, Resources section
- PCF85063A datasheet: https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf
- AXP2101 datasheet: https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf
- CO5300 datasheet: https://files.waveshare.com/wiki/common/Co5300_Datasheet.pdf
- FT3168 datasheet: https://files.waveshare.com/wiki/common/FT3168.pdf
- ESP32-S3 sleep modes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html
- SensorLib (QMI8658, PCF85063): https://github.com/lewisxhe/SensorLib
- XPowersLib (AXP2101): https://github.com/lewisxhe/XPowersLib
- Rust watch firmwares for this board (reference only): waveshare-watch-rs (infinition) and its
  fork by bourumir-wyngs
