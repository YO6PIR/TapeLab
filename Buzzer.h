#pragma once

#include <Arduino.h>

enum class BeepPattern : uint8_t
{
    Touch,
    Success,
    Failure
};

class Buzzer
{
public:
    void begin();
    void update();
    void play(BeepPattern pattern);
    void stop();
    void setEnabled(bool enabled);
    bool isEnabled() const;
    bool isPlaying() const;
    void beepNow(uint16_t durationMs = 50);
    void beepFailureNow();
    void beepTripleNow();
    

private:
    enum class State : uint8_t { Idle, Tone1, Gap, Tone2 };
    void startTone();

    State state = State::Idle;
    bool enabled = true;
    uint32_t stateStartedMs = 0;
    uint16_t toneDurationMs = 0;
   
};

extern Buzzer buzzer;
