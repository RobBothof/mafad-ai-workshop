#include "workshop/WS2812.h"

#define RGBLED_PIN 40

LEDBuildin ledRGB;

void setup() {

  pinMode(RGBLED_PIN, OUTPUT);

  ledRGB.init(RGBLED_PIN);
  ledRGB.clear();
  ledRGB.update();

  // Setup printing to serial monitor
  Serial.begin(115200);
  Serial.println("\n--------------------");  
}


void loop() {

  ledRGB.setColor(Color::Red);
  ledRGB.update();

  Serial.println("RED");  

  delay(1000);

  ledRGB.setColor(Color::Green);
  ledRGB.update();

  Serial.println("GREEN");  

  delay(1000);
    
  ledRGB.setColor(Color::Blue);
  ledRGB.update();

  Serial.println("BLUE");  

  delay(1000);
}