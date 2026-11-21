#include "Language.h"

#include <cstdio>

static int s_language = Language::English;

static const char *s_dayNames[2][7] = {
    { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" },
    { "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi", "Pazar" } };
// The Turkish short days as in the always-on glyphs (tools/make_aod_glyphs.py).
static const char *s_shortDays[2][7] = { { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" },
                                         { "Pzt", "Sal", "Çar", "Per", "Cum", "Cmt", "Paz" } };
static const char *s_dayInitials[2][7] = { { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" },
                                           { "Pt", "Sa", "Ça", "Pe", "Cu", "Ct", "Pa" } };
static const char *s_monthNames[2][12] = {
    { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
      "December" },
    { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" } };
static const char *s_shortMonths[2][12] = {
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
    { "Oca", "Şub", "Mar", "Nis", "May", "Haz", "Tem", "Ağu", "Eyl", "Eki", "Kas", "Ara" } };
static const char *s_hijriMonths[2][12] = {
    { "Muharram", "Safar", "Rabi I", "Rabi II", "Jumada I", "Jumada II", "Rajab", "Shaban", "Ramadan", "Shawwal",
      "Dhu al-Qadah", "Dhu al-Hijjah" },
    { "Muharrem", "Safer", "Rebiülevvel", "Rebiülahir", "Cemaziyelevvel", "Cemaziyelahir", "Recep", "Şaban",
      "Ramazan", "Şevval", "Zilkade", "Zilhicce" } };

// In the order of Language::Text; English, then Turkish.
static const char *s_texts[][2] = {
    // Watch face.
    { "Sunset", "Gün batımı" },
    { "Sunrise", "Gün doğumu" },
    // Settings page.
    { "Screen off", "Ekran" },
    { "Always-on", "Bekleme" },
    { "Dev mode", "Geliştirme" },
    { "Auto off", "Oto kapanma" },
    { "On USB", "USB'de" },
    { "Language", "Dil" },
    { "Night mode", "Gece modu" },
    { "On", "Açık" },
    { "Off", "Kapalı" },
    { "Stay on", "Açık kalır" },
    { "Timeout", "Süreli" },
    { "Auto", "Oto" },
    { "Never", "Asla" },
    { "%d s", "%d sn" },
    { "%d min", "%d dk" },
    // Setup page.
    { "Wi-Fi setup", "Kurulum" },
    { "Network", "Ağ" },
    { "Password", "Şifre" },
    { "New password", "Yeni şifre" },
    { "%s  %d:%02d left", "%s  %d:%02d kaldı" },
    { "Not started", "Başlatılmadı" },
    // What the portal set.
    { "Setup portal set", "Kurulumda girilen" },
    { "Place", "Yer" },
    { "Lat/lon", "Konum" },
    { "Zone", "Bölge" },
    { "Offset", "Fark" },
    { "Change", "Değişim" },
    { "Hijri", "Hicri" },
    { "Clock set", "Saat ayarı" },
    { "fixed", "sabit" },
    { "none", "yok" },
    { "never", "hiç" },
    { "Crescent", "Hilal" },
    // Battery and about.
    { "Battery", "Pil" },
    { "on battery", "pilde" },
    { "trickle", "damla şarj" },
    { "pre-charge", "ön şarj" },
    { "constant current", "sabit akım" },
    { "constant voltage", "sabit gerilim" },
    { "done", "doldu" },
    { "not charging", "şarj yok" },
    { "unknown", "bilinmiyor" },
    { "MIT License", "MIT Lisansı" },
    // Brightness box.
    { "Always-on\nat night", "Bekleme\ngece" },
    { "Always-on\nby day", "Bekleme\ngündüz" },
    { "Brightness\n(night)", "Parlaklık\n(gece)" },
    { "Brightness\n(day)", "Parlaklık\n(gündüz)" },
    // Clock page.
    { "World clock", "Dünya saati" },
    { "Here", "Burası" },
    { "Cities: choose them\nin the setup portal", "Şehirleri kurulum\nsayfasında seçin" },
    { "same time", "aynı saat" },
    { "%c%d h", "%c%d sa" },
    { "Stopwatch", "Kronometre" },
    { "running since %s", "başlangıç %s" },
    { "stopped", "durdu" },
    { "Lap %d", "Tur %d" },
    { "Lap", "Tur" },
    { "Reset", "Sıfırla" },
    { "Start", "Başlat" },
    { "Stop", "Durdur" },
    { "Pause", "Duraklat" },
    { "Timer", "Geri sayım" },
    { "Add minutes", "Dakika ekleyin" },
    { "of %d:%02d, ends %s", "%d:%02d, bitiş %s" },
    { "done at %s", "bitti: %s" },
    { "of %d:%02d, paused", "%d:%02d, durdu" },
    { "Week %d, day %d", "%d. hafta, %d. gün" },
    { "Diyanet", "Diyanet" },
    { "Local crescent", "Yerel hilal" },
    { "Day counters", "Gün sayaçları" },
    { "Dates: set them\nin the setup portal", "Tarihleri kurulum\nsayfasında girin" },
    { "today", "bugün" },
    { "in %d d", "%d gün sonra" },
    { "%d d ago", "%d gün önce" },
    // Information pages.
    { "Sun", "Güneş" },
    { "Rise", "Doğuş" },
    { "Set", "Batış" },
    { "Noon", "Öğle" },
    { "Day", "Gün" },
    { "Now", "Şimdi" },
    { "Sunset in", "Batışa" },
    { "Sunrise in", "Doğuşa" },
    { "Twilight", "Alacakaranlık" },
    { "Dawn", "Sabah" },
    { "Dusk", "Akşam" },
    { "Astro", "Astronomik" },
    { "Nautical", "Denizci" },
    { "Civil", "Sivil" },
    { "Blue hour", "Mavi saat" },
    { "  until", "  bitiş" },
    { "Golden", "Altın saat" },
    { "Equinox", "Ekinoks" },
    { "Solstice", "Gündönümü" },
    { "Prayer times", "Namaz vakitleri" },
    { "Imsak", "İmsak" },
    { "Sunrise", "Güneş" },
    { "Dhuhr", "Öğle" },
    { "Asr", "İkindi" },
    { "Maghrib", "Akşam" },
    { "Isha", "Yatsı" },
    { "%s in %s", "%s %s sonra" },
    { "Diyanet, %s", "Diyanet, %s" },
    { "Diyanet (approx.), %s", "Diyanet (yaklaşık), %s" },
    { "Moon", "Ay" },
    { "Lit", "Aydınlık" },
    { "Tides", "Gelgit" },
    { "Dark sky", "Karanlık" },
    { "Crescent", "Hilal" },
    { "spring", "güçlü" },
    { "neap", "zayıf" },
    { "middle", "orta" },
    { "New Moon", "Yeni Ay" },
    { "Waxing crescent", "Büyüyen hilal" },
    { "First quarter", "İlk dördün" },
    { "Waxing gibbous", "Büyüyen Ay" },
    { "Full Moon", "Dolunay" },
    { "Waning gibbous", "Küçülen Ay" },
    { "Last quarter", "Son dördün" },
    { "Waning crescent", "Küçülen hilal" },
    { "--", "--" },
    { "naked eye", "çıplak göz" },
    { "binoculars", "dürbün" },
    { "telescope", "teleskop" },
    { "not visible", "görünmez" },
    { "Sun compass", "Güneş pusulası" },
    { "N", "K" },
    { "Top towards the Sun\nSun %.0f\xC2\xB0 high, %.0f\xC2\xB0", "Üstü Güneş'e dönük\nGüneş %.0f\xC2\xB0, yön %.0f\xC2\xB0" },
    { "Sun below the horizon", "Güneş ufkun altında" },
    { "Sun too high", "Güneş çok yüksekte" },
    { "Time", "Zaman" },
    { "Local", "Yerel" },
    { "Solar", "Güneş" },
    { "Eq. time", "Denklem" },
    { "Ezani", "Ezani" },
    { "Qibla", "Kıble" },
    { "Sidereal", "Yıldız" },
    { "Week/day", "Hafta/gün" },
    { "%.1f d", "%.1f g" },
    // Page names.
    { "Clock", "Saat" },
    { "Info", "Bilgi" },
    { "Settings", "Ayar" },
    // Battery part.
    { "Unplugged %s", "Şarjsız %s" },
    { "%d d %d h", "%d g %d sa" },
    { "%d h %d min", "%d sa %d dk" },
    { "Start Wi-Fi setup\nabove; its QR code\nshows here", "Kurulumu yukarıda\nbaşlatın; QR kodu\nburada görünür" },
    // Planets page.
    { "Planets", "Gezegen" },
    { "Mercury", "Merkür" },
    { "Venus", "Venüs" },
    { "Mars", "Mars" },
    { "Jupiter", "Jüpiter" },
    { "Saturn", "Satürn" },
    { "Uranus", "Uranüs" },
    { "Neptune", "Neptün" },
    { "Rise and set", "Doğuş ve batış" },
    { "Distance", "Uzaklık" },
    { "AU", "AB" },
    { "Light", "Işık" },
    { "E", "D" },
    { "W", "B" },
    // The light time from 100 minutes on (Uranus, Neptune): "169 min" did not fit the column.
    { "%.1f h", "%.1f sa" },
    { "Alarm", "Alarm" },
    { "Tap to stop", "Durdurmak için dokun" },
    { "Alarms", "Alarmlar" },
    { "Alarms are set\nin the setup portal", "Alarmlar kurulum\nportalında kurulur" },
    { "On", "Açık" },
    { "Off", "Kapalı" },
    { "once", "bir kez" },
    { "daily", "her gün" },
    { "An alarm that is on\nuses the battery faster", "Açık alarm pili\ndaha çabuk bitirir" },
    // Launcher: at most 16 characters, the width of a row.
    { "Pages", "Sayfalar" },
    { "Calendar", "Miladi takvim" },
    { "Hijri calendar", "Hicri takvim" },
    { "About", "Hakkında" },
    { "Solar system", "Güneş sistemi" },
    { "Sky", "Gökyüzü" },
    { "Settings", "Ayarlar" },
    { "More settings", "Diğer ayarlar" },
    { "Portal values", "Kurulum bilgisi" },
    { "Wi-Fi setup", "Wi-Fi kurulum" },
    { "Wi-Fi QR code", "Wi-Fi QR kodu" },
    // Tools page; page names at most 16 characters, colours short enough for "SOS + ses  %100  2:00".
    { "Tools", "Araçlar" },
    { "Observing", "Gözlem" },
    { "Torch", "Fener" },
    { "Counter", "Sayaç" },
    { "Random", "Rastgele" },
    { "Red", "Kırmızı" },
    { "Amber", "Amber" },
    { "White", "Beyaz" },
    { "SOS", "SOS" },
    { "SOS + beep", "SOS + ses" },
    { "hold, then drag:\nsideways: colour\nup, down: light", "basılı tut, sürükle:\nyana: renk\nyukarı, aşağı: ışık" },
    { "%d%%", "%%%d" },
    { "%d/h", "%d/sa" },
    { "step %d / %d", "adım %d / %d" },
    { "No target", "Hedef yok" },
    { "Target %d", "Hedef %d" },
    { "Target 3x33", "Hedef 3x33" },
    { "hold number: reset", "sayıyı tut: sıfırla" },
    { "Dice", "Zar" },
    { "Coin", "Para" },
    { "Heads", "Tura" },
    { "Tails", "Yazı" },
    { "tap: roll", "dokun: at" },
    { "Dice again: 1-3", "Zar'a dokun: 1-3" },
    { "BOOT counter", "BOOT sayaç" },
    { "Back", "Geri" },
    // Calculator and unit converter; the units under the values at most 17 characters, the
    // hint's lines at most 19.
    { "Calculator", "Hesap makinesi" },
    { "Unit converter", "Birim çevirici" },
    { "Error", "Hata" },
    { "Angle ↔ time", "Açı ↔ zaman" },
    { "Distance", "Uzaklık" },
    { "Temperature", "Sıcaklık" },
    { "hour angle", "saat açısı" },
    { "degrees", "derece" },
    { "radians", "radyan" },
    { "kilometres", "kilometre" },
    { "astronomical unit", "astronomik birim" },
    { "light years", "ışık yılı" },
    { "parsecs", "parsek" },
    { "°C Celsius", "°C santigrat" },
    { "°F Fahrenheit", "°F fahrenhayt" },
    { "K Kelvin", "K kelvin" },
    { "%s%dh %dm %ds", "%s%ds %dd %dsn" },
    { "tap a value: enter\ntap the title: kind", "değere dokun: gir\nbaşlığa dokun: tür" },
    // Spirit level: the angles' row at most 21 characters ("Pitch -89°  Roll -89°"), the
    // labels under the values at most 17, the hints' lines at most 19 (20 for the last line).
    { "Spirit level", "Su terazisi" },
    { "Level", "Terazi" },
    { "Height", "Yükseklik" },
    { "Pitch %s  Roll %s", "Eğim %s  Yan %s" },
    { "Slope %.1f°", "Yamaç %.1f°" },
    { "tap the title: mode", "başlığa dokun: mod" },
    { "edge altitude", "kenar yüksekliği" },
    { "held: tap again", "tutuldu: dokun" },
    { "distance", "uzaklık" },
    { "height above eye", "gözden yükseklik" },
    { "sight along the top\ntap the angle: hold", "üst kenarla nişan al\naçıya dokun: tut" },
    // Astrolabe: titles at most 19 characters, the rows at most 21, the hints' lines 19.
    { "Astrolabe", "Usturlap" },
    { "Dial", "Kadran" },
    { "Sun altitude", "Güneş yüksekliği" },
    { "Polaris", "Kutup Yıldızı" },
    { "Sundial", "Güneş saati" },
    { "Analemma", "Analemma" },
    { "drag: time", "sürükle: zaman" },
    { "tap: now", "dokun: şimdi" },
    { "Time %02d:%02d", "Saat %02d:%02d" },
    { "Latitude %.1f°", "Enlem %.1f°" },
    { "aim by the shadow\ntap: hold", "gölgesiyle nişan al\ndokun: tut" },
    { "sight along the top\ntap: hold", "üst kenarla nişan al\ndokun: tut" },
    { "Solar time %02d:%02d", "Güneş saati %02d:%02d" },
    { "Clock %02d:%02d", "Saat %02d:%02d" },
    { "Eq. time %s%dm %02ds", "Denklem %s%dd %02dsn" },
    { "Declination %.1f°", "Dik açıklık %.1f°" },
    { "Noon altitude %.1f°", "Öğle yüksekliği %.1f°" },
    // Observing page: polar alignment, Milky Way window, deep-sky finder (SkyParts).
    { "Polar alignment", "Kutup hizalama" },
    { "Milky Way", "Samanyolu" },
    { "Deep-sky finder", "Derin gökyüzü" },
    { "Polaris at %d:%02d", "Kutup Yıldızı %d:%02d" },
    { "Sigma Oct at %d:%02d", "Sigma Oct %d:%02d" },
    { "Hour angle %dh %02dm", "Saat açısı %ds %02dd" },
    { "Inverted (tap flips)", "Ters (dokun: düz)" },
    { "Upright (tap flips)", "Düz (dokun: ters)" },
    { "No location set", "Konum girilmedi" },
    { "Milky Way window", "Samanyolu penceresi" },
    { "Tonight %02d:%02d-%02d:%02d", "Bu gece %02d:%02d-%02d:%02d" },
    { "Highest %02d:%02d %d°", "En yüksek %02d:%02d %d°" },
    { "No window tonight", "Bu gece pencere yok" },
    { "No dark night", "Gece kararmıyor" },
    { "Centre stays low", "Merkez hep alçakta" },
    { "Moon %d%%", "Ay %%%d" },
    { "Moon %d%%, faint", "Ay %%%d, sönük" },
    { "Moon %d%%, down", "Ay %%%d, batık" },
    { "Moon %d%%, up", "Ay %%%d, gökte" },
    { "Moon %d%% sets %02d:%02d", "Ay %%%d batar %02d:%02d" },
    { "Moon %d%% rises %02d:%02d", "Ay %%%d doğar %02d:%02d" },
    { "Next: %d %s", "Sonraki: %d %s" },
    { "None in 120 nights", "120 gecede yok" },
    { "Up now: %d", "Şimdi gökte: %d" },
    { "%d°  best %02d:%02d", "%d°  en iyi %02d:%02d" },
    { "None above 10°", "10° üstünde yok" },
    // Observing page: eyepieces and night photo (OpticsParts).
    { "Eyepieces", "Mercekler" },
    { "Night photo", "Gece fotoğrafı" },
    { "Scope %d mm f/%s", "Teleskop %dmm f/%s" },
    { "Eyepiece %s mm %d°", "Mercek %s mm %d°" },
    { "No eyepiece set", "Mercek girilmedi" },
    { "Magnification", "Büyütme" },
    { "Exit pupil", "Çıkış bebeği" },
    { "True field", "Gerçek alan" },
    { "Limit mag.", "Sınır kadir" },
    { "Resolution", "Ayırma" },
    { "Tap: next eyepiece", "Dokun: sonraki mercek" },
    { "No star trails", "Yıldız izi olmadan" },
    { "ND filter", "ND filtre" },
    { "Star trails", "Yıldız izi" },
    { "500 rule", "500 kuralı" },
    { "NPF rule", "NPF kuralı" },
    { "35 mm equiv.", "Eşdeğer odak" },
    { "Factor", "Çarpan" },
    { "Exposure", "Pozlama" },
    { "%d min every %d s", "%d dk, %d sn arayla" },
    { "Frames", "Kare" },
    { "Trail arc", "İz yayı" },
    { "Tap title: next", "Başlık: sonraki hesap" },
    { "%dm %02ds", "%ddk %02dsn" },
    { "%dh %02dm", "%dsa %02ddk" },
};
// One pair per text: a missing one would shift every text after it.
static_assert( sizeof( s_texts ) / sizeof( s_texts[0] ) == Language::TextCount, "a text pair per Language::Text" );

void Language::set( int language )
{
    s_language = language == Turkish ? Turkish : English;
}

int Language::current()
{
    return s_language;
}

static int clampIndex( int value, int count )
{
    return value < 0 ? 0 : ( value >= count ? count - 1 : value );
}

const char *Language::dayName( int weekday )
{
    return s_dayNames[s_language][clampIndex( weekday, 7 )];
}

const char *Language::shortDay( int weekday )
{
    return s_shortDays[s_language][clampIndex( weekday, 7 )];
}

const char *Language::dayInitials( int weekday )
{
    return s_dayInitials[s_language][clampIndex( weekday, 7 )];
}

const char *Language::monthName( int month )
{
    return s_monthNames[s_language][clampIndex( month - 1, 12 )];
}

const char *Language::shortMonth( int month )
{
    return s_shortMonths[s_language][clampIndex( month - 1, 12 )];
}

// The first letters of the weekdays (Monday first), for a row's chosen days ("P.Ç.C..").
static const char *s_dayLetters[2][7] = { { "M", "T", "W", "T", "F", "S", "S" }, { "P", "S", "Ç", "P", "C", "C", "P" } };

const char *Language::dayLetter( int weekday )
{
    return s_dayLetters[s_language][clampIndex( weekday, 7 )];
}

// Alarms::Event, short enough for a row with an offset ("Gün batımı -120").
static const char *s_alarmEvents[2][17] = {
    { "Sunrise", "Sunset", "Civil dawn", "Civil dusk", "Naut. dawn", "Naut. dusk", "Astro dawn", "Astro dusk",
      "Solar noon", "Moonrise", "Moonset", "Imsak", "Shuruq", "Dhuhr", "Asr", "Maghrib", "Isha" },
    { "Gün doğumu", "Gün batımı", "Sivil sabah", "Sivil akşam", "Denizci sabah", "Denizci akşam", "Astro sabah",
      "Astro akşam", "Güneş öğlesi", "Ay doğuşu", "Ay batışı", "İmsak", "Güneş vakti", "Öğle", "İkindi", "Akşam",
      "Yatsı" } };

const char *Language::alarmEvent( int event )
{
    return s_alarmEvents[s_language][clampIndex( event, 17 )];
}

// The Messier objects' common names, short enough for a row with the number and the magnitude.
struct MessierName
{
    int number;
    const char *names[2];
};
static const MessierName s_messierNames[] = {
    { 1, { "Crab", "Yengeç" } },          { 6, { "Butterfly", "Kelebek" } },    { 7, { "Ptolemy", "Batlamyus" } },
    { 8, { "Lagoon", "Lagün" } },         { 11, { "Wild Duck", "Yaban Ördek" } }, { 13, { "Hercules", "Herkül" } },
    { 16, { "Eagle", "Kartal" } },        { 17, { "Omega", "Omega" } },         { 20, { "Trifid", "Trifid" } },
    { 27, { "Dumbbell", "Halter" } },     { 31, { "Andromeda", "Andromeda" } }, { 33, { "Triangulum", "Üçgen" } },
    { 42, { "Orion", "Orion" } },         { 44, { "Beehive", "Arı Kovanı" } }, { 45, { "Pleiades", "Ülker" } },
    { 51, { "Whirlpool", "Girdap" } },    { 57, { "Ring", "Halka" } },          { 63, { "Sunflower", "Ayçiçeği" } },
    { 64, { "Black Eye", "Kara Göz" } },  { 76, { "Cork", "Mantar" } }, { 81, { "Bode's", "Bode" } },
    { 82, { "Cigar", "Puro" } },          { 83, { "S. Pinwheel", "G. Fırıldak" } }, { 87, { "Virgo A", "Virgo A" } },
    { 97, { "Owl", "Baykuş" } },          { 101, { "Pinwheel", "Fırıldak" } },  { 104, { "Sombrero", "Sombrero" } },
};

const char *Language::messierName( int number )
{
    for ( size_t i = 0; i < sizeof( s_messierNames ) / sizeof( s_messierNames[0] ); ++i ) {
        if ( s_messierNames[i].number == number ) {
            return s_messierNames[i].names[s_language];
        }
    }
    return "";
}

const char *Language::hijriMonth( int month )
{
    return s_hijriMonths[s_language][clampIndex( month - 1, 12 )];
}

void Language::longDate( char *text, size_t size, int weekday, int day, int month )
{
    if ( weekday < 0 ) {
        snprintf( text, size, "%d %s", day, monthName( month ) );
    } else if ( s_language == Turkish ) {
        snprintf( text, size, "%d %s %s", day, monthName( month ), dayName( weekday ) );
    } else {
        snprintf( text, size, "%s %d %s", dayName( weekday ), day, monthName( month ) );
    }
}

const char *Language::text( Text id )
{
    return s_texts[clampIndex( id, TextCount )][s_language];
}
