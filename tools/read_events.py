#!/usr/bin/env python3
"""Reads the watch's event log (main/EventLog.*) over USB and prints it in words; a copy goes to
test-logs/. The watch must be in active mode (press PWR); the port is opened without a reset.

    python tools/read_events.py [--port /dev/ttyACM0]

A crash leaves a core dump in flash (Boot entries show "core dump"); read it with
    idf.py -p /dev/ttyACM0 coredump-info
"""

import argparse
import datetime
import os
import re
import sys
import time

import serial

TYPES = {1: 'boot', 2: 'active end', 3: 'always-on end', 4: 'power off', 5: 'always-on status'}
RESETS = ['unknown', 'power-on', 'external pin', 'software', 'PANIC', 'INTERRUPT WATCHDOG', 'TASK WATCHDOG',
          'WATCHDOG', 'deep sleep', 'BROWNOUT', 'SDIO', 'USB', 'JTAG', 'EFUSE', 'POWER GLITCH', 'CPU LOCKUP']
ACTIVE_ENDS = {1: 'display timeout', 2: 'PWR press'}
POWER_OFFS = {1: 'always-on off or no display', 2: 'no motion', 3: 'battery limit', 4: 'face failed'}
FLAGS = [(1, 'display'), (2, 'touch'), (4, 'portal'), (8, 'USB'), (16, 'CORE DUMP'), (32, 'no time'),
         (64, 'TOUCH RESET AGAIN')]
TOUCH_MODES = {0x00: 'active', 0x01: 'monitor', 0x03: 'sleep', 0xFF: 'no answer'}
# AXP2101 REG20 (power-on source) and REG21 (power-off source) bits.
PMU_ON = [(1, 'PWR key'), (2, 'IRQ pin'), (4, 'VBUS insert'), (8, 'battery charged'), (16, 'battery insert')]
PMU_OFF = [(1, 'PWR key hold'), (2, 'software'), (8, 'VSYS low'), (16, 'VBUS over voltage'),
           (32, 'DCDC under voltage'), (64, 'DCDC over voltage'), (128, 'over temperature')]


def words(fields):
    kind = int(fields['type'])
    code = int(fields['code'])
    flags = int(fields['flags'], 16)
    parts = [TYPES.get(kind, f'type {kind}')]
    if kind == 1:
        parts.append(RESETS[code] if code < len(RESETS) else f'reset {code}')
        parts.append(f'boot #{fields["wakes"]}')
        if 'pmu' in fields and fields['pmu'] != '0x00/0x00':
            on, off = (int(v, 16) for v in fields['pmu'].split('/'))
            sources = [name for bit, name in PMU_ON if on & bit] or [hex(on)]
            reasons = [name for bit, name in PMU_OFF if off & bit] or [hex(off)]
            parts.append(f'PMU on by {"+".join(sources)}, last off by {"+".join(reasons)}')
    elif kind == 2:
        parts.append(ACTIVE_ENDS.get(code, str(code)))
        parts.append(f'{fields["duration"]} s, {fields["touches"]} touches')
        start, end = (int(v, 16) for v in fields['touch'].split('/'))
        parts.append(f'touch controller {TOUCH_MODES.get(start, hex(start))} -> {TOUCH_MODES.get(end, hex(end))}')
        if int(fields['touches']) == 0 and int(fields['duration']) > 3:
            parts.append('NO TOUCHES')
    elif kind == 4:
        parts.append(POWER_OFFS.get(code, str(code)))
    if kind in (3, 4, 5) and fields['duration'] != '0':
        parts.append(f'always-on {fields["duration"]} min, longest wake {fields["longest"]}')
    names = [name for bit, name in FLAGS if flags & bit]
    parts.append(f'battery {fields["battery"]}')
    parts.append(f'heap min {fields["heapmin"]}, stack free main {fields["mainstack"]} LVGL {fields["lvglstack"]}')
    if names:
        parts.append('[' + ', '.join(names) + ']')
    return '; '.join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--port', default='/dev/ttyACM0')
    args = parser.parse_args()

    print('Waiting for the watch: press PWR.')
    # Linux raises DTR and RTS when it opens the port. pyserial's defaults keep both raised; lowering
    # DTR while RTS is still high is the USB-Serial-JTAG reset request, which would restart the watch
    # and clear its RTC memory.
    port = None
    # The command is repeated until the watch answers: a release build logs no "Active mode" line
    # to wait for. A line break first ends any partial line in the watch's input. The port goes
    # away whenever the watch sleeps; it is opened again when it returns.
    lines = []
    started = False
    start = time.time()
    last = 0
    while time.time() - start < 120:
        try:
            if port is None:
                port = serial.Serial(args.port, 115200, timeout=1)
            if not started and time.time() - last > 1:
                port.write(b'\nE\n')
                last = time.time()
            line = port.readline().decode(errors='replace').strip()
        except (serial.SerialException, OSError):
            if port is not None:
                port.close()
                port = None
            time.sleep(0.2)
            continue
        if line == 'EVENTS BEGIN':
            started = True
        if line.startswith('EVENT '):
            lines.append(line)
        if line == 'EVENTS END':
            break
    else:
        print('The watch did not answer within 2 min.')
        return 1
    os.makedirs('test-logs', exist_ok=True)
    path = os.path.join('test-logs', f'events-{datetime.datetime.now():%Y%m%d-%H%M%S}.log')
    with open(path, 'w') as log:
        for line in lines:
            log.write(line + '\n')
            match = re.match(r'EVENT (\S+ \S+|no time) (.*)', line)
            if not match:
                continue
            fields = dict(item.split('=', 1) for item in match.group(2).split())
            print(f'{match.group(1)}  {words(fields)}')
    print(f'{len(lines)} entries. Raw log: {path}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
