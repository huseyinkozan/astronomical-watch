#include "Display.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"

static const char *TAG = "display";

static const int s_lockTimeoutMs = 100;

static lv_obj_t *createLabel( lv_obj_t *screen, const lv_font_t *font, int yOffset )
{
    lv_obj_t *label = lv_label_create( screen );
    lv_obj_set_style_text_font( label, font, 0 );
    lv_obj_set_style_text_color( label, lv_color_white(), 0 );
    lv_label_set_text( label, "" );
    lv_obj_align( label, LV_ALIGN_CENTER, 0, yOffset );
    return label;
}

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
    m_timeLabel = createLabel( screen, &lv_font_montserrat_48, -60 );
    m_dateLabel = createLabel( screen, &lv_font_montserrat_28, 10 );
    m_batteryLabel = createLabel( screen, &lv_font_montserrat_28, 70 );
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

void Display::showStatus( const Rtc::DateTime &time, uint16_t millivolts, uint8_t percent, bool vbusGood )
{
    if ( m_timeLabel == nullptr || !bsp_display_lock( s_lockTimeoutMs ) ) {
        return;
    }
    lv_label_set_text_fmt( m_timeLabel, "%02u:%02u:%02u", time.hour, time.minute, time.second );
    lv_label_set_text_fmt( m_dateLabel, "%04u-%02u-%02u UTC", time.year, time.month, time.day );
    lv_label_set_text_fmt( m_batteryLabel, "%u %%  %u mV%s", percent, millivolts, vbusGood ? "  USB" : "" );
    bsp_display_unlock();
}
