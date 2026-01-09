#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

class JunoTapeDecoder {
public:
    struct DecodeResult {
        bool success = false;
        std::vector<uint8_t> data;
        juce::String errorMessage;
    };

    static inline DecodeResult decodeWavFile(const juce::File& file)
    {
        DecodeResult result;
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr) {
            result.errorMessage = "Could not read WAV file: " + file.getFileName();
            return result;
        }
        
        juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);

        // --- Signal Pre-processing for Robustness ---

        // 1. Mix to mono if the source is stereo.
        if (buffer.getNumChannels() > 1) {
            buffer.addFrom(0, 0, buffer, 1, 0, buffer.getNumSamples());
            buffer.applyGain(0.5f);
        }

        float* samples = buffer.getWritePointer(0);
        const int numSamples = buffer.getNumSamples();

        // 2. Manually calculate and remove DC offset.
        double dcOffset = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            dcOffset += samples[i];
        }
        if (numSamples > 0) {
            dcOffset /= numSamples;
        }
        if (std::abs(dcOffset) > 0.0) {
            for (int i = 0; i < numSamples; ++i) {
                samples[i] -= static_cast<float>(dcOffset);
            }
        }
        
        // 3. Normalize the signal to use the full dynamic range.
        float maxPeak = buffer.getMagnitude(0, 0, numSamples);
        if (maxPeak > 0.0001f) {
            buffer.applyGain(1.0f / maxPeak);
        } else {
            result.errorMessage = "Signal is silence or too quiet after DC removal.";
            return result;
        }

        // --- FSK Decoding ---

        double sr = reader->sampleRate;
        
        // [Final Fix] Re-instating the robust energy window detection.
        std::vector<int> crossings;
        bool isPositive = samples[0] > 0.0f;
        
        int windowSize = (int)(sr / 1200.0) / 4;
        if (windowSize == 0) windowSize = 1;
        
        for (int i = windowSize; i < numSamples - windowSize; ++i) {
            float windowEnergy = 0.0f;
            for(int w = -windowSize; w <= windowSize; ++w) {
                windowEnergy += std::abs(samples[i+w]);
            }
            windowEnergy /= (windowSize * 2 + 1);
            
            float threshold = windowEnergy * 0.15f; 
            
            if (isPositive && samples[i] < -threshold) {
                crossings.push_back(i);
                isPositive = false;
            }
            else if (!isPositive && samples[i] > threshold) {
                crossings.push_back(i);
                isPositive = true;
            }
        }
        
        if (crossings.size() < 20) { 
            result.errorMessage = "Signal too weak or short after processing.";
            return result;
        }

        double midHalfPeriodSeconds = 1.0 / 3400.0;
        std::vector<bool> state(numSamples, true);
        
        if (crossings.size() > 1) {
            for (size_t i = 1; i < crossings.size(); ++i) {
                double halfPeriodSeconds = (double)(crossings[i] - crossings[i-1]) / sr;
                bool mark = (halfPeriodSeconds < midHalfPeriodSeconds);
                for (int s = crossings[i-1]; s < crossings[i]; ++s) state[s] = mark;
            }
        }
        
        double bitsPerSecond = 1200.0; 
        double samplesPerBit = sr / bitsPerSecond;
        std::vector<uint8_t> decodedBytes;
        
        int s = 0;
        while (s < numSamples - (int)(samplesPerBit * 11)) {
            if (state[s] == true && state[s+1] == false) { // Start bit
                double checkPos = (double)s + 1.0 + samplesPerBit * 1.5; 
                
                uint8_t byte = 0;
                bool validFrame = true;
                
                for (int b = 0; b < 8; ++b) {
                    if (checkPos >= numSamples) { validFrame = false; break; }
                    if (state[(int)checkPos]) byte |= (1 << b);
                    checkPos += samplesPerBit;
                }
                
                if (validFrame && checkPos < numSamples && state[(int)checkPos] == true) { // Stop bit
                     decodedBytes.push_back(byte);
                     s = (int)checkPos; 
                     continue;
                }
            }
            s++;
        }
        
        if (decodedBytes.empty()) {
            result.errorMessage = "No valid serial frames found in the signal.";
            return result;
        }

        // --- Block Parsing and Checksum Validation ---
        std::vector<uint8_t> validatedPatches;
        const uint8_t blockHeader = 0xA5;
        const uint8_t blockEnd = 0xAC;

        for (size_t i = 0; i < decodedBytes.size(); ++i) {
            if (decodedBytes[i] == blockHeader) {
                if (i + 20 < decodedBytes.size() && decodedBytes[i + 20] == blockEnd) {
                    uint8_t checksum = 0;
                    for (int j = 0; j < 18; ++j) {
                        checksum += decodedBytes[i + 1 + j];
                    }
                    checksum &= 0x7F;

                    if (checksum == decodedBytes[i + 19]) {
                        validatedPatches.insert(validatedPatches.end(), decodedBytes.begin() + i + 1, decodedBytes.begin() + i + 19);
                        i += 20;
                    }
                }
            }
        }

        result.data = std::move(validatedPatches);
        result.success = !result.data.empty();
        if (!result.success) {
            result.errorMessage = "Decoded audio, but no valid Juno-106 data blocks were found.";
        }
        
        return result;
    }
};
