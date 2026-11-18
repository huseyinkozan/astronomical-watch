#include "Display.h"

#include "Dates.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include "Styles.h"
#include "Fonts.h"
#include "Panel.h"
#include "Language.h"
#include "PartOrder.h"

#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "bsp/touch.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include "sdkconfig.h"

static const char *TAG = "display";

static const int s_lockTimeoutMs = 100;

// LVGL draw buffer: 40 lines (the BSP uses 100). LVGL waits for the transfer of one band before
// drawing the next into it. With sw_rotate esp_lvgl_port allocates a second buffer of the same
// size; the smaller bands leave internal RAM for the Wi-Fi setup portal (docs/hardware.md).
static const int s_bufferLines = 40;

// LVGL turns a press into a scroll, and drops its click, after 10 px of movement (0.8 mm): a
// fingertip on a wrist moves more than that during a tap. 30 px (2.4 mm) still leaves the page
// swipe responsive.
static const int s_scrollLimitPx = 30;

// Settings rows: 7.8 mm high and 26 mm wide, the largest that keeps four rows (and the setup
// page) inside the corner gauges with a margin. No vertical scroll competes with the page swipe.
static const int s_rowHeight = 96;
static const int s_rowWidth = 318;
static const int s_rowGap = 12;
static const Language::Text s_settingNames[Settings::PageItemCount] = {
    Language::ScreenOff, Language::AlwaysOnRow, Language::DevMode, Language::AutoOff, Language::OnUsb,
    Language::LanguageRow };
// Four rows fit on a part; the next ones go to the part below.
static const int s_rowsPerPart = 4;

// Page order, left to right.
enum Page
{
    FacePage,
    ClockPage,     // world clock, timers and calendars, one over the other (ClockPages)
    InfoPage,      // the information parts, one over the other (InfoPart)
    SettingsPage,  // the settings, and under them the setup portal
    PageCount
};
static_assert( PageCount == 4, "Display.h sizes m_tiles and m_built for four pages" );

enum SettingsPart
{
    SettingsRows,
    SettingsMoreRows,
    SetupInfoPart,  // what the setup portal set, read only
#if CONFIG_WATCH_SETUP_PORTAL
    SetupPart,
    SetupCodePart,  // the network as a QR code
#endif
    SettingsPartCount
};

// The development screenshots keep one number per screen: the pages, with the information page's
// parts in their own places. Part -1: no part.
struct Shot
{
    const char *name;
    int page;
    int part;
};
[[maybe_unused]] static const Shot s_shots[] = {
    { "face", FacePage, -1 },
    { "world", ClockPage, ClockPages::WorldPart }, { "stopwatch", ClockPage, ClockPages::StopwatchPart },
    { "timer", ClockPage, ClockPages::TimerPart }, { "gregorian", ClockPage, ClockPages::GregorianPart },
    { "hijri", ClockPage, ClockPages::HijriPart }, { "days", ClockPage, ClockPages::DaysPart },
    { "sun", InfoPage, Display::InfoSun }, { "twilight", InfoPage, Display::InfoTwilight },
    { "prayer", InfoPage, Display::InfoPrayer },
    { "moon", InfoPage, Display::InfoMoon }, { "compass", InfoPage, Display::InfoCompass },
    { "time", InfoPage, Display::InfoTime },
    { "battery", InfoPage, Display::InfoBattery }, { "settings", SettingsPage, SettingsRows },
    { "settings-2", SettingsPage, SettingsMoreRows }, { "setup-info", SettingsPage, SetupInfoPart },
#if CONFIG_WATCH_SETUP_PORTAL
    { "setup", SettingsPage, SetupPart },
#endif
    { "about", InfoPage, Display::InfoAbout },
#if CONFIG_WATCH_SETUP_PORTAL
    // Last, so that the numbers of the others stay those of the published pictures.
    { "setup-code", SettingsPage, SetupCodePart },
#endif
};
[[maybe_unused]] static const int s_shotCount = sizeof( s_shots ) / sizeof( s_shots[0] );


static void buildAboutPage( lv_obj_t *page )
{
    lv_obj_set_flex_flow( page, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_row( page, 24, 0 );
    lv_obj_set_scrollbar_mode( page, LV_SCROLLBAR_MODE_OFF );
    const char *lines[4] = { "Astronomical Watch", esp_app_get_description()->version,
                             "gitlab.com/huseyinkozan/\nastronomical-watch", Language::text( Language::License ) };
    for ( int i = 0; i < 4; ++i ) {
        lv_obj_t *label = lv_label_create( page );
        // Grey for the details, white for the name.
        Styles::text( label, &jetbrains_mono_26, i == 0 ? lv_color_white() : lv_color_hex( 0xA0A0A0 ) );
        Styles::centred( label );
        lv_obj_set_width( label, lv_pct( 100 ) );
        lv_label_set_long_mode( label, LV_LABEL_LONG_MODE_WRAP );
        lv_label_set_text( label, lines[i] );
    }
}

static lv_obj_t *createLabel( lv_obj_t *parent, const lv_font_t *font, lv_align_t align, int yOffset )
{
    lv_obj_t *label = lv_label_create( parent );
    Styles::text( label, font, lv_color_white() );
    lv_label_set_text( label, "" );
    lv_obj_align( label, align, 0, yOffset );
    // A press on the text belongs to the row around it.
    lv_obj_set_clickable( label, false );
    return label;
}

static lv_obj_t *createLabel( lv_obj_t *parent, const lv_font_t *font, int yOffset )
{
    return createLabel( parent, font, LV_ALIGN_CENTER, yOffset );
}

// The CO5300 takes window addresses that start on an even pixel and end on an odd one (BSP).
static void roundArea( lv_event_t *event )
{
    lv_area_t *area = static_cast<lv_area_t *>( lv_event_get_param( event ) );
    area->x1 = ( area->x1 >> 1 ) << 1;
    area->y1 = ( area->y1 >> 1 ) << 1;
    area->x2 = ( ( area->x2 >> 1 ) << 1 ) + 1;
    area->y2 = ( ( area->y2 >> 1 ) << 1 ) + 1;
}

// The same steps as bsp_display_start(), except that the panel is added with lvgl_port_add_disp().
// The BSP adds this QSPI panel as an RGB panel: on that path esp_lvgl_port reports each band as
// flushed as soon as its DMA transfer is queued, so LVGL draws the next band into the buffer that
// is still being sent (horizontal stripes on the panel), and it stores RGB panel callbacks into
// the SH8601 panel object. lvgl_port_add_disp() reports the band when the transfer is done.
static esp_lcd_panel_io_handle_t s_panelIo = nullptr;
static esp_lcd_panel_handle_t s_panel = nullptr;
static esp_lcd_touch_handle_t s_touch = nullptr;

// Settings::Mirror on the panel and the touch: the panel's own address order (MADCTL 0x36: MX
// 0x40 left-right, MY 0x80 top-bottom; docs/hardware.md, the column gap stays 22), so drawing
// costs nothing more and the always-on wakes keep it, and the touch points mirrored to match.
static void applyMirror( int mirror )
{
    if ( s_panelIo == nullptr ) {
        return;
    }
    const uint8_t madctl = static_cast<uint8_t>( ( ( mirror & 1 ) ? 0x40 : 0 ) | ( ( mirror & 2 ) ? 0x80 : 0 ) );
    esp_lcd_panel_io_tx_param( s_panelIo, ( 0x02 << 24 ) | ( 0x36 << 8 ), &madctl, 1 );
    if ( s_touch != nullptr ) {
        esp_lcd_touch_set_mirror_x( s_touch, ( mirror & 1 ) != 0 );
        esp_lcd_touch_set_mirror_y( s_touch, ( mirror & 2 ) != 0 );
    }
}

static void setBrightness( int percent )
{
    if ( s_panelIo != nullptr ) {
        Panel::setBrightness( s_panelIo, percent );
    }
}

// The brightness to set once LVGL has drawn its first frame (0: done). The panel keeps the old
// image (the always-on face) until then, so it stays dark.
static int s_pendingBrightness = 0;

static void onFirstFrame( lv_event_t * )
{
    if ( s_pendingBrightness > 0 ) {
        setBrightness( s_pendingBrightness );
        s_pendingBrightness = 0;
    }
}

static lv_display_t *startLvgl( lv_indev_t *&touch )
{
    lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
    // The port's 7 KB left 56 bytes while the face pages were swiped (v0.0.3 crashed): all
    // drawing runs on this stack.
    portConfig.task_stack = 9 * 1024;
    if ( lvgl_port_init( &portConfig ) != ESP_OK ) {
        return nullptr;
    }
    // Not bsp_display_new(): it ends with full brightness, which flashed the old image (the
    // always-on face) until LVGL's first frame. The panel stays dark until start() has drawn.
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t io = nullptr;
    if ( !Panel::openForLvgl( BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8, io, panel ) ) {
        return nullptr;
    }
    s_panelIo = io;
    s_panel = panel;
    lvgl_port_display_cfg_t displayConfig = {};
    displayConfig.io_handle = io;
    displayConfig.panel_handle = panel;
    displayConfig.buffer_size = BSP_LCD_H_RES * s_bufferLines;
    displayConfig.hres = BSP_LCD_H_RES;
    displayConfig.vres = BSP_LCD_V_RES;
    displayConfig.color_format = LV_COLOR_FORMAT_RGB565;
    // Buffer flags as in the BSP. With buff_dma set and sw_rotate cleared the touch controller
    // raised no interrupt at all; the cause is not known (docs/hardware.md, FT3168).
    displayConfig.flags.sw_rotate = true;
    displayConfig.flags.swap_bytes = true;
    lv_display_t *display = lvgl_port_add_disp( &displayConfig );
    if ( display == nullptr ) {
        return nullptr;
    }
    lv_display_add_event_cb( display, roundArea, LV_EVENT_INVALIDATE_AREA, nullptr );

    esp_lcd_touch_handle_t touchHandle = nullptr;
    const esp_err_t touchErr = bsp_touch_new( nullptr, &touchHandle );
    if ( touchErr == ESP_OK ) {
        lvgl_port_touch_cfg_t touchConfig = {};
        touchConfig.disp = display;
        touchConfig.handle = touchHandle;
        touch = lvgl_port_add_touch( &touchConfig );
        s_touch = touchHandle;
    }
    if ( touch == nullptr ) {
        ESP_LOGE( TAG, "Touch start failed: %s", esp_err_to_name( touchErr ) );
    }
    return display;
}

#if CONFIG_WATCH_DEV_LOGGING
// Logs where each touch starts, to check the touch coordinates against the panel.
static void onTouchPressed( lv_event_t *event )
{
    lv_point_t point = {};
    lv_indev_get_point( static_cast<lv_indev_t *>( lv_event_get_target( event ) ), &point );
    ESP_LOGI( TAG, "Touch at x=%d y=%d", static_cast<int>( point.x ), static_cast<int>( point.y ) );
}
#endif

bool Display::start( Settings &settings )
{
    m_settings = &settings;
    const int brightnessPercent = settings.value( Settings::Brightness );
    lv_indev_t *touch = nullptr;
    lv_display_t *display = startLvgl( touch );
    if ( display == nullptr ) {
        ESP_LOGE( TAG, "Display start failed" );
        return false;
    }
    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        ESP_LOGE( TAG, "Display lock timeout" );
        return false;
    }
    // Before the first frame (the panel stays dark until it).
    applyMirror( settings.value( Settings::Mirror ) );
    // Black background: an AMOLED pixel that is off draws no current.
    // How long building the pages takes: every PWR wake pays it before the first frame.
    const int64_t buildStartUs = esp_timer_get_time();
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color( screen, lv_color_black(), 0 );
    lv_obj_t *pages = lv_tileview_create( screen );
    m_pages = pages;
    lv_obj_set_style_bg_color( pages, lv_color_black(), 0 );
    lv_obj_set_scrollbar_mode( pages, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *tiles[PageCount] = {};
    for ( int i = 0; i < PageCount; ++i ) {
        const lv_dir_t direction = i == 0 ? LV_DIR_RIGHT : ( i == PageCount - 1 ? LV_DIR_LEFT : LV_DIR_HOR );
        tiles[i] = lv_tileview_add_tile( pages, static_cast<uint8_t>( i ), 0, direction );
        m_tiles[i] = tiles[i];
    }
    // The watch face and its neighbours now; the other pages when a neighbour is shown, so that
    // a glance at the time builds no more than it shows (ensureBuilt).
    m_built[FacePage] = true;
    m_built[ClockPage] = true;
    lv_obj_t *facePage = tiles[FacePage];
    m_faceTile = facePage;
    // A vertical swipe on the face steps the face page; the pages themselves only swipe sideways.
    // LVGL sends a gesture to the first ancestor that does not pass it on, so the face keeps it.
    lv_obj_set_gesture_bubble( m_faceTile, false );
    // Content sliding past the edges must not make the pages scroll.
    lv_obj_set_scrollable( m_faceTile, false );
    lv_obj_add_event_cb( m_faceTile, onGesture, LV_EVENT_GESTURE, this );
    lv_tileview_set_tile_by_index( pages, FacePage, 0, LV_ANIM_OFF );
    lv_obj_add_event_cb( pages, onPageChanged, LV_EVENT_VALUE_CHANGED, this );
    m_corners.build();
    buildBrightnessControl();
    lv_obj_add_event_cb( m_faceTile, onBrightnessDrag, LV_EVENT_PRESSED, this );
    lv_obj_add_event_cb( m_faceTile, onBrightnessDrag, LV_EVENT_LONG_PRESSED, this );
    lv_obj_add_event_cb( m_faceTile, onBrightnessDrag, LV_EVENT_PRESSING, this );
    lv_obj_add_event_cb( m_faceTile, onBrightnessDrag, LV_EVENT_RELEASED, this );
    lv_obj_add_event_cb( m_faceTile, onBrightnessDrag, LV_EVENT_PRESS_LOST, this );
    m_clock.build( tiles[ClockPage] );
    m_clockTile = tiles[ClockPage];
    lv_obj_add_event_cb( m_clockTile, onClockPartChanged, LV_EVENT_VALUE_CHANGED, this );

    m_face.build( facePage );
    // Over the page contents, shown only for a moment after a step.
    m_faceMarks.build( facePage, FaceConfig::MaxPages );
    m_faceMarks.setVisible( false );
    showFacePage( 0 );
    applyOrders();
    // On the top layer, so that they stay in place while the pages move.
    m_pageMarks.build( lv_layer_top(), PageCount, true );
    m_pageMarks.setVisible( false );
    lv_obj_add_event_cb( pages, onPagesScrolled, LV_EVENT_SCROLL, this );

    m_touchStarted = touch != nullptr;
    if ( touch != nullptr ) {
        lv_indev_add_event_cb( touch, onTouchCounted, LV_EVENT_PRESSED, this );
        lv_indev_set_scroll_limit( touch, s_scrollLimitPx );
#if CONFIG_WATCH_DEV_LOGGING
        lv_indev_add_event_cb( touch, onTouchPressed, LV_EVENT_PRESSED, nullptr );
#endif
    }
    lv_display_add_event_cb( display, onFlushStart, LV_EVENT_FLUSH_START, this );
#if CONFIG_WATCH_DEV_LOGGING
    lv_display_add_event_cb( display, onRefreshReady, LV_EVENT_REFR_READY, this );
#endif
    m_started = true;
    // The LVGL task draws the first frame (rendering needs its larger stack), then lights it.
    s_pendingBrightness = brightnessPercent;
    lv_display_add_event_cb( display, onFirstFrame, LV_EVENT_REFR_READY, nullptr );
    bsp_display_unlock();

    // Internal RAM left for the rest of active mode (the Wi-Fi setup portal needs about 100 KB).
    ESP_LOGI( TAG, "Display started, pages built in %d ms, brightness %d %%, internal heap %u free, %u largest",
              static_cast<int>( ( esp_timer_get_time() - buildStartUs ) / 1000 ), brightnessPercent,
              static_cast<unsigned>( heap_caps_get_free_size( MALLOC_CAP_INTERNAL ) ),
              static_cast<unsigned>( heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ) ) );
    return true;
}

// The information page: built when a neighbouring page is shown (see onPageChanged), then
// filled with the values kept since the start.
void Display::buildInfoPage()
{
    lv_obj_t *tile = m_tiles[InfoPage];
    m_info.build( tile, InfoCount, Language::text( Language::InfoPageName ) );
    m_sky.build( m_info.part( InfoSun ), m_info.part( InfoTwilight ), m_info.part( InfoPrayer ),
                 m_info.part( InfoMoon ), m_info.part( InfoCompass ), m_info.part( InfoTime ) );
    buildAboutPage( m_info.part( InfoAbout ) );
    lv_obj_t *batteryPage = m_info.part( InfoBattery );
    lv_obj_t *title = createLabel( batteryPage, &jetbrains_mono_26, -120 );
    lv_label_set_text( title, Language::text( Language::Battery ) );
    m_percentLabel = createLabel( batteryPage, &jetbrains_mono_48, -50 );
    m_voltageLabel = createLabel( batteryPage, &jetbrains_mono_26, 20 );
    m_chargeLabel = createLabel( batteryPage, &jetbrains_mono_26, 70 );
    m_unpluggedLabel = createLabel( batteryPage, &jetbrains_mono_26, 120 );
    m_sky.setPlace( m_setupTexts[0] );
    if ( m_haveSky ) {
        m_sky.show( m_day, m_now, m_offsetMinutes );
    }
    showBattery();
    applyOrders();
}

// The settings page, built like the information page.
void Display::buildSettingsPage()
{
    lv_obj_t *tile = m_tiles[SettingsPage];
    m_settingsPage.build( tile, SettingsPartCount, Language::text( Language::SettingsPageName ) );
    lv_obj_t *settingsParts[2] = { m_settingsPage.part( SettingsRows ), m_settingsPage.part( SettingsMoreRows ) };
#if CONFIG_WATCH_SETUP_PORTAL
    buildSetupPage( m_settingsPage.part( SetupPart ) );
    buildSetupCodePage( m_settingsPage.part( SetupCodePart ) );
    lv_obj_add_event_cb( tile, onSettingsPartChanged, LV_EVENT_VALUE_CHANGED, this );
#endif
    for ( int i = 0; i < 2; ++i ) {
        lv_obj_set_flex_flow( settingsParts[i], LV_FLEX_FLOW_COLUMN );
        // The rows of the second part start where those of the first do: a single row in the
        // middle looked like a pop-up.
        lv_obj_set_flex_align( settingsParts[i], i == 0 ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                               LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
        if ( i == 1 ) {
            const int rows = s_rowsPerPart * s_rowHeight + ( s_rowsPerPart - 1 ) * s_rowGap;
            lv_obj_set_style_pad_top( settingsParts[i],
                                      StackPage::HeaderPad + ( Panel::HEIGHT - StackPage::HeaderPad - rows ) / 2, 0 );
        }
        lv_obj_set_style_pad_hor( settingsParts[i], 16, 0 );
        lv_obj_set_style_pad_row( settingsParts[i], s_rowGap, 0 );
    }
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        lv_obj_t *part = settingsParts[i < s_rowsPerPart ? 0 : 1];
        m_settingValues[i] = createRow( part, Language::text( s_settingNames[i] ), onSettingClicked );
        m_settingRows[i] = lv_obj_get_parent( m_settingValues[i] );
        showSetting( i );
    }

    // The Setup part: names left, values right, like the information pages.
    static const Language::Text s_setupNames[SetupInfoRows] = { Language::Place, Language::LatLon, Language::Zone,
                                                               Language::Offset, Language::Change, Language::Hijri,
                                                               Language::ClockSet };
    // Names from x 30, values right-aligned in 220 px ending 30 px from the edge; a long value
    // ends in dots (13 letters of 16 px).
    TablePart::Layout layout = { Language::text( Language::SetupTitle ), SetupInfoRows, {}, 76, 40, 36, 30, 350, 160, 110, 13 };
    for ( int i = 0; i < SetupInfoRows; ++i ) {
        layout.kinds[i] = TablePart::OneValue;
        m_setupTable.text().set( i, 0, Language::text( s_setupNames[i] ) );
        m_setupTable.text().set( i, 1, m_setupTexts[i] );
    }
    m_setupTable.build( m_settingsPage.part( SetupInfoPart ), layout );
#if CONFIG_WATCH_SETUP_PORTAL
    applySetup();
#endif
    applyOrders();
}

void Display::setPages( const FaceConfig::Pages &pages )
{
    const bool locked = m_started && bsp_display_lock( s_lockTimeoutMs );
    m_facePages = pages;
    if ( locked ) {
        showFacePage( 0 );
        bsp_display_unlock();
    }
}

void Display::showFaceTile()
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    lv_tileview_set_tile_by_index( m_pages, FacePage, 0, LV_ANIM_ON );
    bsp_display_unlock();
}

void Display::showFacePage( int direction )
{
    int list[FaceConfig::MaxPages] = {};
    const int shown = FaceConfig::shown( m_facePages, list );
    // A page left out (hidden, or removed in the portal) falls back to the first one shown.
    if ( PartOrder::position( list, shown, m_settings->value( Settings::FaceStyle ) ) < 0 ) {
        m_settings->setValue( Settings::FaceStyle, list[0] );
    }
    const int page = m_settings->value( Settings::FaceStyle );
    m_faceMarks.setCount( shown );
    m_faceMarks.show( PartOrder::position( list, shown, page ) );
    m_face.show( m_facePages.pages[page], direction );
}

void Display::applyOrders()
{
    int parts[PartOrder::MaxParts] = {};
    int shown = PartOrder::decode( m_settings->value( Settings::ClockOrder ), m_settings->value( Settings::ClockHidden ),
                                   ClockPages::PartCount, -1, parts );
    m_clock.stack().setOrder( parts, shown );
    if ( m_built[InfoPage] ) {
        shown = PartOrder::decode( m_settings->value( Settings::InfoOrder ), m_settings->value( Settings::InfoHidden ),
                                   InfoCount, -1, parts );
        m_info.setOrder( parts, shown );
    }
    if ( m_built[SettingsPage] ) {
#if CONFIG_WATCH_SETUP_PORTAL
        const int alwaysShown = SetupPart;
#else
        const int alwaysShown = -1;
#endif
        shown = PartOrder::decode( m_settings->value( Settings::SettingsOrder ), m_settings->value( Settings::SettingsHidden ),
                                   SettingsPartCount, alwaysShown, parts );
        m_settingsPage.setOrder( parts, shown );
    }
}

// Builds a page that is not built yet; the watch face and its neighbours are built at the start.
void Display::ensureBuilt( int page )
{
    if ( page < 0 || page >= PageCount || m_built[page] ) {
        return;
    }
    m_built[page] = true;
    if ( page == InfoPage ) {
        buildInfoPage();
        if ( m_freedPart[page] >= 0 ) {
            m_info.restore( m_freedPart[page] );
        }
    } else if ( page == SettingsPage ) {
        buildSettingsPage();
        if ( m_freedPart[page] >= 0 ) {
            m_settingsPage.restore( m_freedPart[page] );
        }
    }
}

int Display::activePage() const
{
    lv_obj_t *tile = lv_tileview_get_tile_active( m_pages );
    for ( int i = 0; i < PageCount; ++i ) {
        if ( m_tiles[i] == tile ) {
            return i;
        }
    }
    return FacePage;
}

void Display::keepPages( int shown )
{
    const int pages[2] = { InfoPage, SettingsPage };
    for ( int i = 0; i < 2; ++i ) {
        const int page = pages[i];
        bool near = page >= shown - 1 && page <= shown + 1;
        if ( page == InfoPage && m_setup.running && shown != InfoPage ) {
            near = false;
        }
        if ( near ) {
            ensureBuilt( page );
        } else {
            freePage( page );
        }
    }
}

void Display::freePage( int page )
{
    if ( ( page != InfoPage && page != SettingsPage ) || !m_built[page] ) {
        return;
    }
    lv_obj_t *tile = m_tiles[page];
    if ( page == InfoPage ) {
        m_freedPart[page] = m_info.current();
        m_info.forget();
        m_sky.forget();
        m_percentLabel = nullptr;
        m_voltageLabel = nullptr;
        m_chargeLabel = nullptr;
        m_unpluggedLabel = nullptr;
    } else {
        m_freedPart[page] = m_settingsPage.current();
        m_settingsPage.forget();
#if CONFIG_WATCH_SETUP_PORTAL
        lv_obj_remove_event_cb_with_user_data( tile, onSettingsPartChanged, this );
#endif
        for ( int i = 0; i < Settings::PageItemCount; ++i ) {
            m_settingRows[i] = nullptr;
            m_settingValues[i] = nullptr;
        }
        m_portalRow = nullptr;
        m_portalValue = nullptr;
        m_passwordRow = nullptr;
        m_networkLabel = nullptr;
        m_passwordLabel = nullptr;
        m_setupInfo = nullptr;
        m_joinCode = nullptr;
        m_joinHint = nullptr;
        m_setupTable.forget();
    }
    lv_obj_clean( tile );
    m_built[page] = false;
}

// A full-width row with a name on the left; returns the value label on the right.
lv_obj_t *Display::createRow( lv_obj_t *parent, const char *name, lv_event_cb_t onClicked )
{
    lv_obj_t *row = lv_obj_create( parent );
    lv_obj_set_size( row, s_rowWidth, s_rowHeight );
    lv_obj_set_scrollable( row, false );
    // A finger that slides off the row cancels the tap: no row changes on release.
    lv_obj_set_press_lock( row, false );
    Styles::row( row );
    lv_obj_add_event_cb( row, onClicked, LV_EVENT_CLICKED, this );
    lv_obj_t *nameLabel = createLabel( row, &jetbrains_mono_26, LV_ALIGN_LEFT_MID, 0 );
    lv_label_set_text( nameLabel, name );
    return createLabel( row, &jetbrains_mono_26, LV_ALIGN_RIGHT_MID, 0 );
}

static lv_obj_t *createInfoLabel( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, const char *text )
{
    lv_obj_t *label = lv_label_create( parent );
    Styles::text( label, font, colour );
    lv_label_set_text( label, text );
    return label;
}

// The portal switch at the top, then what a phone needs to join (grey captions, white values),
// and a button for a new password at the bottom: centred text with an icon, unlike the
// name-and-value rows, so that it does not look like an input field.
void Display::buildSetupPage( lv_obj_t *page )
{
    lv_obj_set_flex_flow( page, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_hor( page, 16, 0 );
    lv_obj_set_style_pad_row( page, s_rowGap, 0 );
    lv_obj_set_scrollbar_mode( page, LV_SCROLLBAR_MODE_OFF );
    m_portalValue = createRow( page, Language::text( Language::WifiSetup ), onSetupClicked );
    m_portalRow = lv_obj_get_parent( m_portalValue );

    lv_obj_t *info = lv_obj_create( page );
    lv_obj_set_size( info, lv_pct( 100 ), LV_SIZE_CONTENT );
    lv_obj_set_style_bg_opa( info, LV_OPA_TRANSP, 0 );
    lv_obj_set_style_border_width( info, 0, 0 );
    lv_obj_set_style_pad_all( info, 0, 0 );
    lv_obj_set_style_pad_row( info, 0, 0 );
    lv_obj_set_scrollable( info, false );
    lv_obj_set_clickable( info, false );
    lv_obj_set_flex_flow( info, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    const lv_color_t grey = lv_color_hex( 0xA0A0A0 );
    createInfoLabel( info, &jetbrains_mono_26, grey, Language::text( Language::Network ) );
    m_networkLabel = createInfoLabel( info, &jetbrains_mono_26, lv_color_white(), "" );
    // A gap separates the network from the password.
    lv_obj_set_style_margin_top( createInfoLabel( info, &jetbrains_mono_26, grey, Language::text( Language::Password ) ), 12, 0 );
    m_passwordLabel = createInfoLabel( info, &jetbrains_mono_48, lv_color_white(), "" );
    m_setupInfo = createInfoLabel( info, &jetbrains_mono_26, grey, "" );

    lv_obj_t *buttonLabel = createRow( page, "", onSetupClicked );
    m_passwordRow = lv_obj_get_parent( buttonLabel );
    lv_obj_align( buttonLabel, LV_ALIGN_CENTER, 0, 0 );
    lv_label_set_text( buttonLabel, Language::text( Language::NewPassword ) );
}

void Display::onSetupClicked( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    lv_obj_t *row = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    if ( row == display->m_portalRow ) {
        display->m_requests |= TogglePortal;
        ESP_LOGI( TAG, "Setup portal tapped" );
    } else if ( row == display->m_passwordRow ) {
        display->m_requests |= NewPassword;
        ESP_LOGI( TAG, "New password tapped" );
    }
}

// The part below the setup part: the network as a Wi-Fi QR code, as large as the screen allows, for
// a phone's camera (a 160 px code was too small for a phone to find). Dark modules on a light
// ground with a quiet zone around them, as cameras expect, in a 320 px square that stays inside
// the corner gauges.
static const int s_joinCodeSize = 320;
static const int s_joinQuietModules = 2;
// Not full white: a large white area at the face's brightness glared in a phone camera, and the
// Wi-Fi settings' QR reader could not read the code until the brightness was lowered.
static const lv_color_t s_joinPaper = LV_COLOR_MAKE( 0x80, 0x80, 0x80 );
// The panel brightness while the code shows, whatever the setting: tried with a phone's Wi-Fi QR
// reader (2026-11-18), which needed the brightness lowered to about 40 %.
static const int s_joinCodeBrightness = 40;

void Display::buildSetupCodePage( lv_obj_t *page )
{
    m_joinCode = lv_obj_create( page );
    lv_obj_remove_style_all( m_joinCode );
    lv_obj_set_size( m_joinCode, s_joinCodeSize, s_joinCodeSize );
    lv_obj_set_clickable( m_joinCode, false );
    lv_obj_align( m_joinCode, LV_ALIGN_CENTER, 0, 0 );
    lv_obj_set_hidden( m_joinCode, true );
    lv_obj_add_event_cb( m_joinCode, onDrawJoinCode, LV_EVENT_DRAW_MAIN, this );
    m_joinHint = lv_label_create( page );
    lv_obj_set_style_text_font( m_joinHint, &jetbrains_mono_26, 0 );
    lv_obj_set_style_text_color( m_joinHint, lv_color_hex( 0xA0A0A0 ), 0 );
    lv_obj_set_style_text_align( m_joinHint, LV_TEXT_ALIGN_CENTER, 0 );
    lv_label_set_text( m_joinHint, Language::text( Language::JoinCodeHint ) );
    lv_obj_align( m_joinHint, LV_ALIGN_CENTER, 0, 0 );
    lv_obj_set_clickable( m_joinHint, false );
}

void Display::onDrawJoinCode( lv_event_t *event )
{
    const Display *display = static_cast<const Display *>( lv_event_get_user_data( event ) );
    const uint8_t *modules = display->m_joinModules;
    const int size = qrcodegen_getSize( modules );
    if ( size <= 0 ) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer( event );
    lv_area_t coords = {};
    lv_obj_get_coords( static_cast<lv_obj_t *>( lv_event_get_current_target( event ) ), &coords );
    // Whole pixels per module, the code centred in the square.
    const int scale = s_joinCodeSize / ( size + 2 * s_joinQuietModules );
    const int side = scale * ( size + 2 * s_joinQuietModules );
    const int left = coords.x1 + ( s_joinCodeSize - side ) / 2;
    const int top = coords.y1 + ( s_joinCodeSize - side ) / 2;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init( &box );
    box.bg_color = s_joinPaper;
    const lv_area_t paper = { left, top, left + side - 1, top + side - 1 };
    lv_draw_rect( layer, &box, &paper );
    // The dark modules of each row as runs: one rectangle per run.
    box.bg_color = lv_color_black();
    const int x0 = left + s_joinQuietModules * scale;
    const int y0 = top + s_joinQuietModules * scale;
    for ( int y = 0; y < size; ++y ) {
        int x = 0;
        while ( x < size ) {
            if ( !qrcodegen_getModule( modules, x, y ) ) {
                ++x;
                continue;
            }
            int end = x;
            while ( end < size && qrcodegen_getModule( modules, end, y ) ) {
                ++end;
            }
            const lv_area_t run = { x0 + x * scale, y0 + y * scale, x0 + end * scale - 1, y0 + ( y + 1 ) * scale - 1 };
            lv_draw_rect( layer, &box, &run );
            x = end;
        }
    }
}

int Display::takeRequests()
{
    return m_requests.exchange( 0 );
}

void Display::showSetup( const Setup &setup )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    const bool portalChanged = setup.running != m_setup.running;
    m_setup = setup;
    if ( portalChanged ) {
        keepPages( activePage() );
    }
    applySetup();
    bsp_display_unlock();
}

void Display::applySetup()
{
    if ( m_setupInfo == nullptr || m_setup.ssid == nullptr ) {
        return;
    }
    const Setup &setup = m_setup;
    lv_label_set_text( m_portalValue, Language::text( setup.running ? Language::On : Language::Off ) );
    lv_label_set_text( m_networkLabel, setup.ssid );
    lv_label_set_text( m_passwordLabel, setup.password );
    // The standard Wi-Fi QR text; the network name and the digits of the password need no
    // escaping. Encoded again only when it changes (this runs every second).
    char payload[sizeof( m_joinPayload )];
    snprintf( payload, sizeof( payload ), "WIFI:T:WPA;S:%s;P:%s;;", setup.ssid, setup.password );
    if ( strcmp( payload, m_joinPayload ) != 0 ) {
        strlcpy( m_joinPayload, payload, sizeof( m_joinPayload ) );
        uint8_t work[sizeof( m_joinModules )];
        if ( !qrcodegen_encodeText( m_joinPayload, work, m_joinModules, qrcodegen_Ecc_MEDIUM, 1, 5, qrcodegen_Mask_AUTO,
                                    true ) ) {
            m_joinModules[0] = 0;
        }
        lv_obj_invalidate( m_joinCode );
    }
    lv_obj_set_hidden( m_joinCode, !setup.running );
    lv_obj_set_hidden( m_joinHint, setup.running );
    // A new password only while the access point is off; the button dims meanwhile.
    lv_obj_set_style_opa( m_passwordRow, setup.running ? LV_OPA_40 : LV_OPA_COVER, 0 );
    if ( setup.running ) {
        lv_label_set_text_fmt( m_setupInfo, Language::text( Language::TimeLeft ), setup.address, setup.secondsLeft / 60,
                               setup.secondsLeft % 60 );
    } else {
        lv_label_set_text( m_setupInfo, Language::text( Language::NotStarted ) );
    }
}

void Display::showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes, const FaceTable &table,
                       bool located )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_sky.show( day, now, utcOffsetMinutes );
    m_day = day;
    m_now = now;
    m_offsetMinutes = utcOffsetMinutes;
    m_haveSky = true;
    m_moonPercent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    m_waxing = day.phase.elongation < 180;
    m_face.setMoon( m_moonPercent, m_waxing );
#if CONFIG_WATCH_DEV_LOGGING
    if ( m_skyOverride ) {
        // Development ("V"): the Milky Way's sky for a picture of the band.
        Sky::Now shown = now;
        shown.galacticCentre.altitude = m_overrideAltitude;
        shown.galacticCentre.azimuth = m_overrideAzimuth;
        shown.sun.altitude = m_overrideSunAltitude;
        m_face.setSky( Corners::fromSky( day ), shown, located );
    } else {
        m_face.setSky( Corners::fromSky( day ), now, located );
    }
#else
    m_face.setSky( Corners::fromSky( day ), now, located );
#endif
    m_faceTable = table;
    FaceText::Inputs inputs = {};
    const Rtc::DateTime &time = m_localTime;
    inputs.timeValid = m_timeValid && time.month >= 1 && time.month <= 12 && time.day >= 1 && time.day <= 31;
    inputs.year = time.year;
    inputs.month = time.month;
    inputs.day = time.day;
    inputs.hour = time.hour;
    inputs.minute = time.minute;
    inputs.second = time.second;
    inputs.weekday = inputs.timeValid ? Dates::weekday( Dates::dayNumber( time.year, time.month, time.day ) ) : 0;
    inputs.skyValid = located;
    inputs.now = now.time;
    inputs.table = &m_faceTable;
    inputs.batteryPercent = m_battery.chargeState != nullptr ? m_battery.percent : -1;
    inputs.sunAltitude = now.sun.altitude;
    inputs.moonAltitude = now.moon.altitude;
    inputs.altitudesValid = located;
    m_face.update( inputs );
    bsp_display_unlock();
}

void Display::showCorners( const Corners::Gauge gauges[Corners::CornerCount], bool night )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_corners.show( gauges );
    m_night = night;
    bsp_display_unlock();
}

// The bar of the brightness control runs over this many pixels: dragging its length covers 0 to
// 100 %.
static const int s_barTop = 140;
static const int s_barHeight = 222;  // also the length of the always-on level's bar
static const int s_faceBoxY = 118;
static const int s_aodBoxY = 280;
static const int s_aodBarY = 436;

// A box with a value in large digits and its name under it, hidden.
lv_obj_t *Display::createLevelBox( lv_obj_t *layer, int y, lv_obj_t *&value, lv_obj_t *&title )
{
    lv_obj_t *box = lv_obj_create( layer );
    lv_obj_remove_style_all( box );
    lv_obj_set_size( box, 250, 140 );
    lv_obj_align( box, LV_ALIGN_TOP_MID, -10, y );
    lv_obj_set_style_bg_color( box, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( box, LV_OPA_COVER, 0 );
    lv_obj_set_style_border_color( box, lv_color_hex( 0x505050 ), 0 );
    lv_obj_set_style_border_width( box, 2, 0 );
    lv_obj_set_style_radius( box, 16, 0 );
    lv_obj_set_clickable( box, false );
    lv_obj_set_scrollable( box, false );
    lv_obj_set_hidden( box, true );
    value = lv_label_create( box );
    lv_obj_set_style_text_font( value, &jetbrains_mono_48, 0 );
    lv_obj_set_style_text_color( value, lv_color_white(), 0 );
    lv_obj_align( value, LV_ALIGN_TOP_MID, 0, 12 );
    title = lv_label_create( box );
    lv_obj_set_style_text_font( title, &jetbrains_mono_26, 0 );
    lv_obj_set_style_text_color( title, lv_color_hex( 0xA0A0A0 ), 0 );
    lv_obj_set_style_text_align( title, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_align( title, LV_ALIGN_TOP_MID, 0, 68 );
    return box;
}

// A bar from 0 to 100, vertical when it is higher than wide, hidden.
lv_obj_t *Display::createLevelBar( lv_obj_t *layer, int x, int y, int width, int height )
{
    lv_obj_t *bar = lv_bar_create( layer );
    lv_obj_remove_style_all( bar );
    lv_obj_set_size( bar, width, height );
    lv_obj_set_pos( bar, x, y );
    lv_bar_set_range( bar, 0, 100 );
    lv_obj_set_style_border_color( bar, lv_color_hex( 0x808080 ), LV_PART_MAIN );
    lv_obj_set_style_border_width( bar, 2, LV_PART_MAIN );
    lv_obj_set_style_radius( bar, 9, LV_PART_MAIN );
    lv_obj_set_style_pad_all( bar, 4, LV_PART_MAIN );
    lv_obj_set_style_bg_color( bar, lv_color_white(), LV_PART_INDICATOR );
    lv_obj_set_style_bg_opa( bar, LV_OPA_COVER, LV_PART_INDICATOR );
    lv_obj_set_style_radius( bar, 5, LV_PART_INDICATOR );
    lv_obj_set_clickable( bar, false );
    lv_obj_set_hidden( bar, true );
    return bar;
}

// The face's level: a box at the top and a vertical bar on the right; the always-on level: a box
// below it and a horizontal bar at the bottom. Both show at the long press, so that both
// directions can be seen; the drag's direction keeps one.
void Display::buildBrightnessControl()
{
    lv_obj_t *layer = lv_layer_top();
    // Over the corner gauges: the page under the control is dimmed.
    m_brightnessShade = lv_obj_create( layer );
    lv_obj_remove_style_all( m_brightnessShade );
    lv_obj_set_size( m_brightnessShade, Panel::WIDTH, Panel::HEIGHT );
    lv_obj_set_style_bg_color( m_brightnessShade, lv_color_black(), 0 );
    lv_obj_set_clickable( m_brightnessShade, false );
    lv_obj_set_hidden( m_brightnessShade, true );

    m_brightnessBox = createLevelBox( layer, s_faceBoxY, m_brightnessValue, m_brightnessTitle );
    m_aodBox = createLevelBox( layer, s_aodBoxY, m_aodValue, m_aodTitle );
    m_brightnessBar = createLevelBar( layer, 352, s_barTop, 18, s_barHeight );
    m_aodBar = createLevelBar( layer, Panel::WIDTH / 2 - 10 - s_barHeight / 2, s_aodBarY, s_barHeight, 18 );
}

void Display::showBrightness( int percent )
{
    const bool alwaysOn = m_adjustItem != Settings::Brightness;
    lv_label_set_text_fmt( alwaysOn ? m_aodValue : m_brightnessValue, "%d %%", percent );
    lv_bar_set_value( alwaysOn ? m_aodBar : m_brightnessBar, percent, LV_ANIM_OFF );
    setBrightness( percent );
}

// The long press: both levels shown, the face's to be set until the drag goes sideways.
void Display::startAdjust( int x, int y )
{
    m_adjusting = true;
    m_adjustAxisChosen = false;
    m_adjustItem = Settings::Brightness;
    m_adjustStart = m_settings->value( m_adjustItem );
    m_adjustPercent = m_adjustStart;
    m_adjustX = x;
    m_adjustY = y;
    // The finger may drift sideways: the pages must not swipe while the value is set.
    lv_obj_set_scrollable( m_pages, false );
    lv_obj_set_style_bg_opa( m_brightnessShade, LV_OPA_80, 0 );
    // The active level is one for day and night; the period is shown for information.
    lv_label_set_text( m_brightnessTitle,
                       Language::text( m_night ? Language::BrightnessNight : Language::BrightnessDay ) );
    // The always-on level of the moment, until the drag goes sideways.
    const Settings::Item aodItem = m_night ? Settings::AodNightBrightness : Settings::AodDayBrightness;
    lv_label_set_text_fmt( m_aodValue, "%d %%", m_settings->value( aodItem ) );
    lv_bar_set_value( m_aodBar, m_settings->value( aodItem ), LV_ANIM_OFF );
    lv_label_set_text( m_aodTitle, Language::text( m_night ? Language::AodNight : Language::AodDay ) );
    lv_obj_t *const shown[5] = { m_brightnessShade, m_brightnessBox, m_brightnessBar,
                                 m_aodBox, m_aodBar };
    for ( int i = 0; i < 5; ++i ) {
        lv_obj_set_hidden( shown[i], false );
    }
    showBrightness( m_adjustPercent );
}

// After the long press the finger must move this far before the drag's direction counts.
static const int s_axisPx = 20;

void Display::onBrightnessDrag( lv_event_t *event )
{
    Display *self = static_cast<Display *>( lv_event_get_user_data( event ) );
    const lv_event_code_t code = lv_event_get_code( event );
    lv_point_t point = {};
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    if ( code == LV_EVENT_PRESSED ) {
        self->m_pressX = point.x;
        self->m_pressY = point.y;
        self->m_pressMoved = false;
        return;
    }
    if ( code == LV_EVENT_LONG_PRESSED ) {
        if ( !self->m_pressMoved ) {
            self->startAdjust( point.x, point.y );
        }
        return;
    }
    if ( !self->m_adjusting ) {
        if ( abs( point.x - self->m_pressX ) >= s_axisPx || abs( point.y - self->m_pressY ) >= s_axisPx ) {
            self->m_pressMoved = true;
        }
        return;
    }
    self->m_lastX = point.x;
    self->m_lastY = point.y;
    const int dx = point.x - self->m_adjustX;
    const int dy = self->m_adjustY - point.y;
    if ( code == LV_EVENT_PRESSING ) {
        if ( !self->m_adjustAxisChosen ) {
            if ( abs( dx ) < s_axisPx && abs( dy ) < s_axisPx ) {
                return;
            }
            self->m_adjustAxisChosen = true;
            const bool sideways = abs( dx ) > abs( dy );
            if ( sideways ) {
                // Sideways: the always-on level of the moment, the face shown at that level (no
                // shade).
                self->m_adjustItem = self->m_night ? Settings::AodNightBrightness : Settings::AodDayBrightness;
                self->m_adjustStart = self->m_settings->value( self->m_adjustItem );
                lv_obj_set_style_bg_opa( self->m_brightnessShade, LV_OPA_TRANSP, 0 );
            }
            lv_obj_set_hidden( self->m_brightnessBox, sideways );
            lv_obj_set_hidden( self->m_brightnessBar, sideways );
            lv_obj_set_hidden( self->m_aodBox, !sideways );
            lv_obj_set_hidden( self->m_aodBar, !sideways );
        }
        const bool alwaysOn = self->m_adjustItem != Settings::Brightness;
        const int minimum = alwaysOn ? 1 : 5;
        int percent = self->m_adjustStart + ( alwaysOn ? dx : dy ) * 100 / s_barHeight;
        percent = percent < minimum ? minimum : ( percent > 100 ? 100 : percent );
        if ( percent != self->m_adjustPercent ) {
            self->m_adjustPercent = percent;
            self->showBrightness( percent );
        }
        return;
    }
    // Released, or the press was lost.
    self->m_adjusting = false;
    self->m_settings->setValue( self->m_adjustItem, self->m_adjustPercent );
    lv_obj_set_scrollable( self->m_pages, true );
    lv_obj_t *const shown[5] = { self->m_brightnessShade, self->m_brightnessBox, self->m_brightnessBar, self->m_aodBox,
                                 self->m_aodBar };
    for ( int i = 0; i < 5; ++i ) {
        lv_obj_set_hidden( shown[i], true );
    }
    setBrightness( self->m_settings->value( Settings::Brightness ) );
    ESP_LOGI( TAG, "Brightness setting %d: %d %%", self->m_adjustItem, self->m_adjustPercent );
}

void Display::onPageChanged( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    lv_obj_t *tile = lv_tileview_get_tile_active( display->m_pages );
    display->updatePageSwipe();
    display->updateJoinCodeBrightness();
    // The next page beyond this one, while this one is looked at; the far ones freed.
    for ( int i = 0; i < PageCount; ++i ) {
        if ( display->m_tiles[i] == tile ) {
            display->keepPages( i );
        }
    }
}

void Display::onSettingsPartChanged( lv_event_t *event )
{
    static_cast<Display *>( lv_event_get_user_data( event ) )->updateJoinCodeBrightness();
}

void Display::updateJoinCodeBrightness()
{
#if CONFIG_WATCH_SETUP_PORTAL
    const bool code = m_built[SettingsPage] && lv_tileview_get_tile_active( m_pages ) == m_tiles[SettingsPage] &&
                      m_settingsPage.current() == SetupCodePart;
#else
    const bool code = false;
#endif
    if ( code != m_joinCodeDimmed ) {
        m_joinCodeDimmed = code;
        setBrightness( code ? s_joinCodeBrightness : m_settings->value( Settings::Brightness ) );
    }
}

void Display::onClockPartChanged( lv_event_t *event )
{
    static_cast<Display *>( lv_event_get_user_data( event ) )->updatePageSwipe();
}

void Display::updatePageSwipe()
{
    const bool clock = lv_tileview_get_tile_active( m_pages ) == m_clockTile;
    lv_obj_set_scrollable( m_pages, !( clock && m_clock.swipesSideways() ) );
}

void Display::onPagesScrolled( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    const int scrollX = lv_obj_get_scroll_x( display->m_pages );
    int page = ( scrollX + Panel::WIDTH / 2 ) / Panel::WIDTH;
    page = page < 0 ? 0 : ( page >= PageCount ? PageCount - 1 : page );
    // Each scroll step restarts the timer: the marks stay while the pages move and a moment after.
    display->m_pageMarks.flash( page );
}

void Display::refreshSettings()
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    applyOrders();
    showFacePage( 0 );
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        showSetting( i );
    }
    setBrightness( m_settings->value( Settings::Brightness ) );
    applyMirror( m_settings->value( Settings::Mirror ) );
    lv_obj_invalidate( lv_screen_active() );
    // A save from the portal while the code shows keeps it dim.
    m_joinCodeDimmed = false;
    updateJoinCodeBrightness();
    bsp_display_unlock();
}

int Display::adjustAxis()
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return -1;
    }
    const int axis = m_adjusting && m_adjustAxisChosen ? ( m_adjustItem == Settings::Brightness ? 0 : 1 ) : -1;
    bsp_display_unlock();
    return axis;
}

void Display::toggleMirror( int axis )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    const int mirror = m_settings->value( Settings::Mirror ) ^ ( axis == 1 ? 1 : 2 );
    m_settings->setValue( Settings::Mirror, mirror );
    applyMirror( mirror );
    // The finger stays where it is, but its point now reads mirrored: the drag goes on from there
    // and from the value it had, without a jump.
    m_adjustStart = m_adjustPercent;
    m_adjustX = axis == 1 ? Panel::WIDTH - m_lastX : m_lastX;
    m_adjustY = axis == 0 ? Panel::HEIGHT - m_lastY : m_lastY;
    lv_obj_invalidate( lv_screen_active() );
    lv_obj_invalidate( lv_layer_top() );
    bsp_display_unlock();
    ESP_LOGI( TAG, "Mirror %d (PWR during the brightness drag)", mirror );
}

void Display::stop()
{
    if ( m_started ) {
        bsp_display_lock( s_lockTimeoutMs );
    }
}

void Display::update( const Rtc::DateTime &time, int utcOffsetMinutes, const Battery &battery, bool timeValid )
{
    if ( !m_started ) {
        return;
    }
    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_battery = battery;
    // The firmware never sets TZ, so mktime() works in UTC.
    tm fields = {};
    fields.tm_year = time.year - 1900;
    fields.tm_mon = time.month - 1;
    fields.tm_mday = time.day;
    fields.tm_hour = time.hour;
    fields.tm_min = time.minute;
    fields.tm_sec = time.second;
    m_clock.update( static_cast<int64_t>( mktime( &fields ) ) - utcOffsetMinutes * 60, utcOffsetMinutes, time );

    showBattery();
    m_localTime = time;
    m_timeValid = timeValid;
    bsp_display_unlock();
}

void Display::showBattery()
{
    if ( m_percentLabel == nullptr || m_battery.chargeState == nullptr ) {
        return;
    }
    lv_label_set_text_fmt( m_percentLabel, "%u %%", m_battery.percent );
    lv_label_set_text_fmt( m_voltageLabel, "%u mV", m_battery.millivolts );
    // Axp2101 names the charger state in English.
    static const char *s_states[7] = { "trickle", "pre-charge", "constant current", "constant voltage", "done",
                                       "not charging", "unknown" };
    const char *state = Language::text( Language::OnBattery );
    if ( m_battery.vbusGood ) {
        state = Language::text( Language::ChargeUnknown );
        for ( int i = 0; i < 7; ++i ) {
            if ( strcmp( m_battery.chargeState, s_states[i] ) == 0 ) {
                state = Language::text( static_cast<Language::Text>( Language::ChargeTrickle + i ) );
            }
        }
    }
    lv_label_set_text( m_chargeLabel, state );
    // How long the watch has run since it came off the charger.
    const int minutes = m_battery.unpluggedMinutes;
    char duration[24] = "";
    if ( minutes >= 24 * 60 ) {
        snprintf( duration, sizeof( duration ), Language::text( Language::DurationDayHours ), minutes / ( 24 * 60 ),
                  minutes / 60 % 24 );
    } else if ( minutes >= 0 ) {
        snprintf( duration, sizeof( duration ), Language::text( Language::DurationHourMins ), minutes / 60, minutes % 60 );
    }
    if ( minutes >= 0 ) {
        lv_label_set_text_fmt( m_unpluggedLabel, Language::text( Language::Unplugged ), duration );
    } else {
        lv_label_set_text( m_unpluggedLabel, "" );
    }
}

void Display::showSetupInfo( const char *const values[SetupInfoRows] )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    for ( int i = 0; i < SetupInfoRows; ++i ) {
        snprintf( m_setupTexts[i], sizeof( m_setupTexts[i] ), "%s", values[i] );
        m_setupTable.text().set( i, 1, m_setupTexts[i] );
    }
    m_setupTable.refresh();
    m_sky.setPlace( m_setupTexts[0] );
    bsp_display_unlock();
}

void Display::setCounters( const DayCounters &counters )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_clock.setCounters( counters );
    bsp_display_unlock();
}

void Display::setCities( const WorldClock &worldClock )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_clock.setCities( worldClock );
    bsp_display_unlock();
}

bool Display::takeHijriRequest( ClockPages::HijriRequest &request )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return false;
    }
    const bool wanted = m_clock.takeHijriRequest( request );
    bsp_display_unlock();
    return wanted;
}

void Display::showHijriMonth( const Hijri::Month &month, int today, int method )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_clock.showHijriMonth( month, today, method );
    bsp_display_unlock();
}

void Display::onGesture( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    const lv_dir_t direction = lv_indev_get_gesture_dir( lv_indev_active() );
    // A drag that sets the brightness is no page swipe.
    if ( display->m_adjusting || ( direction != LV_DIR_TOP && direction != LV_DIR_BOTTOM ) ) {
        return;
    }
    // Up: the next page shown; down: the one before. The choice is saved with the other settings
    // when active mode ends.
    int list[FaceConfig::MaxPages] = {};
    const int shown = FaceConfig::shown( display->m_facePages, list );
    if ( shown < 2 ) {
        return;
    }
    const int slide = direction == LV_DIR_TOP ? 1 : -1;
    const int page = PartOrder::step( list, shown, display->m_settings->value( Settings::FaceStyle ), slide );
    display->m_settings->setValue( Settings::FaceStyle, page );
    ESP_LOGI( TAG, "Face page %d", page );
    display->m_faceMarks.flash( PartOrder::position( list, shown, page ) );
    display->showFacePage( slide );
}

// Before each band goes to the panel: the corner gauges are written into it (CornerView), then
// a development screenshot reads it.
void Display::onFlushStart( lv_event_t *event )
{
    Display *self = static_cast<Display *>( lv_event_get_user_data( event ) );
    const lv_area_t *area = static_cast<const lv_area_t *>( lv_event_get_param( event ) );
    lv_draw_buf_t *band = lv_display_get_buf_active( lv_display_get_default() );
    self->m_corners.paint( *area, reinterpret_cast<uint16_t *>( band->data ) );
#if CONFIG_WATCH_DEV_LOGGING
    if ( !self->m_capturing ) {
        return;
    }
    // The draw buffer holds the band in LVGL's RGB565, row after row (esp_lvgl_port swaps the
    // bytes only inside the flush).
    const uint16_t *pixels = reinterpret_cast<const uint16_t *>( band->data );
    const int width = lv_area_get_width( area );
    // Runs of one colour as "<RGB565 hex>*<count>", comma-separated: the screen is mostly black.
    static char line[24 + 11 * BSP_LCD_H_RES];
    for ( int row = 0; row < lv_area_get_height( area ); ++row ) {
        const uint16_t *rowPixels = pixels + row * width;
        int length = snprintf( line, sizeof( line ), "SHOT %d %d ", static_cast<int>( area->y1 ) + row,
                               static_cast<int>( area->x1 ) );
        int column = 0;
        while ( column < width ) {
            int count = 1;
            while ( column + count < width && rowPixels[column + count] == rowPixels[column] ) {
                ++count;
            }
            length += snprintf( line + length, sizeof( line ) - length, "%s%04x*%d", column == 0 ? "" : ",",
                                rowPixels[column], count );
            column += count;
        }
        puts( line );
    }
    // Sending a band takes a while; let the idle task run so that the task watchdog stays quiet.
    vTaskDelay( 1 );
#endif
}

#if CONFIG_WATCH_DEV_LOGGING
void Display::showFaceStyle( int page )
{
    if ( !m_started || page < 0 || page >= m_facePages.count || !bsp_display_lock( 1000 ) ) {
        return;
    }
    m_face.show( m_facePages.pages[page] );
    bsp_display_unlock();
}

void Display::testAddressMode( int value, int gap )
{
    if ( !m_started || s_panelIo == nullptr || !bsp_display_lock( 1000 ) ) {
        return;
    }
    // MADCTL (0x36), sent as Panel sends its commands (QSPI opcode 0x02).
    const uint8_t parameter = static_cast<uint8_t>( value );
    esp_lcd_panel_io_tx_param( s_panelIo, ( 0x02 << 24 ) | ( 0x36 << 8 ), &parameter, 1 );
    esp_lcd_panel_set_gap( s_panel, gap, 0 );
    lv_obj_invalidate( lv_screen_active() );
    lv_obj_invalidate( lv_layer_top() );
    bsp_display_unlock();
    ESP_LOGI( TAG, "Address mode 0x%02X, column gap %d", parameter, gap );
}

void Display::overrideSky( double altitude, double azimuth, double sunAltitude )
{
    m_skyOverride = true;
    m_overrideAltitude = altitude;
    m_overrideAzimuth = azimuth;
    m_overrideSunAltitude = sunAltitude;
}

void Display::showLevels()
{
    if ( !m_started || !bsp_display_lock( 1000 ) ) {
        return;
    }
    startAdjust( Panel::WIDTH / 2, Panel::HEIGHT / 2 );
    bsp_display_unlock();
}

void Display::setFaceSlot( int page, int slot, int item )
{
    if ( slot < 0 || slot >= FaceConfig::SlotCount || item < 0 || item > 255 ) {
        return;
    }
    if ( page >= 0 && page < m_facePages.count ) {
        m_facePages.pages[page].items[slot] = static_cast<uint8_t>( item );
    }
    showFaceStyle( page );
}

void Display::setScene( bool scene )
{
    if ( !m_started || !bsp_display_lock( 1000 ) ) {
        return;
    }
    m_clock.setScene( scene );
    bsp_display_unlock();
}

void Display::swipeTo( int page )
{
    if ( !m_started || page < 0 || page >= PageCount || !bsp_display_lock( 1000 ) ) {
        return;
    }
    lv_tileview_set_tile_by_index( m_pages, static_cast<uint32_t>( page ), 0, LV_ANIM_ON );
    bsp_display_unlock();
}

bool Display::pressClockButton( const char *name )
{
    if ( !m_started || !bsp_display_lock( 1000 ) ) {
        return false;
    }
    const bool pressed = m_clock.press( name );
    bsp_display_unlock();
    return pressed;
}

void Display::capture( int page )
{
    if ( m_pages == nullptr || page < 0 || page >= s_shotCount || !bsp_display_lock( 1000 ) ) {
        return;
    }
    ESP_LOGI( TAG, "Screenshot of page %d", page );
    const Shot &shot = s_shots[page];
    ensureBuilt( shot.page );
    lv_tileview_set_tile_by_index( m_pages, static_cast<uint32_t>( shot.page ), 0, LV_ANIM_OFF );
    if ( shot.part >= 0 ) {
        StackPage &stack = shot.page == InfoPage ? m_info : ( shot.page == ClockPage ? m_clock.stack() : m_settingsPage );
        stack.show( shot.part, 0 );
    }
    // The jump to the page counts as a swipe; the pictures show the pages at rest.
    m_pageMarks.setVisible( false );
    lv_display_trigger_activity( nullptr );
    printf( "SHOT BEGIN %d %d %d %s\n", page, BSP_LCD_H_RES, BSP_LCD_V_RES, shot.name );
    // The LVGL task draws the whole screen at its next refresh: rendering needs its larger stack
    // (the main task's overflowed). onRefreshReady() ends the capture.
    m_capturing = true;
    lv_obj_invalidate( lv_screen_active() );
    bsp_display_unlock();
}

void Display::onRefreshReady( lv_event_t *event )
{
    Display *self = static_cast<Display *>( lv_event_get_user_data( event ) );
    if ( self->m_capturing ) {
        self->m_capturing = false;
        printf( "SHOT END\n" );
        fflush( stdout );
    }
}
#endif

void Display::onSettingClicked( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    lv_obj_t *row = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        if ( display->m_settingRows[i] != row ) {
            continue;
        }
        const Settings::Item item = static_cast<Settings::Item>( i );
        display->m_settings->next( item );
        display->showSetting( i );
        ESP_LOGI( TAG, "Setting %d: %s", i, lv_label_get_text( display->m_settingValues[i] ) );
        if ( item == Settings::Language ) {
            display->m_requests |= Restart;
        }
    }
}

void Display::showSetting( int item )
{
    const int value = m_settings->value( static_cast<Settings::Item>( item ) );
    lv_obj_t *label = m_settingValues[item];
    if ( label == nullptr ) {
        return;
    }
    if ( item == Settings::DisplayTimeout ) {
        lv_label_set_text_fmt( label, Language::text( Language::Seconds ), value );
    } else if ( item == Settings::AlwaysOnFace || item == Settings::DevMode ) {
        lv_label_set_text( label, Language::text( value != 0 ? Language::On : Language::Off ) );
    } else if ( item == Settings::UsbStayOn ) {
        lv_label_set_text( label, Language::text( value != 0 ? Language::StayOn : Language::TimeoutValue ) );
    } else if ( item == Settings::Language ) {
        // Each language by its own name.
        lv_label_set_text( label, value == Language::Turkish ? "Türkçe" : "English" );
    } else if ( value == 0 ) {
        lv_label_set_text( label, Language::text( Language::Never ) );
    } else {
        lv_label_set_text_fmt( label, Language::text( Language::Minutes ), value );
    }
}

void Display::onTouchCounted( lv_event_t *event )
{
    static_cast<Display *>( lv_event_get_user_data( event ) )->m_touches++;
}

int Display::touches() const
{
    return m_touches;
}

bool Display::touchStarted() const
{
    return m_touchStarted;
}

uint32_t Display::inactiveMs()
{
    if ( m_started && bsp_display_lock( s_lockTimeoutMs ) ) {
        m_inactiveMs = lv_display_get_inactive_time( nullptr );
        bsp_display_unlock();
    }
    return m_inactiveMs;
}
