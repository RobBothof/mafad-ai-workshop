/*
 * WS2812_ARRAY - WS2812B Library for ESP32S3 (CORE >= v3) using RMT
 * 
 * Supports: WS2812B, WS2812, WS2811 compatible LEDs
 *
 * Rob Bothof [rbothof@xs4all.nl]
 */

#ifndef WS2812_H
#define WS2812_H

#include <Arduino.h>

class Color {
public:
  union {
    struct {
      uint8_t b;
      uint8_t r;
      uint8_t g;
      uint8_t x; // 8 bit spacer so we can map hex values like 0xFF00FF to r,g,b
    };
    uint32_t hex;
  };
  
  // Constructors
  Color() : r(0), g(0), b(0) {}
  Color(uint8_t red, uint8_t green, uint8_t blue) : x(0), r(red), g(green), b(blue) {}
  Color(uint32_t rawvalue) : hex(rawvalue) {}
  
  // HSL to RGB conversion (integer math only)
  // h: 0-255 (maps to 0-360 degrees), s: 0-255, l: 0-255
  static Color FromHSL(uint8_t h, uint8_t s, uint8_t l) {
    if (s == 0) {
      // Achromatic (gray)
      return Color(l, l, l);
    }
    
    uint8_t region = h / 43;  // 0-5
    uint8_t remainder = (h - (region * 43)) * 6;
    
    uint8_t p = (l * (255 - s)) >> 8;
    uint8_t q = (l * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (l * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
      case 0:  return Color(l, t, p);
      case 1:  return Color(q, l, p);
      case 2:  return Color(p, l, t);
      case 3:  return Color(p, q, l);
      case 4:  return Color(t, p, l);
      default: return Color(l, p, q);
    }
  }

  // Named colors
  static const Color Black;
  static const Color Red;
  static const Color Green;
  static const Color Blue;
  static const Color Yellow;
  static const Color Cyan;
  static const Color Magenta;
  static const Color White;
  static const Color Orange;
  static const Color Purple;
  static const Color Pink;
  
  // Copy Operator
  Color& operator=(const Color& other) {
    x = 0;
    r = other.r;
    g = other.g;
    b = other.b;
    return *this;
  }
};

// Define named colors
inline const Color Color::Black = Color(0, 0, 0);
inline const Color Color::Red = Color(255, 0, 0);
inline const Color Color::Green = Color(0, 255, 0);
inline const Color Color::Blue = Color(0, 0, 255);
inline const Color Color::Yellow = Color(255, 255, 0);
inline const Color Color::Cyan = Color(0, 255, 255);
inline const Color Color::Magenta = Color(255, 0, 255);
inline const Color Color::White = Color(255, 255, 255);
inline const Color Color::Orange = Color(255, 165, 0);
inline const Color Color::Purple = Color(128, 0, 128);
inline const Color Color::Pink = Color(255, 192, 203);

class LEDBuildin {
private:
  bool initialized;
  uint8_t pin;

  public:
  LEDBuildin() : pin(0), initialized(false) {}

  Color led;  

  void init(uint8_t datapin) {
    pin = datapin;
    #if SOC_RMT_SUPPORTED

    if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
      Serial.println("RMT initialization failed!");
      return;
    }
    initialized=true;
    #endif
  }
  
  void update() {
    if (!initialized) return;
    
    #if SOC_RMT_SUPPORTED
  
    rmt_data_t led_data[24];
    int led_data_index = 0;
    uint8_t led_data_value=0;

      for (int c = 0; c < 3; c++) {
        for (int bit = 0; bit < 8; bit++) {
          if (c==1) led_data_value = led.r;
          if (c==0) led_data_value = led.g;
          if (c==2) led_data_value = led.b;

          if (led_data_value & (1 << (7 - bit))) {
            led_data[led_data_index].level0 = 1; // HIGH bit
            led_data[led_data_index].duration0 = 8; // 0.8us
            led_data[led_data_index].level1 = 0; // LOW bit
            led_data[led_data_index].duration1 = 4; // 0.4us
          } else {
            led_data[led_data_index].level0 = 1; // LOW bit
            led_data[led_data_index].duration0 = 4; // 0.4us
            led_data[led_data_index].level1 = 0; // HIGH bit
            led_data[led_data_index].duration1 = 8; // 0.8us
          }
          led_data_index++;
        }
      }
    

    // Write the data to the LEDRING
    rmtWrite(pin, led_data, RMT_SYMBOLS_OF(led_data), RMT_WAIT_FOR_EVER);

    #else
    Serial.println("RMT is not supported on this target.");
    #endif
  }
  
  void clear() {
    led.hex=0x000000;
  }

  void setColor(Color color) {
    led = color;
  }

};

class WS2812 {
private:
  uint16_t numLeds;
  bool initialized;
  uint8_t pin;
  
public:
  WS2812() : pin(0), leds(nullptr), numLeds(0), initialized(false) {}

  Color* leds;

  void init(uint8_t datapin, Color* ledArray, uint16_t count) {
    pin = datapin;
    leds = ledArray;
    numLeds = count;
    #if SOC_RMT_SUPPORTED

    if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
      Serial.println("RMT initialization failed!");
      return;
    }
    initialized=true;
    #endif
  }
  
  void update() {
    if (!initialized || leds == nullptr) return;
    
    #if SOC_RMT_SUPPORTED
  
    rmt_data_t led_data[24*numLeds];
    int led_data_index = 0;
    uint8_t led_data_value=0;

    for (uint16_t l = 0; l < numLeds; l++) {
      for (int c = 0; c < 3; c++) {
        for (int bit = 0; bit < 8; bit++) {
          if (c==0) led_data_value = leds[l].r;
          if (c==1) led_data_value = leds[l].g;
          if (c==2) led_data_value = leds[l].b;

          if (led_data_value & (1 << (7 - bit))) {
            led_data[led_data_index].level0 = 1; // HIGH bit
            led_data[led_data_index].duration0 = 8; // 0.8us
            led_data[led_data_index].level1 = 0; // LOW bit
            led_data[led_data_index].duration1 = 4; // 0.4us
          } else {
            led_data[led_data_index].level0 = 1; // LOW bit
            led_data[led_data_index].duration0 = 4; // 0.4us
            led_data[led_data_index].level1 = 0; // HIGH bit
            led_data[led_data_index].duration1 = 8; // 0.8us
          }
          led_data_index++;
        }
      }
    }

    // Write the data to the LEDRING
    rmtWrite(pin, led_data, RMT_SYMBOLS_OF(led_data), RMT_WAIT_FOR_EVER);

    #else
    Serial.println("RMT is not supported on this target.");
    #endif
  }
  
  void clear() {
    if (leds == nullptr) return;
    for (uint16_t i = 0; i < numLeds; i++) {
      leds[i].hex=0x000000;
    }
  }
  
  Color& operator[](uint16_t index) {
    return leds[index];
  }
};


#endif // WS2812
