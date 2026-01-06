#ifndef WORKSHOP_SDCARD_H
#define WORKSHOP_SDCARD_H

#include "ai-workshop-main.h"

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define CROP_OFFSET 2000          // size of crop offset (samples)


class SDCard
{
private:
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

    bool writeCroppedFiles(const char *waveFileName, int32_t *sampleBuffer, uint32_t sampleOffset, uint32_t numSamples=NN_WINDOW_SIZE)
    {
        if (waveFileName == nullptr || sampleBuffer == nullptr || numSamples == 0)
        {
            return false;
        }

        if (numSamples > SAMPLE_BUFFER_SIZE)
        {
            numSamples = SAMPLE_BUFFER_SIZE;
        }

        if (sampleOffset + numSamples > SAMPLE_BUFFER_SIZE)
        {
            sampleOffset = SAMPLE_BUFFER_SIZE - numSamples;
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
        for (uint s = sampleOffset; s < sampleOffset + numSamples; s++)
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


    void writeAudioFile(int32_t *sampleBuffer, uint32_t duration, String baseName, String deviceName, uint32_t fileIndex, uint8_t numCrops=8)
    {
        uint32_t numSamples = (duration * SAMPLE_RATE) / 1000;

        if (numSamples > SAMPLE_BUFFER_SIZE) numSamples = SAMPLE_BUFFER_SIZE;

        if (numCrops == 0) {
            // not using crops store in root of SD card
            // Save full file
            String masterName = "/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + ".wav";  
            writeMasterFile(masterName.c_str(), sampleBuffer, numSamples);
            Serial.println("Wrote file: " + masterName); 
            return;
        }

        ensureDir("/master");

        // Save full file
        String masterName = "/master/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + ".wav";  
        writeMasterFile(masterName.c_str(), sampleBuffer, numSamples);
        Serial.println("Wrote master file: " + masterName); 

        // Save cropped files
        ensureDir("/crops");

        // Edge case: not enough audio to make a 1s crop
        if (numSamples < NN_WINDOW_SIZE) {
            return;
        }

        uint32_t sampleMargin = numSamples - NN_WINDOW_SIZE;
        uint maxCrops = sampleMargin / CROP_OFFSET + 1; // every 100 ms

        bool used[24] = { false };
        uint16_t idxs[24] = {0};
        uint8_t picked = 0;

        if (maxCrops > 24) maxCrops = 24;

        // pick a 8 crops within maxCrops
        if (numCrops > maxCrops) numCrops = maxCrops;

        if (numCrops <= 1) {
            
            uint32_t offset = sampleMargin / 2;

            String name = "/crops/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + "_" + padZeros(offset, 5) + ".wav";  
            writeCroppedFiles(name.c_str(), sampleBuffer, offset, NN_WINDOW_SIZE);
        
            Serial.println("Wrote file: " + name);
            return;
        }

        for (uint8_t b = 0; b < numCrops; b++)
        {
            uint32_t start = (uint32_t)b * maxCrops / numCrops;
            uint32_t end   = (uint32_t)(b + 1) * maxCrops / numCrops;
            if (end == 0) continue;
            end -= 1;

            if (end >= maxCrops) end = maxCrops - 1;
            if (start > end) start = end;

            uint32_t span = end - start + 1;
            uint32_t idx = start + (uint32_t)random((long)span);

            // avoid duplicates (wrap inside bin)
            for (uint32_t tries = 0; tries < span && used[idx]; tries++)
            {
                idx++;
                if (idx > end) idx = start;
            }

            // fallback: global search forward
            if (used[idx])
            {
                for (uint32_t j = 0; j < maxCrops; j++)
                {
                    uint32_t k = (idx + j) % maxCrops;
                    if (!used[k]) { idx = k; break; }
                }
            }

            used[idx] = true;
            idxs[picked++] = (uint16_t)idx;
        }

        // sort indices (tiny list)
        for (uint8_t i = 1; i < picked; i++)
        {
            uint16_t key = idxs[i];
            int8_t k = (int8_t)i - 1;
            while (k >= 0 && idxs[k] > key)
            {
                idxs[k + 1] = idxs[k];
                k--;
            }
            idxs[k + 1] = key;
        }

        for (uint8_t i = 0; i < picked; i++)
        {
            uint32_t offset = (uint32_t)idxs[i] * (uint32_t)CROP_OFFSET;

            // CHANGED: padZeros(offset, 5)
            String name = "/crops/" + baseName + "." + deviceName + padZeros(fileIndex, 6) + "_" + padZeros(offset, 5) + ".wav";
            writeCroppedFiles(name.c_str(), sampleBuffer, offset, NN_WINDOW_SIZE);
            Serial.println("Wrote file: " + name);
        }
       
    }
};

#endif
