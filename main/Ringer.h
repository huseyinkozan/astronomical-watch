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
    // and the metronome stop for it.
    bool start();

    // The torch's SOS beeps (ToolPages) at the alarm's lowest level, on the clock of the light
    // (Morse::sosLit from `startUs`, esp_timer time) until stopTool() or stop(). Nothing happens
    // while an alarm rings.
    bool startSos( int64_t startUs );

    // The metronome's beats (ToolParts) at the alarm's lowest level: one every `periodUs` from
    // `startUs` (esp_timer time), 1000 Hz, the first of every `beatsInBar` at 1500 Hz (below 2:
    // no accent). While the beats play, a call only sets the new clock (no new codec start).
    // Nothing happens while an alarm rings.
    bool startBeats( int64_t startUs, int64_t periodUs, int beatsInBar );

    // Stops the SOS beeps or the metronome; does not wait unless `wait`: until the codec is
    // powered down.
    void stopTool( bool wait );

    // A short signal (the regatta start's, Regatta::Signal): `beeps` tones of `toneMs` at 1000 Hz
    // and the lowest level, 150 ms apart, then the codec is powered down. False, and nothing
    // played, while another sound plays. Does not wait.
    bool signal( int beeps, int toneMs );

    // Stops the sound and waits until the codec is in its power-down state.
    void stop();

    // True while the sound plays.
    bool ringing() const;

    // True when the last ringing ended by itself after 60 s (cleared by start()).
    bool finished() const;
};
