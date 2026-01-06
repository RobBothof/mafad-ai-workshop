#include <Bounce2.h>

#include <ai-workshop-main.h>
#include <ai-workshop-ws2812.h>
#include <ai-workshop-sound.h>
#include <ai-workshop-pitches.h>
#include <ai-workshop-sdcard.h>
#include <ai-workshop-mic.h>

#define MY_DEVICE "MAFAD"

#define I2S_MIC_SCK_PIN 1 // clockPin
#define I2S_MIC_WS_PIN 7  // wordSelectPin
#define I2S_MIC_SD_PIN 10 // channelSelectPin

#define SDCARD_CS_PIN 6 // sdcard chip select pin

#define AUDIO_OUT_PIN 11 // amplifier / speaker pin
#define BUTTON_PIN 17    // button pin
#define LEDRING_PIN 43   // ledring data pin

#define LDR_LEFT_PIN 8  // left lightsensor pin
#define LDR_RIGHT_PIN 9 // right lightsensor pin

#define NUM_LEDS 8 // the ledring uses 8 leds

#define CREATE_CROPS false // create cropped audio files around the sound event

uint32_t randomness = 0;

// Create an array/list to store 8 colors for the ledring.
Color leds[NUM_LEDS];

// Create a button object (with debouncing).
Bounce2::Button Button1 = Bounce2::Button();

// Create a led ring object.
WS2812 ledRing;

// Create a sd card object.
SDCard sdCard;

// Create a boolean to remember if an sd card is present.
bool hasSDCard = false;

// Create a microphone object.
i2sMic microphone;

// Create somes variables to keep track of elapsed time.
uint32_t log_timer = 0;
uint32_t sound_timer = 0;
uint32_t next_sound_time = 100;

// Create a value to keep track of the recordings we store
uint32_t recordIndex = 0;

void setup()
{
    // Setup printing to serial monitor
    Serial.begin(115200);
    Serial.println();

    // Setup randomness 
    initializeRandomness();

    // Setup LightSensors
    pinMode(LDR_LEFT_PIN, INPUT);
    pinMode(LDR_RIGHT_PIN, INPUT);

    // Setup microphone
    microphone.setup(I2S_MIC_SCK_PIN, I2S_MIC_WS_PIN, I2S_MIC_SD_PIN);

    // Set up Button
    Button1.attach(BUTTON_PIN, INPUT_PULLDOWN);
    // Set debounce time
    Button1.interval(10);
    Button1.setPressedState(HIGH);

    // Try to mount the SD Card and remember if it is present
    hasSDCard = sdCard.setup(SDCARD_CS_PIN);

    // load the last used recording index from the device EEPROM
    recordIndex = restoreIndex();

    // Setup LedRing
    pinMode(LEDRING_PIN, OUTPUT);
    ledRing.init(LEDRING_PIN, leds, NUM_LEDS);
    ledRing.clear();
    ledRing.update();
    

    // Setup audio output
    pinMode(AUDIO_OUT_PIN, OUTPUT);

    Serial.println("--------------------");
    Serial.println("* Hardware testing *");


    Serial.println();
    Serial.println("Press button to record.");
    Serial.println();

    // prime the timer (set it to current time)
    log_timer = millis();
    sound_timer = millis();
}

void loop()
{
    Button1.update();
    if (Button1.pressed())
    {
        // Set LED to RED
        if (recordIndex % 8 == 0)
        {
            ledRing.clear();
            ledRing.update();
        }

        ledRing[recordIndex % 8] = Color::Red;
        ledRing.update();

        // Stop any playing tone
        stopTone(AUDIO_OUT_PIN);

        // Delay to not record the button click
        delay(400);

        Serial.println("Now recording 3 seconds..");

        microphone.startRecording();

        // Wait for 3 seconds
        delay(3000);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getRecordedLengthMs());
        Serial.println(" miliseconds recorded, ");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getRecordedData(),
                microphone.getRecordedLengthMs(),
                "my_sound",
                MY_DEVICE,
                recordIndex,
                CREATE_CROPS // dont't create cropped files
            );

            // saved, set the led to GREEN
            ledRing[recordIndex % 8] = Color::Green;
            ledRing.update();

            // increase our index for the next recording
            // so each of our files will have a unique name
            recordIndex++;

            // store the last used recording index into the device EEPROM
            storeIndex(recordIndex);            
        } 
        else 
        {
            // not saved, turn the led back to black
            ledRing[recordIndex % 8] = Color::Black;
            ledRing.update();
        }
    }

    // Print the values of the lightsensors every second.
    // this prevents the values being logged continuously, stalling the device
    if (millis() > log_timer + 1000)
    {
        // set our timer to the current time.
        log_timer = millis();
        Serial.print("Light Sensor L:");
        Serial.print(analogRead(LDR_LEFT_PIN));
        Serial.print(" | Light Sensor R:");
        Serial.println(analogRead(LDR_RIGHT_PIN));
    }

    // Make some random sounds, usefull as background noise for the dataset
    if (millis() > sound_timer + next_sound_time)
    {
        // set our timer to the current time.
        sound_timer = millis();
        next_sound_time = random(100, 500); // next sound between 0.1 and 0.5 seconds

        if (random(3) < 2) {
            // play the note for a short duration
            startTone(AUDIO_OUT_PIN, random(70,1500));
        } else {
            // stop any playing tone
            stopTone(AUDIO_OUT_PIN);
        }
    }
}