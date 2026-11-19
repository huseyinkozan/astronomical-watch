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
    { "On", "Açık" },
    { "Off", "Kapalı" },
    { "Stay on", "Açık kalır" },
    { "Timeout", "Süreli" },
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
