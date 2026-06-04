#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/JunoADSR.h"
#include "BaseClass/ADSRGeneric.h"

class JunoADSRTests : public juce::UnitTest {
public:
    JunoADSRTests() : juce::UnitTest("JunoADSR Tests", "JunoDSP") {}

    void runTest() override {
        beginTest("ADSR Lifecycle (Juno Authentic)");
        
        JunoADSR adsr;
        adsr.setSampleRate(44100.0);
        
        // Initial state
        expect(!adsr.isActive());
        
        // Note On
        adsr.noteOn();
        expect(adsr.isActive());
    }
};

class JunoADSRTimingTest : public juce::UnitTest {
public:
    JunoADSRTimingTest() : juce::UnitTest("Juno ADSR Timing", "JunoDSP") {}

    void runTest() override {
        beginTest("Attack timing behaves monotonically");

        const double sampleRate = 44100.0;
        JunoADSR env;
        env.setSampleRate(sampleRate);

        env.setAttack(0.01f);
        env.setDecay(0.30f);
        env.setSustain(0.5f);
        env.setRelease(0.20f);

        env.noteOn();

        const int maxSamples = (int)(0.1 * sampleRate);
        float last = -1.0f;
        int peakValue = 0;

        for (int i = 0; i < maxSamples; ++i) {
            auto v = env.getNextSample();
            // During the Attack phase, the envelope must never decrease
            // (the 1e-4 tolerance covers floating-point interpolation noise
            //  between MCU ticks in J106 mode).
            expect(v >= last - 1e-4f);
            
            // Use stage detection to break at attack→decay transition.
            // We cannot rely on v >= 1.0f because in J106 mode the linear
            // interpolation between the final attack tick (mEnvNext=1.0f)
            // and the previous tick value never reaches exactly 1.0f before
            // the next MCU tick fires and transitions to Decay, causing
            // the loop to run through all maxSamples and fail on every
            // decay sample.
            if (env.getCurrentStage() != JunoADSR::Stage::Attack)
                break;
            
            last = v;
            peakValue = i;
        }
        
        // The envelope should have entered the Attack phase at all
        expect(peakValue > 0, "Envelope should have started attacking");
        // By the time attack completes, the envelope should be near 1.0
        expect(last > 0.9f,
               "Envelope should reach near 1.0 by end of attack, got " + juce::String(last, 4));
    }
};
