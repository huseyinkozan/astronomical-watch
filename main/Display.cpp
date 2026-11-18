#include "Display.h"

#include <cstring>
#include <ctime>
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
    AlwaysOnPage,
    FacePage,
    ClockPage,     // world clock, timers and calendars, one over the other (ClockPages)
    InfoPage,      // the information parts, one over the other (InfoPart)
    SettingsPage,  // the settings, and under them the setup portal
    PageCount
};
static_assert( PageCount == 5, "Display.h sizes m_tiles and m_built for five pages" );

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
    { "always-on", AlwaysOnPage, -1 }, { "face", FacePage, -1 },
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
        lv_obj_set_style_text_font( label, &jetbrains_mono_26, 0 );
        // Grey for the details, white for the name.
        lv_obj_set_style_text_color( label, i == 0 ? lv_color_white() : lv_color_hex( 0xA0A0A0 ), 0 );
        lv_obj_set_style_text_align( label, LV_TEXT_ALIGN_CENTER, 0 );
        lv_obj_set_width( label, lv_pct( 100 ) );
        lv_label_set_long_mode( label, LV_LABEL_LONG_MODE_WRAP );
        lv_label_set_text( label, lines[i] );
    }
}

static lv_obj_t *createLabel( lv_obj_t *parent, const lv_font_t *font, lv_align_t align, int yOffset )
{
    lv_obj_t *label = lv_label_create( parent );
    lv_obj_set_style_text_font( label, font, 0 );
    lv_obj_set_style_text_color( label, lv_color_white(), 0 );
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
    const lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
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
    m_built[AlwaysOnPage] = true;
    m_built[FacePage] = true;
    m_built[ClockPage] = true;
    lv_obj_t *facePage = tiles[FacePage];
    m_aodTile = tiles[AlwaysOnPage];
    m_faceTile = facePage;
    m_aodPage.build( m_aodTile );
    // A vertical swipe on these two pages steps the style; the pages themselves only swipe sideways.
    // LVGL sends a gesture to the first ancestor that does not pass it on, so these keep it.
    lv_obj_set_gesture_bubble( m_aodTile, false );
    lv_obj_set_gesture_bubble( m_faceTile, false );
    // Content sliding past the edges must not make the pages scroll.
    lv_obj_set_scrollable( m_aodTile, false );
    lv_obj_set_scrollable( m_faceTile, false );
    lv_obj_add_event_cb( m_aodTile, onGesture, LV_EVENT_GESTURE, this );
    lv_obj_add_event_cb( m_faceTile, onGesture, LV_EVENT_GESTURE, this );
    lv_tileview_set_tile_by_index( pages, FacePage, 0, LV_ANIM_OFF );
    lv_obj_add_event_cb( pages, onPageChanged, LV_EVENT_VALUE_CHANGED, this );
    m_corners.build();
    buildBrightnessControl();
    for ( int i = 0; i < 2; ++i ) {
        lv_obj_t *tile = i == 0 ? m_aodTile : m_faceTile;
        lv_obj_add_event_cb( tile, onBrightnessDrag, LV_EVENT_LONG_PRESSED, this );
        lv_obj_add_event_cb( tile, onBrightnessDrag, LV_EVENT_PRESSING, this );
        lv_obj_add_event_cb( tile, onBrightnessDrag, LV_EVENT_RELEASED, this );
        lv_obj_add_event_cb( tile, onBrightnessDrag, LV_EVENT_PRESS_LOST, this );
    }
    m_clock.build( tiles[ClockPage] );
    m_clockTile = tiles[ClockPage];
    lv_obj_add_event_cb( m_clockTile, onClockPartChanged, LV_EVENT_VALUE_CHANGED, this );

    m_face.build( facePage );
    m_face.setStyle( m_settings->value( Settings::FaceStyle ), m_settings->value( Settings::FaceItems ) );
    // Over the page contents: the always-on page shows its marks, the face only after a step.
    m_aodMarks.build( m_aodTile, AlwaysOn::StyleCount );
    m_aodMarks.show( m_settings->value( Settings::AodStyle ) );
    m_faceMarks.build( facePage, WatchFace::StyleCount );
    m_faceMarks.setVisible( false );
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
    lv_obj_t *setupInfo = m_settingsPage.part( SetupInfoPart );
    lv_obj_t *setupTitle = createLabel( setupInfo, &jetbrains_mono_26, LV_ALIGN_TOP_MID, 76 );
    lv_label_set_text( setupTitle, Language::text( Language::SetupTitle ) );
    lv_obj_set_style_text_color( setupTitle, lv_color_hex( 0xA0A0A0 ), 0 );
    for ( int i = 0; i < SetupInfoRows; ++i ) {
        lv_obj_t *name = createLabel( setupInfo, &jetbrains_mono_26, LV_ALIGN_TOP_LEFT, 116 + i * 36 );
        lv_obj_set_x( name, 30 );
        lv_obj_set_style_text_color( name, lv_color_hex( 0xA0A0A0 ), 0 );
        lv_label_set_text( name, Language::text( s_setupNames[i] ) );
        m_setupValues[i] = createLabel( setupInfo, &jetbrains_mono_26, LV_ALIGN_TOP_RIGHT, 116 + i * 36 );
        lv_obj_set_x( m_setupValues[i], -30 );
        // One line each: a long value ends in dots.
        lv_obj_set_size( m_setupValues[i], 220, 30 );
        lv_obj_set_style_text_align( m_setupValues[i], LV_TEXT_ALIGN_RIGHT, 0 );
        lv_label_set_long_mode( m_setupValues[i], LV_LABEL_LONG_MODE_DOTS );
    }

    for ( int i = 0; i < SetupInfoRows; ++i ) {
        lv_label_set_text( m_setupValues[i], m_setupTexts[i] );
    }
#if CONFIG_WATCH_SETUP_PORTAL
    applySetup();
#endif
    applyOrders();
}

int Display::shownStyles( bool alwaysOn, int parts[8] ) const
{
    if ( alwaysOn ) {
        return PartOrder::decode( m_settings->value( Settings::AodOrder ), m_settings->value( Settings::AodHidden ),
                                  AlwaysOn::StyleCount, -1, parts );
    }
    return PartOrder::decode( m_settings->value( Settings::FaceOrder ), m_settings->value( Settings::FaceHidden ),
                              WatchFace::StyleCount, -1, parts );
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
    // A style left out falls back to the first one shown.
    for ( int i = 0; i < 2; ++i ) {
        const bool alwaysOn = i == 0;
        const Settings::Item item = alwaysOn ? Settings::AodStyle : Settings::FaceStyle;
        shown = shownStyles( alwaysOn, parts );
        if ( PartOrder::position( parts, shown, m_settings->value( item ) ) < 0 ) {
            m_settings->setValue( item, parts[0] );
        }
        PageMarks &marks = alwaysOn ? m_aodMarks : m_faceMarks;
        marks.setCount( shown );
        marks.show( PartOrder::position( parts, shown, m_settings->value( item ) ) );
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
    } else if ( page == SettingsPage ) {
        buildSettingsPage();
    }
}

// A full-width row with a name on the left; returns the value label on the right.
lv_obj_t *Display::createRow( lv_obj_t *parent, const char *name, lv_event_cb_t onClicked )
{
    lv_obj_t *row = lv_obj_create( parent );
    lv_obj_set_size( row, s_rowWidth, s_rowHeight );
    lv_obj_set_scrollable( row, false );
    // A finger that slides off the row cancels the tap: no row changes on release.
    lv_obj_set_press_lock( row, false );
    // A black row with a mid-grey outline: large areas of dark grey shimmer on this AMOLED
    // panel (seen on the board), and black pixels draw no current.
    lv_obj_set_style_border_width( row, 2, 0 );
    lv_obj_set_style_border_color( row, lv_color_hex( 0x808080 ), 0 );
    lv_obj_set_style_radius( row, 24, 0 );
    lv_obj_set_style_pad_hor( row, 24, 0 );
    lv_obj_set_style_bg_color( row, lv_color_black(), 0 );
    // The row lights up while pressed, so the finger sees that the tap arrived.
    lv_obj_set_style_bg_color( row, lv_color_hex( 0x606060 ), LV_STATE_PRESSED );
    lv_obj_add_event_cb( row, onClicked, LV_EVENT_CLICKED, this );
    lv_obj_t *nameLabel = createLabel( row, &jetbrains_mono_26, LV_ALIGN_LEFT_MID, 0 );
    lv_label_set_text( nameLabel, name );
    return createLabel( row, &jetbrains_mono_26, LV_ALIGN_RIGHT_MID, 0 );
}

static lv_obj_t *createInfoLabel( lv_obj_t *parent, const lv_font_t *font, lv_color_t colour, const char *text )
{
    lv_obj_t *label = lv_label_create( parent );
    lv_obj_set_style_text_font( label, font, 0 );
    lv_obj_set_style_text_color( label, colour, 0 );
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
    m_setup = setup;
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

void Display::showSky( const Sky::Day &day, const Sky::Now &now, int utcOffsetMinutes )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_sky.show( day, now, utcOffsetMinutes );
    m_face.showSky( day, now, utcOffsetMinutes );
    m_day = day;
    m_now = now;
    m_offsetMinutes = utcOffsetMinutes;
    m_haveSky = true;
    m_moonPercent = static_cast<int>( day.phase.illumination * 100 + 0.5 );
    m_waxing = day.phase.elongation < 180;
    refreshAlwaysOnPage( false );
    bsp_display_unlock();
}

void Display::showCorners( const Corners::Gauge gauges[Corners::CornerCount] )
{
    if ( !m_started || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_corners.show( gauges );
    m_night = gauges[Corners::TopLeft].kind == Corners::Night;
    bsp_display_unlock();
}

// The bar of the brightness control runs over this many pixels: dragging its length covers 0 to
// 100 %.
static const int s_barTop = 140;
static const int s_barHeight = 222;

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

    m_brightnessBox = lv_obj_create( layer );
    lv_obj_remove_style_all( m_brightnessBox );
    lv_obj_set_size( m_brightnessBox, 250, 150 );
    lv_obj_set_style_bg_color( m_brightnessBox, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( m_brightnessBox, LV_OPA_COVER, 0 );
    lv_obj_set_style_border_color( m_brightnessBox, lv_color_hex( 0x505050 ), 0 );
    lv_obj_set_style_border_width( m_brightnessBox, 2, 0 );
    lv_obj_set_style_radius( m_brightnessBox, 16, 0 );
    lv_obj_set_clickable( m_brightnessBox, false );
    lv_obj_set_scrollable( m_brightnessBox, false );
    lv_obj_set_hidden( m_brightnessBox, true );
    m_brightnessValue = lv_label_create( m_brightnessBox );
    lv_obj_set_style_text_font( m_brightnessValue, &jetbrains_mono_48, 0 );
    lv_obj_set_style_text_color( m_brightnessValue, lv_color_white(), 0 );
    lv_obj_align( m_brightnessValue, LV_ALIGN_TOP_MID, 0, 16 );
    m_brightnessTitle = lv_label_create( m_brightnessBox );
    lv_obj_set_style_text_font( m_brightnessTitle, &jetbrains_mono_26, 0 );
    lv_obj_set_style_text_color( m_brightnessTitle, lv_color_hex( 0xA0A0A0 ), 0 );
    lv_obj_set_style_text_align( m_brightnessTitle, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_align( m_brightnessTitle, LV_ALIGN_TOP_MID, 0, 76 );

    m_brightnessBar = lv_bar_create( layer );
    lv_obj_remove_style_all( m_brightnessBar );
    lv_obj_set_size( m_brightnessBar, 18, s_barHeight );
    lv_obj_set_pos( m_brightnessBar, 352, s_barTop );
    lv_bar_set_range( m_brightnessBar, 0, 100 );
    lv_obj_set_style_border_color( m_brightnessBar, lv_color_hex( 0x808080 ), LV_PART_MAIN );
    lv_obj_set_style_border_width( m_brightnessBar, 2, LV_PART_MAIN );
    lv_obj_set_style_radius( m_brightnessBar, 9, LV_PART_MAIN );
    lv_obj_set_style_pad_all( m_brightnessBar, 4, LV_PART_MAIN );
    lv_obj_set_style_bg_color( m_brightnessBar, lv_color_white(), LV_PART_INDICATOR );
    lv_obj_set_style_bg_opa( m_brightnessBar, LV_OPA_COVER, LV_PART_INDICATOR );
    lv_obj_set_style_radius( m_brightnessBar, 5, LV_PART_INDICATOR );
    lv_obj_set_clickable( m_brightnessBar, false );
    lv_obj_set_hidden( m_brightnessBar, true );
}

void Display::showBrightness( int percent )
{
    lv_label_set_text_fmt( m_brightnessValue, "%d %%", percent );
    lv_bar_set_value( m_brightnessBar, percent, LV_ANIM_OFF );
    setBrightness( percent );
}

void Display::onBrightnessDrag( lv_event_t *event )
{
    Display *self = static_cast<Display *>( lv_event_get_user_data( event ) );
    const lv_event_code_t code = lv_event_get_code( event );
    lv_point_t point = {};
    lv_indev_t *indev = lv_indev_active();
    if ( indev != nullptr ) {
        lv_indev_get_point( indev, &point );
    }
    const bool alwaysOn = lv_event_get_current_target( event ) == self->m_aodTile;
    if ( code == LV_EVENT_LONG_PRESSED ) {
        self->m_adjusting = true;
        self->m_adjustItem = !alwaysOn ? Settings::Brightness
                                       : ( self->m_night ? Settings::AodNightBrightness : Settings::AodDayBrightness );
        self->m_adjustStart = self->m_settings->value( self->m_adjustItem );
        self->m_adjustPercent = self->m_adjustStart;
        self->m_adjustY = point.y;
        // The finger may drift sideways: the pages must not swipe while the value is set.
        lv_obj_set_scrollable( self->m_pages, false );
        // The always-on page stays undimmed, so that its brightness can be judged; the box goes
        // to the bottom there.
        lv_obj_set_style_bg_opa( self->m_brightnessShade, alwaysOn ? LV_OPA_TRANSP : LV_OPA_80, 0 );
        lv_obj_align( self->m_brightnessBox, LV_ALIGN_TOP_MID, -10, alwaysOn ? 330 : 176 );
        // The active level is one for day and night; the period is shown for information.
        const char *title = nullptr;
        if ( alwaysOn ) {
            title = Language::text( self->m_night ? Language::AodNight : Language::AodDay );
        } else {
            title = Language::text( self->m_night ? Language::BrightnessNight : Language::BrightnessDay );
        }
        lv_label_set_text( self->m_brightnessTitle, title );
        lv_obj_set_hidden( self->m_brightnessShade, false );
        lv_obj_set_hidden( self->m_brightnessBox, false );
        lv_obj_set_hidden( self->m_brightnessBar, false );
        self->showBrightness( self->m_adjustPercent );
        return;
    }
    if ( !self->m_adjusting ) {
        return;
    }
    if ( code == LV_EVENT_PRESSING ) {
        const int minimum = alwaysOn ? 1 : 5;
        int percent = self->m_adjustStart + ( self->m_adjustY - point.y ) * 100 / s_barHeight;
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
    lv_obj_set_hidden( self->m_brightnessShade, true );
    lv_obj_set_hidden( self->m_brightnessBox, true );
    lv_obj_set_hidden( self->m_brightnessBar, true );
    setBrightness( self->m_settings->value( Settings::Brightness ) );
    ESP_LOGI( TAG, "Brightness setting %d: %d %%", self->m_adjustItem, self->m_adjustPercent );
}

void Display::onPageChanged( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    lv_obj_t *tile = lv_tileview_get_tile_active( display->m_pages );
    display->m_corners.setAlwaysOnLook( tile == display->m_aodTile );
    display->updatePageSwipe();
    display->updateJoinCodeBrightness();
    // The next page beyond this one, while this one is looked at.
    for ( int i = 0; i < PageCount; ++i ) {
        if ( display->m_tiles[i] == tile ) {
            display->ensureBuilt( i - 1 );
            display->ensureBuilt( i + 1 );
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
    m_aodShown[1] = -1;
    applyOrders();
    m_face.setStyle( m_settings->value( Settings::FaceStyle ), m_settings->value( Settings::FaceItems ) );
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        showSetting( i );
    }
    setBrightness( m_settings->value( Settings::Brightness ) );
    // A save from the portal while the code shows keeps it dim.
    m_joinCodeDimmed = false;
    updateJoinCodeBrightness();
    bsp_display_unlock();
}

void Display::stop()
{
    if ( m_started ) {
        bsp_display_lock( s_lockTimeoutMs );
    }
}

void Display::update( const Rtc::DateTime &time, int utcOffsetMinutes, const Battery &battery )
{
    if ( !m_started ) {
        return;
    }
    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    m_face.showTime( time );
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
    refreshAlwaysOnPage( false );
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
        if ( m_setupValues[i] != nullptr ) {
            lv_label_set_text( m_setupValues[i], m_setupTexts[i] );
        }
    }
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

void Display::refreshAlwaysOnPage( bool force, int direction )
{
    // Before the first update (or after a skipped one) the local time is still all zero.
    if ( m_localTime.month == 0 ) {
        return;
    }
    AlwaysOn::Look look = { m_settings->value( Settings::AodStyle ), m_settings->value( Settings::AodItems ),
                            m_moonPercent, m_waxing, {}, 0, 0, m_settings->value( Settings::Language ) };
    if ( m_haveSky ) {
        look.corners = Corners::fromSky( m_day );
    }
    const int shown[4] = { m_localTime.hour * 60 + m_localTime.minute, look.style, look.items, look.moonPercent };
    bool same = !force;
    for ( int i = 0; i < 4; ++i ) {
        same = same && m_aodShown[i] == shown[i];
        m_aodShown[i] = shown[i];
    }
    if ( !same ) {
        m_aodPage.show( look, m_localTime, m_now.time, direction );
    }
}

void Display::onGesture( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    const lv_dir_t direction = lv_indev_get_gesture_dir( lv_indev_active() );
    // A drag that sets the brightness is no style swipe.
    if ( display->m_adjusting || ( direction != LV_DIR_TOP && direction != LV_DIR_BOTTOM ) ) {
        return;
    }
    const bool alwaysOn = lv_event_get_current_target( event ) == display->m_aodTile;
    const Settings::Item item = alwaysOn ? Settings::AodStyle : Settings::FaceStyle;
    // Up: the next style shown; down: the one before. The choice is saved with the other
    // settings when active mode ends.
    int parts[PartOrder::MaxParts] = {};
    const int shown = display->shownStyles( alwaysOn, parts );
    if ( shown < 2 ) {
        return;
    }
    const int slide = direction == LV_DIR_TOP ? 1 : -1;
    display->m_settings->setValue( item, PartOrder::step( parts, shown, display->m_settings->value( item ), slide ) );
    const int position = PartOrder::position( parts, shown, display->m_settings->value( item ) );
    ESP_LOGI( TAG, "%s style %d", alwaysOn ? "Always-on" : "Face", display->m_settings->value( item ) );
    if ( alwaysOn ) {
        display->refreshAlwaysOnPage( true, slide );
        display->m_aodMarks.show( position );
        return;
    }
    display->m_faceMarks.flash( position );
    display->m_face.setStyle( display->m_settings->value( Settings::FaceStyle ),
                              display->m_settings->value( Settings::FaceItems ), slide );
    display->m_face.showTime( display->m_localTime );
    if ( display->m_haveSky ) {
        display->m_face.showSky( display->m_day, display->m_now, display->m_offsetMinutes );
    }
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
void Display::showFaceStyle( int style )
{
    if ( !m_started || !bsp_display_lock( 1000 ) ) {
        return;
    }
    m_face.setStyle( style, m_settings->value( Settings::FaceItems ) );
    m_face.showTime( m_localTime );
    if ( m_haveSky ) {
        m_face.showSky( m_day, m_now, m_offsetMinutes );
    }
    bsp_display_unlock();
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
