#ifndef WORKSHOP_LIBRARY_H
#define WORKSHOP_LIBRARY_H

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <driver/i2s.h>

// Needed for TaskHandle_t / xTaskCreatePinnedToCore / pdPASS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


class i2sMic {
    private:
        TaskHandle_t _task = nullptr;

        // async state
        volatile bool _done = false;
        volatile int  _samplesRecorded = 0;

        int32_t* _buffer = nullptr;
        uint32_t _bufferSize = 0;
        uint32_t _sampleRate = 0;
        uint32_t _recLength = 0;

        static void recordTask(void* parameter) {
            i2sMic* _mic = static_cast<i2sMic*>(parameter);
            size_t bytes_read = 0;
            i2s_read(I2S_NUM_0, _mic->_buffer, sizeof(int32_t) * _mic->_recLength, &bytes_read, portMAX_DELAY);
            _mic->_samplesRecorded = static_cast<int>(bytes_read / sizeof(int32_t));
            _mic->_done = true;
            _mic->_task = nullptr;
            vTaskDelete(nullptr);
        }

    public:
    // Constructors
    i2sMic() {}

    bool setup(uint32_t sampleRate, uint32_t bufferSize, int clockPin, int wordSelectPin, int channelSelectPin) {
        _sampleRate = sampleRate;
        _bufferSize = bufferSize;

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = _sampleRate,
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

    // int record(void *buffer, size_t sampleTimeMs) {
    //     _recLength = (sampleTimeMs * (_sampleRate / 1000)); // assuming 20k

    //     if (_recLength > _bufferSize) {
    //         _recLength = _bufferSize;
    //     }

    //     size_t bytes_read = 0;
    //     if (buffer != nullptr || _recLength != 0) {
    //         i2s_read(I2S_NUM_0, buffer, sizeof(int32_t) * _recLength, &bytes_read, portMAX_DELAY);
    //     }
    //     return static_cast<int>(bytes_read / sizeof(int32_t));
    // }

    bool isRecordDone() {
        return _done;
    }

    int getNumSamplesRecorded() {
        return _samplesRecorded;
    }

    bool startRecord(int32_t *buffer, size_t sampleTimeMs) {
        if (buffer == nullptr || sampleTimeMs == 0) return false;
        if (_task != nullptr) return false; // already recording

        _recLength = (sampleTimeMs * _sampleRate) / 1000; // assuming 20k
        if (_recLength > _bufferSize) {
            _recLength = _bufferSize;
        }
        _buffer = buffer;
        _samplesRecorded = 0;
        _done = false;

        if (pdPASS != xTaskCreatePinnedToCore(recordTask, "i2sMicRec", 4096, this, 2,&_task, 0))
        {
            _task = nullptr;
            return false;
        }
        return true;

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
    
    String padZeros(int number, int width) {
        String result = String(number);
        while (result.length() < width) {
            result = "0" + result;
        }
        return result;
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

    bool writeWavFile(const char * waveFileName, int32_t* sampleBuffer, uint32_t numSamples, uint32_t sampleRate) {
        if (waveFileName==nullptr || sampleBuffer == nullptr || numSamples==0) {
            return false;
        }
        File file = SD.open(waveFileName, FILE_WRITE);
        if(!file){
            Serial.println("Failed to open file for writing");
            return false;
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
        return true;
    }

    bool writeWavFile(String waveFileName, int32_t* sampleBuffer, uint32_t numSamples, uint32_t sampleRate) {
        return writeWavFile(waveFileName.c_str(), sampleBuffer, numSamples, sampleRate);
    }

    String makeFilename(String baseName, int index, int digits = 4) {
        return "/" + baseName + "_" + padZeros(index, digits) + ".wav";
    }
};

#endif
