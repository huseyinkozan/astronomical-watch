#!/usr/bin/env python3
"""Guided hardware test: tells the tester what to do and checks the serial log.

Each step shows an instruction and waits until an expected log line (or a USB
disconnect/reconnect) appears. Missed steps time out and the test continues.
The full log, with host timestamps, is saved under test-logs/.

Run inside the ESP-IDF environment, which provides pyserial:
    python tools/guided_test.py off [--port /dev/ttyACM0]
"""

import argparse
import datetime
import os
import re
import subprocess
import sys
import threading
import time

import serial

USB_CONNECTED = '<usb connected>'
USB_DISCONNECTED = '<usb disconnected>'

ACT_STYLE = '\033[1;38;5;208m'   # bold orange: the tester must do something
WAIT_STYLE = '\033[1;36m'     # bold cyan: hands off
GREEN = '\033[32m'
RED = '\033[31m'
DIM = '\033[2m'
RESET = '\033[0m'

ACT = 'act'
WAIT = 'wait'
QUIET = 'quiet'
SEND_TIME = 'send_time'
ASK = 'ask'

RTC_LINE = re.compile(r'RTC (\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d) UTC')

# A step is (kind, instruction, regular expression, timeout in seconds).
# kind ACT rings the terminal bell and waits without a time limit, at the tester's pace; an empty
# instruction keeps waiting without a new prompt.
# kind QUIET (shown as CHECK) passes when the pattern does NOT appear within the timeout.
# kind SEND_TIME sends the host's UTC time to the watch, then waits for the pattern.
# kind ASK asks the tester a yes/no question (a visual check) and logs the answer.
# Every 'RTC ... UTC' line, logged at the RTC's second tick, gets the RTC - host difference.
TESTS = {
    # Off mode (docs/hardware.md, PWR key), build with sdkconfig.noaod: without the always-on
    # face the watch powers off when active mode ends. Software power-off returns every chip
    # and AXP rail to its power-on default, so each power-on runs the full sleep preparation.
    'off': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'Quiesce done: 6/6 steps OK', 10),
        (WAIT, '', r'Power-on press \(REG27\)', 10),
        (ACT, 'Press PWR briefly, once, within 10 s.', r'PWR press: active mode ends', 30),
        (WAIT, 'The watch powers off now. Keep USB plugged in.', r'Powering off', 10),
        (WAIT, '', USB_DISCONNECTED, 10),
        (QUIET, 'Do not touch for 5 min: the watch must stay off with USB plugged in (and charge).',
         USB_CONNECTED, 300),
        (QUIET, 'Tap PWR very briefly (well under half a second): the watch must stay off.',
         USB_CONNECTED, 15),
        (ACT, 'Press and hold PWR for about 1 s, then release it.', r'wake source: power-on/reset', 40),
        (WAIT, '', r'Quiesce done: 6/6 steps OK', 10),
        (WAIT, '', r'Power-on press \(REG27\) 0x15 -> 0x15', 10),
        (WAIT, '', r'PWROFF status \(REG21\)=0x02', 10),
        (WAIT, 'Waiting for the power-off.', r'Powering off', 25),
        (WAIT, '', USB_DISCONNECTED, 10),
    ],
    # Sets the RTC from the host clock and checks that it keeps time through off mode
    # (docs/hardware.md, PCF85063A section). Build with sdkconfig.noaod.
    'rtc': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'RTC \d{4}-', 10),
        (SEND_TIME, 'Setting the RTC from the host clock.', r'RTC set to .* ESP_OK', 10),
        (WAIT, 'The watch powers off 10 s after boot. Keep USB plugged in.', r'Powering off', 25),
        (WAIT, '', USB_DISCONNECTED, 10),
        (ACT, 'Wait about 1 min, then press PWR for about 1 s.', r'wake source: power-on/reset', 180),
        (WAIT, '', r'RTC \d{4}-.* OS 0', 10),
        (WAIT, 'Waiting for the power-off.', r'Powering off', 25),
    ],
    # Display basics (M4), build with sdkconfig.noaod. Only the screen needs watching until the
    # question comes.
    'display': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, 'Watch the SCREEN now (about 15 s): time with running seconds, date with UTC and '
         'battery. The question follows afterwards.', r'Display started', 20),
        (WAIT, '', r'Powering off', 30),
        (WAIT, '', USB_DISCONNECTED, 20),
        (ASK, 'Did the screen show the time with running seconds, the date with UTC and the battery?',
         '', 0),
    ],
    # Always-on face (M5): the image must stay while the ESP32 deep-sleeps, and change each minute
    # (docs/hardware.md, open question 4). Only the screen needs watching until the questions.
    'aod': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, 'Watch the SCREEN for about 3 minutes. After about 10 s the active face is replaced '
         'by a dim HH:MM (UTC) in large segments; it should change at each new minute and move by '
         'a few pixels. The questions follow afterwards.', r'Display started', 20),
        (WAIT, '', r'Entering deep sleep', 30),
        (QUIET, '', r'Guru Meditation|abort\(\)', 180),
        (ASK, 'Did the dim HH:MM appear and stay on the screen between the minutes?', '', 0),
        (ASK, 'Did it change at each new minute without the screen going black or flashing?', '', 0),
        (ASK, 'Did it move by a few pixels from one minute to the next?', '', 0),
        (ACT, 'Press PWR briefly to wake the watch.', r'wake source: wake: PWR key', 60),
        (WAIT, '', r'Last timer wake', 10),
        (WAIT, 'Watch the screen: the active face should come back.', r'Display started', 20),
        (ASK, 'Did the active face (time with seconds, date, battery) come back?', '', 0),
    ],
    # Reads the RTC once against the host clock (drift over hours or days). Build with
    # sdkconfig.noaod.
    'clock': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'RTC \d{4}-', 10),
        (WAIT, 'Waiting for the power-off.', r'Powering off', 25),
    ],
    # Motion measurement (M6, docs/hardware.md open question 3), build with sdkconfig.motionlog:
    # one accelerometer reading per always-on minute, 30 min on the desk, then 60 min worn.
    'motion': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, 'Put the watch on the desk, screen up, with USB plugged in. Do not touch it or the '
         'desk from now on.', r'Motion x=', 60),
        (QUIET, 'Hands off for 30 min (watch and desk).', r'Motion sample failed|Guru Meditation|abort\(\)',
         1800),
        (ACT, 'Put the watch on your wrist, then press PWR briefly.', r'Motion log: \d+', 300),
        (ACT, 'Unplug USB and wear the watch for about 60 min with normal use: sitting, typing, '
         'walking, and a while with the arm still. Then plug USB in and press PWR briefly.',
         r'wake source: wake: PWR key', 4800),
        (WAIT, '', r'Motion log: \d+', 30),
    ],
    # Motion power-off (M6), build with sdkconfig.motionquick: a check every minute, power-off
    # after 3 min without motion.
    'motioncheck': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, 'Leave the watch on the desk, screen up, with USB plugged in. Do not touch it.',
         r'Motion reference', 60),
        (WAIT, 'It should power off in about 4 min.', r'still for 3 checks', 300),
        (WAIT, '', r'Powering off', 30),
        (ACT, 'Pick the watch up (USB stays in) and press PWR for about 1 s. Then hold it in your '
         'hand and turn it a little every 20 s until told otherwise.',
         r'wake source: power-on/reset', 900),
        (WAIT, '', r'Motion reference', 60),
        (WAIT, '', r'change \d{2,} mg, still for 0 checks', 200),
        (QUIET, 'Keep turning it a little every 20 s for 4 min: it must stay on.', r'Powering off', 240),
        (ACT, 'Put the watch on the desk and leave it: it should power off within about 5 min.',
         r'Powering off', 400),
    ],
    # Short second half of 'motioncheck' (same build): motion keeps the watch on, then it powers
    # off on the desk. The tester is needed for about 2 min only.
    'motionhand': [
        (ACT, 'Press PWR briefly if you have not yet. Then hold the watch in your hand (USB stays in) '
         'and turn it a little every 20 s until told to put it down.', r'Motion reference', 90),
        (WAIT, '', r'change \d{2,} mg, still for 0 checks', 120),
        (ACT, 'Put the watch on the desk and leave it; you can go. It should power off within about '
         '4 min.', r'Powering off', 360),
    ],
    # Active mode (M7): display timeout, touch coordinates, pages and the PWR press.
    'active': [
        (WAIT, 'The board is being reset. Do not touch the screen.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'Display timeout: active mode ends', 30),
        (ACT, 'Press PWR briefly. Then touch the screen about 1 cm from each corner, one after the '
         'other: top left, top right, bottom right, bottom left.', r'Touch at', 300),
        (WAIT, '', r'Touch at', 30),
        (WAIT, '', r'Touch at', 30),
        (WAIT, '', r'Touch at', 30),
        (WAIT, 'Do not touch: always-on should follow in about 10 s.', r'Display timeout', 30),
        (ACT, 'Press PWR briefly. Swipe left: the battery page should appear. Then swipe right: '
         'the watch face should come back.', r'Touch at', 300),
        (WAIT, '', r'Touch at', 60),
        (ASK, 'Did the battery page (Battery, percent, mV, charger state) appear, and did the watch '
         'face come back?', '', 0),
        (ACT, 'Press PWR briefly. When the face appears, tap the screen every 3-4 s.',
         r'Active mode, display timeout', 300),
        (QUIET, 'Keep tapping every 3-4 s for 20 s: the display must stay on.', r'Display timeout', 20),
        (WAIT, 'Stop tapping: always-on should follow within 10 s.', r'Display timeout', 15),
        (ACT, 'Press PWR briefly. When the face appears, press PWR briefly again.',
         r'Active mode, display timeout', 300),
        (WAIT, '', r'PWR press: active mode ends', 30),
        (ASK, 'Did the dim always-on face appear at once after the second press?', '', 0),
    ],
    # Settings page (M8), default build: every tap on a row steps its value, the values are
    # saved in NVS when active mode ends and survive off mode and deep sleep. Starts from the
    # defaults (10 s, On, Mid, 30 min) and ends with them restored.
    'settings': [
        (WAIT, 'The board is being reset. Do not touch the screen.', r'Settings: timeout', 20),
        (WAIT, '', r'Display timeout: active mode ends', 30),
        (ACT, 'Press PWR briefly. Swipe left twice to the settings page. If the screen goes dim '
         'later, press PWR and swipe back.', r'Touch at', 0),
        (ACT, 'Tap "Screen off" 10 times, slowly.', r'Setting Screen off', 0),
    ] + [(ACT, '', r'Setting Screen off', 0)] * 9 + [
        (ACT, 'Tap "Brightness" 3 times, slowly. Watch the brightness.', r'Setting Brightness', 0),
        (ACT, '', r'Setting Brightness', 0),
        (ACT, '', r'Setting Brightness', 0),
        (ACT, 'Tap "Auto off" 4 times.', r'Setting Auto off', 0),
        (ACT, '', r'Setting Auto off', 0),
        (ACT, '', r'Setting Auto off', 0),
        (ACT, '', r'Setting Auto off', 0),
        (ACT, 'Tap "Always-on" 2 times.', r'Setting Always-on', 0),
        (ACT, '', r'Setting Always-on', 0),
        (ASK, 'Did each tap change the value, and the brightness?', '', 0),
        (ACT, 'If the screen is dim: press PWR, swipe to the settings. '
         'Tap "Screen off" until it shows 5 s.', r'Setting Screen off: 5 s', 0),
        (ACT, 'Tap "Always-on" once (Off). Then do not touch.', r'Setting Always-on: Off', 0),
        (WAIT, '', r'Display timeout: active mode ends', 20),
        (WAIT, '', r'Settings saved \(\d changed\): ESP_OK', 10),
        (WAIT, 'The watch powers off now.', r'Powering off', 15),
        (ACT, 'Press PWR for about 1 s. Do not touch the screen.', r'wake source: power-on/reset', 0),
        (WAIT, '', r'Settings: timeout 5 s, always-on 0', 10),
        (WAIT, '', r'Active mode, display timeout 5 s', 20),
        (WAIT, '', r'Display timeout: active mode ends', 15),
        (WAIT, '', r'Powering off', 15),
        (ACT, 'Press PWR for about 1 s. Swipe to the settings. Tap "Screen off" once (10 s).',
         r'Setting Screen off: 10 s', 0),
        (ACT, 'Tap "Always-on" once (On). Then do not touch.', r'Setting Always-on: On', 0),
        (WAIT, '', r'Settings saved \(2 changed\): ESP_OK', 30),
        (WAIT, '', r'Entering deep sleep', 15),
        (ACT, 'Press PWR briefly.', r'Settings: timeout 10 s, always-on 1', 0),
        (WAIT, 'Do not touch.', r'Display timeout: active mode ends', 20),
    ],
    # Reads the battery drain log; the board must not be reset, or RTC memory is cleared.
    'battery': [
        (WAIT, 'Reading the battery log.', r'Battery log: \d+ of \d+ samples', 30),
        (WAIT, '', r'Entering deep sleep', 40),
    ],
}

NO_RESET_TESTS = {'battery', 'motionhand'}


def notify(title, instruction):
    """Desktop notification for the tester away from the terminal. It stays 2 min, longer than
    the short USB plug notifications, and then closes by itself so that old ones do not pile up."""
    try:
        subprocess.run(['notify-send', '-t', '120000', '-a', 'Watch test', title, instruction],
                       timeout=5, check=False)
    except (OSError, subprocess.TimeoutExpired):
        pass


def printInstruction(kind, instruction):
    if kind != WAIT:
        notify({ACT: 'Action', QUIET: 'Check', ASK: 'Question'}.get(kind, 'Watch test'), instruction)
    if kind == ACT:
        print(f'\n\a{ACT_STYLE}ACTION: {instruction}{RESET}')
    elif kind == QUIET:
        print(f'\n\a{ACT_STYLE}CHECK: {instruction}{RESET}')
    elif kind == ASK:
        print(f'\n\a{ACT_STYLE}QUESTION: {instruction}{RESET}')
    else:
        print(f'\n{WAIT_STYLE}WAIT - do not touch: {instruction}{RESET}')


class SerialReader(threading.Thread):
    """Reads the port, survives USB re-enumeration and records every line."""

    def __init__(self, port, logFile):
        super().__init__(daemon=True)
        self.port = port
        self.logFile = logFile
        self.lines = []
        self.lock = threading.Lock()
        self.stopped = False
        self.serial = None
        self.resetRequested = False

    def emit(self, line):
        now = datetime.datetime.now(datetime.timezone.utc)
        stamp = now.astimezone().strftime('%H:%M:%S.%f')[:-3]
        match = RTC_LINE.search(line)
        if match:
            rtc = datetime.datetime(*map(int, match.groups()), tzinfo=datetime.timezone.utc)
            line += f' [RTC - host: {(rtc - now).total_seconds():+.3f} s]'
        self.logFile.write(f'{stamp} {line}\n')
        self.logFile.flush()
        with self.lock:
            self.lines.append(line)
        if line.startswith('<') or re.search(r'\b(main|sleep|rtc):', line):
            print(f'{DIM}    {line}{RESET}')

    def run(self):
        pending = ''
        while not self.stopped:
            try:
                if self.serial is None:
                    self.serial = serial.Serial(self.port, 115200, timeout=0.1)
                    self.emit(USB_CONNECTED)
                if self.resetRequested:
                    self.resetRequested = False
                    self.serial.dtr = False
                    self.serial.rts = True
                    time.sleep(0.1)
                    self.serial.rts = False
                # Return as soon as anything arrives: a full read waits for the 0.1 s timeout,
                # which would delay the host timestamps of the RTC lines.
                data = self.serial.read(self.serial.in_waiting or 1)
                if data:
                    pending += data.decode(errors='replace')
                    while '\n' in pending:
                        line, pending = pending.split('\n', 1)
                        self.emit(line.rstrip('\r'))
            except (serial.SerialException, OSError):
                if self.serial is not None:
                    try:
                        self.serial.close()
                    except OSError:
                        pass
                    self.serial = None
                    pending = ''
                    self.emit(USB_DISCONNECTED)
                time.sleep(0.05)


def sendTime(reader):
    """Sends second S at S + 0.48 s: the watch polls every 20 ms and its RTC ticks 0.508 s after
    it is set, so the RTC's next second lands on the host's."""
    now = time.time()
    target = int(now) + 0.48
    if target <= now:
        target += 1
    time.sleep(target - now)
    second = datetime.datetime.fromtimestamp(int(target), datetime.timezone.utc)
    if reader.serial is not None:
        reader.serial.write(f'T {second:%Y-%m-%d %H:%M:%S}\n'.encode())


def runSteps(reader, steps):
    results = []
    cursor = 0
    for index, (kind, instruction, pattern, timeout) in enumerate(steps, start=1):
        if instruction:
            printInstruction(kind, instruction)
        if kind == ASK:
            answer = input('    y/n: ').strip().lower()
            passed = answer.startswith('y')
            reader.emit(f'<answer: {answer or "-"}: {instruction}>')
            mark = f'{GREEN}YES{RESET}' if passed else f'{RED}NO{RESET}'
            print(f'    [{index:2}] {mark}')
            results.append((index, instruction, passed))
            continue
        if kind == SEND_TIME:
            sendTime(reader)
        regex = re.compile(re.escape(pattern) if pattern.startswith('<') else pattern)
        start = time.time()
        matched = False
        while kind == ACT or time.time() - start < timeout:
            with reader.lock:
                lines = reader.lines[cursor:]
            for offset, line in enumerate(lines):
                if regex.search(line):
                    cursor += offset + 1
                    matched = True
                    break
            if matched:
                break
            time.sleep(0.05)
        elapsed = time.time() - start
        if kind == QUIET:
            passed = not matched
            mark = f'{GREEN}OK{RESET}' if passed else f'{RED}UNEXPECTED{RESET}'
            print(f'    [{index:2}] {mark} no {pattern} ({elapsed:.1f} s)')
        else:
            passed = matched
            mark = f'{GREEN}OK{RESET}' if passed else f'{RED}TIMEOUT{RESET}'
            print(f'    [{index:2}] {mark} {pattern} ({elapsed:.1f} s)')
        results.append((index, pattern, passed))
    return results


def waitForBoard(reader):
    """A sleeping or switched-off watch has no USB port: ask for a press before the reset."""
    time.sleep(1.5)
    if reader.serial is not None:
        return True
    printInstruction(ACT, 'The watch is asleep or off. Press PWR for about 1 s.')
    while reader.serial is None:
        time.sleep(0.05)
    return reader.serial is not None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('test', choices=sorted(TESTS))
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--no-reset', action='store_true', help='do not reset the board at the start')
    args = parser.parse_args()

    os.makedirs('test-logs', exist_ok=True)
    logPath = os.path.join('test-logs', f'{args.test}-{datetime.datetime.now():%Y%m%d-%H%M%S}.log')
    print(f'Test {args.test}: {ACT_STYLE}ACTION{RESET} = do it now (bell), '
          f'{WAIT_STYLE}WAIT{RESET} = do not touch.')
    input('Press Enter when you are ready to start...')
    with open(logPath, 'w') as logFile:
        reader = SerialReader(args.port, logFile)
        reader.resetRequested = not args.no_reset and args.test not in NO_RESET_TESTS
        reader.start()
        try:
            if waitForBoard(reader):
                results = runSteps(reader, TESTS[args.test])
            else:
                print(f'{RED}No USB connection; check the cable and the port.{RESET}')
                results = []
        except KeyboardInterrupt:
            print('\nInterrupted.')
            results = []
        reader.stopped = True
        time.sleep(0.2)

    passed = sum(1 for result in results if result[2])
    print(f'\n{passed}/{len(results)} steps passed. Log: {logPath}')
    return 0 if results and passed == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
