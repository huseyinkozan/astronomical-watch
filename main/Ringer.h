#pragma once

#include <cstdint>

// The alarm's sound (docs/superpowers/specs/2026-11-19-alarms-design.md): beeps alternating
// 1000 and 1500 Hz (the speaker's loudest regions), 100 ms with 150 ms of silence, rising from
// -18 dB by 3 dB every 2 s to -12 dB, for at most 60 s. A task of its own plays them while the
// main task keeps the display and the keys.
class Ringer
{
public:
    // Starts the sound; false when the task or the codec could not start. The torch's SOS beeps
    // stop for it.
    bool start();

    // The torch's SOS beeps (ToolPages) at the alarm's lowest level, on the clock of the light
    // (Morse::sosLit from `startUs`, esp_timer time) until stopSos() or stop(). Nothing happens
    // while an alarm rings. stopSos() does not wait; `wait`: until the codec is powered down.
    bool startSos( int64_t startUs );
    void stopSos( bool wait );

    // Stops the sound and waits until the codec is in its power-down state.
    void stop();

    // True while the sound plays.
    bool ringing() const;

    // True when the last ringing ended by itself after 60 s (cleared by start()).
    bool finished() const;
};
