#include <JuceHeader.h>
#include "../Synth/JunoDCO.h"
#include "../Synth/JunoADSR.h"
#include "JunoConstants.h"

/**
 * JunoUnitTests
 * 
 * Initial unit tests for core DSP components.
 */
class JunoDCOTests : public juce::UnitTest {
public:
    JunoDCOTests() : juce::UnitTest("JunoDCO Tests", "DSP") {}

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

class JunoADSRTests : public juce::UnitTest {
public:
    JunoADSRTests() : juce::UnitTest("JunoADSR Tests", "DSP") {}

    void runTest() override {
        beginTest("ADSR Lifecycle");
        
        JunoADSR adsr;
        adsr.setSampleRate(44100.0);
        
        // Initial state
        expect(!adsr.isActive());
        
        // Note On
        adsr.noteOn();
        expect(adsr.isActive());
        
        // Initial sample should be near 0 (attack start)
        float firstSample = adsr.getNextSample();
        expect(firstSample >= 0.0f);
        
        // Note Off
        adsr.noteOff();
        // Since attack/decay/release are defaults, it might still be active
        expect(adsr.isActive());
    }
};

// Static instances to register tests
static JunoDCOTests dcoTests;
static JunoADSRTests adsrTests;
