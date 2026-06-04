#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/ChorusBBD.h"

class ChorusBBDTests : public juce::UnitTest {
public:
    ChorusBBDTests() : juce::UnitTest("ChorusBBD Tests", "JunoDSP") {}

    void runTest() override {
        beginTest("Stereo Phase Relationship");
        ChorusBBD chorus;
        chorus.prepare(44100.0, 512);
        chorus.setMode(ChorusBBD::Mode::ChorusI);
        chorus.setMix(1.0f);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        // Constant input to observe LFO modulation in delay lines
        for (int i = 0; i < 512; ++i) {
            buffer.setSample(0, i, 1.0f);
            buffer.setSample(1, i, 1.0f);
        }

        chorus.process(buffer);

        // In Chorus I, L/R should be different (antiphase LFO)
        bool anyDifference = false;
        for (int i = 0; i < 512; ++i) {
            if (std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) > 1e-4f) {
                anyDifference = true;
                break;
            }
        }
        expect(anyDifference, "Chorus I should produce stereo difference via antiphase LFO");

        beginTest("Saturation & Filtering");
        chorus.reset();
        chorus.setMode(ChorusBBD::Mode::ChorusI);
        
        // Very hot signal to trigger saturation
        buffer.clear();
        for (int i = 0; i < 512; ++i) buffer.setSample(0, i, 10.0f);
        
        chorus.process(buffer);
        
        // Output should be limited by tanh
        float maxVal = 0.0f;
        for (int i = 0; i < 512; ++i) maxVal = std::max(maxVal, std::abs(buffer.getSample(0, i)));
        
        // std::tanh(10 * 1.2) is ~1.0. Even with dry mix, it shouldn't be 10.
        expect(maxVal < 5.0f, "Chorus should saturate hot signals");
    }
};
