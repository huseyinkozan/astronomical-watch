# Sixth round: drops always filled (to come: dim; at night half filled); egg cells as rings (to
# come: dim; the estimated ovulation larger); a logged intercourse as an egg open on its outer
# side with a sperm inside: a filled dot whose straight tail leaves through the opening. The arrow
# runs from the day's number to today's box.
import math, sys, datetime
sys.path.insert(0, sys.argv[2])
import mock4 as m
from mock2 import moon, base, centre, F26, F20, F18, W, H, WHITE, GREY, RED, BLUE, NRED, NRED_DIM
from PIL import Image, ImageDraw, ImageFont
OUT = sys.argv[2]
F14 = ImageFont.truetype(sys.argv[1] + '/JetBrainsMono-Regular.ttf', 14)
AMBER = (255, 176, 64)
TDAY = (m.TODAY - m.START).days + 1
OV = round(m.MEAN) - 13
INTERCOURSE = {10, 11}   # cycle days logged

def cday(t):
    date = m.TODAY + datetime.timedelta(days=t)
    if date >= m.START:
        return (date - m.START).days + 1, False
    return (date - m.PREV).days + 1, True

def drop(d, x, y, colour, half=False):
    r = 4.2
    if half:
        # At night a drop to come: its outline, the lower half filled.
        d.ellipse([x - r, y - r + 2, x + r, y + r + 2], outline=colour, width=1)
        d.line([(x, y - 8), (x - r * 0.9, y + 0.8)], fill=colour, width=1)
        d.line([(x, y - 8), (x + r * 0.9, y + 0.8)], fill=colour, width=1)
        d.rectangle([x - r * 0.6, y - 1, x + r * 0.6, y + 1.5], fill=(0, 0, 0))
        d.pieslice([x - r, y - r + 2, x + r, y + r + 2], 0, 180, fill=colour)
    else:
        d.ellipse([x - r, y - r + 2, x + r, y + r + 2], fill=colour)
        d.polygon([(x, y - 8), (x - r * 0.92, y + 0.6), (x + r * 0.92, y + 0.6)], fill=colour)

def egg(d, x, y, colour, big=False):
    r = 6.5 if big else 4.8
    d.ellipse([x - r, y - r, x + r, y + r], outline=colour, width=2 if big else 1)

def sperm_egg(d, x, y, colour, out_angle):
    # The ring open towards `out_angle` (radians), a dot in the middle, its tail out of the gap.
    r = 6
    deg = math.degrees(out_angle)
    d.arc([x - r, y - r, x + r, y + r], deg + 32, deg + 360 - 32, fill=colour, width=2)
    d.ellipse([x - 2.4, y - 2.4, x + 2.4, y + 2.4], fill=colour)
    d.line([(x, y), (x + 11 * math.cos(out_angle), y + 11 * math.sin(out_angle))], fill=colour, width=1)

def spiral(night=False):
    im, d = base('Döngü', night)
    for i in range(30):
        p = i / 30.0; a = math.radians(-90 + p * 360)
        moon(d, m.CX + 182 * math.cos(a), m.CY + 182 * math.sin(a), 5, p, NRED if night else (190, 190, 200), NRED_DIM if night else (35, 35, 40))
    for t in range(-18, 24):
        f = 1.0
        if t < -12: f = max(0.25, 1 - (-12 - t) * 0.12)
        if t > 17: f = max(0.2, 1 - (t - 17) * 0.13)
        grey = m.fade(NRED_DIM if night else (150, 150, 150), f)
        m.box(d, t, 'fill' if t <= 0 else 'outline', grey)
        day, prev = cday(t)
        future = t > 0
        x, y, a = m.at(t, -18)
        if not future and day <= 5:
            drop(d, x, y, m.fade(NRED if night else RED, f))
        if future and not prev and m.EARLY <= day - 1 <= m.LATE:
            if night:
                drop(d, x, y, m.fade(NRED, f), half=True)
            else:
                mid = abs(day - 1 - m.MEAN) <= 1.5
                drop(d, x, y, m.fade((150, 60, 72) if mid else (100, 45, 52), f))
        if m.FERT[0] <= day <= m.FERT[1] and not prev:
            x, y, a = m.at(t, 18)
            colour = (NRED if not future else NRED_DIM) if night else (BLUE if not future else (60, 90, 150))
            if not future and day in INTERCOURSE:
                sperm_egg(d, x, y, colour, a)
            else:
                egg(d, x, y, colour, day == OV)
    # The arrow: from the day's number straight to today's box.
    col = NRED if night else WHITE
    _, _, a = m.at(0)
    x0, y0 = m.CX + 62 * math.cos(a), m.CY + 62 * math.sin(a)
    x1, y1 = m.CX + 92 * math.cos(a), m.CY + 92 * math.sin(a)
    d.line([(x0, y0), (x1, y1)], fill=col, width=2)
    d.polygon([(x1 + 4 * math.cos(a), y1 + 4 * math.sin(a)),
               (x1 - 7 * math.cos(a) + 5 * math.cos(a + math.pi / 2), y1 - 7 * math.sin(a) + 5 * math.sin(a + math.pi / 2)),
               (x1 - 7 * math.cos(a) - 5 * math.cos(a + math.pi / 2), y1 - 7 * math.sin(a) - 5 * math.sin(a + math.pi / 2))], fill=col)
    centre(d, m.CY - 34, '%d.' % TDAY, m.F44, col)
    centre(d, m.CY + 16, 'gün', F20, NRED if night else GREY)
    centre(d, m.CY + 40, '1 Ara', F20, NRED if night else WHITE)
    early = m.START + datetime.timedelta(days=m.EARLY); late = m.START + datetime.timedelta(days=m.LATE)
    centre(d, 432, 'Tahmindir, korunma ve gebelik için değil', F14, NRED if night else AMBER)
    # The next period's range at the bottom, a drop before it instead of the word.
    text = '%d-%d Ara' % (early.day, late.day)
    tw = d.textlength(text, font=F26)
    x = (W - tw - 22) / 2
    big = lambda xx, yy, c: (d.ellipse([xx - 6.5, yy - 3.5, xx + 6.5, yy + 9.5], fill=c),
                             d.polygon([(xx, yy - 13), (xx - 6, yy + 1), (xx + 6, yy + 1)], fill=c))
    big(x + 7, 470, NRED if night else RED)
    d.text((x + 22, 456), text, font=F26, fill=NRED if night else RED)
    return im

def legend():
    im, d = base('Anlamlar')
    y = 62
    def row(day, night, text):
        nonlocal y
        day(45, y + 16); night(105, y + 16)
        d.text((150, y + 5), text, font=F18, fill=WHITE)
        y += 46
    row(lambda x, yy: d.rectangle([x - 15, yy - 8, x + 15, yy + 8], fill=(150, 150, 150)),
        lambda x, yy: d.rectangle([x - 15, yy - 8, x + 15, yy + 8], fill=NRED_DIM), 'geçen gün')
    row(lambda x, yy: d.rectangle([x - 15, yy - 8, x + 15, yy + 8], outline=(150, 150, 150), width=2),
        lambda x, yy: d.rectangle([x - 15, yy - 8, x + 15, yy + 8], outline=NRED_DIM, width=2), 'gelecek gün')
    row(lambda x, yy: drop(d, x, yy, RED), lambda x, yy: drop(d, x, yy, NRED), 'kanama')
    row(lambda x, yy: drop(d, x, yy, (150, 60, 72)), lambda x, yy: drop(d, x, yy, NRED, True), 'kanama tahmini')
    row(lambda x, yy: egg(d, x, yy, BLUE), lambda x, yy: egg(d, x, yy, NRED), 'doğurgan')
    row(lambda x, yy: egg(d, x, yy, (60, 90, 150)), lambda x, yy: egg(d, x, yy, NRED_DIM), 'doğurgan tahmini')
    row(lambda x, yy: egg(d, x, yy, BLUE, True), lambda x, yy: egg(d, x, yy, NRED, True), 'yumurtlama (tahmini)')
    row(lambda x, yy: sperm_egg(d, x - 3, yy, BLUE, 0), lambda x, yy: sperm_egg(d, x - 3, yy, NRED, 0), 'birliktelik')
    centre(d, 440, 'sol: gündüz   sağ: gece', F18, GREY)
    return im

if __name__ == "__main__":
    shots = [('B spiral', spiral()), ('B spiral, gece', spiral(True)), ('Anlamlar', legend())]
    sheet = Image.new('RGB', (3 * (W + 20) + 20, H + 70), (28, 28, 28))
    sd = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(shots):
        x = 20 + i * (W + 20); y = 50
        sheet.paste(im, (x, y))
        sd.text((x, y - 36), name, font=F26, fill=(230, 230, 230))
        im.save(OUT + '/spiral6-%d.png' % i)
    sheet.save(OUT + '/sheet6.png')
    im = Image.open(OUT + '/spiral6-0.png').crop((40, 120, 300, 420)).resize((520, 600), Image.LANCZOS)
    im.save(OUT + '/spiral6-zoom.png')
