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

// Create a 32bits buffer to store audio.
int32_t audioBuffer[SAMPLE_BUFFER_SIZE]; 

// Create a boolean to remember if an sd card is present.
bool hasSDCard = false;

// Create a value to keep track of elapsed time.
unsigned long log_timer=0;

// Create a value to keep track of the recordings we store
unsigned int fileIndex=0;

// Define a series of notes. (Cminor7 chord)
int notes[] = { 
  NOTE_C2, NOTE_DS2, NOTE_G2, NOTE_AS2, 
  NOTE_C3, NOTE_DS3, NOTE_G3, NOTE_AS3, 
  NOTE_C4, NOTE_DS4, NOTE_G4, NOTE_AS4, 
  NOTE_C5, NOTE_DS5, NOTE_G5, NOTE_AS5, 
  NOTE_C6
};


String noteNames[] = {
  "C2", "D#2", "G2", "A#2",
  "C3", "D#3", "G3", "A#3",
  "C4", "D#4", "G4", "A#4",
  "C5", "D#5", "G5", "A#5",
  "C6"
};



void setup(){

  // Setup LightSensors
  pinMode(LDR_LEFT_PIN,INPUT);
  pinMode(LDR_RIGHT_PIN,INPUT);

  // Setup LedRing
  pinMode(LEDRING_PIN, OUTPUT);
  ledRing.init(LEDRING_PIN, leds, NUM_LEDS);

  // Turn all leds off
  ledRing.clear();
  ledRing.update();

  // Setup microphone
  microphone.setup(SAMPLE_RATE, SAMPLE_BUFFER_SIZE, I2S_MIC_SCK_PIN, I2S_MIC_WS_PIN, I2S_MIC_SD_PIN);

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

  Serial.println();
  Serial.println("Press button to record a set of long and short tones.");
  Serial.println();

  // prime the timer (set it to current time)
  log_timer = millis();
}

void loop(){
  Button1.update();

  if(Button1.pressed()) {  // Returns true if the button was pressed
      if (fileIndex % 8 == 0) {
        ledRing.clear();
        ledRing.update();
      }

      // wait 1 second before starting with recording
      delay(1000); 
      ledRing[fileIndex%8].hex=0xFF0000;
      ledRing.update();

      // record short tones

      for (int note=0; note < sizeof(notes)/sizeof(notes[0]); note++) {
        // Set note frequency.
        analogWriteFrequency(AUDIO_OUT_PIN, notes[note]);

        // Start recording (500ms) in the background
        microphone.startRecord(audioBuffer, 500);

        // Turn audio on.
        analogWrite(AUDIO_OUT_PIN, 512);

        // Wait 100 miliseconds.
        delay(100);

        // Sound off.
        analogWrite(AUDIO_OUT_PIN, 0);

        // wait until recording is done
        while (!microphone.isRecordDone()) {
          delay(10);
        }

        // Get number of samples recorded
        int numSamplesRecorded = microphone.getNumSamplesRecorded();

        // Print number of samples that are recorded.
        Serial.print(numSamplesRecorded);
        Serial.println(" samples recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard) {

          // Create a filename with the format: /c4_short_0001.wav 
          String filename = sdCard.makeFilename(noteNames[note] + "_short", fileIndex);

          Serial.print("Saving on SDCard as:");
          Serial.println(filename);

          sdCard.writeWavFile(filename, audioBuffer, numSamplesRecorded, SAMPLE_RATE);
        }      
        Serial.println();

      }

      // record long tones
      for (int note=0; note < sizeof(notes)/sizeof(notes[0]); note++) {

        // Set note frequency.
        analogWriteFrequency(AUDIO_OUT_PIN, notes[note]);

        // Start recording 1 second in the background
        microphone.startRecord(audioBuffer, 1000);

        // Turn audio on.
        analogWrite(AUDIO_OUT_PIN, 512);

        // Wait 600 miliseconds.
        delay(500);

        // Sound off.
        analogWrite(AUDIO_OUT_PIN, 0);

        // wait until recording is done
        while (!microphone.isRecordDone()) {
          delay(10);
        }

        // Get number of samples recorded
        int numSamplesRecorded = microphone.getNumSamplesRecorded();

        // Print number of samples that are recorded.
        Serial.print(numSamplesRecorded);
        Serial.println(" samples recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard) {

          // Create a filename with the format: /c4_short_0001.wav 
          String filename = sdCard.makeFilename(noteNames[note] + "_long", fileIndex);

          Serial.print("Saving on SDCard as:");
          Serial.println(filename);

          sdCard.writeWavFile(filename, audioBuffer, numSamplesRecorded, SAMPLE_RATE);
        }     
        Serial.println();
      }

      ledRing[fileIndex%8]=Color::Green;
      ledRing.update();

      fileIndex++;

  } 
}