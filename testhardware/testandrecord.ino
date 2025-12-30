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

// Define a series of notes. (Cminor chord)
int melody[] = { NOTE_C3, NOTE_D3, NOTE_DS3, NOTE_F3, NOTE_G3, NOTE_GS3, NOTE_AS3, NOTE_C4};

// Create a 32bits buffer to store audio.
int32_t audioBuffer[SAMPLE_BUFFER_SIZE]; 

// Create a boolean to remember if an sd card is present.
bool hasSDCard = false;

// Create a value to keep track of elapsed time.
unsigned long log_timer=0;

// Create a value to keep track of the recordings we store
unsigned int fileIndex=0;

// Create a series of notes.
int notes[] = {
  NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2,
  NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3,
  NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
  NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
  NOTE_C6
};




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

  delay(1000);

  // Play 8 notes and light up 8 LEDs
  for (int index = 0; index < 8; index++) {

    // Set the LED corressponding with to a random color (random Hue)
    leds[index] = Color::FromHSL(random(255),255,128);
    // Write the led values to the ledRing.
    ledRing.update();

    // set the corresponding frequency of the note.
    analogWriteFrequency(AUDIO_OUT_PIN, melody[index]);

    // Sound on.
    analogWrite(AUDIO_OUT_PIN, 512);

    // Wait 1/2 second.
    delay(500);

    // Sound on.
    analogWrite(AUDIO_OUT_PIN, 0);

    // Set all leds to off
    for (int l = 0; l < NUM_LEDS; l++) {
      leds[l].hex = 0x000000;
    }
    // Write the led values to the ledRing.
    ledRing.update();

    // Wait 1/5 seconds.
    delay(200);

  }

  Serial.println();
  Serial.println("Press button to record a test sample");
  Serial.println();

  // prime the timer (set it to current time)
  log_timer = millis();
}

void loop(){
  Button1.update();

  if(Button1.pressed()) {  // Returns true if the button was pressed

      // wait 1/5 of a second before starting with recording
      delay(200); 
      Serial.println("Now recording 2 seconds..");

      int numSamplesRecorded = microphone.record(audioBuffer, SAMPLE_BUFFER_SIZE);
  
      Serial.print("samples recorded:");
      Serial.println(numSamplesRecorded);
      Serial.println();

      // save the recorded audio to the SDCard (if present)
      if (hasSDCard) {
        // Create a filename with the format: /recording_0001.wav 
        String filename = sdCard.makeFilename("/recording", fileIndex);

        // Increase the file index with 1
        fileIndex++;

        Serial.print("Saving on SDCard as:");
        Serial.println(filename);
        Serial.println();

        sdCard.writeWavFile(filename, audioBuffer, numSamplesRecorded, SAMPLE_RATE);
      }      
  } 

  // Print the values of the lightsensors every second.
  // this prevents the values being logged continuously, stalling the device
  if (millis() > log_timer + 1000) {
    // set our timer to the current time.
    log_timer = millis();
    Serial.print("Light Sensor L:");
    Serial.print(analogRead(LDR_LEFT_PIN));
    Serial.print(" | Light Sensor R:");
    Serial.println(analogRead(LDR_RIGHT_PIN));
  }


}