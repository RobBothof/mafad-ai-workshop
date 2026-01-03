
#include <Bounce2.h>

#include <WS2812.h>
#include <ai-workshop.h>
#include <pitches.h>

// add:
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#define MY_DEVICE "MAFAD"
#define MY_SONG_1 "hello_there"
#define MY_SONG_2 "i_love_cake"
#define MY_SONG_3 "get_bonus"

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

// Define a series of notes. (Cminor7 chord)
int notes[] = {
    NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2,
    NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3,
    NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
    NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
    NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
    NOTE_C7, 0, 0, 0, 0, 0, 0};

int numNotes = 53;

void playMySong1()
{
    Tone(AUDIO_OUT_PIN, NOTE_C3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_C4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_D3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_D4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_DS3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_DS4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_F3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_F4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_G3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_G4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_GS3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_GS4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_AS3, 60);
    Tone(AUDIO_OUT_PIN, NOTE_AS4, 60);
    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_C4, 60);
    Tone(AUDIO_OUT_PIN, NOTE_C5, 60);
  }

void playMySong2()
{
    for (int loop = 0; loop < 50; loop++)
    {
        Tone(AUDIO_OUT_PIN, 800 - loop * 5, 15);
        Silence(15);
    }
}

void playMySong3()
{
    for (int loop = 0; loop < 4; loop++)
    {
        Tone(AUDIO_OUT_PIN, NOTE_C4, 30);
        Tone(AUDIO_OUT_PIN, NOTE_DS4, 30);
        Tone(AUDIO_OUT_PIN, NOTE_G4, 30);
    }

    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_C3, 90);
    Silence(30);

    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_G2, 90);
    Silence(30);

    for (int loop = 0; loop < 4; loop++)
    {
        Tone(AUDIO_OUT_PIN, NOTE_C4, 30);
        Tone(AUDIO_OUT_PIN, NOTE_DS4, 30);
        Tone(AUDIO_OUT_PIN, NOTE_G4, 30);
    }

    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_C3, 90);
    Silence(30);

    Silence(30);
    Tone(AUDIO_OUT_PIN, NOTE_G2, 90);
    Silence(30);

    for (int loop = 0; loop < 8; loop++)
    {
        Tone(AUDIO_OUT_PIN, NOTE_C5, 30);
        Tone(AUDIO_OUT_PIN, NOTE_AS4, 30);
        Tone(AUDIO_OUT_PIN, NOTE_G5, 30);
    }
}

void playRandomSong(int lengthMax)
{
    int maxToneLength = lengthMax;
    int choice = random(0, 3);
    if (choice == 0)
        maxToneLength = 100;
    if (choice == 1)
        maxToneLength = 350;
    if (choice == 2)
        maxToneLength = 1250;

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

        int note = random(0, numNotes);
        Tone(AUDIO_OUT_PIN, notes[note], duration);
    }
}

void setup()
{
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

    // Setup printing to serial monitor
    Serial.begin(115200);
    Serial.println();
    Serial.println("--------------------");
    Serial.println("* Dataset Recorder *");
    Serial.println();
    Serial.println("Press button to record.");
    Serial.println();
}

void loop()
{
    Button1.update();

    if (Button1.pressed())
    { // Returns true if the button was pressed
        if (recordIndex % 8 == 0)
        {
            ledRing.clear();
            ledRing.update();
        }

        // wait 1 second before starting with recording
        delay(1000);

        // Set LED to RED
        ledRing[recordIndex % 8] = Color::Red;
        ledRing.update();

        // Set the randomness for this recording session
        randomness = random(4);

        //--------- Record our song #1
        // Start recording and play random tones.
        microphone.startRecording();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400); 

        playMySong1();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400); 

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" miliseconds recorded, ");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {
            // write the audio file
            String filename = sdCard.writeAudioFile(
                microphone.getData(), 
                microphone.getLength(),
                MY_SONG_1, 
                MY_DEVICE, 
                recordIndex
            );
        }

        Serial.println();

        // pause 1 second
        delay(1000);

        //--------- Record our song #2

        // Start recording and play random tones.
        microphone.startRecording();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400); 

        playMySong2();

        // Record 400 milliseconds of ambient noise / silence after the melody.
        delay(400); 

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" miliseconds recorded, ");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {

            // Create a filename with the format: /random.0001.wav
            String filename = sdCard.makeNewFilename(MY_SONG_2, MY_DEVICE, recordIndex);

            Serial.print("Saving on SDCard as: ");
            Serial.println(filename);

            // write the audio file
            sdCard.writeWavFile(filename, microphone.getData(), microphone.getLength());
        }
        Serial.println();
        // pause 1/2 seconds
        delay(1000);

        //--------- Record our song #3

        // Start recording and play random tones.
        microphone.startRecording();

        playMySong3();

        // Add some space to the end and stop the timer, calculate the length of the recording
        delay(30);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" miliseconds recorded, ");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {

            // Create a filename with the format: /random.0001.wav
            String filename = sdCard.makeNewFilename(MY_SONG_3, MY_DEVICE, recordIndex);

            Serial.print("Saving on SDCard as: ");
            Serial.println(filename);

            // write the audio file
            sdCard.writeWavFile(filename, microphone.getData(), microphone.getLength());
        }
        Serial.println();
        // pause 1/2 seconds
        delay(1000);

        //--------- Record random notes

        // Start recording and play random tones.
        microphone.startRecording();

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400); 

        playRandomSong(2000);

        // Record 400 milliseconds of ambient noise / silence before starting the song.
        delay(400); 

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" miliseconds recorded, ");

        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {

            // Create a filename with the format: /random.0001.wav
            String filename = sdCard.makeNewFilename("random", MY_DEVICE, recordIndex);

            Serial.print("Saving on SDCard as: ");
            Serial.println(filename);

            // write the audio file
            sdCard.writeWavFile(filename, microphone.getData(), microphone.getLength());
        }
        Serial.println();
        // pause 1/2 seconds
        delay(1000);

        //--------- Record noise / silence

        // Start recording and play random tones.
        microphone.startRecording();

        // wait for 3 seconds (this will be split up during training)
        delay(3000);

        // stop and wait for recording to finish.
        microphone.stopRecording();

        // Print recording statistics.
        Serial.print(microphone.getLength());
        Serial.println(" miliseconds recorded, ");


        // save the recorded audio to the SDCard (if present)
        if (hasSDCard)
        {

            // Create a filename with the format: /random.device0001.wav
            String filename = sdCard.makeNewFilename("noise", MY_DEVICE, recordIndex);

            Serial.print("Saving on SDCard as: ");
            Serial.println(filename);

            // write the audio file
            sdCard.writeWavFile(filename, microphone.getData(), microphone.getLength());
        }
        Serial.println();

        // Set this led to green
        ledRing[recordIndex % 8] = Color::Green;
        ledRing.update();

        // increase our index for the next recording
        // so each of our files will have a unique name
        recordIndex++;

        // store the last used recording index into the device EEPROM
        storeIndex(recordIndex);

    }
}