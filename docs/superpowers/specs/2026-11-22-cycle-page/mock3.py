# Third round: a ring on a fixed scale (40 days a turn) with the Moon's phases, past cycles'
# ends as ticks and the next start's window; past cycles as aligned rows; Moon rows.
import math, sys, datetime
sys.path.insert(0, sys.argv[2])
from mock2 import phase, moon, base, centre, segment, F26, F20, F18, F72, W, H, WHITE, GREY, DIM, DARK, RED, BLUE, BLUE_LIGHT, NRED, NRED_DIM
OUT = sys.argv[2]
START = datetime.date(2026, 11, 20)
TODAY_DATE = datetime.date(2026, 12, 1)
TODAY = (TODAY_DATE - START).days + 1
PAST = [(datetime.date(2026, 5, 3), 29, 5), (datetime.date(2026, 6, 1), 29, 5), (datetime.date(2026, 6, 30), 27, 4),
        (datetime.date(2026, 7, 27), 28, 5), (datetime.date(2026, 8, 24), 31, 6), (datetime.date(2026, 9, 24), 26, 5),
        (datetime.date(2026, 10, 25), 26, 5)]
LENGTHS = [c[1] for c in PAST[-6:]]
MEAN = sum(LENGTHS) / len(LENGTHS)
SD = (sum((x - MEAN) ** 2 for x in LENGTHS) / (len(LENGTHS) - 1)) ** 0.5
EARLY, LATE = round(MEAN - 2 * SD), round(MEAN + 2 * SD)
FERTILE = (min(LENGTHS) - 18, max(LENGTHS) - 11)
TURN = 40
AMBER = (255, 176, 64)

def ang(day):  # the start of `day` (1-based) on the ring, degrees from 12 o'clock clockwise
    return -90 + (day - 1) * 360 / TURN

def ring(night=False):
    im, d = base('Döngü', night)
    cx, cy = W // 2, 222
    for day in range(1, TURN + 1):
        a0, a1 = ang(day) + 1, ang(day + 1) - 1
        if day <= 5:
            s, c = 'fill', (NRED if night else RED)
        elif FERTILE[0] <= day <= FERTILE[1]:
            s, c = ('fill' if day <= TODAY else 'hatch'), (NRED if night else BLUE)
            if day > TODAY: s = 'hatch'
            else: s = 'hatch'
        elif day <= TODAY:
            s, c = 'fill', (NRED_DIM if night else (110, 110, 110))
        else:
            s, c = 'outline', (NRED_DIM if night else DIM)
        if EARLY <= day - 1 <= LATE and day > TODAY:
            s, c = 'outline', (NRED if night else RED)   # the next start's window
        segment(d, cx, cy, 130, 146, a0, a1, s, c)
        am = math.radians(ang(day) + 180 / TURN)
        date = START + datetime.timedelta(days=day - 1)
        lit = NRED if night else ((255, 255, 255) if day == TODAY else (190, 190, 200))
        moon(d, cx + 114 * math.cos(am), cy + 114 * math.sin(am), 5.5, phase(date), lit, NRED_DIM if night else (35, 35, 40))
    # Past cycles' ends: ticks outside the rim, at the day the next one started.
    for length in LENGTHS:
        a = math.radians(ang(length + 1))
        col = NRED if night else WHITE
        d.line([(cx + 150 * math.cos(a), cy + 150 * math.sin(a)), (cx + 162 * math.cos(a), cy + 162 * math.sin(a))], fill=col, width=2)
    a = math.radians(ang(TODAY) + 180 / TURN)
    px, py = cx + 100 * math.cos(a), cy + 100 * math.sin(a)
    col = NRED if night else WHITE
    d.polygon([(px, py), (px - 9 * math.cos(a + 0.45), py - 9 * math.sin(a + 0.45)), (px - 9 * math.cos(a - 0.45), py - 9 * math.sin(a - 0.45))], fill=col)
    centre(d, cy - 70, '%d.' % TODAY, F72, col)
    centre(d, cy + 8, 'gün', F26, NRED if night else GREY)
    centre(d, cy + 40, 'doğurgan', F18, NRED if night else BLUE)
    centre(d, cy + 60, 'tahmini', F18, NRED if night else GREY)
    early = START + datetime.timedelta(days=EARLY); late = START + datetime.timedelta(days=LATE)
    centre(d, 392, 'Adet %d-%d Ara' % (early.day, late.day), F26, NRED if night else RED)
    centre(d, 428, 'Tahmindir; doğum kontrolü', F18, NRED if night else AMBER)
    centre(d, 450, 've gebelik için kullanmayın', F18, NRED if night else AMBER)
    return im

def rows(night=False):
    im, d = base('Geçmiş', night)
    x0, px = 64, 7.2
    # The ruler: days 1 to 40, a mark every 7 days.
    for day in (1, 8, 15, 22, 29, 36):
        x = x0 + (day - 1) * px
        d.text((x - 4, 52), str(day), font=F18, fill=NRED_DIM if night else GREY)
    months = ['May', 'Haz', 'Haz', 'Tem', 'Ağu', 'Eyl', 'Eki']
    y = 84
    allc = PAST + [(START, None, 5)]
    for i, (start, length, period) in enumerate(allc):
        name = 'Kas' if length is None else months[i]
        d.text((20, y - 2), name, font=F18, fill=NRED_DIM if night else GREY)
        n = TODAY if length is None else length
        for day in range(1, n + 1):
            x = x0 + (day - 1) * px
            c = (NRED if night else RED) if day <= period else (NRED_DIM if night else ((150, 150, 150) if length is None else (85, 85, 85)))
            d.rectangle([x, y, x + px - 1.5, y + 18], fill=c)
        if length is None:
            # The next start's window, outlined: from the earliest to the latest likely day.
            d.rectangle([x0 + EARLY * px, y - 2, x0 + (LATE + 1) * px - 1.5, y + 20], outline=NRED if night else RED, width=2)
            tx = x0 + (TODAY - 0.5) * px
            d.polygon([(tx, y + 22), (tx - 6, y + 32), (tx + 6, y + 32)], fill=NRED if night else WHITE)
        else:
            d.text((x0 + n * px + 6, y - 2), str(length), font=F18, fill=NRED if night else WHITE)
        y += 34
    rows_ = [('Ortalama', '%.1f gün' % MEAN), ('Değişim', '%d-%d (±%.1f)' % (min(LENGTHS), max(LENGTHS), SD)), ('Düzen', 'düzenli')]
    y = 370
    for name, value in rows_:
        d.text((40, y), name, font=F26, fill=NRED_DIM if night else GREY)
        d.text((W - 40 - d.textlength(value, font=F26), y), value, font=F26, fill=NRED if night else WHITE)
        y += 40
    return im

def moon_rows():
    im, d = base('Ay ile')
    x0, px = 50, 10.3
    for i in range(30):
        x = x0 + i * px
        if i % 7 == 0 or i == 29:
            moon(d, x + 4, 58, 7, i / 29.53, (190, 190, 200), (35, 35, 40))
    # Rows: lunar months from the new moon of 2026-04-17 (approx.); each start a red dot.
    new0 = datetime.date(2026, 4, 17)
    starts = [c[0] for c in PAST] + [START]
    for row in range(8):
        y = 92 + row * 34
        d.line([(x0, y + 9), (x0 + 29.53 * px, y + 9)], fill=DARK, width=1)
        m0 = new0 + datetime.timedelta(days=round(row * 29.53))
        d.text((8, y - 2), ['Nis', 'May', 'Haz', 'Tem', 'Ağu', 'Eyl', 'Eki', 'Kas'][row], font=F18, fill=GREY)
        for s in starts:
            off = (s - new0).days - row * 29.53
            if 0 <= off < 29.53:
                x = x0 + off * px
                d.ellipse([x - 6, y + 3, x + 6, y + 15], fill=RED)
    centre(d, 380, 'satır: bir Ay ayı (yeni ay solda)', F18, GREY)
    centre(d, 404, 'nokta: adetin başladığı gün', F18, GREY)
    centre(d, 436, 'Bilgi içindir; Ay döngüyü', F18, AMBER)
    centre(d, 458, 'belirlemez (araştırmalar)', F18, AMBER)
    return im

if __name__ == "__main__":
    shots = [('B Halka (40 gün)', ring()), ('B gece', ring(True)), ('A Geçmiş satırları', rows()),
             ('A gece', rows(True)), ('C Ay ile', moon_rows())]
    from PIL import Image, ImageDraw
    sheet = Image.new('RGB', (3 * (W + 20) + 20, 2 * (H + 50) + 20), (28, 28, 28))
    sd = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(shots):
        x = 20 + (i % 3) * (W + 20); y = 50 + (i // 3) * (H + 50)
        sheet.paste(im, (x, y))
        sd.text((x, y - 36), name, font=F26, fill=(230, 230, 230))
    sheet.save(OUT + '/sheet3.png')
    print(MEAN, SD, EARLY, LATE, FERTILE)
