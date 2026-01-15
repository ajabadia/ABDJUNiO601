#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
 * JunoTapeEncoder
 * Converts Juno-106 patch data into 1200 baud FSK audio samples.
 * Format: 0 = 1200Hz, 1 = 2400Hz.
 */
class JunoTapeEncoder {
public:
    static juce::AudioBuffer<float> encodePatch(const uint8_t* data18, double sampleRate = 44100.0)
    {
        // 1. Prepare Byte Stream
        // [Fidelidad] Pilot Tone (1 sec 2400Hz) + Header + Data + Checksum + Trailer
        std::vector<uint8_t> bytes;
        
        // Block structure: A5 [Data 18] [Checksum] AC
        bytes.push_back(0xA5);
        uint8_t checksum = 0;
        for (int i = 0; i < 18; ++i) {
            bytes.push_back(data18[i]);
            checksum += data18[i];
        }
        bytes.push_back(checksum & 0x7F);
        bytes.push_back(0xAC);

        // 2. Convert to Bit Stream (Asynchronous Serial: Start(0), 8-bits LSB-first, Stop(1))
        std::vector<bool> bits;
        
        // Pre-roll (Pilot tone - logic high / 2400Hz)
        for (int i = 0; i < 600; ++i) bits.push_back(true);

        for (uint8_t b : bytes) {
            bits.push_back(false); // Start bit (Logic 0 / 1200Hz)
            for (int i = 0; i < 8; ++i) {
                bits.push_back((b & (1 << i)) != 0);
            }
            bits.push_back(true); // Stop bit (Logic 1 / 2400Hz)
            bits.push_back(true); // Extra stop bit for reliability
        }
        
        // Post-roll
        for (int i = 0; i < 100; ++i) bits.push_back(true);

        // 3. Generate Audio Samples
        double bitsPerSecond = 1200.0;
        double samplesPerBit = sampleRate / bitsPerSecond;
        int totalSamples = (int)(bits.size() * samplesPerBit);
        
        juce::AudioBuffer<float> buffer(1, totalSamples);
        float* samples = buffer.getWritePointer(0);
        
        double phase = 0.0;
        for (size_t bitIdx = 0; bitIdx < bits.size(); ++bitIdx) {
            bool logicLevel = bits[bitIdx];
            double freq = logicLevel ? 2400.0 : 1200.0;
            double phaseIncrement = (juce::MathConstants<double>::twoPi * freq) / sampleRate;
            
            int startSample = (int)(bitIdx * samplesPerBit);
            int endSample = (int)((bitIdx + 1) * samplesPerBit);
            
            for (int i = startSample; i < endSample; ++i) {
                samples[i] = (float)std::sin(phase);
                phase += phaseIncrement;
            }
        }
        
        return buffer;
    }

    static juce::Result saveToWav(const juce::File& file, const uint8_t* data18)
    {
        double sr = 44100.0;
        auto buffer = encodePatch(data18, sr);
        
        juce::WavAudioFormat wavFormat;
#pragma warning(push)
#pragma warning(disable: 4996)
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(file),
                                                                                  sr, 1, 16, {}, 0));
#pragma warning(pop)
        if (writer != nullptr) {
            writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            return juce::Result::ok();
        }
        
        return juce::Result::fail("Could not create WAV writer.");
    }
};
