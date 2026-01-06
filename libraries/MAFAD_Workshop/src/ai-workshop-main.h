#ifndef WORKSHOP_MAIN_H
#define WORKSHOP_MAIN_H

#define SAMPLE_RATE 20000         // recording quality 20khz (20.000 samples per second)
#define SAMPLE_BUFFER_SIZE 60000  // size of the audio buffer, 3 seconds ( 3 * 20.000 = 60000 samples)
#define NN_WINDOW_SIZE 20000      // size of the NN / inference window (samples)

#include <Arduino.h>
#include <EEPROM.h>

static inline uint32_t restoreIndex()
{
    EEPROM.begin(32);
    uint32_t index = EEPROM.readUInt(0);
    if (index == 0xFFFFFFFF) index = 1;
    return index;
}

static inline void storeIndex(uint32_t index)
{
    EEPROM.begin(32);
    EEPROM.writeUInt(0, index);
    EEPROM.commit();
}

static void initializeRandomness()
{
    randomSeed((uint32_t)esp_random());
}

static inline int16_t clip32(int32_t value)
    {
        if (value > INT16_MAX)
            return INT16_MAX;
        if (value < INT16_MIN)
            return INT16_MIN;
        return static_cast<int16_t>(value);
    }

    static inline int32_t clip(int32_t value)
    {
        if (value > INT16_MAX)
            return INT16_MAX;
        if (value < INT16_MIN)
            return INT16_MIN;
        return value;
    }

#endif // WORKSHOP_MAIN_H