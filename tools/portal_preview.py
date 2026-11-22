#!/usr/bin/env python3
"""Serves the setup portal's pages (main/portal/) with a fake watch on localhost, for checking the
page in a browser without the watch, and takes the README's picture of it.

    python tools/portal_preview.py                  # http://127.0.0.1:8765/, Ctrl+C to stop
    python tools/portal_preview.py --shot docs/screenshots/portal.png
    python tools/portal_preview.py --shot portal-tr.png --language 1

The fake watch shows the fixed scene of the screenshots (Istanbul), never real settings, and the
version the firmware would get (git describe, as ESP-IDF). A save is printed, not applied. --shot
needs Chromium (headless) on the PATH.
"""

import argparse
import http.server
import json
import os
import subprocess
import sys
import threading

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'main', 'portal')
PORT = 8765
STATE = {
    'timeout': 10, 'alwaysOn': 1, 'brightness': 77, 'autoOff': 30, 'face': 0, 'offset': 180,
    'lat': 41.0082, 'lon': 28.9784, 'hijri': 0, 'aodDay': 25, 'aodNight': 2,
    'devMode': 0, 'usbStayOn': 1, 'nightMode': 1, 'isha': 17, 'language': 0, 'clockOrder': 0, 'clockHidden': 0, 'infoOrder': 0,
    'infoHidden': 0, 'settingsOrder': 0, 'settingsHidden': 0, 'mirror': 0, 'planetsOrder': 0, 'planetsHidden': 0,
    'pagesOrder': 0 | 1 << 3 | 4 << 6 | 5 << 9 | 2 << 12 | 3 << 15, 'toolsOrder': 0 | 3 << 3 | 4 << 6 | 5 << 9 | 1 << 12 | 2 << 15, 'toolsHidden': 1 << 6,
    'observingOrder': 1 | 2 << 3 | 3 << 6 | 4 << 9 | 5 << 12 | 6 << 15 | 0 << 18 | 7 << 21, 'observingHidden': 1 << 6 | 1 << 7,
    'bootCounter': 1,
    'scopeFocal': 1000, 'scopeAperture': 125, 'ep1Focal': 250, 'ep1Field': 52, 'ep2Focal': 100, 'ep2Field': 52,
    'ep3Focal': 0, 'ep3Field': 52, 'ep4Focal': 0, 'ep4Field': 52, 'camFocal': 24, 'camFNumber': 28, 'camSensor': 1,
    'baseShutter': 7, 'ndStops': 10, 'trailMinutes': 60, 'trailInterval': 30, 'satLon': 420,
    'version': 'preview', 'battery': 80, 'usb': 0, 'updateMin': 30, 'coreDump': 0,
    'zone': 'Europe/Istanbul', 'place': 'Istanbul',
    'cities': [['London', 'Europe/London'], ['New York', 'America/New_York']],
    'days': [['Trip home', '2026-07-03']],
    # Alarms::encode: a weekday clock alarm and "10 min before sunset", every day.
    'alarms': ['0|7|0|0|0|31|1', '1|0|0|1|-10|127|1'],
    # FaceConfig::defaults, as FaceConfig::encode writes them, and the catalogues' sizes.
    'faces': '1,0,1,0,6,2,4,0,9,2,0,1,4,0,0,0,0,0,0,0,0,17;2,0,0,0,6,0,0,0,9,2,8,1,4,0,0,0,0,0,0,0,0,80;0,0,1,0,0,2,5,0,9,2,8,1,4,0,0,0,0,0,0,0,0,209;1,1,1,0,7,12,0,0,2,2,13,1,5,0,0,0,0,0,0,0,0,25;0,0,0,0,0,0,2,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0/1,2,3,19,0,0,0,0',
    'faceItems': [20, 17],
    'faceDefaults': '1,0,1,0,6,2,4,0,9,2,0,1,4,0,0,0,0,0,0,0,0,17;2,0,0,0,6,0,0,0,9,2,8,1,4,0,0,0,0,0,0,0,0,80;0,0,1,0,0,2,5,0,9,2,8,1,4,0,0,0,0,0,0,0,0,209;1,1,1,0,7,12,0,0,2,2,13,1,5,0,0,0,0,0,0,0,0,25;0,0,0,0,0,0,2,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0/1,2,3,19,0,0,0,0',
}
# The emergency card (/api/card), a made-up one.
CARD = {'name': 'Ayşe Yılmaz', 'blood': 'A Rh+', 'allergies': 'Penisilin', 'medication': '', 'contact': 'Mehmet Yılmaz',
        'phone': '+90 555 000 00 00', 'note': ''}
FILES = {'/': ('index.html', 'text/html'), '/help': ('help.html', 'text/html'), '/tools': ('tools.html', 'text/html'),
         '/update': ('update.html', 'text/html'),
         '/cities.js': ('cities.js', 'text/javascript')}


class Handler(http.server.BaseHTTPRequestHandler):
    def send(self, body, kind):
        data = body.encode()
        self.send_response(200)
        self.send_header('Content-Type', kind + '; charset=utf-8')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split('?')[0]
        if path == '/api/state':
            self.send(json.dumps(STATE), 'application/json')
        elif path == '/api/card':
            self.send(json.dumps(CARD), 'application/json')
        elif path == '/api/ping':
            self.send('pong', 'text/plain')
        else:
            name, kind = FILES.get(path, FILES['/'])
            with open(os.path.join(ROOT, name), encoding='utf-8') as source:
                self.send(source.read(), kind)

    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        if self.path == '/api/update':
            print(f'Update: {len(body)} bytes (not applied)')
            self.send('Updated', 'text/plain')
            return
        print('Card:' if self.path == '/api/card' else 'Saved:', body.decode())
        self.send('Saved', 'text/plain')

    def log_message(self, format, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--shot', help='write a phone-sized picture of the page to this PNG and stop')
    parser.add_argument('--height', type=int, default=3000, help='picture height in pixels')
    parser.add_argument('--language', type=int, default=0, help="the watch's language: 0 English, 1 Turkish")
    parser.add_argument('--page', default='/', help='the page to picture, e.g. /help#tr, /tools?tr, /#settings')
    args = parser.parse_args()
    STATE['language'] = args.language
    # The name ESP-IDF gives the image: annotated tags only.
    describe = subprocess.run(['git', 'describe', '--always', '--dirty'], capture_output=True, text=True,
                              cwd=os.path.dirname(os.path.abspath(__file__)))
    if describe.returncode == 0:
        STATE['version'] = describe.stdout.strip()
    server = http.server.HTTPServer(('127.0.0.1', PORT), Handler)
    if not args.shot:
        print(f'http://127.0.0.1:{PORT}/')
        server.serve_forever()
        return 0
    threading.Thread(target=server.serve_forever, daemon=True).start()
    result = subprocess.run(['chromium', '--headless=new', '--disable-gpu', '--no-sandbox', '--hide-scrollbars',
                             f'--window-size=400,{args.height}', '--virtual-time-budget=4000',
                             f'--screenshot={os.path.abspath(args.shot)}', f'http://127.0.0.1:{PORT}{args.page}'],
                            capture_output=True, text=True, timeout=60)
    server.shutdown()
    print(args.shot if result.returncode == 0 else result.stderr)
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
