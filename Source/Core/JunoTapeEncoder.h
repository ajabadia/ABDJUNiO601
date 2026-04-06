#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

class JunoTapeEncoder {
public:
    static inline juce::AudioBuffer<float> encodePatches(const std::vector<std::vector<uint8_t>>& patches, double sampleRate)
    {
        const double bitsPerSecond = 1200.0;
        const double freqLow = 1300.0;  // Space (0) - Roland Standard
        const double freqHigh = 2100.0; // Mark (1)  - Roland Standard
        
        std::vector<bool> bitStream;
        
        // 1. Leader (Pilot tone): 2 seconds of Marks
        for (int i = 0; i < (int)(bitsPerSecond * 2.0); ++i) bitStream.push_back(true);
        
        for (const auto& data : patches) {
            if (data.size() != 18) continue;
            
            // No proprietary header for total hardware fidelity
            
            // Data
            uint8_t checksum = 0;
            for (uint8_t b : data) {
                appendByte(bitStream, b);
                checksum += b;
            }
            checksum &= 0x7F;
            
            // Checksum
            appendByte(bitStream, checksum);
            
            // No proprietary end byte
            
            // Inter-block gap (0.1s of Marks)
            for (int i = 0; i < (int)(bitsPerSecond * 0.1); ++i) bitStream.push_back(true);
        }
        
        // Trailer: 0.5 seconds of Marks
        for (int i = 0; i < (int)(bitsPerSecond * 0.5); ++i) bitStream.push_back(true);
        
        // Render bitstream to audio
        int numSamples = (int)(bitStream.size() * (sampleRate / bitsPerSecond));
        juce::AudioBuffer<float> buffer(1, numSamples);
        float* samples = buffer.getWritePointer(0);
        
        double phase = 0.0;
        double currentBitStartSample = 0.0;
        double samplesPerBit = sampleRate / bitsPerSecond;
        
        for (size_t b = 0; b < bitStream.size(); ++b) {
            bool bit = bitStream[b];
            double freq = bit ? freqHigh : freqLow;
            double phaseDelta = (juce::MathConstants<double>::twoPi * freq) / sampleRate;
            
            int startS = (int)currentBitStartSample;
            int endS = (int)(currentBitStartSample + samplesPerBit);
            
            for (int s = startS; s < endS && s < numSamples; ++s) {
                samples[s] = (float)std::sin(phase);
                phase += phaseDelta;
            }
            currentBitStartSample += samplesPerBit;
        }
        
        return buffer;
    }

private:
    static void appendByte(std::vector<bool>& stream, uint8_t byte) {
        // Start bit (0)
        stream.push_back(false);
        // 8 Data bits (LSB first)
        for (int i = 0; i < 8; ++i) {
            stream.push_back((byte & (1 << i)) != 0);
        }
        // Stop bit (1)
        stream.push_back(true);
    }
};
