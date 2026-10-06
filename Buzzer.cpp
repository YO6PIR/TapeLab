#include "Buzzer.h"

#include "Config.h"

Buzzer buzzer;

void Buzzer::begin()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    state = State::Idle;
}

void Buzzer::startTone()
{
    digitalWrite(BUZZER_PIN, HIGH);
}

void Buzzer::stop()
{
    digitalWrite(BUZZER_PIN, LOW);
    state = State::Idle;
    toneDurationMs = 0;
}

void Buzzer::setEnabled(bool value)
{
    enabled = value;
    if (!enabled)
        stop();
}

bool Buzzer::isEnabled() const { return enabled; }
bool Buzzer::isPlaying() const { return state != State::Idle; }

void Buzzer::play(BeepPattern pattern)
{
    if (!enabled)
        return;

    stop();
    toneDurationMs = pattern == BeepPattern::Touch ? BEEP_TOUCH_MS :
        (pattern == BeepPattern::Success ? BEEP_SUCCESS_MS : BEEP_FAILURE_MS);
    state = State::Tone1;
    stateStartedMs = millis();
    startTone();
}

void Buzzer::update()
{
    if (state == State::Idle)
        return;

    const uint32_t elapsed = millis() - stateStartedMs;
    if (state == State::Tone1 && elapsed >= toneDurationMs)
    {
        digitalWrite(BUZZER_PIN, LOW);
        if (toneDurationMs == BEEP_FAILURE_MS)
        {
            state = State::Gap;
            stateStartedMs = millis();
        }
        else
            stop();
    }
    else if (state == State::Gap && elapsed >= BEEP_GAP_MS)
    {
        state = State::Tone2;
        stateStartedMs = millis();
        startTone();
    }
    else if (state == State::Tone2 && elapsed >= BEEP_FAILURE_MS)
    {
        stop();
    }
}

void Buzzer::beepNow(uint16_t durationMs)
{
    if (!enabled)
        return;
   

    // Oprește orice secvență anterioară controlată prin update()
    stop();

    // Pornește tonul
    startTone();

    const uint32_t startedMs = millis();

    // Așteptare scurtă, autonomă
    while ((uint32_t)(millis() - startedMs) < durationMs)
    {
        yield();
    }

    // Oprește sigur buzzerul
    stop();
    
}

void Buzzer::beepFailureNow()
{
    if (!enabled)
        return;

    beepNow(BEEP_FAILURE_MS);

    const uint32_t gapStartedMs = millis();

    while ((uint32_t)(millis() - gapStartedMs) < BEEP_GAP_MS)
    {
        yield();
    }

    beepNow(BEEP_FAILURE_MS);
}


void Buzzer::beepTripleNow()
{
    if (!enabled)
        return;

    stop();

    for (uint8_t i = 0; i < 3; ++i)
    {
        startTone();

        const uint32_t toneStartedMs = millis();

        while ((uint32_t)(millis() - toneStartedMs) < BEEP_FAILURE_MS)
        {
            yield();
        }

        stop();

        if (i < 2)
        {
            const uint32_t gapStartedMs = millis();

            while ((uint32_t)(millis() - gapStartedMs) < BEEP_GAP_MS)
            {
                yield();
            }
        }
    }

    stop();
}
