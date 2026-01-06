#ifndef WORKSHOP_SOUND_H
#define WORKSHOP_SOUND_H

#include "ai-workshop-main.h"

#include <Arduino.h>

// One shared variable (defined in exactly one .cpp/.ino)
extern uint32_t randomness;

static void Silence(uint32_t ms)
{
    if (ms < randomness)
    {
        delay(ms);
    }
    else
    {
        delay(ms + random(randomness * 2) - randomness);
    }
}


static void startTone(uint8_t pin, unsigned int frequency)
{
    const uint8_t bits = 12;
    static int8_t attachedPin = -1;

    uint32_t f = frequency;
    if (frequency >= randomness * 2)
    {
        f = frequency + random(randomness * 2) - randomness;
    }

    const uint32_t maxDuty = (1u << bits) - 1u;
    uint32_t duty = 2000u + (uint32_t)random(96);
    if (duty > maxDuty)
        duty = maxDuty;

    uint8_t ch_pin = 7;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// Arduino-ESP32 core v3.x (pin-based LEDC)
    ch_pin = pin;
    if (attachedPin != ch_pin)
    {
        if (attachedPin >= 0)
            ledcDetach((uint8_t)attachedPin);
        ledcAttach(pin, 1000 /*dummy*/, bits);
        attachedPin = ch_pin;
    }
#else
    // Arduino-ESP32 core v2.x (channel-based LEDC)
    static bool initialized = false;

    if (!initialized)
    {
        ledcSetup(ch_pin, 1000 /*dummy*/, bits);
        initialized = true;
    }

    if (attachedPin != pin)
    {
        if (attachedPin >= 0)
            ledcDetachPin((uint8_t)attachedPin);
        ledcAttachPin(pin, ch_pin);
        attachedPin = pin;
    }

#endif

    ledcChangeFrequency(ch_pin, f, bits);
    ledcWrite(ch_pin, duty);
}

static void stopTone(uint8_t pin)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(pin, 0);
#else
    const uint8_t ch = 7;
    ledcWrite(ch, 0);
#endif
}

static void playTone(uint8_t pin, unsigned int frequency, unsigned long duration)
{
    startTone(pin, frequency);
    Silence((uint32_t)duration);
    stopTone(pin);
}

#endif // WORKSHOP_SOUND_H


