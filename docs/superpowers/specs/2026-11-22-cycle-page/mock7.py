# All the decided parts of the cycle page on one sheet, in the final symbols: boxes are days
# (filled: passed, outlined: to come), drops bleeding (size: the logged flow; dim: estimated),
# rings fertile days (larger: the estimated ovulation; open with a sperm: intercourse).
import math, sys, datetime
sys.path.insert(0, sys.argv[2])
import mock4 as m
import mock6 as s6
import mock3 as m3
from mock2 import moon, base, centre, F26, F20, F18, W, H, WHITE, GREY, DIM, DARK, RED, BLUE, NRED, NRED_DIM
from PIL import Image, ImageDraw, ImageFont
FONTS, OUT = sys.argv[1], sys.argv[2]
FACE = sys.argv[3] if len(sys.argv) > 3 else None
AOD = sys.argv[4] if len(sys.argv) > 4 else None
F14 = ImageFont.truetype(FONTS + '/JetBrainsMono-Regular.ttf', 14)
AMBER = (255, 176, 64)
DIMRED = (150, 60, 72); DIMBLUE = (60, 90, 150)
FLOW = {1: 2, 2: 3, 3: 3, 4: 2, 5: 1}      # the logged flow of the last period: 1 light .. 3 heavy
SCALE = {1: 0.75, 2: 1.0, 3: 1.3}

def drop(d, x, y, colour, size=1.0, half=False):
    r = 4.2 * size
    if half:
        d.ellipse([x - r, y - r + 2 * size, x + r, y + r + 2 * size], outline=colour, width=1)
        d.pieslice([x - r, y - r + 2 * size, x + r, y + r + 2 * size], 0, 180, fill=colour)
        d.line([(x, y - 8 * size), (x - r * 0.9, y + 0.8)], fill=colour); d.line([(x, y - 8 * size), (x + r * 0.9, y + 0.8)], fill=colour)
    else:
        d.ellipse([x - r, y - r + 2 * size, x + r, y + r + 2 * size], fill=colour)
        d.polygon([(x, y - 8 * size), (x - r * 0.92, y + 0.6 * size), (x + r * 0.92, y + 0.6 * size)], fill=colour)

# 1-2. The spiral, with the logged flow as the drops' size.
def spiral(night=False):
    im = s6.spiral(night)
    d = ImageDraw.Draw(im)
    for t in range(-18, 1):
        day, prev = s6.cday(t)
        if not prev and day <= 5:
            x, y, _ = m.at(t, -18)
            d.rectangle([x - 8, y - 12, x + 8, y + 9], fill=(0, 0, 0))
            drop(d, x, y, NRED if night else RED, SCALE[FLOW[day]])
    return im

# 4. The calendar: today in a box; under each number its marks.
def calendar(night=False):
    im, d = base('Döngü', night)
    centre(d, 48, 'Aralık 2026', F26, NRED if night else WHITE)
    for i, n in enumerate(['Pt', 'Sa', 'Ça', 'Pe', 'Cu', 'Ct', 'Pa']):
        d.text((60 + i * 48 - d.textlength(n, font=F20) / 2, 88), n, font=F20, fill=NRED_DIM if night else GREY)
    for day in range(1, 32):
        col = (1 + day - 1) % 7; row = (1 + day - 1) // 7
        x = 60 + col * 48; y = 132 + row * 56
        cyc = day + 11; nxt = cyc - m.START.day + 1  # unused
        cday = day + 11
        t = day - 1
        if t == 0:
            d.rectangle([x - 22, y - 16, x + 22, y + 34], outline=NRED if night else WHITE, width=2)
        txt = str(day)
        d.text((x - d.textlength(txt, font=F20) / 2, y - 12), txt, font=F20, fill=NRED if night else WHITE)
        marks = []
        if m.FERT[0] <= cday <= m.FERT[1]:
            marks.append('ovu' if cday == s6.OV else 'egg')
        n = cday - 1
        if m.EARLY <= n <= m.LATE:
            marks.append('drop')
        mx = x - 7 * (len(marks) - 1)
        for k in marks:
            if k == 'drop':
                if night: drop(d, mx, y + 22, NRED, 0.85, half=True)
                else: drop(d, mx, y + 22, DIMRED, 0.85)
            else:
                s6.egg(d, mx, y + 22, NRED_DIM if night else DIMBLUE, k == 'ovu')
            mx += 14
    centre(d, 470, 'güne dokun: kayıt', F18, NRED_DIM if night else GREY)
    return im

# 5. History rows: past cycles aligned at day 1, the current one with its estimated next start.
def history():
    im, d = base('Geçmiş')
    x0, px = 64, 7.2
    for day in (1, 8, 15, 22, 29, 36):
        d.text((x0 + (day - 1) * px - 4, 52), str(day), font=F18, fill=GREY)
    months = ['May', 'Haz', 'Haz', 'Tem', 'Ağu', 'Eyl', 'Eki']
    y = 84
    for i, (start, length, period) in enumerate(m3.PAST + [(m.START, None, 5)]):
        d.text((20, y - 2), 'Kas' if length is None else months[i], font=F18, fill=GREY)
        n = m3.TODAY if length is None else length
        for day in range(1, n + 1):
            xx = x0 + (day - 1) * px
            c = RED if day <= period else ((150, 150, 150) if length is None else (85, 85, 85))
            d.rectangle([xx, y, xx + px - 1.5, y + 18], fill=c)
        if length is None:
            for day in range(n + 1, m3.LATE + 2):
                xx = x0 + (day - 1) * px
                c = DIMRED if m3.EARLY <= day - 1 <= m3.LATE else (60, 60, 60)
                d.rectangle([xx, y, xx + px - 1.5, y + 18], outline=c)
            tx = x0 + (m3.TODAY - 0.5) * px
            d.polygon([(tx, y + 22), (tx - 6, y + 32), (tx + 6, y + 32)], fill=WHITE)
        else:
            d.text((x0 + n * px + 6, y - 2), str(length), font=F18, fill=WHITE)
        y += 34
    rows = [('Ortalama', '%.1f gün' % m3.MEAN), ('Değişim', '%d-%d (±%.1f)' % (min(m3.LENGTHS), max(m3.LENGTHS), m3.SD)), ('Düzen', 'düzenli')]
    y = 370
    for name, value in rows:
        d.text((40, y), name, font=F26, fill=GREY)
        d.text((W - 40 - d.textlength(value, font=F26), y), value, font=F26, fill=WHITE)
        y += 40
    return im

# 7. Pregnancy chance: the cycle's days with the marks, the dates and the limits.
def pregnancy():
    im, d = base('Gebelik')
    x0, cw = 30, (W - 60) / 32.0
    tday = (m.TODAY - m.START).days + 1
    for day in range(1, 33):
        xx = x0 + (day - 1) * cw
        if day <= tday: d.rectangle([xx + 1, 110, xx + cw - 1, 130], fill=(150, 150, 150))
        else: d.rectangle([xx + 1, 110, xx + cw - 1, 130], outline=(110, 110, 110))
        cx = xx + cw / 2
        if day <= 5: drop(d, cx, 148, RED, 0.7)
        if m.EARLY <= day - 1 <= m.LATE: drop(d, cx, 148, DIMRED, 0.7)
        if m.FERT[0] <= day <= m.FERT[1]:
            s6.egg(d, cx, 94, BLUE if day <= tday else DIMBLUE, day == s6.OV)
    tx = x0 + (tday - 0.5) * cw
    d.polygon([(tx, 162), (tx - 6, 174), (tx + 6, 174)], fill=WHITE)
    ov = m.START + datetime.timedelta(days=s6.OV - 1)
    f0 = m.START + datetime.timedelta(days=m.FERT[0] - 1); f1 = m.START + datetime.timedelta(days=m.FERT[1] - 1)
    e0 = m.START + datetime.timedelta(days=m.EARLY); e1 = m.START + datetime.timedelta(days=m.LATE)
    rows = [('egg', 'Doğurgan', '%d Kas-%d Ara' % (f0.day, f1.day)), ('ovu', 'Yumurtlama', '%d Ara' % ov.day),
            ('drop', 'Sonraki adet', '%d-%d Ara' % (e0.day, e1.day))]
    y = 196
    for k, name, value in rows:
        if k == 'drop': drop(d, 48, y + 16, RED)
        else: s6.egg(d, 48, y + 16, BLUE, k == 'ovu')
        d.text((70, y + 4), name, font=F20, fill=GREY)
        d.text((W - 36 - d.textlength(value, font=F20), y + 4), value, font=F20, fill=WHITE)
        y += 46
    for i, line in enumerate(['Tahmindir: korunma ve gebelik', 'için kullanılmamalı. Takvim',
                              'yöntemleriyle yılda 100 kadından', '12-24\'ü gebe kalır (tipik).']):
        centre(d, 350 + i * 24, line, F18, AMBER)
    return im

# 8. Religious days (Diyanet).
def religious():
    im, d = base('Hayız')
    rows = [('Hayız', '3. gün', RED), ('Namaz', 'kılınmaz', WHITE), ('Oruç', 'tutulmaz', WHITE),
            ('En çok', '10 gün', GREY), ('Temizlik', 'en az 15 gün', GREY), ('Kaza orucu', '6 gün', AMBER)]
    y = 64
    for name, value, colour in rows:
        d.text((40, y), name, font=F26, fill=GREY)
        d.text((W - 40 - d.textlength(value, font=F26), y), value, font=F26, fill=colour)
        y += 48
    centre(d, 370, '10 günü aşarsa istihaze:', F20, AMBER)
    centre(d, 396, 'namaz ve oruç devam eder', F20, AMBER)
    centre(d, 422, '(Diyanet)', F20, GREY)
    return im

def cycleColour(day):
    if day <= 5: return RED
    if m.FERT[0] <= day <= m.FERT[1]: return BLUE
    return (150, 150, 150)

# 9. The face: the cycle gauge bottom left (passed days in their colours, the rest dim) and the
# icon (the cycle's ring around the day's number).
def face():
    im = Image.open(FACE).convert('RGB')
    d = ImageDraw.Draw(im)
    tday = (m.TODAY - m.START).days + 1
    cx, cy = 120, H - 121
    d.arc([cx - 122, cy - 122, cx + 122, cy + 122], 90, 180, fill=(0, 0, 0), width=14)
    length = round(m.MEAN)
    for day in range(1, length + 1):
        a0 = 180 - (day - 1) * 90 / length; a1 = 180 - day * 90 / length
        c = cycleColour(day)
        if day > tday: c = tuple(v // 3 for v in c)
        d.arc([cx - 117, cy - 117, cx + 117, cy + 117], a1, a0, fill=c, width=5)
    ix, iy = 66, 432
    d.ellipse([ix - 28, iy - 28, ix + 28, iy + 28], fill=(0, 0, 0))
    for day in range(1, length + 1):
        a0 = -90 + (day - 1) * 360 / length + 2; a1 = -90 + day * 360 / length - 2
        c = cycleColour(day) if day <= tday else DARK
        d.arc([ix - 21, iy - 21, ix + 21, iy + 21], a0, a1, fill=c, width=4)
    t = str(tday)
    d.text((ix - d.textlength(t, font=F20) / 2, iy - 13), t, font=F20, fill=WHITE)
    return im

# 10. The always-on face: the gauge 2 px in idle colours, the icon one-bit.
def always_on():
    im = Image.open(AOD).convert('RGB')
    d = ImageDraw.Draw(im)
    tday = (m.TODAY - m.START).days + 1
    cx, cy = 120, H - 121
    d.rectangle([0, H - 125, 125, H], fill=(0, 0, 0))
    length = round(m.MEAN)
    for day in range(1, tday + 1):
        a0 = 180 - (day - 1) * 90 / length; a1 = 180 - day * 90 / length
        c = (255, 0, 0) if day <= 5 else ((0, 0, 255) if m.FERT[0] <= day <= m.FERT[1] else (255, 255, 255))
        d.arc([cx - 115, cy - 115, cx + 115, cy + 115], a1, a0, fill=c, width=2)
    for a in (90, 135, 180):
        x = cx + 116 * math.cos(math.radians(a)); y = cy + 116 * math.sin(math.radians(a))
        d.rectangle([x - 1, y - 1, x + 1, y + 1], fill=(255, 255, 255))
    ix, iy = 66, 432
    for day in range(1, length + 1):
        a0 = -90 + (day - 1) * 360 / length + 3; a1 = -90 + day * 360 / length - 3
        if day <= tday: d.arc([ix - 20, iy - 20, ix + 20, iy + 20], a0, a1, fill=(255, 255, 255), width=2)
        elif day % 2 == 0: d.arc([ix - 20, iy - 20, ix + 20, iy + 20], a0, a1, fill=(90, 90, 90), width=1)
    t = str(tday)
    d.text((ix - d.textlength(t, font=F20) / 2, iy - 13), t, font=F20, fill=(255, 255, 255))
    return im

if __name__ == "__main__":
    shots = [('1 Döngü (spiral)', spiral()), ('1 gece', spiral(True)), ('Anlamlar', s6.legend()),
             ('2 Takvim', calendar()), ('3 Geçmiş', history()), ('4 Ay ile', m3.moon_rows()),
             ('5 Gebelik', pregnancy()), ('6 Hayız (gizli)', religious()), ('Köşe ve simge', face()),
             ('Bekleme yüzü', always_on())]
    cols = 5
    sheet = Image.new('RGB', (cols * (W + 20) + 20, 2 * (H + 50) + 20), (28, 28, 28))
    sd = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(shots):
        x = 20 + (i % cols) * (W + 20); y = 50 + (i // cols) * (H + 50)
        sheet.paste(im, (x, y))
        sd.text((x, y - 36), name, font=F26, fill=(230, 230, 230))
    sheet.save(OUT + '/cycle-pages.png')
