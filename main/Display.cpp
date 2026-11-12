#include "Display.h"

#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "sdkconfig.h"

static const char *TAG = "display";

static const int s_lockTimeoutMs = 100;

// LVGL draw buffer: 100 lines, as in the BSP. LVGL waits for the transfer of one band before
// drawing the next into it.
static const int s_bufferLines = 100;

// LVGL turns a press into a scroll, and drops its click, after 10 px of movement (0.8 mm): a
// fingertip on a wrist moves more than that during a tap. 30 px (2.4 mm) still leaves the page
// swipe responsive.
static const int s_scrollLimitPx = 30;

// Settings rows: full width and about the Wear OS minimum touch target of 48 dp (9 mm, 110 px on
// this panel). Four rows fill the page, so no vertical scroll competes with the page swipe.
static const int s_rowHeight = 108;
static const int s_rowGap = 12;
static const char *s_settingNames[Settings::ItemCount] = { "Screen off", "Always-on", "Brightness", "Auto off" };

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
    lv_obj_set_style_bg_color( pages, lv_color_black(), 0 );
    lv_obj_set_scrollbar_mode( pages, LV_SCROLLBAR_MODE_OFF );
    lv_obj_t *facePage = lv_tileview_add_tile( pages, 0, 0, LV_DIR_RIGHT );
    lv_obj_t *batteryPage = lv_tileview_add_tile( pages, 1, 0, LV_DIR_HOR );
    lv_obj_t *settingsPage = lv_tileview_add_tile( pages, 2, 0, LV_DIR_LEFT );

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
    for ( int i = 0; i < Settings::ItemCount; ++i ) {
        lv_obj_t *row = lv_obj_create( settingsPage );
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
        lv_obj_add_event_cb( row, onSettingClicked, LV_EVENT_CLICKED, this );
        lv_obj_t *name = createLabel( row, &lv_font_montserrat_28, LV_ALIGN_LEFT_MID, 0 );
        lv_label_set_text( name, s_settingNames[i] );
        m_settingRows[i] = row;
        m_settingValues[i] = createLabel( row, &lv_font_montserrat_28, LV_ALIGN_RIGHT_MID, 0 );
        showSetting( i );
    }

    if ( touch != nullptr ) {
        lv_indev_set_scroll_limit( touch, s_scrollLimitPx );
#if CONFIG_WATCH_DEV_LOGGING
        lv_indev_add_event_cb( touch, onTouchPressed, LV_EVENT_PRESSED, nullptr );
#endif
    }
    bsp_display_unlock();

    ESP_LOGI( TAG, "Display started, brightness %d %%", brightnessPercent );
    return true;
}

void Display::stop()
{
    if ( m_timeLabel != nullptr ) {
        bsp_display_lock( s_lockTimeoutMs );
    }
}

void Display::update( const Rtc::DateTime &time, const Battery &battery )
{
    if ( m_timeLabel == nullptr ) {
        return;
    }
    if ( !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    lv_label_set_text_fmt( m_timeLabel, "%02u:%02u:%02u", time.hour, time.minute, time.second );
    lv_label_set_text_fmt( m_dateLabel, "%04u-%02u-%02u UTC", time.year, time.month, time.day );
    lv_label_set_text_fmt( m_batteryLabel, "%u %%%s", battery.percent, battery.vbusGood ? "  USB" : "" );

    lv_label_set_text_fmt( m_percentLabel, "%u %%", battery.percent );
    lv_label_set_text_fmt( m_voltageLabel, "%u mV", battery.millivolts );
    lv_label_set_text_fmt( m_chargeLabel, "%s", battery.vbusGood ? battery.chargeState : "on battery" );
    bsp_display_unlock();
}

void Display::onSettingClicked( lv_event_t *event )
{
    Display *display = static_cast<Display *>( lv_event_get_user_data( event ) );
    lv_obj_t *row = static_cast<lv_obj_t *>( lv_event_get_current_target( event ) );
    for ( int i = 0; i < Settings::ItemCount; ++i ) {
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
