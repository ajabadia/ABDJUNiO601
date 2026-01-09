#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
 * JunoTapeDecoder
 * Decodes Roland Juno-106 FSK tape audio (1300Hz/2100Hz) into binary patch data.
 */
class JunoTapeDecoder {
public:
    struct DecodeResult {
        bool success = false;
        std::vector<uint8_t> data;
        juce::String errorMessage;
    };

    static constexpr float kFreq0 = 1300.0f;
    static constexpr float kFreq1 = 2100.0f;

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
        
        // Handle stereo files by reading both channels and mixing to mono
        juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);
        
        if (buffer.getNumChannels() > 1) {
            buffer.addFrom(0, 0, buffer, 1, 0, buffer.getNumSamples()); // Mix right channel into left
            buffer.applyGain(0.5f); // Attenuate to avoid clipping
        }
        
        // Auto-Normalization of input buffer (now operating on mono channel 0)
        float maxPeak = buffer.getMagnitude(0, 0, buffer.getNumSamples());

        if (maxPeak > 0.0001f) {
            buffer.applyGain(1.0f / maxPeak);
        } else {
            result.errorMessage = "Signal is silence.";
            return result;
        }

        const float* samples = buffer.getReadPointer(0);
        double sr = reader->sampleRate;
        const int numSamples = buffer.getNumSamples();

        // Energy Window detection for noisy recordings
        std::vector<int> crossings;
        bool isPositive = samples[0] > 0.0f;
        
        int windowSize = (int)(sr / 1200.0) / 4;
        for (int i = windowSize; i < numSamples - windowSize; ++i) {
            float windowEnergy = 0.0f;
            for(int w = -windowSize; w <= windowSize; ++w) windowEnergy += std::abs(samples[i+w]);
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
            result.errorMessage = "Signal too weak or short.";
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
            if (state[s] == true && state[s+1] == false) { // Start bit detected (Mark to Space transition)
                double checkPos = (double)s + 1.0 + samplesPerBit * 1.5; // Center of first data bit
                
                uint8_t byte = 0;
                bool validFrame = true;
                
                for (int b = 0; b < 8; ++b) {
                    if (checkPos >= numSamples) { validFrame = false; break; }
                    if (state[(int)checkPos]) byte |= (1 << b);
                    checkPos += samplesPerBit;
                }
                
                // Check for stop bit (Mark)
                if (validFrame && checkPos < numSamples && state[(int)checkPos] == true) {
                     decodedBytes.push_back(byte);
                     s = (int)checkPos; // Move to the end of the frame
                     continue;
                }
            }
            s++;
        }
        
        if (decodedBytes.empty()) {
            result.errorMessage = "No bytes were decoded from the signal.";
            return result;
        }

        // --- Block Parsing and Checksum Validation ---
        std::vector<uint8_t> validatedPatches;
        const uint8_t blockHeader = 0xA5; // Standard Roland Tape Header
        const uint8_t blockEnd = 0xAC;    // Standard Roland Tape End-of-Block

        for (size_t i = 0; i < decodedBytes.size(); ++i)
        {
            if (decodedBytes[i] == blockHeader)
            {
                // Found a potential block. Format: A5, 18 data bytes, checksum, AC. Total size: 21 bytes.
                if (i + 20 < decodedBytes.size() && decodedBytes[i + 20] == blockEnd)
                {
                    uint8_t checksum = 0;
                    for (int j = 0; j < 18; ++j) {
                        checksum += decodedBytes[i + 1 + j];
                    }
                    checksum &= 0x7F; // Checksum is the 7-bit sum of the 18 data bytes.

                    if (checksum == decodedBytes[i + 19])
                    {
                        // Block is valid, add the 18 patch bytes to our results.
                        validatedPatches.insert(validatedPatches.end(), decodedBytes.begin() + i + 1, decodedBytes.begin() + i + 19);
                        i += 20; // Skip past this validated block.
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
