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

RTC_LINE = re.compile(r'RTC (\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d) UTC')

# A step is (kind, instruction, regular expression, timeout in seconds).
# kind ACT rings the terminal bell; an empty instruction keeps waiting without a new prompt.
# kind QUIET (shown as CHECK) passes when the pattern does NOT appear within the timeout.
# kind SEND_TIME sends the host's UTC time to the watch, then waits for the pattern.
# Every 'RTC ... UTC' line, logged at the RTC's second tick, gets the RTC - host difference.
TESTS = {
    # Off mode (docs/hardware.md, PWR key). Software power-off returns every chip
    # and AXP rail to its power-on default, so each power-on runs the full sleep preparation.
    'off': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'Quiesce done: 6/6 steps OK', 10),
        (WAIT, '', r'Power-on press \(REG27\)', 10),
        (ACT, 'Press PWR briefly, once, within 10 s.', r'Awake window restarted', 30),
        (WAIT, 'The watch powers off 10 s after the last press. Keep USB plugged in.',
         r'Powering off', 25),
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
    # (docs/hardware.md, open question 10).
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
    # Reads the RTC once against the host clock (drift over hours or days).
    'clock': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (WAIT, '', r'RTC \d{4}-', 10),
        (WAIT, 'Waiting for the power-off.', r'Powering off', 25),
    ],
    # Reads the battery drain log; the board must not be reset, or RTC memory is cleared.
    'battery': [
        (WAIT, 'Reading the battery log.', r'Battery log: \d+ of \d+ samples', 30),
        (WAIT, '', r'Entering deep sleep', 40),
    ],
}

NO_RESET_TESTS = {'battery'}


def printInstruction(kind, instruction):
    if kind == ACT:
        print(f'\n\a{ACT_STYLE}ACTION: {instruction}{RESET}')
    elif kind == QUIET:
        print(f'\n\a{ACT_STYLE}CHECK: {instruction}{RESET}')
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
        if kind == SEND_TIME:
            sendTime(reader)
        regex = re.compile(re.escape(pattern) if pattern.startswith('<') else pattern)
        start = time.time()
        matched = False
        while time.time() - start < timeout:
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
    start = time.time()
    while reader.serial is None and time.time() - start < 60:
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
