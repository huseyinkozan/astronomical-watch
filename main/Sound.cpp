#include "Sound.h"

#include <cmath>
#include <cstdio>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "es8311_codec.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

static const char *TAG = "sound";

static i2s_chan_handle_t s_tx = nullptr;
static const audio_codec_data_if_t *s_dataIf = nullptr;
static const audio_codec_ctrl_if_t *s_ctrlIf = nullptr;
static const audio_codec_gpio_if_t *s_gpioIf = nullptr;
static const audio_codec_if_t *s_codecIf = nullptr;
static esp_codec_dev_handle_t s_device = nullptr;
// esp_codec_dev_close() disables the channel itself; a second disable logs an error.
static bool s_channelEnabled = false;

// One block of samples: 16 ms at 16 kHz, written while the DMA plays the previous one.
static const int s_blockSamples = 256;
static int16_t s_block[s_blockSamples];

esp_err_t Sound::start()
{
    if ( s_device != nullptr ) {
        return ESP_OK;
    }
    // GPIO46 has a 10k pull-down on the board: low keeps the amplifier in shutdown.
    gpio_config_t amplifier = {};
    amplifier.pin_bit_mask = 1ULL << BSP_POWER_AMP_IO;
    amplifier.mode = GPIO_MODE_OUTPUT;
    gpio_config( &amplifier );
    gpio_set_level( BSP_POWER_AMP_IO, 0 );

    // Transmit only: the microphones are not used, so no receive channel and DMA run.
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG( I2S_NUM_0, I2S_ROLE_MASTER );
    channel.auto_clear = true;
    esp_err_t err = i2s_new_channel( &channel, &s_tx, nullptr );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "I2S channel: %s", esp_err_to_name( err ) );
        return err;
    }
    i2s_std_config_t std = {};
    std.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG( SampleRate );
    std.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG( I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO );
    std.gpio_cfg.mclk = BSP_I2S_MCLK;
    std.gpio_cfg.bclk = BSP_I2S_SCLK;
    std.gpio_cfg.ws = BSP_I2S_LCLK;
    std.gpio_cfg.dout = BSP_I2S_DOUT;
    std.gpio_cfg.din = I2S_GPIO_UNUSED;
    err = i2s_channel_init_std_mode( s_tx, &std );
    if ( err == ESP_OK ) {
        err = i2s_channel_enable( s_tx );
        s_channelEnabled = err == ESP_OK;
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "I2S start: %s", esp_err_to_name( err ) );
        stop();
        return err;
    }

    audio_codec_i2s_cfg_t i2s = {};
    i2s.port = I2S_NUM_0;
    i2s.tx_handle = s_tx;
    s_dataIf = audio_codec_new_i2s_data( &i2s );
    audio_codec_i2c_cfg_t i2c = {};
    i2c.port = BSP_I2C_NUM;
    i2c.addr = ES8311_CODEC_DEFAULT_ADDR;
    i2c.bus_handle = bsp_i2c_get_handle();
    s_ctrlIf = audio_codec_new_i2c_ctrl( &i2c );
    s_gpioIf = audio_codec_new_gpio();
    if ( s_dataIf == nullptr || s_ctrlIf == nullptr || s_gpioIf == nullptr ) {
        ESP_LOGE( TAG, "codec interfaces" );
        stop();
        return ESP_FAIL;
    }
    es8311_codec_cfg_t es8311 = {};
    es8311.ctrl_if = s_ctrlIf;
    es8311.gpio_if = s_gpioIf;
    es8311.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    // The amplifier is switched here (setAmplifier), not by the driver: its 120 ms start-up.
    es8311.pa_pin = -1;
    es8311.use_mclk = true;
    // The amplifier runs from 3.3 V on this board (the BSP assumes 5 V).
    es8311.hw_gain.pa_voltage = 3.3;
    es8311.hw_gain.codec_dac_voltage = 3.3;
    s_codecIf = es8311_codec_new( &es8311 );
    if ( s_codecIf == nullptr ) {
        ESP_LOGE( TAG, "ES8311 not found" );
        stop();
        return ESP_FAIL;
    }
    esp_codec_dev_cfg_t device = {};
    device.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    device.codec_if = s_codecIf;
    device.data_if = s_dataIf;
    s_device = esp_codec_dev_new( &device );
    esp_codec_dev_sample_info_t format = {};
    format.bits_per_sample = 16;
    format.channel = 1;
    format.sample_rate = SampleRate;
    if ( s_device == nullptr || esp_codec_dev_open( s_device, &format ) != ESP_CODEC_DEV_OK ) {
        ESP_LOGE( TAG, "codec open" );
        stop();
        return ESP_FAIL;
    }
    // Quiet until the caller chooses: -18 dB (full scale rang in the owner's ears).
    setVolume( 0x9B );
    // The driver powers the ADC side too, unused here (no microphone): PGA and modulator off
    // (0x0E, its reset value), the ADC bias and reference off (0x0D bits 5 and 4).
    esp_codec_dev_write_reg( s_device, 0x0E, 0x6A );
    esp_codec_dev_write_reg( s_device, 0x0D, 0x31 );
    // The driver's first write to the ES8311 fails now and then (it writes register 0x44 twice
    // for this): the registers that matter, read back.
    const int registers[] = { 0x01, 0x0D, 0x0E, 0x12, 0x14, 0x32, 0x44 };
    char text[96];
    int used = 0;
    for ( int i = 0; i < static_cast<int>( sizeof( registers ) / sizeof( registers[0] ) ); ++i ) {
        int value = -1;
        esp_codec_dev_read_reg( s_device, registers[i], &value );
        used += snprintf( text + used, sizeof( text ) - used, " %02X=%02X", registers[i], value & 0xFF );
    }
    ESP_LOGI( TAG, "ES8311 after start:%s", text );
    return ESP_OK;
}

void Sound::setAmplifier( bool on )
{
    gpio_set_level( BSP_POWER_AMP_IO, on ? 1 : 0 );
}

void Sound::setVolume( int value )
{
    if ( s_device != nullptr ) {
        esp_codec_dev_write_reg( s_device, 0x32, value );
        int read = -1;
        esp_codec_dev_read_reg( s_device, 0x32, &read );
        ESP_LOGD( TAG, "Volume 0x%02X, read back 0x%02X", value, read & 0xFF );
    }
}

esp_err_t Sound::tone( int frequency, int milliseconds, bool square )
{
    const int total = SampleRate * milliseconds / 1000;
    const int ramp = SampleRate * 5 / 1000;
    const float step = 2.0f * static_cast<float>( M_PI ) * frequency / SampleRate;
    int done = 0;
    while ( done < total ) {
        const int count = total - done < s_blockSamples ? total - done : s_blockSamples;
        for ( int i = 0; i < count; ++i ) {
            const int n = done + i;
            float value = sinf( step * n );
            if ( square ) {
                value = value >= 0 ? 1.0f : -1.0f;
            }
            float gain = 1.0f;
            if ( n < ramp ) {
                gain = static_cast<float>( n ) / ramp;
            } else if ( total - n < ramp ) {
                gain = static_cast<float>( total - n ) / ramp;
            }
            s_block[i] = static_cast<int16_t>( 32000.0f * gain * value );
        }
        const esp_err_t err = write( s_block, count );
        if ( err != ESP_OK ) {
            return err;
        }
        done += count;
    }
    return ESP_OK;
}

esp_err_t Sound::silence( int milliseconds )
{
    const int total = SampleRate * milliseconds / 1000;
    for ( int i = 0; i < s_blockSamples; ++i ) {
        s_block[i] = 0;
    }
    int done = 0;
    while ( done < total ) {
        const int count = total - done < s_blockSamples ? total - done : s_blockSamples;
        const esp_err_t err = write( s_block, count );
        if ( err != ESP_OK ) {
            return err;
        }
        done += count;
    }
    return ESP_OK;
}

void Sound::lowPowerCodec()
{
    if ( s_device == nullptr ) {
        return;
    }
    // ES8311 datasheet rev 10.0, pp. 18-19: 0x0E PGA and ADC modulator off (its reset value);
    // 0x0D the ADC bias and reference off, the DAC side and VMID as the driver left them
    // (0x01); 0x0F the DAC and its reference in low-power mode.
    esp_codec_dev_write_reg( s_device, 0x0E, 0x6A );
    esp_codec_dev_write_reg( s_device, 0x0D, 0x31 );
    esp_codec_dev_write_reg( s_device, 0x0F, 0x84 );
}

void Sound::stop()
{
    if ( s_device != nullptr ) {
        esp_codec_dev_set_out_mute( s_device, true );
    }
    setAmplifier( false );
    if ( s_device != nullptr ) {
        // The driver's suspend sequence ends with 0x0D 0xFC, the reset value, while MCLK runs.
        esp_codec_dev_close( s_device );
        esp_codec_dev_delete( s_device );
        s_device = nullptr;
        s_channelEnabled = false;
    }
    if ( s_codecIf != nullptr ) {
        audio_codec_delete_codec_if( s_codecIf );
        s_codecIf = nullptr;
    }
    if ( s_gpioIf != nullptr ) {
        audio_codec_delete_gpio_if( s_gpioIf );
        s_gpioIf = nullptr;
    }
    if ( s_ctrlIf != nullptr ) {
        audio_codec_delete_ctrl_if( s_ctrlIf );
        s_ctrlIf = nullptr;
    }
    if ( s_dataIf != nullptr ) {
        audio_codec_delete_data_if( s_dataIf );
        s_dataIf = nullptr;
    }
    if ( s_tx != nullptr ) {
        if ( s_channelEnabled ) {
            i2s_channel_disable( s_tx );
            s_channelEnabled = false;
        }
        i2s_del_channel( s_tx );
        s_tx = nullptr;
    }
}

bool Sound::started() const
{
    return s_device != nullptr;
}

esp_err_t Sound::write( const int16_t *samples, int count )
{
    if ( s_device == nullptr ) {
        return ESP_ERR_INVALID_STATE;
    }
    const int result = esp_codec_dev_write( s_device, const_cast<int16_t *>( samples ), count * 2 );
    return result == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}
