// The watch face's areas: a face picture with arrows to each area, and a table of what each area
// can show. The lists follow the portal's (main/portal/index.html: faceTexts, faceCorners,
// gaugeItems, the templates and clocks); keep them in step when an item is added there.

// Pictures of a screenshot's region, by CSS: [screenshot, centre x, centre y, size]. The cycle icon is
// on no default face: face-cycle is the second face page with it bottom left (tools/screenshot.py
// with --send "G 1 6 18" --send "F 1").
const iconSpots = {
  moon: ['face-2', 66, 432],
  weekday: ['face-2', 66, 70],
  time: ['face-0', 344, 68],
  wifi: ['face-2', 340, 432],
  cycle: ['face-cycle', 64, 432, 62],
};
const ICON = 52;

// Each label at the height `y` of the face, its arrow to `at` (face pixels).
const anatomyAreas = [
  { id: 'gauges', name: ['Corner gauge', 'Köşe göstergesi'], side: 0, y: 40, at: [14, 96] },
  { id: 'icons', name: ['Corner icon', 'Köşe simgesi'], side: 0, y: 112, at: [42, 74] },
  { id: 'moonArc', name: ["The Moon's arc", "Ay'ın yayı"], side: 0, y: 205, at: [90, 205] },
  { id: 'clock', name: ['Clock', 'Saat'], side: 0, y: 300, at: [176, 282] },
  { id: 'icons', name: ['Corner icon', 'Köşe simgesi'], side: 0, y: 392, at: [44, 428] },
  { id: 'gauges', name: ['Corner gauge', 'Köşe göstergesi'], side: 0, y: 470, at: [14, 410] },
  { id: 'texts', name: ['Top edge text', 'Üst kenar yazısı'], side: 1, y: 25, at: [262, 25] },
  { id: 'texts', name: ['Top text', 'Üst yazı'], side: 1, y: 62, at: [262, 60] },
  { id: 'planets', name: ['Planets', 'Gezegenler'], side: 1, y: 160, at: [326, 184] },
  { id: 'sunArc', name: ["The Sun's arc", "Güneş'in yayı"], side: 1, y: 260, at: [368, 260] },
  { id: 'icons', name: ['Corner icon', 'Köşe simgesi'], side: 1, y: 400, at: [364, 428] },
  { id: 'texts', name: ['Bottom edge text', 'Alt kenar yazısı'], side: 1, y: 478, at: [250, 478] },
];

const textItems = [['Date, long', 'Tarih, uzun', 'Sunday 21 June', '21 Haziran Pazar'],
  ['Date, short', 'Tarih, kısa', 'Jun 21', '21 Haz'], ['Hijri date', 'Hicri tarih', '5 Muharram 1448', '5 Muharrem 1448'],
  ['Next sunrise or sunset', 'Sonraki gün doğumu ya da batımı', 'Sunset 1:21', 'Gün batımı 1:21'],
  ['Next prayer time', 'Sonraki namaz vakti', 'Maghrib 1:28', 'Akşam 1:28'], ['Seconds', 'Saniye', ':42', ':42'],
  ['Battery', 'Pil', '68 %', '68 %'], ['World clock city', 'Dünya saati şehri', 'London 17:19', 'London 17:19'],
  ['Day counter', 'Gün sayacı', 'Holiday 12', 'Tatil 12'], ['Sun altitude', 'Güneş yüksekliği', '13°', '13°'],
  ['Moon altitude', 'Ay yüksekliği', '25°', '25°'], ['Time (HH:MM)', 'Saat (SS:DD)', '19:19', '19:19'],
  ['Weekday', 'Hafta günü', 'Sun', 'Paz'], ['Day of the month', 'Ayın günü', '21', '21'], ['Month', 'Ay adı', 'Jun', 'Haz'],
  ['Hijri day of the month', 'Hicri ayın günü', '5', '5'], ['Hijri month', 'Hicri ay adı', 'Muh', 'Muh'],
  ['Hour (HH)', 'Saat (SS)', '19', '19'], ['Minute (MM)', 'Dakika (DD)', '19', '19'],
  ['Emergency info: the emergency card\'s blood type, in red; a tap opens the card', 'Acil bilgisi: acil durum kartındaki kan grubu, kırmızı; dokununca kart açılır', 'A Rh+', 'A Rh+']];

const cornerIcons = [
  ['moon', 'Moon phase', 'Ay evresi', 'The Moon as it looks now, lit on the side the Sun lights. A tap opens the Moon page.',
   'Ay şu an nasıl görünüyorsa öyle, Güneş\'in aydınlattığı yanı parlak. Dokununca Ay sayfası açılır.'],
  ['weekday', 'Weekday ring', 'Hafta günü halkası', 'Today\'s initial inside a ring of the week\'s seven days. A tap opens the calendar.',
   'Bugünün baş harfi, haftanın yedi gününden oluşan bir halkanın içinde. Dokununca takvim açılır.'],
  ['time', 'Time icon', 'Zaman simgesi', 'An hourglass while a timer runs; else, while an alarm is on, a bell inside the ring of its days; else a dim bell. A tap opens the running timer or the alarms.',
   'Bir sayaç çalışırken kum saati; değilse, bir alarm açıkken alarm günlerinin halkası içinde zil; o da yoksa sönük zil. Dokununca çalışan sayaç ya da alarmlar açılır.'],
  ['wifi', 'Wi-Fi icon', 'Wi-Fi simgesi', 'Light blue while the setup portal runs, dim otherwise. A tap opens the Wi-Fi setup.',
   'Kurulum portalı açıkken açık mavi, değilken sönük. Dokununca Wi-Fi kurulumu açılır.'],
  ['cycle', 'Cycle icon', 'Döngü simgesi', 'The menstrual cycle\'s day inside a ring of the cycle: red the period, blue the fertile days. Grey while the cycle page is locked and on the always-on face. A tap opens the cycle page.',
   'Adet döngüsünün günü, döngünün halkası içinde: kırmızı adet, mavi doğurgan günler. Döngü sayfası kilitliyken ve bekleme yüzünde gri. Dokununca döngü sayfası açılır.'],
  ['tide', 'Tide wave icon', 'Gelgit dalga simgesi', 'A wave as high as the tide: a calm line at low water, the crest at the top at high water. Needs the tide interval in the portal; a tap opens the tide part.',
   'Gelgit kadar yüksek bir dalga: alçak suda düz bir çizgi, yüksek suda tepe en üstte. Portalda gelgit gecikmesi gerekir; dokununca gelgit bölümü açılır.'],
];
const cornerTexts = [['Weekday', 'Hafta günü', 'Sun', 'Paz'], ['Day of the month', 'Ayın günü', '21', '21'],
  ['Hijri day of the month', 'Hicri ayın günü', '5', '5'], ['Month', 'Ay adı', 'Jun', 'Haz'],
  ['Hijri month', 'Hicri ay adı', 'Muh', 'Muh'], ['Sun altitude', 'Güneş yüksekliği', '13°', '13°'],
  ['Moon altitude', 'Ay yüksekliği', '25°', '25°'], ['Hour (HH)', 'Saat (SS)', '19', '19'],
  ['Minute (MM)', 'Dakika (DD)', '19', '19'], ['Second', 'Saniye', '42', '42'], ['Time (HH:MM)', 'Saat (SS:DD)', '19:19', '19:19']];

const gaugeRows = [
  ['Day or night', 'Gündüz ya da gece', 'Yellow from sunrise to sunset, blue through the night. Default top left.',
   'Gün doğumundan batımına sarı, gece boyunca mavi. Varsayılan: sol üst.'],
  ['Battery', 'Pil', 'The charge; red at the power-off limit; while charging, the part above it white. Default top right.',
   'Şarj; kapanma sınırında kırmızı; şarj olurken üstündeki kısım beyaz. Varsayılan: sağ üst.'],
  ['Moon above or below the horizon', 'Ay ufkun üstünde ya da altında', 'Bluish white from moonrise to moonset, dark grey while the Moon is down. Default bottom left.',
   'Ay doğuşundan batışına mavimsi beyaz, Ay batıkken koyu gri. Varsayılan: sol alt.'],
  ['Moon phase', 'Ay evresi', 'The lit part grows from the lower end towards full moon, then shrinks towards the upper end.',
   'Aydınlık kısım dolunaya doğru alt uçtan büyür, sonra üst uca doğru küçülür.'],
  ['Next prayer time', 'Sonraki namaz vakti', 'Fills from the last prayer time to the next.', 'Son namaz vaktinden sonrakine dolar.'],
  ['Next sunrise, sunset, moonrise or moonset', 'Sonraki gün doğumu, batımı, Ay doğuşu ya da batışı', 'Fills from the previous one to the next.', 'Bir öncekinden sonrakine dolar.'],
  ['Week, month, Hijri month, year', 'Hafta, ay, Hicri ay, yıl', 'How much of it has passed.', 'Ne kadarının geçtiği.'],
  ['Day counter', 'Gün sayacı', 'A day counter: since it was saved, yearly, or the last 30 or 100 days.', 'Bir gün sayacı: kaydedildiğinden beri, yıllık ya da son 30 veya 100 gün.'],
  ['Sun altitude', 'Güneş yüksekliği', 'How high the Sun stands.', 'Güneş\'in ne kadar yüksekte olduğu.'],
  ['Hour, minute, second', 'Saat, dakika, saniye', 'Like a hand going round; the second stays empty on the always-on face.', 'Dönen bir ibre gibi; saniye bekleme yüzünde boş kalır.'],
  ['Display timeout, time left', 'Ekran zaman aşımı, kalan süre', 'Full at a touch, then shrinking: orange at 40 %, red at 20 %. On the always-on face: the time left until the watch switches off lying still. Default bottom right.',
   'Dokununca dolar, sonra azalır: %40\'ta turuncu, %20\'de kırmızı. Bekleme yüzünde: saat hareketsiz kalırsa kapanmasına kalan süre. Varsayılan: sağ alt.'],
  ['Menstrual cycle', 'Adet döngüsü', 'The cycle\'s days, the period and the fertile days in colour; grey while the cycle page is locked.', 'Döngünün günleri, adet ve doğurgan günler renkli; döngü sayfası kilitliyken gri.'],
  ['Tide', 'Gelgit', 'The tide\'s level from the Moon: cyan while the water rises, blue while it falls. It tells when the water is high or low, not how high; needs the tide interval in the portal.',
   'Ay\'dan hesaplanan gelgit seviyesi: su yükselirken camgöbeği, alçalırken mavi. Suyun ne zaman yüksek ya da alçak olduğunu gösterir, ne kadar olduğunu değil; portalda gelgit gecikmesi gerekir.'],
];

const templateRows = [
  ['face-4', 'Large', 'Büyük', 'A large clock and the text lines; no arcs.', 'Büyük bir saat ve yazı satırları; yay yok.'],
  ['face-1', 'Full arcs', 'Tam yay', 'The Sun\'s and the Moon\'s orbits around a small clock: the part above the horizon is centred at the top and as long as their time above it; the body moves along as the day goes.',
   'Güneş\'in ve Ay\'ın küçük bir saatin çevresindeki yörüngeleri: ufkun üstündeki kısım tepede ortalanır ve ufkun üstünde kaldıkları süre kadar uzundur; gök cismi gün ilerledikçe yol alır.'],
  ['face-2', 'Half arcs', 'Yarım yay', 'Their paths over the horizon, rise on the left and set on the right (the other way south of the equator), a small clock below; the Milky Way band can be shown too.',
   'Ufkun üstündeki yolları, solda doğuş, sağda batış (ekvatorun güneyinde tersi), altta küçük bir saat; Samanyolu kuşağı da gösterilebilir.'],
];
const clockRows = [
  ['face-4', 'Digital: JetBrains Mono', 'Dijital: JetBrains Mono', 'The watch\'s own font.', 'Saatin kendi yazı tipi.'],
  ['face-3', 'Digital: seven segment', 'Dijital: yedi parça', 'Digits of seven segments, drawn by the watch.', 'Saatin çizdiği yedi parçalı rakamlar.'],
  ['face-0', 'Analog', 'Analog', 'Five styles of hands: classic, bold, fine scale, one hand, plain; a seconds hand on the active face if chosen.',
   'Beş çeşit akrep ve yelkovan: klasik, kalın, ince bölmeli, tek ibre, çizgisiz; istenirse etkin yüzde saniye kolu.'],
  [null, 'None', 'Yok', 'No clock: for a face of the arcs and the texts alone.', 'Saat yok: yalnız yaylar ve yazılarla bir yüz için.'],
];

const anatomyText = {
  gauges: ['The four screen corners fill as something passes, on every face page and on the always-on face. Each corner shows one of these, chosen in the setup portal; a notch marks the middle, a tap opens its page.',
           'Ekranın dört köşesi bir şey geçtikçe dolar; her yüz sayfasında ve bekleme yüzünde. Her köşe kurulum portalında seçilen bunlardan birini gösterir; ortadaki çentik yarıyı belirtir, dokununca ilgili sayfa açılır.'],
  icons: ['The four inner corners hold an icon or a short text. The icons:', 'Dört iç köşede bir simge ya da kısa bir yazı durur. Simgeler:'],
  iconsText: ['Or a short text:', 'Ya da kısa bir yazı:'],
  texts: ['Four text lines (top edge, top, bottom, bottom edge) take any of these; each can be shown on the always-on face too. A text longer than its line is cut to whole characters; a tap opens its page.',
          'Dört yazı satırı (üst kenar, üst, alt, alt kenar) bunlardan birini alır; her biri bekleme yüzünde de gösterilebilir. Satırına sığmayan yazı tam harflerle kısaltılır; dokununca ilgili sayfa açılır.'],
  clock: ['The clock of each face page; a tap on it opens the world clock.', 'Her yüz sayfasının saati; dokununca dünya saati açılır.'],
  sunArc: ['The Sun and the Moon move along their arcs as the day goes, on the two arc templates. A tap on the Sun or the Moon opens its page. The templates:',
           'Güneş ve Ay, iki yay şablonunda gün ilerledikçe yaylarında yol alır. Güneş\'e ya da Ay\'a dokununca sayfası açılır. Şablonlar:'],
  planets: ['On the arc templates (the "Planets" box of a face page) the seven planets sit between the Sun\'s and the Moon\'s arcs, by how far along their way from rise to set they are; the Full arcs show them below the horizon too, dim. A tap on the ring between the arcs opens the planets\' sky.',
            'Yay şablonlarında (yüz sayfasının "Gezegenler" kutusu) yedi gezegen, Güneş\'in ve Ay\'ın yayları arasında, doğuştan batışa yollarının ne kadarını aldıklarına göre durur; Tam yay onları ufkun altında da sönük gösterir. Yaylar arasındaki halkaya dokununca gezegenlerin gökyüzü açılır.'],
};
anatomyText.moonArc = anatomyText.sunArc;

let anatomySelected = 'gauges';

function L2(pair) { return pair[language === 'tr' ? 1 : 0]; }

function iconPicture(id) {
  const box = document.createElement('div');
  box.className = 'icon';
  if (!iconSpots[id]) return box;  // no screenshot of it yet
  const [shot, x, y, size = ICON] = iconSpots[id];
  box.style.width = size + 'px';
  box.style.height = size + 'px';
  box.style.backgroundImage = 'url(screenshots/' + language + '/' + shot + '.png)';
  box.style.backgroundPosition = '-' + (x - size / 2) + 'px -' + (y - size / 2) + 'px';
  return box;
}

function thumbnail(shot) {
  const img = document.createElement('img');
  img.className = 'thumb';
  img.alt = '';
  if (shot) img.src = 'screenshots/' + language + '/' + shot + '.png';
  else img.style.visibility = 'hidden';
  return img;
}

// A table of rows; each row is a list of cells (text or an element).
function tableOf(head, rows) {
  const table = document.createElement('table');
  const tr = document.createElement('tr');
  head.forEach((h) => { const th = document.createElement('th'); th.textContent = h; tr.appendChild(th); });
  table.appendChild(tr);
  rows.forEach((cells) => {
    const row = document.createElement('tr');
    cells.forEach((c) => {
      const td = document.createElement('td');
      if (typeof c === 'string') td.textContent = c; else td.appendChild(c);
      row.appendChild(td);
    });
    table.appendChild(row);
  });
  return table;
}

function paragraph(text) {
  const p = document.createElement('p');
  p.textContent = text;
  return p;
}

function drawAnatomyTable() {
  const tr = language === 'tr';
  const box = document.getElementById('anatomyTable');
  box.innerHTML = '';
  const id = anatomySelected === 'moonArc' ? 'sunArc' : anatomySelected;
  box.appendChild(paragraph(L2(anatomyText[id])));
  const item = tr ? 'Seçenek' : 'Item';
  const example = tr ? 'Örnek' : 'Example';
  const what = tr ? 'Ne gösterir' : 'What it shows';
  if (id === 'gauges') {
    box.appendChild(tableOf([item, what], gaugeRows.map((r) => [r[tr ? 1 : 0], r[tr ? 3 : 2]])));
  } else if (id === 'icons') {
    box.appendChild(tableOf([tr ? 'Simge' : 'Icon', tr ? 'Adı' : 'Name', tr ? 'Ne işe yarar' : 'What it does'],
                            cornerIcons.map((r) => [iconPicture(r[0]), r[tr ? 2 : 1], r[tr ? 4 : 3]])));
    box.appendChild(paragraph(L2(anatomyText.iconsText)));
    box.appendChild(tableOf([item, example], cornerTexts.map((r) => [r[tr ? 1 : 0], r[tr ? 3 : 2]])));
  } else if (id === 'texts') {
    box.appendChild(tableOf([item, example], textItems.map((r) => [r[tr ? 1 : 0], r[tr ? 3 : 2]])));
  } else if (id === 'clock') {
    box.appendChild(tableOf(['', item, what], clockRows.map((r) => [thumbnail(r[0]), r[tr ? 2 : 1], r[tr ? 4 : 3]])));
  } else if (id === 'sunArc') {
    box.appendChild(tableOf(['', tr ? 'Şablon' : 'Template', what], templateRows.map((r) => [thumbnail(r[0]), r[tr ? 2 : 1], r[tr ? 4 : 3]])));
  }
}

// The picture with its arrows, as SVG: labels left and right of the face, lines to their places.
function drawAnatomy() {
  const svg = document.getElementById('anatomySvg');
  const NS = 'http://www.w3.org/2000/svg';
  const X0 = 250;
  const Y0 = 20;
  svg.innerHTML = '';
  const el = (name, attributes, parent) => {
    const e = document.createElementNS(NS, name);
    Object.keys(attributes).forEach((k) => e.setAttribute(k, attributes[k]));
    (parent || svg).appendChild(e);
    return e;
  };
  const defs = el('defs', {});
  const marker = el('marker', { id: 'arrowHead', viewBox: '0 0 10 10', refX: 9, refY: 5, markerWidth: 7, markerHeight: 7, orient: 'auto-start-reverse' }, defs);
  el('path', { d: 'M0,0 L10,5 L0,10 z', fill: '#ffb040' }, marker);
  const clip = el('clipPath', { id: 'faceClip' }, defs);
  el('rect', { x: X0, y: Y0, width: 410, height: 502, rx: 120 }, clip);
  el('image', { href: 'screenshots/' + language + '/face-0.png', x: X0, y: Y0, width: 410, height: 502, 'clip-path': 'url(#faceClip)' });
  anatomyAreas.forEach((area) => {
    const side = area.side;
    const y = Y0 + area.y + 6;
    const x = side === 0 ? X0 - 24 : X0 + 410 + 24;
    const group = el('g', { class: 'area' + (area.id === anatomySelected ? ' selected' : ''), tabindex: 0, role: 'button' });
    el('line', { x1: side === 0 ? x + 6 : x - 6, y1: y - 6, x2: X0 + area.at[0], y2: Y0 + area.at[1], 'marker-end': 'url(#arrowHead)' }, group);
    const text = el('text', { x: x, y: y, 'text-anchor': side === 0 ? 'end' : 'start' }, group);
    text.textContent = L2(area.name);
    const choose = () => { anatomySelected = area.id; drawAnatomy(); };
    group.addEventListener('click', choose);
    group.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choose(); } });
  });
  // The same choices as buttons, readable on a phone where the picture's labels are small.
  const buttons = document.getElementById('anatomyButtons');
  buttons.innerHTML = '';
  const seen = new Set();
  anatomyAreas.forEach((area) => {
    const id = area.id === 'moonArc' ? 'sunArc' : area.id;
    if (seen.has(id)) return;
    seen.add(id);
    const b = document.createElement('button');
    b.type = 'button';
    const plural = { texts: ['Text lines', 'Yazı satırları'], sunArc: ['Sun and Moon arcs', 'Güneş ve Ay yayları'],
                     gauges: ['Corner gauges', 'Köşe göstergeleri'], icons: ['Corner icons', 'Köşe simgeleri'] };
    b.textContent = L2(plural[id] || area.name);
    const selected = anatomySelected === 'moonArc' ? 'sunArc' : anatomySelected;
    if (id === selected) b.classList.add('selected');
    b.addEventListener('click', () => { anatomySelected = id; drawAnatomy(); });
    buttons.appendChild(b);
  });
  drawAnatomyTable();
}
