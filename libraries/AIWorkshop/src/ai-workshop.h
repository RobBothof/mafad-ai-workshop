#ifndef WORKSHOP_LIBRARY_H
#define WORKSHOP_LIBRARY_H

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_system.h>
#include <cstring> // memcpy
// Needed for TaskHandle_t / xTaskCreatePinnedToCore / pdPASS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <EEPROM.h>

#define SAMPLE_RATE 20000         // recording quality 20khz (20.000 samples per second)
#define SAMPLE_BUFFER_SIZE 60000  // size of the audio buffer, 3 seconds ( 3 * 20.000 = 60000 samples)
#define DMA_BUFFER_SIZE 2048      // size of the DMA buffer
#define NN_WINDOW_SIZE 20000      // size of the NN / inference window (samples)
#define CROP_OFFSET 2000          // size of crop offset (samples)

static inline uint32_t restoreIndex()
{
    EEPROM.begin(32);
    uint32_t index = EEPROM.readUInt(0);
    if (index == 0xFFFFFFFF) index = 1;
    return index;
}

static inline void storeIndex(uint32_t index)
{
    EEPROM.begin(32);
    EEPROM.writeUInt(0, index);
    EEPROM.commit();
}

// One shared variable (defined in exactly one .cpp/.ino)
extern uint32_t randomness;

static void Silence(uint32_t ms)
{
    if (ms < randomness)
    {
        delay(ms);
    }
    else
    {
        delay(ms + random(randomness * 2) - randomness);
    }
}


static void startTone(uint8_t pin, unsigned int frequency)
{
    const uint8_t bits = 12;
    static int8_t attachedPin = -1;

    uint32_t f = frequency;
    if (frequency >= randomness * 2)
    {
        f = frequency + random(randomness * 2) - randomness;
    }

    const uint32_t maxDuty = (1u << bits) - 1u;
    uint32_t duty = 2000u + (uint32_t)random(96);
    if (duty > maxDuty)
        duty = maxDuty;

    uint8_t ch_pin = 7;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// Arduino-ESP32 core v3.x (pin-based LEDC)
    ch_pin = pin;
    if (attachedPin != ch_pin)
    {
        if (attachedPin >= 0)
            ledcDetach((uint8_t)attachedPin);
        ledcAttach(pin, 1000 /*dummy*/, bits);
        attachedPin = ch_pin;
    }
#else
    // Arduino-ESP32 core v2.x (channel-based LEDC)
    static bool initialized = false;

    if (!initialized)
    {
        ledcSetup(ch_pin, 1000 /*dummy*/, bits);
        initialized = true;
    }

    if (attachedPin != pin)
    {
        if (attachedPin >= 0)
            ledcDetachPin((uint8_t)attachedPin);
        ledcAttachPin(pin, ch_pin);
        attachedPin = pin;
    }

#endif

    ledcChangeFrequency(ch_pin, f, bits);
    ledcWrite(ch_pin, duty);
}

static void stopTone(uint8_t pin)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(pin, 0);
#else
    const uint8_t ch = 7;
    ledcWrite(ch, 0);
#endif
}

static void playTone(uint8_t pin, unsigned int frequency, unsigned long duration)
{
    startTone(pin, frequency);
    Silence((uint32_t)duration);
    stopTone(pin);
}

class i2sMic
{
private:
    TaskHandle_t _task = nullptr;

    // async state
    volatile bool _done = false;
    volatile uint32_t _samplesRecorded = 0;

    volatile uint32_t _recLength = 0;
    size_t _recTimer = 0;

    // PSRAM buffer pointer
    int32_t *_psramBuffer = nullptr;

    // DMA-capable buffer pointer
    int32_t *_dmaBuffer = nullptr;

    
    static void recordTask(void *parameter)
    {
        i2sMic *_mic = static_cast<i2sMic *>(parameter);
        
        uint32_t samplesRecorded = 0;

        while (samplesRecorded < SAMPLE_BUFFER_SIZE)
        {
            size_t bytes_read = 0;
            uint32_t samplesToRead = SAMPLE_BUFFER_SIZE - samplesRecorded;
            if (samplesToRead > DMA_BUFFER_SIZE)
                samplesToRead = DMA_BUFFER_SIZE;

            esp_err_t err = i2s_read(I2S_NUM_0, _mic->_dmaBuffer, sizeof(int32_t) * samplesToRead, &bytes_read, portMAX_DELAY);
    
            if (err != ESP_OK || bytes_read == 0)
            {
                break;
            }

            std::memcpy((uint8_t *)_mic->_psramBuffer + samplesRecorded * sizeof(int32_t),
                        (const uint8_t *)_mic->_dmaBuffer,
                        bytes_read);

            samplesRecorded += bytes_read / sizeof(int32_t);
        }

        // Check if already stopped
        if (_mic->_recLength == 0) {
            _mic->_recLength = (uint32_t)((samplesRecorded * 1000ULL) / SAMPLE_RATE);  
        }

        _mic->_samplesRecorded = samplesRecorded;
        _mic->_done = true;
        _mic->_task = nullptr;

        vTaskDelete(nullptr);
    }

public:

    int32_t *data = nullptr;

    // Constructor
    i2sMic() {}

    bool setup(int clockPin, int wordSelectPin, int channelSelectPin)
    {

        randomSeed((uint32_t)esp_random());

        _psramBuffer = (int32_t *)heap_caps_malloc(
            sizeof(int32_t) * SAMPLE_BUFFER_SIZE,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );

        if (!_psramBuffer)
        {
            Serial.println("ERR: i2sMic PSRAM alloc failed");
            return false;
        }

        data = _psramBuffer;

        _dmaBuffer = (int32_t *)heap_caps_malloc(
            (size_t)DMA_BUFFER_SIZE * sizeof(int32_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        
        if (!_dmaBuffer)
        {
            Serial.println("ERR: i2sMic DMA alloc failed");
            return false;
        }

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
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
            .data_in_num = channelSelectPin};

        i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);

        return true;
    }

    bool isRecordDone()
    {
        return _done;
    }

    int getNumSamplesRecorded()
    {
        return _samplesRecorded;
    }

    int getLength()
    {
        return _recLength;
    }
    
    int32_t* getData()
    {
        return _psramBuffer;
    }


    bool stopRecording()
    {
        // Never started
        if (_recTimer == 0) {
            return false;
        }

        uint32_t t = (uint32_t)(millis() - _recTimer);
        uint32_t maxMs = (uint32_t)((SAMPLE_BUFFER_SIZE * 1000ULL) / SAMPLE_RATE);        
        if (t > maxMs) t = maxMs;

        // Check if already stopped
        if (_recLength == 0) {
            _recLength = t;
        }

        // not recording or finished
        if (_task == nullptr) {
            return _done; 
        }

        // wait for task to finish
        while (!_done)
        {
            delay(10);
        }
        return true;
    }

    bool startRecording()
    {
        if (_task != nullptr)
            return false; // already recording

        _samplesRecorded = 0;
        _recLength = 0;
        _done = false;

        _recTimer = millis();

        if (pdPASS != xTaskCreatePinnedToCore(recordTask, "i2sMicRec", 4096, this, 2, &_task, 0))
        {
            _task = nullptr;
            return false;
        }
        return true;
    }
};

class SDCard
{
private:
    int16_t clip32(int32_t value)
    {
        if (value > INT16_MAX)
            return INT16_MAX;
        if (value < INT16_MIN)
            return INT16_MIN;
        return static_cast<int16_t>(value);
    }

    void writeWavHeader(File f, uint32_t numSamples, uint32_t sampleRate)
    {
        uint32_t b4 = 0;
        uint16_t b2 = 0;
        f.print("RIFF");
        b4 = numSamples * 2 + 44 - 8; // exclude 'RIFF' and filesize field
        f.write((byte *)&b4, 4); // filesize
        f.print("WAVE");
        f.print("fmt ");
        b4 = 16;
        f.write((byte *)&b4, 4); // fmt size
        b2 = 1;
        f.write((byte *)&b2, 2); // 1=PCM
        f.write((byte *)&b2, 2); // num channels, 1 = mono
        b4 = sampleRate;
        f.write((byte *)&b4, 4); // samplerate
        b4 = sampleRate * 2;
        f.write((byte *)&b4, 4); // Sample Rate * BitsPerSample * Channels) / 8.
        b2 = 2;
        f.write((byte *)&b2, 2); // bytes per sample
        b2 = 16;
        f.write((byte *)&b2, 2); // bits per sample
        f.print("data");
        b4 = numSamples * 2;
        f.write((byte *)&b4, 4); // data length in bytes
    }

    String padZeros(uint32_t number, int width)
    {
        String result = String(number);
        while (result.length() < width)
        {
            result = "0" + result;
        }
        return result;
    }

    bool writeCroppedFiles(const char *waveFileName, int32_t *sampleBuffer, uint32_t sampleOffset, uint32_t sampleWindowSize=20000)
    {
        if (waveFileName == nullptr || sampleBuffer == nullptr || sampleWindowSize == 0)
        {
            return false;
        }

        if (sampleWindowSize > SAMPLE_BUFFER_SIZE)
        {
            sampleWindowSize = SAMPLE_BUFFER_SIZE;
        }

        if (sampleOffset + sampleWindowSize > SAMPLE_BUFFER_SIZE)
        {
            sampleOffset = SAMPLE_BUFFER_SIZE - sampleWindowSize;
        }

        if (SD.exists(waveFileName)) {
            SD.remove(waveFileName);
        }

        File file = SD.open(waveFileName, FILE_WRITE);
        if (!file)
        {
            Serial.println("Failed to open file for writing");
            return false;
        }

        // Write the WAV audio header.
        writeWavHeader(file, sampleWindowSize, SAMPLE_RATE);

        // Write the samples.
        for (uint s = sampleOffset; s < sampleOffset + sampleWindowSize; s++)
        {
            int16_t sample = clip32(sampleBuffer[s] / 4096);
            file.write((byte *)&sample, 2);
        }

        // Close the file.
        file.close();
        return true;
    }
    
    static bool ensureDir(const char* path)
    {
        if (SD.exists(path)) return true;
        return SD.mkdir(path);
    }
    
    bool writeMasterFile(const char *waveFileName, int32_t *sampleBuffer, uint32_t numSamples)
    {
        if (waveFileName == nullptr || sampleBuffer == nullptr || numSamples == 0)
        {
            return false;
        }

        if (numSamples > SAMPLE_BUFFER_SIZE)
        {
            numSamples = SAMPLE_BUFFER_SIZE;
        }

        if (SD.exists(waveFileName)) {
            SD.remove(waveFileName);
        }

        File file = SD.open(waveFileName, FILE_WRITE);
        if (!file)
        {
            Serial.println("Failed to open file for writing");
            return false;
        }

        // Write the WAV audio header.
        writeWavHeader(file, numSamples, SAMPLE_RATE);

        // Write the samples.
        for (uint s = 0; s < numSamples; s++)
        {
            int16_t sample = clip32(sampleBuffer[s] / 4096);
            file.write((byte *)&sample, 2);
        }

        // Close the file.
        file.close();
        return true;
    }

public:
    // Constructors
    SDCard() {}

    bool setup(uint8_t SDCARD_CS_PIN)
    {
        if (SD.begin(SDCARD_CS_PIN))
        {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC)
            {
                Serial.println("SDCard Mounted succesfully.");
                return true;
            }
            else
            {
                Serial.println("No SD card attached");
            }
        }
        else
        {
            Serial.println("Card Mount Failed");
        }
        return false;
    }

    void contents(const char *dirname = "/", uint8_t levels = 0)
    {
        Serial.printf("Listing directory: %s\n", dirname);

        File root = SD.open(dirname);
        if (!root)
        {
            Serial.println("Failed to open directory");
            return;
        }
        if (!root.isDirectory())
        {
            Serial.println("Not a directory");
            return;
        }

        File file = root.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                Serial.print("  DIR : ");
                Serial.println(file.name());
                if (levels)
                {
                    contents(file.name(), levels - 1);
                }
            }
            else
            {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.println(file.size());
            }
            file = root.openNextFile();
        }
        Serial.printf("%lluMB of %lluMB in use.\n\n", SD.usedBytes() / (1024 * 1024), SD.totalBytes() / (1024 * 1024));
    }


    void writeAudioFile(int32_t *sampleBuffer, uint32_t duration, String baseName, String deviceName, uint32_t fileIndex, bool saveCrops=true)
    {
        uint32_t numSamples = (duration * SAMPLE_RATE) / 1000;

        if (numSamples > SAMPLE_BUFFER_SIZE) numSamples = SAMPLE_BUFFER_SIZE;

        ensureDir("/master");

        // Save full file
        String masterName = "/master/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + ".wav";  
        writeMasterFile(masterName.c_str(), sampleBuffer, numSamples);
        Serial.println("Wrote master file: " + masterName); 

        if (!saveCrops) {
            return;
        }

        ensureDir("/crops");

        // Save cropped files

        // Edge case: not enough audio to make a 1s crop
        if (numSamples < NN_WINDOW_SIZE) {
            return;
        }

        uint32_t sampleMargin = numSamples - NN_WINDOW_SIZE;

        uint maxCrops = sampleMargin / CROP_OFFSET + 1; // every 100 ms

        uint crops = 8;

        // pick a 8 crops within maxCrops
        if (crops > maxCrops) crops = maxCrops;

        if (crops <= 1) {
            
            uint32_t offset = sampleMargin / 2;

            String name = "/crops/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + "_" + padZeros(offset, 4) + ".wav";  
            writeCroppedFiles(name.c_str(), sampleBuffer, offset, NN_WINDOW_SIZE);
        
            Serial.println("Wrote file: " + name);
            return;
        }
        for (uint i = 0; i < crops; i++)
        {
            uint num = i * (maxCrops - 1);
            uint den = crops - 1;
            uint idx = (num + den / 2) / den;

            uint32_t offset = idx * CROP_OFFSET;

            String name = "/crops/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + "_" + padZeros(offset, 4) + ".wav"; 
            writeCroppedFiles(name.c_str(), sampleBuffer, offset, NN_WINDOW_SIZE);
        
            Serial.println("Wrote file: " + name);
        }
       
    }
};

#endif
