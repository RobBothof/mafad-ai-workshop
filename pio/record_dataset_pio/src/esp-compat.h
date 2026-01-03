#pragma once

// This file is force-included for *all* compilation units (C and C++).
// Only enable the overloads in C++ to avoid breaking .c files.
#ifdef __cplusplus

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

// Provide pin-argument overloads expected by AIWorkshop / sketch.
// ESP32 core provides global versions: analogWriteResolution(bits), analogWriteFrequency(freq).
inline void analogWriteResolution(uint8_t /*pin*/, uint8_t bits) { ::analogWriteResolution(bits); }
inline void analogWriteFrequency(uint8_t /*pin*/, uint32_t freq) { ::analogWriteFrequency(freq); }

#endif // ARDUINO_ARCH_ESP32
#endif // __cplusplus