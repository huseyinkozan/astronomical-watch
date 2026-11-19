#pragma once

#include <cstdint>

#include "esp_err.h"

// The speaker: ESP32 I2S (transmit only) -> ES8311 DAC -> NS4150B amplifier (GPIO46).
// docs/hardware.md, "Speaker and alarm sound". Tones are written by the calling task, which
// waits while the I2S DMA plays them. Nothing of it runs outside start() ... stop().
class Sound
{
public:
    static const int SampleRate = 16000;

    // I2S and the codec on, the amplifier off, the DAC volume at 0 dB.
    esp_err_t start();

    // The amplifier needs 120 ms after switching on before it plays (NS4150B datasheet).
    void setAmplifier( bool on );

    // ES8311 register 0x32: 0xBF is 0 dB, 0.5 dB a step.
    void setVolume( int value );

    // A tone of `milliseconds` (5 ms ramps at both ends against clicks); `square`: a square wave
    // of the same peak instead of a sine.
    esp_err_t tone( int frequency, int milliseconds, bool square );

    esp_err_t silence( int milliseconds );

    // Test: the ADC side of the codec off and the DAC in its low-power modes.
    void lowPowerCodec();

    // Muted, the amplifier off, the codec in its power-down state, I2S released.
    void stop();

    bool started() const;

private:
    esp_err_t write( const int16_t *samples, int count );
};
