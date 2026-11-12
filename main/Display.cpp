#include "Display.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "display";

static const int s_lockTimeoutMs = 100;

static lv_obj_t *createLabel( lv_obj_t *parent, const lv_font_t *font, int yOffset )
{
    lv_obj_t *label = lv_label_create( parent );
    lv_obj_set_style_text_font( label, font, 0 );
    lv_obj_set_style_text_color( label, lv_color_white(), 0 );
    lv_label_set_text( label, "" );
    lv_obj_align( label, LV_ALIGN_CENTER, 0, yOffset );
    return label;
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

bool Display::start( int brightnessPercent )
{
    lv_display_t *display = bsp_display_start();
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
    lv_obj_t *batteryPage = lv_tileview_add_tile( pages, 1, 0, LV_DIR_LEFT );

    m_timeLabel = createLabel( facePage, &lv_font_montserrat_48, -60 );
    m_dateLabel = createLabel( facePage, &lv_font_montserrat_28, 10 );
    m_batteryLabel = createLabel( facePage, &lv_font_montserrat_28, 70 );

    lv_obj_t *title = createLabel( batteryPage, &lv_font_montserrat_28, -120 );
    lv_label_set_text( title, "Battery" );
    m_percentLabel = createLabel( batteryPage, &lv_font_montserrat_48, -50 );
    m_voltageLabel = createLabel( batteryPage, &lv_font_montserrat_28, 20 );
    m_chargeLabel = createLabel( batteryPage, &lv_font_montserrat_28, 70 );

#if CONFIG_WATCH_DEV_LOGGING
    lv_indev_t *touch = bsp_display_get_input_dev();
    if ( touch != nullptr ) {
        lv_indev_add_event_cb( touch, onTouchPressed, LV_EVENT_PRESSED, nullptr );
    }
#endif
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
    if ( m_timeLabel == nullptr || !bsp_display_lock( s_lockTimeoutMs ) ) {
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

uint32_t Display::inactiveMs()
{
    if ( m_timeLabel != nullptr && bsp_display_lock( s_lockTimeoutMs ) ) {
        m_inactiveMs = lv_display_get_inactive_time( nullptr );
        bsp_display_unlock();
    }
    return m_inactiveMs;
}
