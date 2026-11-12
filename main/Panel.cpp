#include "Panel.h"

#include <cstdio>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"

static const char *TAG = "panel";

// CO5300 QSPI write: opcode 0x02 in bits 31:24, command in bits 15:8 (as in esp_lcd_sh8601).
static const int PANEL_CMD_BRIGHTNESS = ( 0x02 << 24 ) | ( 0x51 << 8 );
static const int PANEL_CMD_IDLE_ON = ( 0x02 << 24 ) | ( 0x39 << 8 );
// The panel's columns start 22 pixels into its memory (BSP: esp_lcd_panel_set_gap 0x16).
static const int s_columnGap = 22;

// Strips of constant colour; static arrays are in internal RAM, which the SPI DMA can read.
static const int s_stripPixels = Panel::WIDTH * 8;
alignas( 4 ) static uint16_t s_black[s_stripPixels];
alignas( 4 ) static uint16_t s_white[s_stripPixels];

static const gpio_num_t s_lowPins[5] = { BSP_LCD_PCLK, BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2,
                                         BSP_LCD_DATA3 };

bool Panel::open()
{
    releasePins();
    for ( int i = 0; i < s_stripPixels; ++i ) {
        s_white[i] = 0xFFFF;
    }

    // The SH8601 config macros use designated initializers out of declaration order, which C++
    // rejects; these are their values (QSPI, 40 MHz, 32-bit commands).
    spi_bus_config_t busConfig = {};
    busConfig.data0_io_num = BSP_LCD_DATA0;
    busConfig.data1_io_num = BSP_LCD_DATA1;
    busConfig.sclk_io_num = BSP_LCD_PCLK;
    busConfig.data2_io_num = BSP_LCD_DATA2;
    busConfig.data3_io_num = BSP_LCD_DATA3;
    busConfig.data4_io_num = -1;
    busConfig.data5_io_num = -1;
    busConfig.data6_io_num = -1;
    busConfig.data7_io_num = -1;
    busConfig.max_transfer_sz = s_stripPixels * 2;
    if ( spi_bus_initialize( BSP_LCD_SPI_NUM, &busConfig, SPI_DMA_CH_AUTO ) != ESP_OK ) {
        ESP_LOGE( TAG, "SPI bus init failed" );
        return false;
    }
    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.cs_gpio_num = BSP_LCD_CS;
    ioConfig.dc_gpio_num = GPIO_NUM_NC;
    ioConfig.spi_mode = 0;
    ioConfig.pclk_hz = 40 * 1000 * 1000;
    ioConfig.trans_queue_depth = 10;
    ioConfig.lcd_cmd_bits = 32;
    ioConfig.lcd_param_bits = 8;
    ioConfig.flags.quad_mode = 1;
    if ( esp_lcd_new_panel_io_spi( static_cast<esp_lcd_spi_bus_handle_t>( BSP_LCD_SPI_NUM ), &ioConfig, &m_io ) !=
         ESP_OK ) {
        ESP_LOGE( TAG, "Panel IO failed" );
        return false;
    }
    sh8601_vendor_config_t vendorConfig = {};
    vendorConfig.flags.use_qspi_interface = 1;
    esp_lcd_panel_dev_config_t panelConfig = {};
    panelConfig.reset_gpio_num = GPIO_NUM_NC;   // never reset: the image must stay
    panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panelConfig.bits_per_pixel = 16;
    panelConfig.vendor_config = &vendorConfig;
    if ( esp_lcd_new_panel_sh8601( m_io, &panelConfig, &m_panel ) != ESP_OK ) {
        ESP_LOGE( TAG, "Panel driver failed" );
        return false;
    }
    esp_lcd_panel_set_gap( m_panel, s_columnGap, 0 );
    return true;
}

#if CONFIG_WATCH_DEV_LOGGING
void Panel::record()
{
    m_recording = true;
}
#endif

void Panel::fill( int x, int y, int width, int height, bool white )
{
#if CONFIG_WATCH_DEV_LOGGING
    if ( m_recording ) {
        printf( "SHOT RECT %d %d %d %d %d\n", x, y, width, height, white ? 1 : 0 );
        return;
    }
#endif
    if ( m_panel == nullptr ) {
        return;
    }
    const uint16_t *strip = white ? s_white : s_black;
    // The strip buffers never change, so queued transfers may share them.
    const int stripRows = ( s_stripPixels / width ) & ~1;
    for ( int row = 0; row < height; row += stripRows ) {
        const int rows = ( height - row < stripRows ) ? height - row : stripRows;
        esp_lcd_panel_draw_bitmap( m_panel, x, y + row, x + width, y + row + rows, strip );
    }
}

void Panel::setBrightness( int percent )
{
    if ( m_io == nullptr ) {
        return;
    }
    const uint8_t value = static_cast<uint8_t>( percent * 255 / 100 );
    esp_lcd_panel_io_tx_param( m_io, PANEL_CMD_BRIGHTNESS, &value, 1 );
}

void Panel::setIdleMode()
{
    if ( m_io != nullptr ) {
        esp_lcd_panel_io_tx_param( m_io, PANEL_CMD_IDLE_ON, nullptr, 0 );
    }
}

void Panel::holdPins()
{
    gpio_config_t config = {};
    config.mode = GPIO_MODE_OUTPUT;
    // Set the levels before switching to output, so no line glitches.
    gpio_set_level( BSP_LCD_CS, 1 );
    config.pin_bit_mask = 1ULL << BSP_LCD_CS;
    for ( int i = 0; i < 5; ++i ) {
        gpio_set_level( s_lowPins[i], 0 );
        config.pin_bit_mask |= 1ULL << s_lowPins[i];
    }
    gpio_config( &config );
    gpio_hold_en( BSP_LCD_CS );
    for ( int i = 0; i < 5; ++i ) {
        gpio_hold_en( s_lowPins[i] );
    }
}

void Panel::releasePins()
{
    // Drive the held levels first, so that CS stays high while the hold is released.
    gpio_config_t config = {};
    config.mode = GPIO_MODE_OUTPUT;
    config.pin_bit_mask = 1ULL << BSP_LCD_CS;
    for ( int i = 0; i < 5; ++i ) {
        config.pin_bit_mask |= 1ULL << s_lowPins[i];
    }
    gpio_set_level( BSP_LCD_CS, 1 );
    for ( int i = 0; i < 5; ++i ) {
        gpio_set_level( s_lowPins[i], 0 );
    }
    gpio_config( &config );
    gpio_hold_dis( BSP_LCD_CS );
    for ( int i = 0; i < 5; ++i ) {
        gpio_hold_dis( s_lowPins[i] );
    }
}
