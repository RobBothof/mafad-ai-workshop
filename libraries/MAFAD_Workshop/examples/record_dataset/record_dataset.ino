#include <Bounce2.h>
#include <ai-workshop-main.h>
#include <ai-workshop-ws2812.h>
#include <ai-workshop-sound.h>
#include <ai-workshop-pitches.h>
#include <ai-workshop-sdcard.h>
#include <ai-workshop-mic.h>

#define MY_DEVICE "MAFAD"
#define MY_MELODY_1 "hello_there"
#define MY_MELODY_2 "i_love_cake"
#define MY_MELODY_3 "get_bonus"

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

#define NUM_CROPS 0 // create cropped audio files around the sound event

uint32_t randomness = 0;

// Create an array/list to store 8 colors for the ledring.
Color leds[NUM_LEDS];

// Create a button object (with debouncing).
Bounce2::Button Button1 = Bounce2::Button();

// Create a led ring object.
WS2812 ledRing;

// Create a sd card object.
SDCard sdCard;

// Create a boolean variable to remember if an sd card is present.
bool hasSDCard = false;

// Create a microphone object.
i2sMic microphone;

// Create a value to keep track of the recordings we store
uint32_t recordIndex = 0;

uint8_t activeLED = 0;

// Define a series of notes.
int notes[] = {
    NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2,
    NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3,
    NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
    NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
    NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
    NOTE_C7};

void playMelody1()
{
    playTone(AUDIO_OUT_PIN, NOTE_C3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_C4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_D3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_D4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_DS3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_DS4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_F3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_F4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_G3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_G4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_GS3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_GS4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_AS3, 60);
    playTone(AUDIO_OUT_PIN, NOTE_AS4, 60);
    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_C4, 60);
    playTone(AUDIO_OUT_PIN, NOTE_C5, 60);
}

void playMelody2()
{
    for (int loop = 0; loop < 50; loop++)
    {
        playTone(AUDIO_OUT_PIN, 800 - loop * 5, 15);
        Silence(15);
    }
}

void playMelody3()
{
    for (int loop = 0; loop < 3; loop++)
    {
        playTone(AUDIO_OUT_PIN, NOTE_C4, 30);
        playTone(AUDIO_OUT_PIN, NOTE_DS4, 30);
        playTone(AUDIO_OUT_PIN, NOTE_G4, 30);
    }

    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_C2, 90);
    Silence(30);

    for (int loop = 0; loop < 3; loop++)
    {
        playTone(AUDIO_OUT_PIN, NOTE_C4, 30);
        playTone(AUDIO_OUT_PIN, NOTE_DS4, 30);
        playTone(AUDIO_OUT_PIN, NOTE_G4, 30);
    }

    Silence(30);
    playTone(AUDIO_OUT_PIN, NOTE_C2, 90);
    Silence(30);

    for (int loop = 0; loop < 8; loop++)
    {
        playTone(AUDIO_OUT_PIN, NOTE_C5, 30);
        playTone(AUDIO_OUT_PIN, NOTE_AS4, 30);
        playTone(AUDIO_OUT_PIN, NOTE_G5, 30);
    }
}

void playRandomSounds(int lengthMax)
{
    int maxToneLength = lengthMax;
    int choice = random(0, 3);
    if (choice == 0)
        maxToneLength = 100;
    if (choice == 1)
        maxToneLength = 750;
    if (choice == 2)
        maxToneLength = 1750;

    bool done = false;

    while (!done)
    {
        int duration = random(2, maxToneLength + 3);

        if (lengthMax - duration <= 0)
        {
            duration = lengthMax;
            done = true;
        }

        lengthMax -= duration;

        if (random(0, 5) == 0)
        {
            playTone(AUDIO_OUT_PIN, random(70,2000), duration);
        } else {
            Silence(duration);
        }
    }
}

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

    // Setup LedRing
    pinMode(LEDRING_PIN, OUTPUT);
    ledRing.init(LEDRING_PIN, leds, NUM_LEDS);
    ledRing.clear();
    ledRing.update();

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

    // Setup audio output
    pinMode(AUDIO_OUT_PIN, OUTPUT);

    Serial.println("--------------------");
    Serial.println("* Dataset Recorder *");
    Serial.println();
    Serial.println("Press button to record.");
    Serial.println();

    activeLED = 0;
}

void loop()
{
    Button1.update();

    if (Button1.pressed())
    {
        // Clear the led ring every 8 recordings
        if (activeLED > 7 )
        {
            ledRing.clear();
            ledRing.update();
            activeLED = 0;
        }

        // Set the randomness in tone and duration for this recording session
        randomness = random(4);

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        // Delay to not record the button click
        delay(400);

        //--------- Record Melody #1 ---------

        microphone.startRecording();

        // Record 350 - 450 milliseconds of ambient noise before starting the song (random start).
        delay(random(350, 450));

        playMelody1();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_1,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }

        Serial.println();

        //--------- Record Melody #2 ---------

        microphone.startRecording();

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        // Record 350 - 450 milliseconds of ambient noise before starting the song (random start).
        delay(random(350, 450));

        playMelody2();

        // Record 400 milliseconds of ambient noise / silence after the melody.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_2,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();

        //--------- Record Melody #3 ---------

        microphone.startRecording();

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        // Record 350 - 450 milliseconds of ambient noise before starting the song (random start).
        delay(random(350, 450));

        playMelody3();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();
        
        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_3,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();
        
        //---------------------------------------------

        // increase our index for the next recording
        // so each of our files will have a unique name
        recordIndex++;

        //--------- Record Melody #1 - take 2  ---------

        microphone.startRecording();

        // Record 250 - 350 milliseconds of ambient noise before starting the song (random start).
        delay(random(250, 350));

        playMelody1();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_1,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }

        Serial.println();

        //--------- Record Melody #2 - take 2 ---------

        microphone.startRecording();

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        // Record 250 - 350 milliseconds of ambient noise before starting the song (random start).
        delay(random(250, 350));

        playMelody2();

        // Record 400 milliseconds of ambient noise / silence after the melody.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_2,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();

        //--------- Record Melody #3 - take 2 ---------

        microphone.startRecording();

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        // Record 250 - 350 milliseconds of ambient noise before starting the song (random start).
        delay(random(250, 350));

        playMelody3();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400);

        // stop and wait for recording to finish.
        microphone.stopRecording();
        
        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                MY_MELODY_3,
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();

        //--------- Record random Sounds ---------

        // Start recording and play random tones.
        microphone.startRecording();

        // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        delay(100);

        playRandomSounds(2800);

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(100);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                "random",
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();

        //--------- Ambient noise / Silence ---------

                // Set LED to RED
        ledRing[activeLED] = Color::Red;
        ledRing.update();

        microphone.startRecording();

        // wait for 3 seconds while recording ambient noise
        delay(3000);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Set LED to Blue
        ledRing[activeLED] = 0x000066;
        ledRing.update();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" milliseconds recorded.");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            sdCard.writeAudioFile(
                microphone.getData(),
                microphone.getLength(),
                "noise",
                MY_DEVICE,
                recordIndex,
                NUM_CROPS
            );
        }
        Serial.println();

        // Done with recording.

        // Set this led to green
        ledRing[activeLED] = 0x006600;
        ledRing.update();

        // increase our index for the next recording
        // so each of our files will have a unique name
        recordIndex++;

        // store the last used recording index into the device EEPROM
        storeIndex(recordIndex);
    }
}