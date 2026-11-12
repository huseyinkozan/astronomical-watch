#include "Panel.h"

#include <cstdio>
#include <cstring>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"

static const char *TAG = "panel";

// CO5300 QSPI write: opcode 0x02 in bits 31:24, command in bits 15:8 (as in esp_lcd_sh8601).
static const int PANEL_CMD_BRIGHTNESS = ( 0x02 << 24 ) | ( 0x51 << 8 );
static const int PANEL_CMD_IDLE_ON = ( 0x02 << 24 ) | ( 0x39 << 8 );
static const int PANEL_CMD_NOP = ( 0x02 << 24 ) | ( 0x00 << 8 );
// The panel's columns start 22 pixels into its memory (BSP: esp_lcd_panel_set_gap 0x16).
static const int s_columnGap = 22;

// Strips of constant colour; static arrays are in internal RAM, which the SPI DMA can read.
// Static arrays are in internal RAM, which the SPI DMA can read.
alignas( 4 ) static uint16_t s_buffer[Panel::BufferPixels];

static const gpio_num_t s_lowPins[5] = { BSP_LCD_PCLK, BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2,
                                         BSP_LCD_DATA3 };

// The BSP's init commands (esp32_s3_touch_amoled_2_06.c, lcd_init_cmds) without the last one,
// which sets full brightness right after display on.
static const uint8_t s_c4[] = { 0x80 };
static const uint8_t s_44[] = { 0x01, 0xD1 };
static const uint8_t s_35[] = { 0x00 };
static const uint8_t s_53[] = { 0x20 };
static const uint8_t s_63[] = { 0xFF };
static const uint8_t s_51[] = { 0x00 };
static const uint8_t s_2a[] = { 0x00, 0x16, 0x01, 0xAF };
static const uint8_t s_2b[] = { 0x00, 0x00, 0x01, 0xF5 };
static const sh8601_lcd_init_cmd_t s_initCommands[] = {
    { 0x11, nullptr, 0, 120 }, { 0xC4, s_c4, 1, 0 },  { 0x44, s_44, 2, 0 },  { 0x35, s_35, 1, 0 },
    { 0x53, s_53, 1, 10 },     { 0x63, s_63, 1, 10 }, { 0x51, s_51, 1, 10 }, { 0x2A, s_2a, 4, 0 },
    { 0x2B, s_2b, 4, 0 },      { 0x29, nullptr, 0, 10 },
};

// The QSPI bus, the panel IO and the esp_lcd_sh8601 driver. With `initialise`, the panel is reset
// and initialised; without, it keeps its image (always-on).
static bool createPanel( int maxTransferBytes, bool initialise, esp_lcd_panel_io_handle_t &io,
                         esp_lcd_panel_handle_t &panel )
{
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
    busConfig.max_transfer_sz = maxTransferBytes;
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
    if ( esp_lcd_new_panel_io_spi( static_cast<esp_lcd_spi_bus_handle_t>( BSP_LCD_SPI_NUM ), &ioConfig, &io ) !=
         ESP_OK ) {
        ESP_LOGE( TAG, "Panel IO failed" );
        return false;
    }
    sh8601_vendor_config_t vendorConfig = {};
    vendorConfig.flags.use_qspi_interface = 1;
    if ( initialise ) {
        vendorConfig.init_cmds = s_initCommands;
        vendorConfig.init_cmds_size = sizeof( s_initCommands ) / sizeof( s_initCommands[0] );
    }
    esp_lcd_panel_dev_config_t panelConfig = {};
    panelConfig.reset_gpio_num = initialise ? BSP_LCD_RST : GPIO_NUM_NC;
    panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panelConfig.bits_per_pixel = 16;
    panelConfig.vendor_config = &vendorConfig;
    if ( esp_lcd_new_panel_sh8601( io, &panelConfig, &panel ) != ESP_OK ) {
        ESP_LOGE( TAG, "Panel driver failed" );
        return false;
    }
    if ( initialise ) {
        esp_lcd_panel_reset( panel );
        esp_lcd_panel_init( panel );
    }
    esp_lcd_panel_set_gap( panel, s_columnGap, 0 );
    return true;
}

bool Panel::open()
{
    releasePins();
    return createPanel( BufferPixels * 2, false, m_io, m_panel );
}

bool Panel::openForLvgl( int maxTransferBytes, esp_lcd_panel_io_handle_t &io, esp_lcd_panel_handle_t &panel )
{
    return createPanel( maxTransferBytes, true, io, panel );
}

void Panel::setBrightness( esp_lcd_panel_io_handle_t io, int percent )
{
    const uint8_t value = static_cast<uint8_t>( percent * 255 / 100 );
    esp_lcd_panel_io_tx_param( io, PANEL_CMD_BRIGHTNESS, &value, 1 );
}

#if CONFIG_WATCH_DEV_LOGGING
void Panel::record()
{
    m_recording = true;
}
#endif

void Panel::clear()
{
#if CONFIG_WATCH_DEV_LOGGING
    if ( m_recording ) {
        printf( "SHOT RECT 0 0 %d %d 0\n", WIDTH, HEIGHT );
        return;
    }
#endif
    if ( m_panel == nullptr ) {
        return;
    }
    // Every band sends the same black buffer, so they are queued together; the command at the
    // end waits until they are out, before anyone fills the buffer again.
    memset( s_buffer, 0, sizeof( s_buffer ) );
    const int bandRows = BufferPixels / WIDTH;
    for ( int row = 0; row < HEIGHT; row += bandRows ) {
        const int rows = ( HEIGHT - row < bandRows ) ? HEIGHT - row : bandRows;
        esp_lcd_panel_draw_bitmap( m_panel, 0, row, WIDTH, row + rows, s_buffer );
    }
    esp_lcd_panel_io_tx_param( m_io, PANEL_CMD_NOP, nullptr, 0 );
}

uint16_t *Panel::buffer()
{
    return s_buffer;
}

void Panel::draw( int x, int y, int width, int height, const uint16_t *pixels )
{
#if CONFIG_WATCH_DEV_LOGGING
    if ( m_recording ) {
        // Run-length rows as tools/screenshot.py reads them, in plain RGB565.
        for ( int row = 0; row < height; ++row ) {
            const uint16_t *line = pixels + row * width;
            printf( "SHOT %d %d ", y + row, x );
            int start = 0;
            while ( start < width ) {
                int end = start + 1;
                while ( end < width && line[end] == line[start] ) {
                    ++end;
                }
                const uint16_t value = static_cast<uint16_t>( ( line[start] >> 8 ) | ( line[start] << 8 ) );
                printf( "%s%04X*%d", start == 0 ? "" : ",", value, end - start );
                start = end;
            }
            printf( "\n" );
        }
        return;
    }
#endif
    if ( m_panel == nullptr ) {
        return;
    }
    esp_lcd_panel_draw_bitmap( m_panel, x, y, x + width, y + height, pixels );
    // A command waits until the queued pixels are sent; the CO5300 ignores NOP.
    esp_lcd_panel_io_tx_param( m_io, PANEL_CMD_NOP, nullptr, 0 );
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
