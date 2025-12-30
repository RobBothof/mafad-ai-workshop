#include <Bounce2.h>
#include "workshop/WS2812.h"
#include "workshop/workshop_library.h"
#include "workshop/pitches.h"

#define I2S_MIC_SCK_PIN 1         // clockPin
#define I2S_MIC_WS_PIN 7          // wordSelectPin
#define I2S_MIC_SD_PIN 10         // channelSelectPin

#define SDCARD_CS_PIN 6           // sdcard chip select pin

#define AUDIO_OUT_PIN 11          // amplifier / speaker pin
#define BUTTON_PIN 17             // button pin
#define LEDRING_PIN 43            // ledring data pin

#define LDR_LEFT_PIN 8            // left lightsensor pin
#define LDR_RIGHT_PIN 9           // right lightsensor pin

#define SAMPLE_RATE 20000         // recording quality 20khz (20.000 samples per second)
#define SAMPLE_BUFFER_SIZE 40000  // size of the audio buffer, 2 seconds ( 2 * 20.000 = 40000 samples)

#define NUM_LEDS 8                // the ledring uses 8 leds


// Create an array/list to store 8 colors for the ledring.
Color leds[NUM_LEDS];       
int activeLed = 0;

// Create a button object (with debouncing).
Bounce2::Button Button1 = Bounce2::Button();

// Create a led ring object.
WS2812 ledRing;

// Create a sd card object.
SDCard sdCard;

// Create a microphone object.
i2sMic microphone;

// Create a series of notes.
int notes[] = {
  NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2,
  NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3,
  NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
  NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
  NOTE_C6
};

// Create a 32bits buffer to store audio.
int32_t audioBuffer[SAMPLE_BUFFER_SIZE]; 

// Create a boolean to remember if an sd card is present.
bool hasSDCard = false;

void setup(){

  // Setup LightSensors
  pinMode(LDR_LEFT_PIN,INPUT);
  pinMode(LDR_RIGHT_PIN,INPUT);

  // Setup LedRing
  pinMode(LEDRING_PIN, OUTPUT);
  ledRing.init(LEDRING_PIN, leds, NUM_LEDS);

  // Turn all leds off
  for (int l=0; l < NUM_LEDS; l++) {
    leds[l].hex = 0x000000;
  }
  ledRing.update();

  // Setup microphone
  microphone.setup(SAMPLE_RATE, I2S_MIC_SCK_PIN, I2S_MIC_WS_PIN, I2S_MIC_SD_PIN);

  // Setup printing to serial monitor
  Serial.begin(115200);
  Serial.println("\n--------------------");

  // Set up Button
  Button1.attach(BUTTON_PIN, INPUT_PULLDOWN);
  // Set debounce time
  Button1.interval(10);
  Button1.setPressedState(HIGH); 

  // Setup audio output
  pinMode(AUDIO_OUT_PIN, OUTPUT);
  analogWriteResolution(AUDIO_OUT_PIN, 10);

  // Try to mount the SD Card and remember if it is present
  hasSDCard = sdCard.setup(SDCARD_CS_PIN);

  // Wait for the button to be pressed
  Serial.println("Waiting for Button Press to record a test sample");
  while(!Button1.pressed()) {
    Button1.update();
  }

  // start recording timer
  ulong recordStartTime = millis(); 

  // Record 2 seconds of audio
  int numSamplesRecorded = microphone.record(audioBuffer, SAMPLE_BUFFER_SIZE);

  // end recording timer
  ulong recordTime = millis() - recordStartTime;

  // log the recorded samples
  Serial.print("samples read:");
  Serial.println(numSamplesRecorded);
  Serial.print("record time:");
  Serial.println(recordTime);
  Serial.println();

  // save the recorded audio to the SDCard (if present)
  if (hasSDCard) {
    sdCard.writeWavFile("/test.wav", audioBuffer, numSamplesRecorded, SAMPLE_RATE);
  }

}

void loop(){
  
  // Iterate (loop) over the list of notes
 
  for (int note = 0; note < 49; note++) {

    // set the corresponding frequency of the note.
    analogWriteFrequency(AUDIO_OUT_PIN, notes[note]);
    
    // Sound on.
    analogWrite(AUDIO_OUT_PIN, 512);

    // Wait 1/2 second.
    delay(500);

    // Sound on.
    analogWrite(AUDIO_OUT_PIN, 0);

    // Wait 1/5 second .  
    delay(200);

    // Set all leds off.
    for (int l = 0; l < NUM_LEDS; l++) {
      leds[l].hex = 0x000000;
    }

    // Increase the activeLeds number with 1
    activeLed = activeLed + 1;

    // Start over if we have reached the last led
    if (activeLed > NUM_LEDS) {
      activeLed=0;
    }

    // Set the active LED to a random color (random Hue)
    leds[activeLed] = Color::FromHSL(random(255),255,128);

    // Write the led values to the ledRing.
    ledRing.update();

    // Read and print the values of the lightsensors
    Serial.print("LDR1:");
    Serial.print(analogRead(LDR_LEFT_PIN));
    Serial.print(" | LDR2:");
    Serial.println(analogRead(LDR_RIGHT_PIN));

  }


  delay(1000);

}