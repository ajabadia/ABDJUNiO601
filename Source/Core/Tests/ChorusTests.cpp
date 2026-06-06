#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/ChorusBBD.h"

class ChorusBBDTests : public juce::UnitTest {
public:
    ChorusBBDTests() : juce::UnitTest("ChorusBBD Tests", "JunoDSP") {}

    void runTest() override {
        // Use a dynamic signal (rising saw) so that different L/R delays
        // produce different output samples — constant/DC input makes
        // Hermite interpolation return the same value regardless of delay.
        auto makeDynamicSignal = [](juce::AudioBuffer<float>& buf) {
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample(0, i, std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f));
            // Copy to R so we start with identical channels
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample(1, i, buf.getSample(0, i));
        };

        beginTest("Stereo Phase Relationship");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::ChorusI);
            chorus.setMix(1.0f);

            juce::AudioBuffer<float> buffer(2, 2048);
            buffer.clear();
            makeDynamicSignal(buffer);

            chorus.process(buffer);

            // In Chorus I, L/R should be different (antiphase LFO produces
            // different delay modulation on each channel)
            bool anyDifference = false;
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                if (std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) > 1e-4f) {
                    anyDifference = true;
                    break;
                }
            }
            expect(anyDifference, "Chorus I should produce stereo difference via antiphase LFO");
        }

        beginTest("Saturation & Filtering");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::ChorusI);
            chorus.setMix(1.0f);  // Full wet — no dry signal to mask saturation

            // Very hot signal to trigger BBD tanh saturation
            juce::AudioBuffer<float> buffer(1, 2048);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(0, i, 10.0f);

            chorus.process(buffer);

            // Output should be well below 10.0 due to BBD tanh soft clipping
            float peak = 0.0f;
            for (int i = 200; i < buffer.getNumSamples(); ++i)  // skip first samples (delay line filling)
                peak = std::max(peak, std::abs(buffer.getSample(0, i)));

            // tanh(10*0.12)/0.12 ≈ 6.95, then wet gain 1.257 → ~8.7.
            // With full wet and DC input, BBD output ≈ 1.257 * tanh(1.2) / 0.12 ≈ 8.74
            // This should be noticeably less than 10.0 (input amplitude)
            expect(peak < 9.5f, "Chorus should saturate hot signals (peak should be < 9.5)");
            expect(peak > 0.1f, "Chorus should still produce output (not silent)");
        }
    }
};
