#include "Display.h"

#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "bsp/touch.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
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

// Settings rows: full width and about the Wear OS minimum touch target of 48 dp (9 mm, 110 px on
// this panel). Four rows fill the page, so no vertical scroll competes with the page swipe.
static const int s_rowHeight = 108;
static const int s_rowGap = 12;
static const char *s_settingNames[Settings::PageItemCount] = { "Screen off", "Always-on", "Brightness", "Auto off" };

// Page order, left to right; the names label the development screenshots.
enum Page
{
    FacePage,
    BatteryPage,
    SettingsPage,
#if CONFIG_WATCH_SETUP_PORTAL
    SetupPage,
#endif
    AboutPage,
    PageCount
};
static const char *s_pageNames[PageCount] = { "face", "battery", "settings",
#if CONFIG_WATCH_SETUP_PORTAL
                                              "setup",
#endif
                                              "about" };

static void buildAboutPage( lv_obj_t *page )
{
    lv_obj_set_flex_flow( page, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_row( page, 24, 0 );
    lv_obj_set_scrollbar_mode( page, LV_SCROLLBAR_MODE_OFF );
    const char *lines[4] = { "Astronomical Watch", esp_app_get_description()->version,
                             "gitlab.com/huseyinkozan/\nastronomical-watch", "MIT License" };
    for ( int i = 0; i < 4; ++i ) {
        lv_obj_t *label = lv_label_create( page );
        lv_obj_set_style_text_font( label, &lv_font_montserrat_28, 0 );
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
static lv_display_t *startLvgl( lv_indev_t *&touch )
{
    const lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
    if ( lvgl_port_init( &portConfig ) != ESP_OK ) {
        return nullptr;
    }
    bsp_display_config_t panelConfig = {};
    panelConfig.max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8;
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t io = nullptr;
    if ( bsp_display_new( &panelConfig, &panel, &io ) != ESP_OK ) {
        return nullptr;
    }
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
    bsp_display_brightness_set( brightnessPercent );

    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        ESP_LOGE( TAG, "Display lock timeout" );
        return false;
    }
    // Black background: an AMOLED pixel that is off draws no current.
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color( screen, lv_color_black(), 0 );
    lv_obj_t *pages = lv_tileview_create( screen );
    m_pages = pages;
    lv_obj_set_style_bg_color( pages, lv_color_black(), 0 );
    lv_obj_set_scrollbar_mode( pages, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *facePage = lv_tileview_add_tile( pages, FacePage, 0, LV_DIR_RIGHT );
    lv_obj_t *batteryPage = lv_tileview_add_tile( pages, BatteryPage, 0, LV_DIR_HOR );
    lv_obj_t *settingsPage = lv_tileview_add_tile( pages, SettingsPage, 0, LV_DIR_HOR );
#if CONFIG_WATCH_SETUP_PORTAL
    buildSetupPage( lv_tileview_add_tile( pages, SetupPage, 0, LV_DIR_HOR ) );
#endif
    buildAboutPage( lv_tileview_add_tile( pages, AboutPage, 0, LV_DIR_LEFT ) );

    m_timeLabel = createLabel( facePage, &lv_font_montserrat_48, -60 );
    m_dateLabel = createLabel( facePage, &lv_font_montserrat_28, 10 );
    m_batteryLabel = createLabel( facePage, &lv_font_montserrat_28, 70 );

    lv_obj_t *title = createLabel( batteryPage, &lv_font_montserrat_28, -120 );
    lv_label_set_text( title, "Battery" );
    m_percentLabel = createLabel( batteryPage, &lv_font_montserrat_48, -50 );
    m_voltageLabel = createLabel( batteryPage, &lv_font_montserrat_28, 20 );
    m_chargeLabel = createLabel( batteryPage, &lv_font_montserrat_28, 70 );

    lv_obj_set_flex_flow( settingsPage, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( settingsPage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_hor( settingsPage, 16, 0 );
    lv_obj_set_style_pad_row( settingsPage, s_rowGap, 0 );
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        m_settingValues[i] = createRow( settingsPage, s_settingNames[i], onSettingClicked );
        m_settingRows[i] = lv_obj_get_parent( m_settingValues[i] );
        showSetting( i );
    }

    if ( touch != nullptr ) {
        lv_indev_set_scroll_limit( touch, s_scrollLimitPx );
#if CONFIG_WATCH_DEV_LOGGING
        lv_indev_add_event_cb( touch, onTouchPressed, LV_EVENT_PRESSED, nullptr );
#endif
    }
#if CONFIG_WATCH_DEV_LOGGING
    lv_display_add_event_cb( display, onFlushStart, LV_EVENT_FLUSH_START, this );
    lv_display_add_event_cb( display, onRefreshReady, LV_EVENT_REFR_READY, this );
#endif
    bsp_display_unlock();

    // Internal RAM left for the rest of active mode (the Wi-Fi setup portal needs about 100 KB).
    ESP_LOGI( TAG, "Display started, brightness %d %%, internal heap %u free, %u largest", brightnessPercent,
              static_cast<unsigned>( heap_caps_get_free_size( MALLOC_CAP_INTERNAL ) ),
              static_cast<unsigned>( heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ) ) );
    return true;
}

// A full-width row with a name on the left; returns the value label on the right.
lv_obj_t *Display::createRow( lv_obj_t *parent, const char *name, lv_event_cb_t onClicked )
{
    lv_obj_t *row = lv_obj_create( parent );
    lv_obj_set_size( row, lv_pct( 100 ), s_rowHeight );
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
    lv_obj_t *nameLabel = createLabel( row, &lv_font_montserrat_28, LV_ALIGN_LEFT_MID, 0 );
    lv_label_set_text( nameLabel, name );
    return createLabel( row, &lv_font_montserrat_28, LV_ALIGN_RIGHT_MID, 0 );
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
    m_portalValue = createRow( page, "Wi-Fi setup", onSetupClicked );
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
    createInfoLabel( info, &lv_font_montserrat_28, grey, "Network" );
    m_networkLabel = createInfoLabel( info, &lv_font_montserrat_28, lv_color_white(), "" );
    // A gap separates the network from the password.
    lv_obj_set_style_margin_top( createInfoLabel( info, &lv_font_montserrat_28, grey, "Password" ), 12, 0 );
    m_passwordLabel = createInfoLabel( info, &lv_font_montserrat_48, lv_color_white(), "" );
    m_setupInfo = createInfoLabel( info, &lv_font_montserrat_28, grey, "" );

    lv_obj_t *buttonLabel = createRow( page, "", onSetupClicked );
    m_passwordRow = lv_obj_get_parent( buttonLabel );
    lv_obj_align( buttonLabel, LV_ALIGN_CENTER, 0, 0 );
    lv_label_set_text( buttonLabel, LV_SYMBOL_REFRESH "  New password" );
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

int Display::takeRequests()
{
    return m_requests.exchange( 0 );
}

void Display::showSetup( const Setup &setup )
{
    if ( m_setupInfo == nullptr || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    lv_label_set_text( m_portalValue, setup.running ? "On" : "Off" );
    lv_label_set_text( m_networkLabel, setup.ssid );
    lv_label_set_text( m_passwordLabel, setup.password );
    // A new password only while the access point is off; the button dims meanwhile.
    lv_obj_set_style_opa( m_passwordRow, setup.running ? LV_OPA_40 : LV_OPA_COVER, 0 );
    if ( setup.running ) {
        lv_label_set_text_fmt( m_setupInfo, "%s  %d:%02d left", setup.address, setup.secondsLeft / 60,
                               setup.secondsLeft % 60 );
    } else {
        lv_label_set_text( m_setupInfo, "Not started" );
    }
    bsp_display_unlock();
}

void Display::refreshSettings()
{
    if ( m_timeLabel == nullptr || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    for ( int i = 0; i < Settings::PageItemCount; ++i ) {
        showSetting( i );
    }
    bsp_display_brightness_set( m_settings->value( Settings::Brightness ) );
    bsp_display_unlock();
}

void Display::stop()
{
    if ( m_timeLabel != nullptr ) {
        bsp_display_lock( s_lockTimeoutMs );
    }
}

void Display::update( const Rtc::DateTime &time, int utcOffsetMinutes, const Battery &battery )
{
    if ( m_timeLabel == nullptr ) {
        return;
    }
    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    lv_label_set_text_fmt( m_timeLabel, "%02u:%02u:%02u", time.hour, time.minute, time.second );
    // "UTC", "UTC+3" or "UTC+5:30" after the date.
    char offset[16] = "";
    const int offsetAbs = utcOffsetMinutes < 0 ? -utcOffsetMinutes : utcOffsetMinutes;
    if ( offsetAbs % 60 != 0 ) {
        snprintf( offset, sizeof( offset ), "%c%d:%02d", utcOffsetMinutes < 0 ? '-' : '+', offsetAbs / 60,
                  offsetAbs % 60 );
    } else if ( offsetAbs != 0 ) {
        snprintf( offset, sizeof( offset ), "%c%d", utcOffsetMinutes < 0 ? '-' : '+', offsetAbs / 60 );
    }
    lv_label_set_text_fmt( m_dateLabel, "%04u-%02u-%02u UTC%s", time.year, time.month, time.day, offset );
    lv_label_set_text_fmt( m_batteryLabel, "%u %%%s", battery.percent, battery.vbusGood ? "  USB" : "" );

    lv_label_set_text_fmt( m_percentLabel, "%u %%", battery.percent );
    lv_label_set_text_fmt( m_voltageLabel, "%u mV", battery.millivolts );
    lv_label_set_text_fmt( m_chargeLabel, "%s", battery.vbusGood ? battery.chargeState : "on battery" );
    bsp_display_unlock();
}

#if CONFIG_WATCH_DEV_LOGGING
void Display::onFlushStart( lv_event_t *event )
{
    Display *self = static_cast<Display *>( lv_event_get_user_data( event ) );
    if ( !self->m_capturing ) {
        return;
    }
    // Called before the band goes to the panel: the draw buffer holds it in LVGL's RGB565, row
    // after row (esp_lvgl_port swaps the bytes only inside the flush).
    const lv_area_t *area = static_cast<const lv_area_t *>( lv_event_get_param( event ) );
    const lv_draw_buf_t *buffer = lv_display_get_buf_active( lv_display_get_default() );
    const uint16_t *pixels = reinterpret_cast<const uint16_t *>( buffer->data );
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
}

void Display::capture( int page )
{
    if ( m_pages == nullptr || page < 0 || page >= PageCount || !bsp_display_lock( 1000 ) ) {
        return;
    }
    ESP_LOGI( TAG, "Screenshot of page %d", page );
    lv_tileview_set_tile_by_index( m_pages, static_cast<uint32_t>( page ), 0, LV_ANIM_OFF );
    lv_display_trigger_activity( nullptr );
    printf( "SHOT BEGIN %d %d %d %s\n", page, BSP_LCD_H_RES, BSP_LCD_V_RES, s_pageNames[page] );
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
        if ( item == Settings::Brightness ) {
            bsp_display_brightness_set( display->m_settings->value( item ) );
        }
        ESP_LOGI( TAG, "Setting %s: %s", s_settingNames[i], lv_label_get_text( display->m_settingValues[i] ) );
    }
}

void Display::showSetting( int item )
{
    const int value = m_settings->value( static_cast<Settings::Item>( item ) );
    lv_obj_t *label = m_settingValues[item];
    if ( item == Settings::DisplayTimeout ) {
        lv_label_set_text_fmt( label, "%d s", value );
    } else if ( item == Settings::AlwaysOnFace ) {
        lv_label_set_text( label, value != 0 ? "On" : "Off" );
    } else if ( item == Settings::Brightness ) {
        lv_label_set_text( label, value <= 20 ? "Low" : ( value <= 50 ? "Mid" : "High" ) );
    } else if ( value == 0 ) {
        lv_label_set_text( label, "Never" );
    } else {
        lv_label_set_text_fmt( label, "%d min", value );
    }
}

uint32_t Display::inactiveMs()
{
    if ( m_timeLabel != nullptr && bsp_display_lock( s_lockTimeoutMs ) ) {
        m_inactiveMs = lv_display_get_inactive_time( nullptr );
        bsp_display_unlock();
    }
    return m_inactiveMs;
}
