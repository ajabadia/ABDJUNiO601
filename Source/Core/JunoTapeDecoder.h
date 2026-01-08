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
        
        juce::AudioBuffer<float> buffer(1, (int)reader->lengthInSamples);
        reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);
        
        const float* samples = buffer.getReadPointer(0);
        double sr = reader->sampleRate;
        const int numSamples = buffer.getNumSamples();
        
        // [reimplement.md] Normalize input audio
        float maxPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float absS = std::abs(samples[i]);
            if (absS > maxPeak) maxPeak = absS;
        }
        
        if (maxPeak < 0.0001f) {
            result.errorMessage = "Signal is silence.";
            return result;
        }

        // [reimplement.md] Energy Window detection for noisy recordings
        std::vector<int> crossings;
        bool isPositive = samples[0] > 0.0f;
        
        // Dynamic threshold based on local energy window
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
            if (state[s] == true && state[s+1] == false) {
                double centerOffset = samplesPerBit * 0.5;
                double checkPos = (double)s + 1.0 + centerOffset;
                
                uint8_t byte = 0;
                bool validFrame = true;
                
                for (int b = 0; b < 8; ++b) {
                    checkPos += samplesPerBit;
                    if (checkPos >= numSamples) { validFrame = false; break; }
                    if (state[(int)checkPos]) byte |= (1 << b);
                }
                
                checkPos += samplesPerBit; 
                if (checkPos < numSamples && state[(int)checkPos] == true && validFrame) {
                     decodedBytes.push_back(byte);
                     s = (int)checkPos; 
                     continue;
                }
            }
            s++;
        }
        
        result.data = std::move(decodedBytes);
        result.success = !result.data.empty();
        if (!result.success) result.errorMessage = "No valid data found.";
        
        return result;
    }
};
