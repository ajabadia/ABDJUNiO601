#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/JunoVCF.h"

class JunoVCFTests : public juce::UnitTest {
public:
    JunoVCFTests() : juce::UnitTest("JunoVCF Tests", "JunoDSP") {}

    void runTest() override {
        beginTest("Exponential Mapping");
        JunoVCF vcf;
        vcf.setSampleRate(44100.0);
        
        // Manual verification of internal method computeCutoffHz would be ideal
        // but it's private. We'll test via processSample stability.
        float out = vcf.processSample(0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 440.0f);
        expect(!std::isnan(out));

        beginTest("Auto-oscillation stability");
        vcf.reset();
        // Provide a tiny impulse to kickstart the feedback loop
        vcf.processSample(1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 440.0f);

        // High resonance should produce audio even with zero input after kickstart
        float maxVal = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            float s = vcf.processSample(0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 440.0f);
            maxVal = std::max(maxVal, std::abs(s));
        }
        expect(maxVal > 0.001f, "VCF should auto-oscillate at max resonance after kickstart");
    }
};
