#ifndef WORKSHOP_MIC_H
#define WORKSHOP_MIC_H

#include "ai-workshop-main.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_system.h>
#include <cstring>
#include <climits>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DMA_BUFFER_SIZE 1024      // size of the DMA buffer

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

    // Audio stream state (for inference)

    struct StreamState {
        float* buffers[2] = { nullptr, nullptr };
        volatile uint8_t bufferSelect = 0;
        volatile uint32_t bufferCount = 0;
        volatile uint8_t bufferReady = 0;
        uint32_t sliceSamples = 0;
        volatile bool running = false;
        TaskHandle_t task = nullptr;
        volatile uint32_t overruns = 0;
    };

    StreamState _stream;

    // single active instance for signal.get_data 
    static i2sMic* s_activeStreamInstance;

    static void streamTask(void* parameter)
    {
        i2sMic* microphone = static_cast<i2sMic*>(parameter);
        s_activeStreamInstance = microphone;

        // small warmup (matches EI examples)
        delay(50);

        while (microphone->_stream.running)
        {
            size_t bytesRead = 0;

            esp_err_t err = i2s_read(
                I2S_NUM_0,
                (void*)microphone->_dmaBuffer,
                DMA_BUFFER_SIZE * sizeof(int32_t),
                &bytesRead,
                portMAX_DELAY
            );

            if (!microphone->_stream.running) break;
            if (err != ESP_OK || bytesRead == 0) continue;

            const uint32_t samplesRead = bytesRead / sizeof(int32_t);

            float* activeBuffer = (microphone->_stream.bufferSelect == 0)
                ? microphone->_stream.buffers[0]
                : microphone->_stream.buffers[1];
                        
            for (uint32_t i = 0; i < samplesRead; i++)
            {
                activeBuffer[microphone->_stream.bufferCount++] = (float) (clip(microphone->_dmaBuffer[i] / 4096));

                if (microphone->_stream.bufferCount >= microphone->_stream.sliceSamples)
                {
                    // if consumer didn’t take the previous slice yet, we drop/overwrite and count it
                    if (microphone->_stream.bufferReady == 1) {
                        microphone->_stream.overruns++;
                    }

                    microphone->_stream.bufferSelect ^= 1;
                    microphone->_stream.bufferCount = 0;
                    microphone->_stream.bufferReady = 1;

                    activeBuffer = (microphone->_stream.bufferSelect == 0) ? microphone->_stream.buffers[0] : microphone->_stream.buffers[1];
                }
            }
        }

        microphone->_stream.task = nullptr;
        vTaskDelete(nullptr);
    }        

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
            .dma_buf_count = 8,
            .dma_buf_len = 128,
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
    bool startStream(uint32_t sliceSamples)
    {
        if (_task != nullptr) return false;          // don’t infer while recording task runs
        if (_stream.running) return false;
        if (!_dmaBuffer) return false;
        if (sliceSamples == 0) return false;

        _stream.sliceSamples = sliceSamples;
        _stream.bufferSelect = 0;
        _stream.bufferCount = 0;
        _stream.bufferReady = 0;
        _stream.overruns = 0;
        
        _stream.buffers[0] = (float*)malloc(_stream.sliceSamples * sizeof(float));
        _stream.buffers[1] = (float*)malloc(_stream.sliceSamples * sizeof(float));
        
        if (!_stream.buffers[0] || !_stream.buffers[1]) {
            if (_stream.buffers[0]) { free(_stream.buffers[0]); _stream.buffers[0] = nullptr; }
            if (_stream.buffers[1]) { free(_stream.buffers[1]); _stream.buffers[1] = nullptr; }
            return false;
        }

        i2s_zero_dma_buffer(I2S_NUM_0);

        _stream.running = true;
        if (pdPASS != xTaskCreatePinnedToCore(
                streamTask,
                "MicStreamCapture",
                1024 * 8,
                this,
                10,
                &_stream.task,
                0))
        {
            _stream.running = false;
            free(_stream.buffers[0]); _stream.buffers[0] = nullptr;
            free(_stream.buffers[1]); _stream.buffers[1] = nullptr;
            return false;
        }

        return true;
    }

    // Blocking: waits for next slice and consumes it.
    bool waitForStream()
    {
        while (_stream.bufferReady == 0) {
            delay(1);
        }
        _stream.bufferReady = 0;
        return true;
    }

    bool isStreamReady() const { return _stream.bufferReady == 1; }
    void consumeStream() { _stream.bufferReady = 0; }

    uint32_t getStreamOverruns() const { return _stream.overruns; }

    static int getStreamData(size_t offset, size_t length, float* out_ptr)
    {
        if (!s_activeStreamInstance) return -1;
        float* readyBuffer = s_activeStreamInstance->_stream.buffers[s_activeStreamInstance->_stream.bufferSelect ^ 1];
        memcpy(out_ptr, &readyBuffer[offset], length * sizeof(float));

        return 0;
    }

    void stopStream()
    {
        _stream.running = false;

        if (_stream.buffers[0]) { free(_stream.buffers[0]); _stream.buffers[0] = nullptr; }
        if (_stream.buffers[1]) { free(_stream.buffers[1]); _stream.buffers[1] = nullptr; }

        _stream.bufferReady = 0;
        _stream.bufferCount = 0;
        _stream.sliceSamples = 0;
    }

    uint32_t getStreamSize() const {
        return _stream.sliceSamples;
    }

    bool isRecordDone()
    {
        return _done;
    }

    int getRecordedSamples()
    {
        return _samplesRecorded;
    }

    int getRecordedLengthMs()
    {
        return _recLength;
    }
    
    int32_t* getRecordedData()
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

        // Flush any queued audio so the recording starts "now"
        i2s_zero_dma_buffer(I2S_NUM_0);

        _recTimer = millis();

        if (pdPASS != xTaskCreatePinnedToCore(recordTask, "MicRecordCapture", 4096, this, 2, &_task, 0))
        {
            _task = nullptr;
            return false;
        }
        return true;
    }
};

// static pointer
i2sMic* i2sMic::s_activeStreamInstance = nullptr;

#endif
