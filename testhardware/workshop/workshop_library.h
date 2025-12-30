#ifndef WORKSHOP_LIBRARY_H
#define WORKSHOP_LIBRARY_H

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <driver/i2s.h>

void test(int value) {
    Serial.println(value);
}

class i2sMic {
    public:
    // Constructors
    i2sMic() {}

    bool setup(uint32_t sampleRate, int clockPin, int wordSelectPin, int channelSelectPin) {

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = sampleRate,
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


        i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

        i2s_pin_config_t i2s_mic_pins = {
            .bck_io_num = clockPin,
            .ws_io_num = wordSelectPin,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = channelSelectPin
        };        

        i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);

        return true;
    }

    int record(void *buffer, size_t bufferSize) {
        size_t bytes_read = 0;
        i2s_read(I2S_NUM_0, buffer, sizeof(int32_t) * bufferSize, &bytes_read, portMAX_DELAY);
        return static_cast<int>(bytes_read / sizeof(int32_t));
    }
};


class SDCard {
private:
    int16_t clip32(int32_t value) {
        if (value > INT16_MAX) return INT16_MAX;
        if (value < INT16_MIN) return INT16_MIN;
        return static_cast<int16_t>(value);
    }

    void writeWavHeader(File f, uint32_t numSamples, uint32_t sampleRate) {
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
        b4=sampleRate;
        f.write((byte*)&b4,4);          // samplerate
        b4=sampleRate*2;
        f.write((byte*)&b4,4);          // Sample Rate * BitsPerSample * Channels) / 8.
        b2=2;
        f.write((byte*)&b2,2);          // bytes per sample
        b2=16;
        f.write((byte*)&b2,2);          // bits per sample
        f.print("data");
        b4=numSamples*2+44;
        f.write((byte*)&b4,4);          // data length in bytes
    }
public:
    // Constructors
    SDCard() {}

    bool setup(uint8_t SDCARD_CS_PIN) {
        if(SD.begin(SDCARD_CS_PIN)){
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC)
            {
                Serial.println("SDCard Mounted succesfully.");
                return true;
            } else {
                Serial.println("No SD card attached");
            }    
        } else {
            Serial.println("Card Mount Failed");
        }
        return false;
    }

    void contents(const char * dirname = "/", uint8_t levels=0){
        Serial.printf("Listing directory: %s\n", dirname);

        File root = SD.open(dirname);
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
                contents(file.name(), levels -1);
            }
            } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
            }
            file = root.openNextFile();
        }
        Serial.printf("%lluMB of %lluMB in use.\n\n", SD.usedBytes() / (1024 * 1024), SD.totalBytes() / (1024 * 1024));
    }

    void writeWavFile(const char * waveFileName, int32_t* sampleBuffer, uint32_t numSamples, uint32_t sampleRate) {
        if (waveFileName==nullptr || sampleBuffer == nullptr || numSamples==0) {
            return;
        }
        File file = SD.open(waveFileName, FILE_WRITE);
        if(!file){
        Serial.println("Failed to open file for writing");
        return;
        }

        // Write the WAV audio header.
        writeWavHeader(file, numSamples, sampleRate);

        // Write the samples.
        for (uint s=0; s<numSamples; s++) {
            int16_t sample = clip32(sampleBuffer[s]/4096);
            file.write((byte*)&sample,2);
        }

        // Close the file.
        file.close();

    }
};

#endif
