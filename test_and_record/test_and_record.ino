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

  // Make some sounds

  // set the frequency of the tone in Hz (C4)
  analogWriteFrequency(AUDIO_OUT_PIN, 277);
  // Sound on.
  analogWrite(AUDIO_OUT_PIN, 512);
  // Wait 1/4 second (250 miliseconds).
  delay(250);
  // Sound off.
  analogWrite(AUDIO_OUT_PIN, 0);

  // set the frequency of the tone in Hz (E4)
  analogWriteFrequency(AUDIO_OUT_PIN, 330);
  // Sound on.
  analogWrite(AUDIO_OUT_PIN, 512);
  // Wait 1/4 second (250 miliseconds).
  delay(250);
  // Sound off.
  analogWrite(AUDIO_OUT_PIN, 0);

  Serial.println();
  Serial.println("Press button to record a test sample.");
  Serial.println();

  // prime the timer (set it to current time)
  log_timer = millis();
}

void loop(){
  Button1.update();

  if(Button1.pressed()) {  // Returns true if the button was pressed
      if (fileIndex%8 == 0) {
        ledRing.clear();
        ledRing.update();
      }

      // wait 1/5 of a second before starting with recording
      delay(200); 
      Serial.println("Now recording 2 seconds..");

      ledRing[fileIndex%8] = Color::Red;
      ledRing.update();

      // Start recording 2 seconds in the background
      microphone.startRecord(audioBuffer, 2000);

      // wait until recording is done
      while (!microphone.isRecordDone()) {
        delay(10);
      }

      // Get number of samples recorded
      int numSamplesRecorded = microphone.getNumSamplesRecorded();
  
      Serial.print(numSamplesRecorded);
      Serial.println(" samples recorded.");
      Serial.println();

      // save the recorded audio to the SDCard (if present)
      if (hasSDCard) {
        // Create a filename with the format: /recording_0001.wav 
        String filename = sdCard.makeFilename("/recording", fileIndex);

        Serial.print("Saving on SDCard as:");
        Serial.println(filename);
        Serial.println();

        bool writtenSuccesfully = sdCard.writeWavFile(filename, audioBuffer, numSamplesRecorded, SAMPLE_RATE);

        if (writtenSuccesfully) {
          ledRing[fileIndex%8] = Color::Green;
          ledRing.update();

        } else {
          // If the SDCard is not present it will not set the Led to green but to black
          ledRing[fileIndex%8] = Color::Black;
          ledRing.update();

        }

        // Increase the file index with 1
        fileIndex++;

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