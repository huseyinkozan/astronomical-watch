// The site: the language switch, the feature and guide lists, and the watch preview, which
// moves between the watch's screenshots as the watch does (tools/screenshot.py takes them).
'use strict';

// The installer's port picker lists only Espressif's own USB (vendor 0x303a: the ESP32-S3's
// USB-Serial/JTAG, also in the download mode), not every serial port of the computer: ESP Web
// Tools asks without a filter.
if (navigator.serial) {
  const requestPort = navigator.serial.requestPort.bind(navigator.serial);
  navigator.serial.requestPort = (options) =>
    requestPort(options && options.filters ? options : { ...options, filters: [{ usbVendorId: 0x303a }] });
}

let language = location.hash === '#tr' || navigator.language.startsWith('tr') ? 'tr' : 'en';

const features = [
  ['Face pages composed on the setup page: a digital or analog clock, large or among the Sun\'s and the Moon\'s paths, with the Milky Way and the planets',
   'Kurulum sayfasında düzenlenen yüz sayfaları: dijital ya da analog saat, büyük ya da Güneş\'in ve Ay\'ın yolları arasında, Samanyolu ve gezegenlerle'],
  ['Sun and Moon: rise, set, twilights, golden and blue hours, phase, the new crescent, the Hijri date',
   'Güneş ve Ay: doğuş, batış, alacakaranlıklar, altın ve mavi saat, evre, yeni hilal, Hicri tarih'],
  ['Prayer times by Diyanet\'s method, the qibla time, solar, ezani and sidereal time',
   'Diyanet yöntemiyle namaz vakitleri, kıble saati, güneş, ezani ve yıldız zamanı'],
  ['Planets: the solar system, the sky, rise and set, distances', 'Gezegenler: Güneş sistemi, gökyüzü, doğuş ve batış, uzaklıklar'],
  ['Clock page: world clock, stopwatch, countdown with a regatta start, Gregorian and Hijri calendars, day counters, alarms at clock times or sky events',
   'Saat sayfası: dünya saati, kronometre, regata startlı geri sayım, Miladi ve Hicri takvim, gün sayaçları, saatte ya da gök olayında alarmlar'],
  ['Tools: torch with SOS, calculator, unit converter, spirit level, counter, dice, metronome, emergency card',
   'Araçlar: SOS\'lu fener, hesap makinesi, birim çevirici, su terazisi, sayaç, zar, metronom, acil durum kartı'],
  ['Observing: astrolabe, polar alignment, Milky Way window, deep-sky finder, eyepiece and photo calculators, nautical almanac, dish and panel aiming',
   'Gözlem: usturlap, kutup hizalama, Samanyolu penceresi, derin gök bulucu, mercek ve fotoğraf hesapları, seyir almanağı, çanak ve panel yönlendirme'],
  ['Cycle page: menstrual cycle tracking on the watch alone, with an optional PIN; estimates, not for contraception',
   'Döngü sayfası: yalnız saatte adet döngüsü takibi, isteğe bağlı PIN ile; tahminler korunma için değildir'],
  ['A dim always-on face, a red night mode, gauges in the screen corners', 'Soluk bekleme yüzü, gece kırmızısı modu, ekran köşelerinde göstergeler'],
  ['Setup page over the watch\'s own Wi-Fi; English and Turkish', 'Saatin kendi Wi-Fi\'ı üzerinden kurulum sayfası; İngilizce ve Türkçe'],
];

const guide = [
  ['PWR: wakes the watch, and sends it to the always-on face or to sleep', 'PWR: saati uyandırır, bekleme yüzüne ya da uykuya gönderir'],
  ['Swipe sideways: the face, the page list, the page opened from it; up and down: face pages and parts',
   'Yana kaydırma: ön yüz, sayfa listesi, oradan açılan sayfa; yukarı ve aşağı: yüz sayfaları ve parçalar'],
  ['BOOT or the "‹" at the left edge: one step back', 'BOOT ya da sol kenardaki "‹": bir adım geri'],
  ['A tap on the face opens what it shows: the Sun, the Moon, a corner, a line, the clock', 'Ön yüze dokunma gösterdiği şeyi açar: Güneş, Ay, köşe, satır, saat'],
  ['A long press on the face, then a drag: the brightness', 'Ön yüze uzun basıp sürükleme: parlaklık'],
  ['Settings, Wi-Fi setup: the setup page for the time, the place, faces, alarms and updates',
   'Ayarlar, Wi-Fi kurulum: zaman, yer, yüzler, alarmlar ve güncellemeler için kurulum sayfası'],
];

// The stacked pages and their parts in the launcher's default order, with the watch's own names
// (main/Language.cpp) and their screenshots (docs/screenshots/*/README.md).
const pages = [
  { name: ['Clock', 'Saat'], parts: [['1-world', 'World clock', 'Dünya saati'], ['2-stopwatch', 'Stopwatch', 'Kronometre'],
    ['3-timer', 'Timer', 'Geri sayım'], ['4-gregorian', 'Calendar', 'Miladi takvim'], ['5-hijri', 'Hijri calendar', 'Hicri takvim'],
    ['6-days', 'Day counters', 'Gün sayaçları'], ['24-alarms', 'Alarms', 'Alarmlar']] },
  { name: ['Info', 'Bilgi'], parts: [['7-sun', 'Sun', 'Güneş'], ['8-twilight', 'Twilight', 'Alacakaranlık'],
    ['9-prayer', 'Prayer times', 'Namaz vakitleri'], ['10-moon', 'Moon', 'Ay'], ['11-compass', 'Sun compass', 'Güneş pusulası'],
    ['12-time', 'Time', 'Zaman']] },
  { name: ['Tools', 'Araçlar'], parts: [['26-torch', 'Torch', 'Fener'], ['29-calculator', 'Calculator', 'Hesap makinesi'],
    ['30-converter', 'Unit converter', 'Birim çevirici'], ['31-level', 'Spirit level', 'Su terazisi'], ['27-counter', 'Counter', 'Sayaç'],
    ['28-random', 'Random', 'Rastgele'], ['40-metronome', 'Metronome', 'Metronom'], ['41-card', 'Emergency card', 'Acil durum kartı']] },
  { name: ['Observing', 'Gözlem'], parts: [['33-polar', 'Polar alignment', 'Kutup hizalama'], ['34-milky-way', 'Milky Way', 'Samanyolu'],
    ['35-deep-sky', 'Deep-sky finder', 'Derin gökyüzü'], ['36-eyepiece', 'Eyepieces', 'Mercekler'], ['37-photo', 'Night photo', 'Gece fotoğrafı'],
    ['38-almanac', 'Nautical almanac', 'Seyir almanağı'], ['32-astrolabe', 'Astrolabe', 'Usturlap'], ['39-aiming', 'Aiming', 'Çanak ve panel']] },
  { name: ['Planets', 'Gezegen'], parts: [['20-orbits', 'Solar system', 'Güneş sistemi'], ['21-sky', 'Sky', 'Gökyüzü'],
    ['22-rise-set', 'Rise and set', 'Doğuş ve batış'], ['23-distance', 'Distance', 'Uzaklık']] },
  { name: ['Settings', 'Ayarlar'], parts: [['14-settings', 'Settings', 'Ayarlar'], ['15-settings-2', 'More settings', 'Diğer ayarlar'],
    ['16-setup-info', 'Portal values', 'Kurulum bilgisi'], ['17-setup', 'Wi-Fi setup', 'Wi-Fi kurulum'],
    ['19-setup-code', 'Wi-Fi QR code', 'Wi-Fi QR kodu'], ['13-battery', 'Battery', 'Pil'], ['18-about', 'About', 'Hakkında'],
    ['42-reset', 'Factory reset', 'Fabrika ayarları']] },
  { name: ['Cycle', 'Döngü'], parts: [['43-cycle', 'Cycle', 'Döngü'], ['44-cycle-calendar', 'Calendar', 'Takvim'],
    ['45-cycle-history', 'History', 'Geçmiş'], ['46-cycle-moon', 'With the Moon', 'Ay ile'], ['47-cycle-pregnancy', 'Pregnancy', 'Gebelik'],
    ['48-cycle-hayiz', 'Hayz', 'Hayız'], ['49-cycle-settings', 'Cycle settings', 'Döngü ayarları']] },
];
const FACE_PAGES = 5;
const W = 410;
const H = 502;
const state = { tile: 0, face: 0, page: 0, part: 0, view: 0, aod: false, red: false };
// The views of a part beyond its first (views.js, made by tools/make_site.sh): a tap steps them.
function viewCount() { return (typeof partViews !== 'undefined' && partViews[pages[state.page].parts[state.part][0]]) || 1; }

const screen = document.getElementById('screen');
const track = document.getElementById('track');
const list = document.getElementById('list');
const faceTile = document.getElementById('faceTile');
const pageTile = document.getElementById('pageTile');
const images = {};

function index() { return language === 'tr' ? 1 : 0; }

// As the watch's NightRed: Rec. 601 brightness in red, near-black left black, a dimmest red.
function nightRed(context) {
  const data = context.getImageData(0, 0, W, H);
  const p = data.data;
  for (let i = 0; i < p.length; i += 4) {
    if (p[i] < 32 && p[i + 1] < 32 && p[i + 2] < 32) {
      p[i] = p[i + 1] = p[i + 2] = 0;
      continue;
    }
    p[i] = Math.max(48, (77 * p[i] + 150 * p[i + 1] + 29 * p[i + 2]) >> 8);
    p[i + 1] = 0;
    p[i + 2] = 0;
  }
  context.putImageData(data, 0, 0);
}

// A tile's picture; `slide` 1 brings it in from below (the next page, as a swipe up on the
// watch), -1 from above, 0 at once. The old picture moves out the other way.
const SLIDE_MS = 280;
const shown = new Map();  // the name each canvas shows, so that a redraw of the same one stays still
function paint(canvas, name, slide) {
  const context = canvas.getContext('2d', { willReadFrequently: true });
  const src = 'screenshots/' + language + '/' + name + '.png';
  const key = src + (state.red ? ' red' : '');
  if (shown.get(canvas) === key) return;
  shown.set(canvas, key);
  // The picture shown until now, kept for the slide.
  const old = document.createElement('canvas');
  old.width = W;
  old.height = H;
  if (slide) old.getContext('2d').drawImage(canvas, 0, 0);
  const show = (img) => {
    if (shown.get(canvas) !== key) return;  // a later page was asked for meanwhile
    const fresh = document.createElement('canvas');
    fresh.width = W;
    fresh.height = H;
    const f = fresh.getContext('2d', { willReadFrequently: true });
    f.fillStyle = '#000';
    f.fillRect(0, 0, W, H);
    f.drawImage(img, 0, 0, W, H);
    if (state.red) nightRed(f);
    if (!slide) {
      context.drawImage(fresh, 0, 0);
      return;
    }
    const start = performance.now();
    const frame = (now) => {
      if (shown.get(canvas) !== key) return;
      const t = Math.min(1, (now - start) / SLIDE_MS);
      const eased = 1 - Math.pow(1 - t, 3);
      context.fillStyle = '#000';
      context.fillRect(0, 0, W, H);
      context.drawImage(old, 0, -slide * eased * H);
      context.drawImage(fresh, 0, slide * (1 - eased) * H);
      if (t < 1) requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
  };
  if (images[src] && images[src].complete) {
    show(images[src]);
  } else {
    const img = images[src] || new Image();
    images[src] = img;
    img.addEventListener('load', () => show(img), { once: true });
    img.src = src;
  }
}

// ---- The launcher: the watch's geometry (Display.cpp) in HTML ----

// The corner gauges over the HTML list, cut from the launcher's screenshot: only the pixels of
// their quarter rings (radius about 112 to 117 around the corner circles' centres, 120 px in).
let gaugeKey = '';
function paintGauges() {
  const key = language + (state.red ? ' red' : '');
  if (key === gaugeKey) return;
  gaugeKey = key;
  const img = new Image();
  img.onload = () => {
    if (gaugeKey !== key) return;
    const context = document.getElementById('gaugeLayer').getContext('2d', { willReadFrequently: true });
    context.clearRect(0, 0, W, H);
    context.drawImage(img, 0, 0, W, H);
    if (state.red) nightRed(context);
    const data = context.getImageData(0, 0, W, H);
    const p = data.data;
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        const dx = x < 120 ? 120 - x : (x > W - 120 ? x - (W - 120) : -1);
        const dy = y < 120 ? 120 - y : (y > H - 120 ? y - (H - 120) : -1);
        const d = Math.hypot(dx, dy);
        if (dx < 0 || dy < 0 || d < 104 || d > 124) p[(y * W + x) * 4 + 3] = 0;
      }
    }
    context.putImageData(data, 0, 0);
  };
  img.src = 'screenshots/' + language + '/25-launcher.png';
}

const LIST_TOP = 44;
const NAME_HEIGHT = 44;
const ROW_HEIGHT = 68;
const ROW_GAP = 8;
const ROW_X = 52;
const ROW_WIDTH = 306;
const NARROWEST = 96;
let rows = [];

function buildLauncher() {
  document.getElementById('launcherTitle').textContent = language === 'tr' ? 'Sayfalar' : 'Pages';
  list.innerHTML = '';
  rows = [];
  let y = 8;
  pages.forEach((page, p) => {
    const name = document.createElement('div');
    name.className = 'name';
    name.textContent = page.name[index()];
    name.style.top = (y + 6) + 'px';
    list.appendChild(name);
    rows.push({ el: name, top: y + 6, row: false });
    y += NAME_HEIGHT;
    page.parts.forEach((part, i) => {
      const row = document.createElement('div');
      row.className = 'row';
      row.textContent = part[1 + index()];
      row.style.top = y + 'px';
      row.dataset.page = p;
      row.dataset.part = i;
      list.appendChild(row);
      rows.push({ el: row, top: y, row: true });
      y += ROW_HEIGHT + ROW_GAP;
    });
  });
  list.dataset.height = y + 8 + ROW_HEIGHT + ROW_GAP;
  narrow();
}

// The leftmost x clear of the corner gauges between screen rows top and bottom (Display.cpp,
// launcherLeft): rows narrow and dim near the corners.
function launcherLeft(top, bottom) {
  const clear = 117 - 5 - 4;
  const lowerCentre = H - 1 - 120;
  let dy = 0;
  if (top < 120) dy = 120 - top;
  if (bottom > lowerCentre && bottom - lowerCentre > dy) dy = bottom - lowerCentre;
  if (dy >= clear) return W / 2;
  return 120 - Math.sqrt(clear * clear - dy * dy);
}

function narrow() {
  const scroll = list.scrollTop;
  rows.forEach((r) => {
    const top = LIST_TOP + r.top - scroll;
    const bottom = top + (r.row ? ROW_HEIGHT : 32) - 1;
    if (r.row) {
      let width = Math.floor((W / 2 - launcherLeft(top, bottom)) * 2) & ~1;
      width = Math.max(NARROWEST, Math.min(ROW_WIDTH, width));
      r.el.style.left = ((W - width) / 2) + 'px';
      r.el.style.width = width + 'px';
      r.el.style.opacity = 0.3 + 0.7 * (width - NARROWEST) / (ROW_WIDTH - NARROWEST);
      r.el.classList.toggle('current', +r.el.dataset.page === state.page && +r.el.dataset.part === state.part);
    } else {
      const x = Math.max(36, launcherLeft(top, bottom));
      r.el.style.left = x + 'px';
      r.el.style.opacity = Math.max(0.3, 1 - 0.7 * (x - 36) / ((W - NARROWEST) / 2 - 36));
    }
  });
}

// As the watch: the list comes with the last opened part's row second from the top.
function showCurrentRow() {
  const row = rows.find((r) => r.row && +r.el.dataset.page === state.page && +r.el.dataset.part === state.part);
  if (row) scrollList(row.top - (ROW_HEIGHT + ROW_GAP) - 8);
}

function scrollList(top) {
  const max = Math.max(0, +list.dataset.height - list.clientHeight);
  list.scrollTop = Math.max(0, Math.min(max, top));
  narrow();
}

// ---- Moving between tiles, pages and parts ----

function draw(slide) {
  screen.classList.toggle('red', state.red);
  paintGauges();
  paint(faceTile, (state.aod ? 'always-on-' : 'face-') + state.face, state.tile === 0 ? slide : 0);
  const shot = pages[state.page].parts[state.part][0];
  paint(pageTile, state.view > 0 ? shot + '-' + (state.view + 1) : shot, state.tile === 2 ? slide : 0);
  screen.classList.toggle('views', state.tile === 2 && viewCount() > 1);
  track.style.transform = 'translateX(' + (-state.tile * W) + 'px)';
  narrow();
  const names = [['Watch face', 'Ön yüz'], ['Page list', 'Sayfa listesi']];
  let where = '';
  if (state.tile === 0) {
    where = (state.aod ? (language === 'tr' ? 'Bekleme yüzü ' : 'Always-on face ') : names[0][index()] + ' ') + (state.face + 1) + ' / ' + FACE_PAGES;
  } else if (state.tile === 1) {
    where = names[1][index()];
  } else {
    const page = pages[state.page];
    where = page.name[index()] + ': ' + page.parts[state.part][1 + index()];
  }
  document.getElementById('where').textContent = where;
  const key = state.tile === 0 ? (state.aod ? 'always-on' : 'face') : state.tile === 1 ? 'launcher' : pages[state.page].parts[state.part][0];
  document.getElementById('note').textContent = notes[key] ? notes[key][index()] : '';
  // Pages with more views than one: a click on the screen steps them.
  const hint = document.getElementById('viewsHint');
  hint.hidden = !(state.tile === 2 && viewCount() > 1);
  hint.textContent = (language === 'tr' ? 'Görünüm: ' : 'View: ') + (state.view + 1) + '/' + viewCount() + '\n'
    + (language === 'tr' ? 'Sıradakini görmek için ekrana tıklayın.' : 'Click the screen for the next one.');
}

function move(direction) {
  if (state.aod) {
    return;  // the always-on face takes no swipes: PWR wakes it
  }
  if (direction === 'left' && state.tile < 2) {
    state.tile += 1;  // a swipe to the left brings the tile on the right, as on the watch
    if (state.tile === 1) showCurrentRow();
  } else if (direction === 'right' && state.tile > 0) {
    state.tile -= 1;
  } else if (direction === 'up' || direction === 'down') {
    const step = direction === 'up' ? 1 : -1;
    if (state.tile === 0) {
      state.face = (state.face + step + FACE_PAGES) % FACE_PAGES;
    } else if (state.tile === 2) {
      // Round the parts, back to the first after the last.
      const count = pages[state.page].parts.length;
      state.part = (state.part + step + count) % count;
      state.view = 0;
    } else {
      scrollList(list.scrollTop + step * (ROW_HEIGHT + ROW_GAP));
    }
    draw(step);
    return;
  }
  draw();
}

function back() {
  if (state.aod) return;
  if (state.tile > 0) {
    state.tile -= 1;
    draw();
  }
}

function power() {
  // Active mode ends on the always-on face of the page shown; a press wakes it again.
  state.aod = !state.aod;
  state.tile = 0;
  draw();
}

// ---- Input: a drag on the screen as on the watch ----

let start = null;
screen.addEventListener('pointerdown', (e) => {
  start = { x: e.clientX, y: e.clientY, scroll: list.scrollTop, scale: screen.getBoundingClientRect().width / W };
  screen.setPointerCapture(e.pointerId);
});
screen.addEventListener('pointermove', (e) => {
  if (!start || state.tile !== 1 || state.aod) return;
  const dy = (e.clientY - start.y) / start.scale;
  const dx = (e.clientX - start.x) / start.scale;
  if (Math.abs(dy) > Math.abs(dx)) scrollList(start.scroll - dy);
});
screen.addEventListener('pointerup', (e) => {
  if (!start) return;
  const dx = (e.clientX - start.x) / start.scale;
  const dy = (e.clientY - start.y) / start.scale;
  const s = start;
  start = null;
  if (Math.max(Math.abs(dx), Math.abs(dy)) < 12) {
    // A tap: a row of the list, or the "‹" at the left edge.
    const hit = document.elementFromPoint(s.x, s.y);
    if (state.tile === 1 && hit && hit.classList.contains('row')) {
      state.page = +hit.dataset.page;
      state.part = +hit.dataset.part;
      state.view = 0;
      state.tile = 2;
      draw();
    } else if (state.tile >= 1 && (hit && hit.id === 'launcherBack' || (s.x - screen.getBoundingClientRect().left) / s.scale < 40)) {
      back();
    } else if (state.tile === 2 && viewCount() > 1) {
      state.view = (state.view + 1) % viewCount();
      draw();
    }
    return;
  }
  if (Math.abs(dx) > Math.abs(dy) && Math.abs(dx) > 40) move(dx < 0 ? 'left' : 'right');
  else if (state.tile !== 1 && Math.abs(dy) > 40) move(dy < 0 ? 'up' : 'down');
});
// The wheel steps the face pages and the parts, one step per turn of a notch or a touchpad flick.
let wheelTime = 0;
screen.addEventListener('wheel', (e) => {
  if (state.tile === 1) return;  // the list scrolls by itself
  e.preventDefault();
  const now = Date.now();
  if (now - wheelTime < 250 || Math.abs(e.deltaY) < 4) return;
  wheelTime = now;
  move(e.deltaY > 0 ? 'up' : 'down');
}, { passive: false });
list.addEventListener('wheel', (e) => { e.preventDefault(); scrollList(list.scrollTop + e.deltaY); }, { passive: false });

function fit() {
  // The panel's own pixels, scaled to the space it has.
  const k = Math.min(1, (window.innerWidth - 80) / (W + 36));
  screen.style.transform = 'scale(' + k + ')';
  document.getElementById('scaler').style.width = (W * k) + 'px';
  document.getElementById('scaler').style.height = (H * k) + 'px';
}

function translate() {
  document.documentElement.lang = language;
  document.title = language === 'tr' ? 'Astronomik Saat' : 'Astronomical Watch';
  document.querySelectorAll('[data-en]').forEach((e) => { e.textContent = e.dataset[language]; });
  document.getElementById('language').textContent = language === 'tr' ? 'English' : 'Türkçe';
  document.getElementById('featureList').innerHTML = '';
  features.forEach((f) => { const li = document.createElement('li'); li.textContent = f[index()]; document.getElementById('featureList').appendChild(li); });
  document.getElementById('guideList').innerHTML = '';
  guide.forEach((g) => { const li = document.createElement('li'); li.textContent = g[index()]; document.getElementById('guideList').appendChild(li); });
  document.querySelectorAll('[data-shot-img]').forEach((img) => { img.src = 'screenshots/' + language + '/' + img.dataset.shotImg + '.png'; });
  document.querySelectorAll('[data-shot-bg]').forEach((e) => {
    e.style.backgroundImage = 'url(screenshots/' + language + '/' + e.dataset.shotBg + '.png)';
    if (e.dataset.scale) {
      // The portal's pictures are 400 px wide; the part from data-y-<language> down.
      const k = +e.dataset.scale;
      e.style.backgroundSize = (400 * k) + 'px auto';
      e.style.backgroundPosition = '0 ' + (-(+e.dataset['y' + (language === 'tr' ? 'Tr' : 'En')]) * k) + 'px';
    }
  });
  document.querySelectorAll('.portal-shot img').forEach((img) => {
    img.src = 'screenshots/' + language + '/' + img.parentElement.dataset.shot + '.png';
  });
  if (typeof showRelease === 'function') showRelease();
  buildLauncher();
  draw();
  drawAnatomy();
}

document.querySelectorAll('[data-move]').forEach((b) => b.addEventListener('click', () => move(b.dataset.move)));
// A click outside a dialog's box (on the dimmed page) closes it.
document.querySelectorAll('dialog').forEach((d) => d.addEventListener('click', (e) => {
  const r = d.getBoundingClientRect();
  if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) d.close();
}));
document.querySelectorAll('.portal-shot').forEach((b) => b.addEventListener('click', () => {
  document.getElementById('portalImage').src = 'screenshots/' + language + '/' + b.dataset.shot + '.png';
  document.getElementById('portalDialog').showModal();
  document.querySelector('#portalDialog .phone').scrollTop = 0;
}));
// The latest release from the system the site is served from: GitHub on github.io, otherwise
// GitLab (the main repository), the other one when the first gives nothing. The only requests to
// other hosts; nothing shows without them.
const releaseSources = [
  () => fetch('https://gitlab.com/api/v4/projects/huseyinkozan%2Fastronomical-watch/releases/permalink/latest')
    .then((r) => (r.ok ? r.json() : null))
    .then((r) => (r && r.tag_name ? { tag: r.tag_name, date: r.released_at, url: r._links.self,
      files: ((r.assets && r.assets.links) || []).map((l) => ({ name: l.name, url: l.direct_asset_url || l.url })) } : null)),
  () => fetch('https://api.github.com/repos/huseyinkozan/astronomical-watch/releases/latest')
    .then((r) => (r.ok ? r.json() : null))
    .then((r) => (r && r.tag_name ? { tag: r.tag_name, date: r.published_at, url: r.html_url,
      files: (r.assets || []).map((a) => ({ name: a.name, url: a.browser_download_url })) } : null)),
];
if (location.hostname.endsWith('github.io')) releaseSources.reverse();
releaseSources[0]().catch(() => null)
  .then((release) => release || releaseSources[1]().catch(() => null))
  .then((release) => {
    if (!release) return;
    latestRelease = release;
    showRelease();
  });
let latestRelease = null;
function showRelease() {
  const e = document.getElementById('release');
  if (!latestRelease) return;
  e.innerHTML = '';
  e.append((language === 'tr' ? 'Son sürüm: ' : 'Latest version: ') + latestRelease.tag + ' (' + latestRelease.date.slice(0, 10) + ') · ');
  const a = document.createElement('a');
  a.href = latestRelease.url;
  a.textContent = language === 'tr' ? 'sürüm notları' : 'release notes';
  e.append(a);
  e.hidden = false;
  // The download buttons go straight to the release's files.
  const update = latestRelease.files.find((f) => /^astronomical_watch-v[0-9.]+\.bin$/.test(f.name));
  const full = latestRelease.files.find((f) => /-full\.bin$/.test(f.name));
  if (update) {
    document.querySelectorAll('.update-link').forEach((a) => { a.href = update.url; });
    document.querySelectorAll('.update-name').forEach((n) => { n.textContent = update.name; });
  }
  if (full) {
    document.getElementById('downloadFull').href = full.url;
    document.querySelectorAll('#downloadFull, .full-name').forEach((n) => { n.textContent = full.name; });
  }
}
// Without Web Serial (Firefox, Safari, phones) the install cannot run: the button says so.
if (!('serial' in navigator)) {
  document.getElementById('installJump').setAttribute('aria-disabled', 'true');
  document.getElementById('installJump').tabIndex = -1;
  document.getElementById('noSerial').hidden = false;
}
// The install section's tabs: the browser's (default) and the command line's.
document.querySelectorAll('.tab').forEach((tab) => tab.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach((t) => {
    const on = t === tab;
    t.classList.toggle('selected', on);
    t.setAttribute('aria-selected', String(on));
    document.getElementById(t.dataset.tab).hidden = !on;
  });
}));
document.getElementById('portalStepsOpen').addEventListener('click', () => document.getElementById('portalStepsDialog').showModal());
document.getElementById('updateOpen').addEventListener('click', () => document.getElementById('updateDialog').showModal());
document.getElementById('helpOpen').addEventListener('click', () => document.getElementById('helpDialog').showModal());
document.getElementById('boot').addEventListener('click', back);
document.getElementById('pwr').addEventListener('click', power);
document.getElementById('red').addEventListener('click', (e) => {
  state.red = !state.red;
  e.currentTarget.setAttribute('aria-pressed', String(state.red));
  draw();
});
document.getElementById('language').addEventListener('click', () => { language = language === 'tr' ? 'en' : 'tr'; translate(); });
document.addEventListener('keydown', (e) => {
  const keys = { ArrowLeft: 'right', ArrowRight: 'left', ArrowUp: 'down', ArrowDown: 'up' };
  if (keys[e.key] && !e.target.closest('input')) { e.preventDefault(); move(keys[e.key]); }
});
window.addEventListener('resize', fit);
fit();
translate();
