#ifndef WORKSHOP_INFERENCE_H
#define WORKSHOP_INFERENCE_H

#include "ai-workshop-main.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"

// This helper expects the Edge Impulse model header to be included BEFORE it.
// e.g. #include <MAFAD_Classifier_inferencing.h>
#ifndef EI_CLASSIFIER_FREQUENCY
#error "Include your Edge Impulse *_inferencing.h before ai-workshop-inference.h"
#endif

class AiWorkshopInference {
public:
    struct Top {
        int index = -1;
        const char* label = nullptr;
        float score = 0.0f;
    };

    bool begin(int bckPin, int wsPin, int sdPin, i2s_port_t port = I2S_NUM_1, float smoothing = 0.5f) {
        _port = port;
        _smoothing = smoothing;

        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) _ema[i] = 0.0f;

        if (!allocBuffers(EI_CLASSIFIER_SLICE_SIZE)) {
            return false;
        }

        if (i2sInit(EI_CLASSIFIER_FREQUENCY, bckPin, wsPin, sdPin) != ESP_OK) {
            return false;
        }

        _recording = true;
        // Capture task writes into the slice buffer continuously
        xTaskCreate(captureTaskTrampoline, "AIW_Capture", 1024 * 8, this, 10, nullptr);

        return true;
    }

    // Blocking: waits for next slice, then runs EI continuous classifier.
    // Returns true when classification ran successfully.
    bool tick(ei_impulse_result_t& outResult, bool debug = false) {
        if (!_recording) return false;

        if (!waitForSlice()) {
            return false;
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
        signal.get_data = &signalGetDataTrampoline;

        EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &outResult, debug);
        if (r != EI_IMPULSE_OK) {
            return false;
        }

        // EMA smoothing for workshop stability
        _top = Top{};
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            _ema[ix] = _smoothing * _ema[ix] + (1.0f - _smoothing) * outResult.classification[ix].value;
            if (_ema[ix] > _top.score) {
                _top.score = _ema[ix];
                _top.index = (int)ix;
                _top.label = outResult.classification[ix].label;
            }
        }

        return true;
    }

    Top top() const { return _top; }

    void end() {
        _recording = false;
        i2s_driver_uninstall(_port);
        if (_buf0) { free(_buf0); _buf0 = nullptr; }
        if (_buf1) { free(_buf1); _buf1 = nullptr; }
    }

private:
    // --- Buffering ---
    int16_t* _buf0 = nullptr;
    int16_t* _buf1 = nullptr;
    volatile uint8_t _bufSelect = 0;
    volatile uint32_t _bufCount = 0;
    volatile uint8_t _bufReady = 0;
    uint32_t _nSamples = 0;

    // --- Runtime state ---
    i2s_port_t _port = I2S_NUM_1;
    volatile bool _recording = false;

    float _smoothing = 0.5f;
    float _ema[EI_CLASSIFIER_LABEL_COUNT];
    Top _top;

    // small DMA read buffer (32-bit I2S samples)
    static constexpr uint32_t kI2SReadSamples = 1024;
    int32_t _i2sRaw[kI2SReadSamples];

private:
    static int16_t clip32to16(int32_t v) {
        if (v > INT16_MAX) return INT16_MAX;
        if (v < INT16_MIN) return INT16_MIN;
        return (int16_t)v;
    }

    bool allocBuffers(uint32_t nSamples) {
        _nSamples = nSamples;
        _buf0 = (int16_t*)malloc(_nSamples * sizeof(int16_t));
        _buf1 = (int16_t*)malloc(_nSamples * sizeof(int16_t));
        if (!_buf0 || !_buf1) return false;

        _bufSelect = 0;
        _bufCount = 0;
        _bufReady = 0;
        return true;
    }

    bool waitForSlice() {
        // If the consumer falls behind, EI example flags overrun.
        // For workshop: fail fast so user sees it.
        if (_bufReady == 1) {
            return false;
        }
        while (_bufReady == 0) delay(1);
        _bufReady = 0;
        return true;
    }

    void onSamples(uint32_t numSamples) {
        int16_t* active = (_bufSelect == 0) ? _buf0 : _buf1;
        for (uint32_t i = 0; i < numSamples; i++) {
            // ESP32 I2S mic samples are 32-bit; your original code scaled by /4096
            active[_bufCount++] = clip32to16(_i2sRaw[i] / 4096);

            if (_bufCount >= _nSamples) {
                _bufSelect ^= 1;
                _bufCount = 0;
                _bufReady = 1;
                active = (_bufSelect == 0) ? _buf0 : _buf1;
            }
        }
    }

    // EI signal getter reads from the *previous* completed buffer
    static int signalGetDataTrampoline(size_t offset, size_t length, float* out_ptr) {
        return instance()->signalGetData(offset, length, out_ptr);
    }

    int signalGetData(size_t offset, size_t length, float* out_ptr) {
        int16_t* ready = (_bufSelect == 0) ? _buf1 : _buf0;
        numpy::int16_to_float(&ready[offset], out_ptr, length);
        return 0;
    }

    // Trampoline pattern (EI wants function pointer; we want instance method)
    static AiWorkshopInference*& instance() {
        static AiWorkshopInference* s = nullptr;
        return s;
    }

    static void captureTaskTrampoline(void* arg) {
        AiWorkshopInference* self = (AiWorkshopInference*)arg;
        instance() = self;
        self->captureTask();
        vTaskDelete(nullptr);
    }

    void captureTask() {
        while (_recording) {
            size_t bytesRead = 0;
            i2s_read(_port, (void*)_i2sRaw, kI2SReadSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
            if (bytesRead == 0) continue;

            uint32_t samplesRead = (uint32_t)(bytesRead / sizeof(int32_t));
            onSamples(samplesRead);
        }
    }

    esp_err_t i2sInit(uint32_t samplingRate, int bckPin, int wsPin, int sdPin) {
        i2s_config_t cfg = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = samplingRate,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0,
        };

        i2s_pin_config_t pins = {
            .bck_io_num = bckPin,
            .ws_io_num = wsPin,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = sdPin,
        };

        esp_err_t r = i2s_driver_install(_port, &cfg, 0, nullptr);
        if (r != ESP_OK) return r;

        r = i2s_set_pin(_port, &pins);
        if (r != ESP_OK) return r;

        return i2s_zero_dma_buffer(_port);
    }
};

#endif // WORKSHOP_INFERENCE_H