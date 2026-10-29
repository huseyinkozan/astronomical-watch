#!/usr/bin/env python3
"""Guided hardware test: tells the tester what to do and checks the serial log.

Each step shows an instruction and waits until an expected log line (or a USB
disconnect/reconnect) appears. Missed steps time out and the test continues.
The full log, with host timestamps, is saved under test-logs/.

Run inside the ESP-IDF environment, which provides pyserial:
    python tools/guided_test.py m1 [--port /dev/ttyACM0]
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

# A step is (kind, instruction, regular expression, timeout in seconds).
# kind ACT rings the terminal bell; an empty instruction keeps waiting without a new prompt.
TESTS = {
    'm1': [
        (WAIT, 'The board is being reset.', r'wake source: power-on/reset', 20),
        (ACT, 'Press PWR briefly, once, within 10 s.', r'GPIO10 -> 1', 30),
        (WAIT, 'The watch sleeps 10 s after the last press.', r'PWR key IRQ: short 1', 25),
        (WAIT, '', r'Entering deep sleep', 10),
        (WAIT, '', USB_DISCONNECTED, 10),
        (ACT, 'Press PWR briefly to wake the watch (1/3).', r'wake source: wake: PWR key', 40),
        (WAIT, 'Waiting for deep sleep.', r'Entering deep sleep', 25),
        (ACT, 'Press PWR briefly to wake the watch (2/3).', r'wake source: wake: PWR key', 40),
        (WAIT, 'Waiting for deep sleep.', r'Entering deep sleep', 25),
        (ACT, 'Press PWR briefly to wake the watch (3/3).', r'wake source: wake: PWR key', 40),
        (WAIT, 'Waiting for deep sleep.', r'Entering deep sleep', 25),
        (WAIT, '', USB_DISCONNECTED, 10),
        (ACT, 'Hold PWR down for about 8 s until the watch powers off, then release it.',
         r'wake source: wake: PWR key', 40),
        (WAIT, '', USB_DISCONNECTED, 20),
        (ACT, 'Press PWR briefly to power the watch on.', r'wake source: power-on/reset', 40),
        (WAIT, '', r'PWRON status', 10),
        (WAIT, 'Waiting for deep sleep.', r'Entering deep sleep', 25),
    ],
}


def printInstruction(kind, instruction):
    if kind == ACT:
        print(f'\n\a{ACT_STYLE}ACTION: {instruction}{RESET}')
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
        stamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
        self.logFile.write(f'{stamp} {line}\n')
        self.logFile.flush()
        with self.lock:
            self.lines.append(line)
        if line.startswith('<') or re.search(r'\b(main|sleep):', line):
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
                data = self.serial.read(4096)
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


def runSteps(reader, steps):
    results = []
    cursor = 0
    for index, (kind, instruction, pattern, timeout) in enumerate(steps, start=1):
        if instruction:
            printInstruction(kind, instruction)
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
        mark = f'{GREEN}OK{RESET}' if matched else f'{RED}TIMEOUT{RESET}'
        print(f'    [{index:2}] {mark} {pattern} ({elapsed:.1f} s)')
        results.append((index, pattern, matched))
    return results


def waitForBoard(reader):
    """A sleeping watch has no USB port: ask for a wake before the test resets it."""
    time.sleep(1.5)
    if reader.serial is not None:
        return True
    printInstruction(ACT, 'The watch is asleep. Press PWR briefly to wake it.')
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
        reader.resetRequested = not args.no_reset
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
