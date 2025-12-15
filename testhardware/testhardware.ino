#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "pitches.h"
#include <driver/i2s.h>
#include <Bounce2.h>
#include <WS2812.h>

#define NUM_LEDS 8

#define SAMPLE_BUFFER_SIZE 32000
#define SAMPLE_RATE 20000

#define I2S_MIC_SCK_PIN 1
#define I2S_MIC_WS_PIN 7
#define I2S_MIC_SD_PIN 10

#define SDCARD_CS_PIN 6

#define AUDIO_OUT_PIN 11
#define BUTTON_PIN 17
#define LEDRING_PIN 43

#define LDR_LEFT_PIN 8
#define LDR_RIGHT_PIN 9

Color leds[NUM_LEDS];
Bounce2::Button Button1 = Bounce2::Button();

int16_t clip32(int32_t value) {
  if (value > INT16_MAX) return INT16_MAX;
  if (value < INT16_MIN) return INT16_MIN;
  return static_cast<int16_t>(value);
}

WS2812Array ledRing;

// don't mess around with this
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,    
};

// and don't mess around with this
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SCK_PIN,
    .ws_io_num = I2S_MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD_PIN};

int melody[] = {
  NOTE_C3, NOTE_G5, NOTE_G3, NOTE_A3, NOTE_C5, NOTE_C2, NOTE_B3, NOTE_C6
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 4, 4, 4, 4, 4, 4, 4
};

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

// int16_t raw_samples[SAMPLE_BUFFER_SIZE];
int32_t audioBuffer[SAMPLE_BUFFER_SIZE]; // capture 2 seconds at 16khz
unsigned long recTimer = 0;

bool SDCardGood=false;

// Write the waveheader.32000
void writeWavHeader(File f, uint32_t numSamples) {
    uint32_t b4=0;
    uint16_t b2=0;
    f.print("RIFF");
    b4=numSamples*2+44;
    f.write((byte*)&b4,4);         // filesize
    f.print("WAVE");
    f.print("fmt ");
    b4=16;
    f.write((byte*)&b4,4);          // fmt size
    b2=1;
    f.write((byte*)&b2,2);          // 1=PCM
    f.write((byte*)&b2,2);          // num channels, 1 = mono
    b4=SAMPLE_RATE;
    f.write((byte*)&b4,4);          // samplerate
    b4=SAMPLE_RATE*2;
    f.write((byte*)&b4,4);          // Sample Rate * BitsPerSample * Channels) / 8.
    b2=2;
    f.write((byte*)&b2,2);          // bytes per sample
    b2=16;
    f.write((byte*)&b2,2);          // bits per sample
    f.print("data");
    b4=numSamples*2+44;
    f.write((byte*)&b4,4);          // data length in bytes
}

void setup(){

  pinMode(LDR_LEFT_PIN,INPUT);
  pinMode(LDR_RIGHT_PIN,INPUT);
  pinMode(LEDRING_PIN, OUTPUT);

  // Setup LedRing.
  ledRing.init(LEDRING_PIN, leds, NUM_LEDS);
  for (int l=0; l < NUM_LEDS; l++) {
    leds[l].hex = random(0xffffff);
  }
  ledRing.update();

  // Init I2S Microphone.
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);

  delay(100);

  Serial.begin(115200);
  Serial.println("\n--------------------");

  // Set up Button
  Button1.attach( BUTTON_PIN, INPUT_PULLDOWN);
  Button1.interval(10);
  Button1.setPressedState(HIGH); 

  // Setup AUDIO OUT
  pinMode(AUDIO_OUT_PIN, OUTPUT);

  Serial.println("Waiting for Button Press to record a test sample");
  while(!Button1.pressed()) {
    Button1.update();
  }

  if(SD.begin(SDCARD_CS_PIN)){
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC)
    {
      SDCardGood = true;
      Serial.println("SDCard Mounted succesfully.");
    } else {
      Serial.println("No SD card attached");
      return;
    }    delay(2000);

  } else {
    Serial.println("Card Mount Failed");
    return;
  }



  if (SDCardGood) {
    size_t bytes_read = 0;
    ulong timer = millis();
    i2s_read(I2S_NUM_0, audioBuffer, sizeof(int32_t) * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
    ulong rec_time = millis() - timer;
    int numSamples = bytes_read / (sizeof(int32_t));

    Serial.print("samples read:");
    Serial.println(numSamples);
    Serial.print("record time:");
    Serial.println(rec_time);

    // Create the file.
    File file = SD.open("/test.wav", FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file for writing");
      return;
    } 

    // Write the WAV audio header.
    writeWavHeader(file, numSamples);
    
    // Write the samples.
    for (uint s=0; s<numSamples; s++) {
      int16_t sample = clip32(audioBuffer[s]/4096);
      file.write((byte*)&sample,2);
    }

    // Close the file.
    file.close();
   
    // Show SD Card contents. 
    listDir(SD, "/", 0);
    Serial.printf("%lluMB of %lluMB in use.\n\n", SD.usedBytes() / (1024 * 1024), SD.totalBytes() / (1024 * 1024));

  }

}

void loop(){
 for (int thisNote = 0; thisNote < 8; thisNote++) {

    int note=random(8);
    int noteDuration = 1500 / (random(6)+2);


    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    tone(AUDIO_OUT_PIN, melody[note], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes*0.7);

    delay(pauseBetweenNotes*0.3);

  }

  for (int l=0; l < NUM_LEDS; l++) {
    leds[l].hex=random(0xFFFFFF);
  }
  ledRing.update();

  Serial.print("LDR1:");
  Serial.println(analogRead(LDR_LEFT_PIN));
  Serial.print("LDR2:");
  Serial.println(analogRead(LDR_RIGHT_PIN));

  delay(1000);

}