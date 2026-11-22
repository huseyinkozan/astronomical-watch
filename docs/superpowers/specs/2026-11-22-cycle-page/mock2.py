# Second round of cycle page mock-ups: the ring with the Moon's phases, a Moon dial, the night
# mode's patterns and the always-on corner and icon.
import math, sys, datetime
from PIL import Image, ImageDraw, ImageFont
FONTS, OUT = sys.argv[1], sys.argv[2]
AOD = sys.argv[3] if len(sys.argv) > 3 else None
W, H = 410, 502
F26 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 26)
F20 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 20)
F18 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 18)
F72 = ImageFont.truetype(FONTS + '/JetBrainsMono-Light.ttf', 72)
WHITE = (255, 255, 255); GREY = (160, 160, 160); DIM = (70, 70, 70); DARK = (40, 40, 40)
RED = (255, 80, 100); BLUE = (90, 150, 255); BLUE_LIGHT = (150, 200, 255)
NRED = (255, 40, 30); NRED_DIM = (110, 18, 14)  # night mode: one red, two levels

START = datetime.date(2026, 11, 20)   # the last period's start
TODAY = datetime.date(2026, 12, 1)    # cycle day 12
LENGTH, PERIOD, OV = 28, 5, 14
FERTILE = (9, 17)                     # Ogino-Knaus over cycles of 26-31 days (26-18+1 .. 31-11)

def phase(date):
    # The Moon's age in days from a known new moon (2000-01-06 18:14 UTC); a mock-up's precision.
    days = (datetime.datetime(date.year, date.month, date.day, 12) - datetime.datetime(2000, 1, 6, 18, 14)).total_seconds() / 86400
    return (days % 29.530589) / 29.530589

def moon(d, x, y, r, p, lit, dark):
    # A phase disc: the lit part as an ellipse's half, waxing on the right.
    d.ellipse([x - r, y - r, x + r, y + r], fill=dark)
    k = math.cos(2 * math.pi * p)  # 1 new, -1 full
    pts = []
    for i in range(41):
        t = -math.pi / 2 + math.pi * i / 40
        pts.append((x + r * math.cos(t) * (1 if p < 0.5 else -1), y + r * math.sin(t)))
    for i in range(41):
        t = math.pi / 2 - math.pi * i / 40
        pts.append((x + r * k * math.cos(t) * (1 if p < 0.5 else -1), y + r * math.sin(t)))
    d.polygon(pts, fill=lit)

def base(title, night=False):
    im = Image.new('RGB', (W, H), (0, 0, 0))
    d = ImageDraw.Draw(im)
    c = NRED_DIM if night else DARK
    for (cx, cy, a0) in [(120, 120, 180), (W - 121, 120, 270), (120, H - 121, 90), (W - 121, H - 121, 0)]:
        d.arc([cx - 117, cy - 117, cx + 117, cy + 117], a0, a0 + 90, fill=c, width=5)
    tw = d.textlength(title, font=F26)
    d.text(((W - tw) / 2, 2), title, font=F26, fill=NRED if night else WHITE)
    return im, d

def centre(d, y, text, font=F26, fill=WHITE):
    d.text(((W - d.textlength(text, font=font)) / 2, y), text, font=font, fill=fill)

def kind(day):
    if day <= PERIOD: return 'period'
    if day == OV: return 'ovulation'
    if FERTILE[0] <= day <= FERTILE[1]: return 'fertile'
    return 'low'

def segment(d, cx, cy, r0, r1, a0, a1, style, colour):
    # An annular sector, filled, hatched (radial lines) or outlined.
    pts = [(cx + r1 * math.cos(math.radians(a0 + (a1 - a0) * i / 8)), cy + r1 * math.sin(math.radians(a0 + (a1 - a0) * i / 8))) for i in range(9)]
    pts += [(cx + r0 * math.cos(math.radians(a1 - (a1 - a0) * i / 8)), cy + r0 * math.sin(math.radians(a1 - (a1 - a0) * i / 8))) for i in range(9)]
    if style == 'fill':
        d.polygon(pts, fill=colour)
    else:
        d.polygon(pts, outline=colour)
        if style == 'hatch':
            for f in (0.33, 0.66):
                a = math.radians(a0 + (a1 - a0) * f)
                d.line([(cx + r0 * math.cos(a), cy + r0 * math.sin(a)), (cx + r1 * math.cos(a), cy + r1 * math.sin(a))], fill=colour, width=2)

def style(day, night):
    k = kind(day)
    future = day > (TODAY - START).days + 1
    if night:
        # Night: one red; the period filled, the fertile days hatched, the others outlined.
        return {'period': 'fill', 'ovulation': 'fill', 'fertile': 'hatch', 'low': 'outline'}[k], NRED if not (k == 'low') else NRED_DIM
    colour = {'period': RED, 'ovulation': BLUE_LIGHT, 'fertile': BLUE, 'low': DIM}[k]
    # Logged days filled; predicted days (the future) outlined or hatched.
    if k == 'period': return ('fill' if not future else 'outline'), colour
    if k in ('fertile', 'ovulation'): return 'hatch', colour
    return ('fill' if not future else 'outline'), colour

# A. The ring of the cycle's days, each with the Moon's phase of that day inside.
def ring_moons(night=False):
    im, d = base('Döngü', night)
    cx, cy = W // 2, 222
    today = (TODAY - START).days + 1
    for day in range(1, LENGTH + 1):
        a0 = -90 + (day - 1) * 360 / LENGTH + 1.2
        a1 = -90 + day * 360 / LENGTH - 1.2
        s, c = style(day, night)
        segment(d, cx, cy, 128, 146, a0, a1, s, c)
        am = math.radians(-90 + (day - 0.5) * 360 / LENGTH)
        date = START + datetime.timedelta(days=day - 1)
        lit = NRED if night else (WHITE if day == today else (200, 200, 210))
        moon(d, cx + 112 * math.cos(am), cy + 112 * math.sin(am), 6.5, phase(date), lit, NRED_DIM if night else (35, 35, 40))
    am = math.radians(-90 + (today - 0.5) * 360 / LENGTH)
    px, py = cx + 156 * math.cos(am), cy + 156 * math.sin(am)
    col = NRED if night else WHITE
    d.polygon([(px, py), (px + 10 * math.cos(am + 2.7), py + 10 * math.sin(am + 2.7)), (px + 10 * math.cos(am - 2.7), py + 10 * math.sin(am - 2.7))], fill=col)
    centre(d, cy - 74, '%d.' % today, F72, col)
    centre(d, cy + 4, 'gün', F26, NRED if night else GREY)
    centre(d, cy + 34, 'doğurgan', F18, NRED if night else BLUE)
    centre(d, cy + 54, 'tahmini', F18, NRED if night else GREY)
    centre(d, 392, 'Adet 15-21 Ara', F26, NRED if night else RED)
    centre(d, 428, 'Tahmindir; doğum kontrolü', F18, NRED if night else (255, 176, 64))
    centre(d, 450, 've gebelik için kullanmayın', F18, NRED if night else (255, 176, 64))
    return im

# B. A Moon dial: the ring is the Moon's month (new at the top, full at the bottom); the cycles
# are arcs inside it from their start's phase, today's outermost, older ones inside.
def moon_dial():
    im, d = base('Döngü')
    cx, cy = W // 2, 222
    for i in range(30):
        p = i / 30
        a = math.radians(-90 + p * 360)
        moon(d, cx + 150 * math.cos(a), cy + 150 * math.sin(a), 6, p, (200, 200, 210), (35, 35, 40))
    # Past cycles (start, length) as arcs: one turn is 29.5 days, so a shorter cycle ends early.
    cycles = [(datetime.date(2026, 6, 1), 29), (datetime.date(2026, 6, 30), 27), (datetime.date(2026, 7, 27), 28),
              (datetime.date(2026, 8, 24), 31), (datetime.date(2026, 9, 24), 26), (datetime.date(2026, 10, 20), 31),
              (START, LENGTH)]
    for i, (start, length) in enumerate(cycles):
        r = 70 + i * 9
        p0 = phase(start)
        last = i == len(cycles) - 1
        for day in range(1, length + 1):
            a0 = -90 + (p0 + (day - 1) / 29.53) * 360
            a1 = a0 + 360 / 29.53 - 1
            if last and day > (TODAY - START).days + 1:
                c = (60, 60, 60) if kind(day) == 'low' else (RED if kind(day) == 'period' else BLUE)
                d.arc([cx - r, cy - r, cx + r, cy + r], a0, a1, fill=c, width=2)
                continue
            c = RED if day <= 5 else ((120, 120, 120) if last else (70, 70, 70))
            if last and kind(day) in ('fertile', 'ovulation'): c = BLUE
            d.arc([cx - r, cy - r, cx + r, cy + r], a0, a1, fill=c, width=6)
    today = (TODAY - START).days + 1
    a = math.radians(-90 + (phase(START) + (today - 0.5) / 29.53) * 360)
    r = 70 + 6 * 9
    d.ellipse([cx + r * math.cos(a) - 6, cy + r * math.sin(a) - 6, cx + r * math.cos(a) + 6, cy + r * math.sin(a) + 6], outline=WHITE, width=2)
    centre(d, cy - 26, '12. gün', F26, WHITE)
    centre(d, cy + 6, 'Ay 21 günlük', F18, GREY)
    centre(d, 392, 'Adet 15-21 Ara', F26, RED)
    centre(d, 428, 'halka: Ay\'ın bir ayı;', F18, GREY)
    centre(d, 450, 'yaylar: son 7 döngü', F18, GREY)
    return im

# C. The legend of the patterns, by day and by night.
def legend():
    im, d = base('Desenler')
    rows = [('period', 'Adet (girilen)'), ('period-future', 'Adet (tahmini)'), ('fertile', 'Doğurgan (tahmini)'),
            ('low', 'Düşük olasılık')]
    y = 70
    for name, label in rows:
        for col, night in ((40, False), (110, True)):
            if name == 'period': s, c = 'fill', (NRED if night else RED)
            elif name == 'period-future': s, c = 'outline', (NRED if night else RED)
            elif name == 'fertile': s, c = 'hatch', (NRED if night else BLUE)
            else: s, c = 'outline', (NRED_DIM if night else DIM)
            pts = [(col, y), (col + 50, y), (col + 50, y + 34), (col, y + 34)]
            if s == 'fill': d.polygon(pts, fill=c)
            else:
                d.polygon(pts, outline=c)
                if s == 'hatch':
                    for k in range(1, 4): d.line([(col + k * 12, y), (col + k * 12, y + 34)], fill=c, width=2)
        d.text((180, y + 4), label, font=F20, fill=WHITE)
        y += 56
    centre(d, 300, 'sol: gündüz   sağ: gece', F20, GREY)
    centre(d, 340, 'Gece tek kırmızı: anlamı', F20, GREY)
    centre(d, 366, 'dolu / çizgili / boş taşır.', F20, GREY)
    return im

# D. The always-on face with the cycle gauge (bottom left) and the icon (in idle colours: 2 px,
# no channel; the icon one-bit).
def always_on():
    im = Image.open(AOD).convert('RGB')
    d = ImageDraw.Draw(im)
    cx, cy = 120, H - 121
    d.rectangle([0, H - 125, 125, H], fill=(0, 0, 0))
    today = (TODAY - START).days + 1
    for day in range(1, LENGTH + 1):
        a0 = 180 - (day - 1) * 90 / LENGTH
        a1 = 180 - day * 90 / LENGTH
        if day > today: continue
        k = kind(day)
        c = (255, 0, 0) if k == 'period' else ((0, 0, 255) if k in ('fertile', 'ovulation') else (255, 255, 255))
        d.arc([cx - 115, cy - 115, cx + 115, cy + 115], a1, a0, fill=c, width=2)
    # Notches at both ends and the middle, as the always-on gauges have.
    for a in (90, 135, 180):
        x = cx + 116 * math.cos(math.radians(a)); y = cy + 116 * math.sin(math.radians(a))
        d.rectangle([x - 1, y - 1, x + 1, y + 1], fill=(255, 255, 255))
    ix, iy = 66, 432
    for day in range(1, LENGTH + 1):
        a0 = -90 + (day - 1) * 360 / LENGTH + 3
        a1 = -90 + day * 360 / LENGTH - 3
        if day <= today:
            d.arc([ix - 20, iy - 20, ix + 20, iy + 20], a0, a1, fill=(255, 255, 255), width=2)
        elif day % 2 == 0:
            d.arc([ix - 20, iy - 20, ix + 20, iy + 20], a0, a1, fill=(90, 90, 90), width=1)
    t = str(today)
    d.text((ix - d.textlength(t, font=F20) / 2, iy - 13), t, font=F20, fill=(255, 255, 255))
    return im

if __name__ == "__main__":
  shots = [('A Halka + Ay evreleri', ring_moons()), ('A gece modu', ring_moons(True)), ('B Ay kadranı', moon_dial()),
           ('C Desenler', legend()), ('D Bekleme yüzü', always_on())]
  sheet = Image.new('RGB', (3 * (W + 20) + 20, 2 * (H + 50) + 20), (28, 28, 28))
  sd = ImageDraw.Draw(sheet)
  for i, (name, im) in enumerate(shots):
      x = 20 + (i % 3) * (W + 20); y = 50 + (i // 3) * (H + 50)
      sheet.paste(im, (x, y))
      sd.text((x, y - 36), name, font=F26, fill=(230, 230, 230))
  sheet.save(OUT + '/sheet2.png')
