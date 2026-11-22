# Fourth round: B as a piece of a spiral. One turn is the Moon's month (29.53 days), so the Moon's
# phases form a fixed ring around it; the past at the inner end, the future at the outer end, both
# fading out. Days that passed are boxes: filled = low chance, outlined = fertile (estimated); red
# = bleeding (at night, bleeding boxes are taller). The future is a thin track with the estimates
# beside it like progress bars: the fertile window and the next period's range.
import math, sys, datetime
sys.path.insert(0, sys.argv[2])
from mock2 import phase, moon, base, centre, F26, F20, F18, W, H, WHITE, GREY, DIM, DARK, RED, BLUE, NRED, NRED_DIM
from PIL import Image, ImageDraw, ImageFont
F44 = ImageFont.truetype(sys.argv[1] + '/JetBrainsMono-Light.ttf', 44)
OUT = sys.argv[2]
AMBER = (255, 176, 64)
START = datetime.date(2026, 11, 20)
TODAY = datetime.date(2026, 12, 1)
PREV = datetime.date(2026, 10, 25)            # the cycle before: 26 days, 5 bleeding
LENGTHS = [29, 27, 28, 31, 26, 26]
MEAN = sum(LENGTHS) / 6.0
SD = (sum((x - MEAN) ** 2 for x in LENGTHS) / 5) ** 0.5
EARLY, LATE = round(MEAN - 2 * SD), round(MEAN + 2 * SD)   # days after START
FERT = (min(LENGTHS) - 18, max(LENGTHS) - 11)              # cycle days
SYN = 29.530589
CX, CY = W // 2, 236

def theta(date):
    # The angle of a date on the spiral: its Moon phase (new moon at the top).
    return -90 + phase(date) * 360

def radius(t):
    # t: days from today; the arm moves out 52 px a turn, today at 112 px.
    return 112 + t * 52 / SYN

def at(t, r_off=0):
    a = math.radians(theta(TODAY) + ( t + 0.5 ) * 360 / SYN)
    r = radius(t + 0.5) + r_off
    return CX + r * math.cos(a), CY + r * math.sin(a), a

def box(d, t, style, colour, tall=False):
    a0 = theta(TODAY) + t * 360 / SYN + 1.6; a1 = a0 + 360 / SYN - 3.2
    rm = radius(t + 0.5)
    h = 8
    pts = []
    for i in range(7):
        a = math.radians(a0 + (a1 - a0) * i / 6); r = rm + h
        pts.append((CX + r * math.cos(a), CY + r * math.sin(a)))
    for i in range(7):
        a = math.radians(a1 - (a1 - a0) * i / 6); r = rm - h
        pts.append((CX + r * math.cos(a), CY + r * math.sin(a)))
    if style == 'fill': d.polygon(pts, fill=colour)
    else: d.polygon(pts, outline=colour, width=2)

def drop(d, x, y, colour, size=1.0):
    # A drop, tip up: a disc and a triangle meeting it at its tangents.
    r = 4.2 * size
    d.ellipse([x - r, y - r + 2, x + r, y + r + 2], fill=colour)
    d.polygon([(x, y - 8 * size), (x - r * 0.92, y + 0.6), (x + r * 0.92, y + 0.6)], fill=colour)

def fade(colour, f):
    return tuple(int(c * f) for c in colour)

def spiral(night=False):
    im, d = base('Döngü', night)
    # The Moon's phases around the spiral: fixed, as one turn is one Moon month.
    for i in range(30):
        p = i / 30.0
        a = math.radians(-90 + p * 360)
        moon(d, CX + 176 * math.cos(a), CY + 176 * math.sin(a), 5, p, NRED if night else (190, 190, 200), NRED_DIM if night else (35, 35, 40))
    # The past: from 18 days before today (the end of the previous cycle) to today.
    for t in range(-18, 1):
        date = TODAY + datetime.timedelta(days=t)
        if date >= START:
            cday = (date - START).days + 1; bleed = cday <= 5
        else:
            cday = (date - PREV).days + 1; bleed = cday <= 5
        fertile = FERT[0] <= cday <= FERT[1]
        f = 1.0 if t > -12 else max(0.25, 1 - (-12 - t) * 0.12)   # the far past fades out
        if night:
            colour = fade(NRED if fertile else NRED_DIM, f)
        else:
            colour = fade(BLUE if fertile else (150, 150, 150), f)
        box(d, t, 'outline' if fertile else 'fill', colour)
        if bleed:
            x, y, _ = at(t, -17)
            drop(d, x, y, fade(NRED if night else RED, f))
    # The future: a thin track, fading out at the end.
    for k in range(10, 25):
        f = 1.0 if k < 18 else max(0.0, 1 - (k - 18) * 0.15)
        pts = [at(k + i / 10.0 - 0.5)[:2] for i in range(11)]
        d.line(pts, fill=fade(NRED_DIM if night else DIM, f), width=2, joint='curve')
    # The estimates beside the track (outside it), as progress bars: the fertile window's rest
    # (outlined: fertile), the next period's range (red; thicker in its likely middle).
    def bar(t0, t1, colour, width, off):
        steps = int((t1 - t0) * 10)
        pts = [at(t0 + i / 10.0 - 0.5, off)[:2] for i in range(steps + 1)]
        d.line(pts, fill=colour, width=width, joint='curve')
    tday = (TODAY - START).days + 1
    if tday < FERT[1]:
        bar(1, FERT[1] - tday + 1, NRED if night else BLUE, 2, 12)
        bar(1, FERT[1] - tday + 1, NRED if night else BLUE, 2, 20)
    mean = round(MEAN) - tday + 1
    bar(EARLY - tday + 1, LATE - tday + 2, NRED_DIM if night else (120, 40, 50), 4, 14)
    bar(mean - 1, mean + 2, NRED if night else RED, 8, 14)
    # Today's arrow, pointing at today's box from inside.
    x, y, a = at(0, -16)
    col = NRED if night else WHITE
    d.polygon([(x, y), (x - 10 * math.cos(a + 0.4), y - 10 * math.sin(a + 0.4)), (x - 10 * math.cos(a - 0.4), y - 10 * math.sin(a - 0.4))], fill=col)
    centre(d, CY - 34, '%d.' % tday, F44, col)
    centre(d, CY + 16, 'gün', F20, NRED if night else GREY)
    early = START + datetime.timedelta(days=EARLY); late = START + datetime.timedelta(days=LATE)
    centre(d, 430, 'Adet %d-%d Ara' % (early.day, late.day), F26, NRED if night else RED)
    centre(d, 466, 'Tahmindir, korunma ve gebelik için değil', ImageFont.truetype(sys.argv[1] + '/JetBrainsMono-Regular.ttf', 14), NRED if night else AMBER)
    return im

def legend():
    im, d = base('Anlamlar')
    y = 64
    for s_, c, nc, text in (('fill', (150, 150, 150), NRED_DIM, 'düşük olasılık'), ('outline', BLUE, NRED, 'doğurgan (tahmini)')):
        for col, night in ((30, False), (90, True)):
            rect = [col, y + 9, col + 40, y + 25]
            if s_ == 'fill': d.rectangle(rect, fill=nc if night else c)
            else: d.rectangle(rect, outline=nc if night else c, width=2)
        d.text((150, y + 5), text, font=F18, fill=WHITE)
        y += 50
    for col, night in ((50, False), (110, True)):
        drop(d, col, y + 16, NRED if night else RED)
    d.text((150, y + 5), 'kanama (girilen)', font=F18, fill=WHITE)
    y += 50
    d.line([(30, y + 18), (130, y + 18)], fill=DIM, width=2)
    d.text((150, y + 5), 'gelecek (iz)', font=F18, fill=WHITE)
    y += 44
    d.line([(30, y + 12), (130, y + 12)], fill=BLUE, width=2); d.line([(30, y + 20), (130, y + 20)], fill=BLUE, width=2)
    d.text((150, y + 5), 'doğurgan tahmini', font=F18, fill=WHITE)
    y += 44
    d.line([(30, y + 16), (130, y + 16)], fill=(120, 40, 50), width=4); d.line([(65, y + 16), (95, y + 16)], fill=RED, width=8)
    d.text((150, y + 5), 'adet tahmini', font=F18, fill=WHITE)
    centre(d, 400, 'kutu: doğurganlık', F18, GREY)
    centre(d, 422, '(dolu düşük, boş doğurgan)', F18, GREY)
    centre(d, 444, 'damla: kanama', F18, GREY)
    return im

if __name__ == "__main__":
    shots = [('B spiral', spiral()), ('B spiral, gece', spiral(True)), ('Anlamlar', legend())]
    sheet = Image.new('RGB', (3 * (W + 20) + 20, H + 70), (28, 28, 28))
    sd = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(shots):
        x = 20 + i * (W + 20); y = 50
        sheet.paste(im, (x, y))
        sd.text((x, y - 36), name, font=F26, fill=(230, 230, 230))
        im.save(OUT + '/spiral-%d.png' % i)
    sheet.save(OUT + '/sheet4.png')
