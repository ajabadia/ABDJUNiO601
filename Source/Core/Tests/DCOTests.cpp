#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/JunoDCO.h"

class JunoDCOTests : public juce::UnitTest {
public:
    JunoDCOTests() : juce::UnitTest("JunoDCO Tests", "JunoDSP") { printf("JunoDCOTests registered\n"); }

    void runTest() override {
        beginTest("DCO Frequency Calculation");
        
        JunoDCO dco;
        dco.prepare(44100.0, 512);
        
        // Test base frequency
        dco.setFrequency(440.0f);
        // We can't easily check private state, but we can check if it produces audio
        float sample = dco.getNextSample(0.0f);
        expect(std::abs(sample) <= 1.0f);
        
        beginTest("DCO Timer Quantization");
        // Test if quantization logic doesn't crash
        dco.setFrequency(20.0f); // Low freq
        sample = dco.getNextSample(0.0f);
        expect(!std::isnan(sample));
        
        dco.setFrequency(15000.0f); // High freq
        sample = dco.getNextSample(0.0f);
        expect(!std::isnan(sample));
    }
};

class JunoSubOscTests : public juce::UnitTest {
public:
    JunoSubOscTests() : juce::UnitTest("JunoSubOsc Tests", "JunoDSP") {}

    void runTest() override {
        beginTest("Frequency and Phase Alignment");
        JunoDCO dco;
        dco.prepare(44100.0, 512);
        dco.setFrequency(440.0f);
        dco.setSawLevel(0.0f);
        dco.setPulseLevel(0.0f);
        dco.setSubLevel(1.0f);

        // Run for a few cycles
        float lastVal = 0.0f;
        int toggles = 0;
        const int samples = 1000;
        for (int i = 0; i < samples; ++i) {
            float val = dco.getNextSample(0.0f);
            // Sign flip detection is more robust against PolyBLEP smoothing
            if ((val > 0.0f && lastVal < 0.0f) || (val < 0.0f && lastVal > 0.0f)) {
                toggles++;
            }
            lastVal = val;
        }

        // 440Hz at 44.1kHz -> ~100 samples per DCO cycle.
        // In 1000 samples, ~10 DCO cycles.
        // Sub-osc should toggle once per DCO cycle reset.
        // Wait, if it's -1 octave, it toggles every 1.0 phase reset.
        // DCO cycle 1: Reset -> toggle High.
        // DCO cycle 2: Reset -> toggle Low.
        // Total toggles in 10 cycles should be 10.
        expect(toggles >= 9 && toggles <= 11, "Sub-Osc should toggle strictly at every DCO phase reset");
    }
};

class JunoNoiseTests : public juce::UnitTest {
public:
    JunoNoiseTests() : juce::UnitTest("JunoNoise Tests", "JunoDSP") {}

    void runTest() override {
        beginTest("Noise Mixing Ratio (RMS Scale)");
        JunoDCO dco;
        dco.prepare(44100.0, 512);
        
        // Measure Sawtooth RMS for reference
        dco.setFrequency(440.0f);
        dco.setSawLevel(1.0f);
        dco.setPulseLevel(0.0f);
        dco.setSubLevel(0.0f);
        dco.setNoiseLevel(0.0f);
        
        float sawSumSq = 0.0f;
        const int samples = 4410; // 100ms
        for (int i = 0; i < samples; ++i) {
            float s = dco.getNextSample(0.0f);
            sawSumSq += s * s;
        }
        float sawRMS = std::sqrt(sawSumSq / samples);
        
        // Measure Noise RMS at max level
        dco.setSawLevel(0.0f);
        dco.setNoiseLevel(1.0f);
        float noiseSumSq = 0.0f;
        for (int i = 0; i < samples; ++i) {
            float n = dco.getNextSample(0.0f);
            noiseSumSq += n * n;
        }
        float noiseRMS = std::sqrt(noiseSumSq / samples);
        
        // Expected ratio: noiseRMS should be approximately 0.6 * sawRMS.
        // Pure Saw RMS (peak 1.0) is 1/sqrt(3) ~= 0.577.
        // Pure White Noise RMS (peak 1.0) is 1/sqrt(3) ~= 0.577.
        // With kNoiseAmpScale = 0.6, we expect noiseRMS ratio to be ~0.6 (before filtering).
        // The LPF at 12kHz will slightly reduce RMS. 
        float ratio = noiseRMS / sawRMS;
        expect(ratio > 0.4f && ratio < 0.7f, "Noise level should be balanced correctly relative to Sawtooth");
    }
};
