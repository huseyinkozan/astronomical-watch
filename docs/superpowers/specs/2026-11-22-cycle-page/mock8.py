# The corner icon and gauge: the whole cycle in dim phase colours, the passed part bright, a
# white mark at today; while the page is locked (and always on the always-on face) neutral: one
# grey, no phase colours. Shown large (3x) and at 1x on a face.
import math, sys
sys.path.insert(0, sys.argv[2])
import mock4 as m
from PIL import Image, ImageDraw, ImageFont
FONTS, OUT, FACE = sys.argv[1], sys.argv[2], sys.argv[3]
F20 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 20)
F26 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 26)
RED = (255, 80, 100); BLUE = (90, 150, 255); GREY = (150, 150, 150); WHITE = (255, 255, 255)
LENGTH = 28; TODAY = 12; FERT = (8, 20)
H = 502

def colour(day):
    if day <= 5: return RED
    if FERT[0] <= day <= FERT[1]: return BLUE
    return GREY

def dim(c, k=0.3): return tuple(int(v * k) for v in c)

def icon(d, ix, iy, locked, s=1.0):
    r = 21 * s
    for day in range(1, LENGTH + 1):
        a0 = -90 + (day - 1) * 360 / LENGTH + 2; a1 = -90 + day * 360 / LENGTH - 2
        c = GREY if locked else colour(day)
        c = c if day <= TODAY else dim(c)
        d.arc([ix - r, iy - r, ix + r, iy + r], a0, a1, fill=c, width=max(1, int(4 * s)))
    # Today's mark: a small white triangle outside the ring, pointing in.
    a = math.radians(-90 + (TODAY - 0.5) * 360 / LENGTH)
    tip = (ix + (r + 2 * s) * math.cos(a), iy + (r + 2 * s) * math.sin(a))
    base = r + 9 * s
    p1 = (ix + base * math.cos(a + 0.18), iy + base * math.sin(a + 0.18))
    p2 = (ix + base * math.cos(a - 0.18), iy + base * math.sin(a - 0.18))
    d.polygon([tip, p1, p2], fill=WHITE)
    f = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', int(20 * s))
    t = str(TODAY)
    d.text((ix - d.textlength(t, font=f) / 2, iy - 13 * s), t, font=f, fill=WHITE)

def gauge(d, locked, cx=120, cy=H - 121, R=117):
    d.arc([cx - R - 6, cy - R - 6, cx + R + 6, cy + R + 6], 90, 180, fill=(0, 0, 0), width=16)
    for day in range(1, LENGTH + 1):
        # From the gauge's lower end (the bottom) up to the left edge.
        a0 = 90 + (day - 1) * 90 / LENGTH; a1 = 90 + day * 90 / LENGTH
        c = GREY if locked else colour(day)
        c = c if day <= TODAY else dim(c)
        d.arc([cx - R, cy - R, cx + R, cy + R], a0, a1, fill=c, width=5)
    a = math.radians(90 + TODAY * 90 / LENGTH)
    # Today: the end of the bright part, with a white notch across it.
    x0, y0 = cx + (R - 6) * math.cos(a), cy + (R - 6) * math.sin(a)
    x1, y1 = cx + (R + 1) * math.cos(a), cy + (R + 1) * math.sin(a)
    d.line([(x0, y0), (x1, y1)], fill=WHITE, width=3)

def face(locked):
    im = Image.open(FACE).convert('RGB')
    d = ImageDraw.Draw(im)
    gauge(d, locked)
    d.ellipse([66 - 32, 432 - 32, 66 + 32, 432 + 32], fill=(0, 0, 0))
    icon(d, 66, 432, locked)
    return im

def close(locked):
    im = Image.new('RGB', (300, 300), (0, 0, 0))
    d = ImageDraw.Draw(im)
    icon(d, 150, 150, locked, 3.0)
    return im

sheet = Image.new('RGB', (2 * 430 + 2 * 320 + 40, 620), (28, 28, 28))
sd = ImageDraw.Draw(sheet)
items = [('Simge, açık (3x)', close(False), 20), ('Simge, kilitli (3x)', close(True), 340),
         ('Yüz, kilit açık', face(False), 660), ('Yüz, kilitli', face(True), 1090)]
for name, im, x in items:
    sheet.paste(im, (x, 60))
    sd.text((x, 20), name, font=F26, fill=(230, 230, 230))
sheet.save(OUT + '/corner-lock.png')
