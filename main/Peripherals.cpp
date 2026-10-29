#include "Peripherals.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "quiesce";

static const int s_i2cTimeoutMs = 50;

static const uint8_t TOUCH_ADDRESS = 0x38;
static const uint8_t TOUCH_REG_POWER_MODE = 0xA5;
static const uint8_t TOUCH_POWER_MODE_SLEEP = 0x03;

// CO5300 QSPI write: opcode 0x02 in bits 31:24, command in bits 15:8 (as in esp_lcd_sh8601).
static const int PANEL_CMD_DEEP_STANDBY = ( 0x02 << 24 ) | ( 0x4F << 8 );

static const uint8_t IMU_ADDRESS = 0x6B;
static const uint8_t IMU_REG_WHO_AM_I = 0x00;
static const uint8_t IMU_REG_CTRL1 = 0x02;
static const uint8_t IMU_REG_CTRL7 = 0x08;
static const uint8_t IMU_CTRL1_SENSOR_DISABLE = 0x01;

static const uint8_t RTC_ADDRESS = 0x51;
static const uint8_t RTC_REG_CONTROL2 = 0x01;
static const uint8_t RTC_COF_CLKOUT_OFF = 0x07;

// Kept in RTC memory: cleared on power-on and reset, so the steps run again then.
RTC_DATA_ATTR static bool s_quiesced;
RTC_DATA_ATTR static bool s_resetLinesHeld;

#if CONFIG_WATCH_QUIESCE
static const bool s_quiesceEnabled = true;
#else
static const bool s_quiesceEnabled = false;
#endif

static esp_err_t addDevice( i2c_master_bus_handle_t bus, uint8_t address, i2c_master_dev_handle_t &device )
{
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = 400000;
    return i2c_master_bus_add_device( bus, &config, &device );
}

static esp_err_t readRegister( i2c_master_dev_handle_t device, uint8_t reg, uint8_t &value )
{
    return i2c_master_transmit_receive( device, &reg, 1, &value, 1, s_i2cTimeoutMs );
}

static esp_err_t writeRegister( i2c_master_dev_handle_t device, uint8_t reg, uint8_t value )
{
    const uint8_t data[2] = { reg, value };
    return i2c_master_transmit( device, data, sizeof( data ), s_i2cTimeoutMs );
}

// Drives a reset line LOW for lowMs, then HIGH, and waits settleMs.
static void pulseReset( gpio_num_t gpio, int lowMs, int settleMs )
{
    gpio_hold_dis( gpio );
    gpio_set_level( gpio, 1 );
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << gpio;
    config.mode = GPIO_MODE_OUTPUT;
    gpio_config( &config );
    gpio_set_level( gpio, 0 );
    vTaskDelay( pdMS_TO_TICKS( lowMs ) );
    gpio_set_level( gpio, 1 );
    vTaskDelay( pdMS_TO_TICKS( settleMs ) );
}

// FT3168: a reset puts it into active mode; it must get the sleep command before any other
// device on the bus is accessed (FT3168 datasheet 2.3).
static bool sleepTouch( i2c_master_bus_handle_t bus )
{
    pulseReset( BSP_LCD_TOUCH_RST, 10, 100 );
    i2c_master_dev_handle_t touch = nullptr;
    if ( addDevice( bus, TOUCH_ADDRESS, touch ) != ESP_OK ) {
        return false;
    }
    uint8_t mode = 0;
    const esp_err_t before = readRegister( touch, TOUCH_REG_POWER_MODE, mode );
    const esp_err_t write = writeRegister( touch, TOUCH_REG_POWER_MODE, TOUCH_POWER_MODE_SLEEP );
    vTaskDelay( pdMS_TO_TICKS( 20 ) );
    uint8_t modeAfter = 0;
    const esp_err_t after = readRegister( touch, TOUCH_REG_POWER_MODE, modeAfter );
    i2c_master_bus_rm_device( touch );

    ESP_LOGI( TAG, "Touch: before %s (power mode 0x%02X), sleep command %s, after %s", esp_err_to_name( before ), mode,
              esp_err_to_name( write ), after == ESP_OK ? "still responds" : "no response" );
    return before == ESP_OK && write == ESP_OK;
}

// CO5300 starts in Sleep In (about 110-135 uA); Deep Standby is about 3 uA and is left only by
// a RESX low pulse (CO5300 datasheet 7.5.37). The BSP has no API that opens the panel IO
// without switching the display on, so the QSPI bus is opened here for one command.
static bool standbyPanel()
{
    pulseReset( BSP_LCD_RST, 10, 120 );

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
    busConfig.max_transfer_sz = 64;
    if ( spi_bus_initialize( BSP_LCD_SPI_NUM, &busConfig, SPI_DMA_DISABLED ) != ESP_OK ) {
        ESP_LOGE( TAG, "Panel: SPI bus init failed" );
        return false;
    }

    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.cs_gpio_num = BSP_LCD_CS;
    ioConfig.dc_gpio_num = GPIO_NUM_NC;
    ioConfig.spi_mode = 0;
    ioConfig.pclk_hz = 10 * 1000 * 1000;
    ioConfig.trans_queue_depth = 1;
    ioConfig.lcd_cmd_bits = 32;
    ioConfig.lcd_param_bits = 8;
    ioConfig.flags.quad_mode = 1;

    esp_lcd_panel_io_handle_t io = nullptr;
    esp_err_t err = esp_lcd_new_panel_io_spi( static_cast<esp_lcd_spi_bus_handle_t>( BSP_LCD_SPI_NUM ), &ioConfig, &io );
    if ( err == ESP_OK ) {
        const uint8_t enter = 0x01;
        err = esp_lcd_panel_io_tx_param( io, PANEL_CMD_DEEP_STANDBY, &enter, 1 );
        esp_lcd_panel_io_del( io );
    }
    spi_bus_free( BSP_LCD_SPI_NUM );

    ESP_LOGI( TAG, "Panel: deep standby command %s", esp_err_to_name( err ) );
    return err == ESP_OK;
}

// QMI8658: sensors off (CTRL7 = 0) and internal oscillator off (CTRL1 bit0), about 6 uA.
static bool powerDownImu( i2c_master_bus_handle_t bus )
{
    i2c_master_dev_handle_t imu = nullptr;
    if ( addDevice( bus, IMU_ADDRESS, imu ) != ESP_OK ) {
        return false;
    }
    uint8_t whoAmI = 0;
    uint8_t ctrl1 = 0;
    uint8_t ctrl7 = 0xFF;
    esp_err_t err = readRegister( imu, IMU_REG_WHO_AM_I, whoAmI );
    if ( err == ESP_OK ) {
        err = writeRegister( imu, IMU_REG_CTRL7, 0x00 );
    }
    if ( err == ESP_OK ) {
        err = readRegister( imu, IMU_REG_CTRL1, ctrl1 );
    }
    if ( err == ESP_OK ) {
        err = writeRegister( imu, IMU_REG_CTRL1, ctrl1 | IMU_CTRL1_SENSOR_DISABLE );
    }
    if ( err == ESP_OK ) {
        readRegister( imu, IMU_REG_CTRL1, ctrl1 );
        readRegister( imu, IMU_REG_CTRL7, ctrl7 );
    }
    i2c_master_bus_rm_device( imu );

    const bool ok = err == ESP_OK && ctrl7 == 0x00 && ( ctrl1 & IMU_CTRL1_SENSOR_DISABLE );
    ESP_LOGI( TAG, "IMU: WHO_AM_I=0x%02X CTRL1=0x%02X CTRL7=0x%02X -> %s", whoAmI, ctrl1, ctrl7,
              ok ? "powered down" : esp_err_to_name( err ) );
    return ok;
}

// PCF85063: CLKOUT runs at 32.768 kHz from power-on; COF = 111 stops it. Writing 1 to the AF/TF
// flag bits leaves them unchanged, so the read value can be written back with COF set.
static bool stopRtcClockOut( i2c_master_bus_handle_t bus )
{
    i2c_master_dev_handle_t rtc = nullptr;
    if ( addDevice( bus, RTC_ADDRESS, rtc ) != ESP_OK ) {
        return false;
    }
    uint8_t control2 = 0;
    esp_err_t err = readRegister( rtc, RTC_REG_CONTROL2, control2 );
    if ( err == ESP_OK ) {
        err = writeRegister( rtc, RTC_REG_CONTROL2, control2 | RTC_COF_CLKOUT_OFF );
    }
    if ( err == ESP_OK ) {
        err = readRegister( rtc, RTC_REG_CONTROL2, control2 );
    }
    i2c_master_bus_rm_device( rtc );

    const bool ok = err == ESP_OK && ( control2 & RTC_COF_CLKOUT_OFF ) == RTC_COF_CLKOUT_OFF;
    ESP_LOGI( TAG, "RTC: CONTROL2=0x%02X -> CLKOUT %s", control2, ok ? "off" : "still on" );
    return ok;
}

// After a deep-sleep wake: the devices that stay reachable must still answer.
static void checkI2cDevices( i2c_master_bus_handle_t bus )
{
    i2c_master_dev_handle_t device = nullptr;
    uint8_t whoAmI = 0;
    uint8_t control2 = 0;
    esp_err_t imuErr = addDevice( bus, IMU_ADDRESS, device );
    if ( imuErr == ESP_OK ) {
        imuErr = readRegister( device, IMU_REG_WHO_AM_I, whoAmI );
        i2c_master_bus_rm_device( device );
    }
    esp_err_t rtcErr = addDevice( bus, RTC_ADDRESS, device );
    if ( rtcErr == ESP_OK ) {
        rtcErr = readRegister( device, RTC_REG_CONTROL2, control2 );
        i2c_master_bus_rm_device( device );
    }
    const bool imuOk = imuErr == ESP_OK && whoAmI == 0x05;
    const bool rtcOk = rtcErr == ESP_OK && ( control2 & RTC_COF_CLKOUT_OFF ) == RTC_COF_CLKOUT_OFF;
    ESP_LOGI( TAG, "Already quiesced. I2C check: IMU %s (0x%02X), RTC %s (CONTROL2=0x%02X)", imuOk ? "ok" : "FAIL",
              whoAmI, rtcOk ? "ok" : "FAIL", control2 );
}

void Peripherals::quiesceOnce( i2c_master_bus_handle_t bus, Axp2101 &pmu )
{
    if ( !s_quiesceEnabled ) {
        ESP_LOGW( TAG, "Quiesce disabled (CONFIG_WATCH_QUIESCE=n)" );
        return;
    }
    if ( s_quiesced ) {
        checkI2cDevices( bus );
        return;
    }
    int passed = 0;
    passed += sleepTouch( bus ) ? 1 : 0;
    passed += standbyPanel() ? 1 : 0;
    passed += powerDownImu( bus ) ? 1 : 0;
    passed += stopRtcClockOut( bus ) ? 1 : 0;
    passed += pmu.disableUnusedRails() == ESP_OK ? 1 : 0;
    s_quiesced = true;
    ESP_LOGI( TAG, "Quiesce done: %d/5 steps OK", passed );
}

void Peripherals::holdResetLines()
{
    if ( !s_quiesceEnabled || s_resetLinesHeld ) {
        return;
    }
    const gpio_num_t lines[2] = { BSP_LCD_RST, BSP_LCD_TOUCH_RST };
    for ( int i = 0; i < 2; ++i ) {
        // Set the level before switching to output, so the line never glitches low.
        gpio_set_level( lines[i], 1 );
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << lines[i];
        config.mode = GPIO_MODE_OUTPUT;
        gpio_config( &config );
        gpio_hold_en( lines[i] );
    }
    s_resetLinesHeld = true;
}
