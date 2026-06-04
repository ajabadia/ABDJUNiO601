#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "JunoSysExEngine.h"
#include "SynthParams.h"

class JunoSysExTests : public juce::UnitTest {
public:
    JunoSysExTests() : juce::UnitTest("JunoSysEx RoundTrip", "JunoSysEx") {}

    void runTest() override {
        beginTest("PatchDump round-trip preserves all params");

        SynthParams original;
        // Set some recognizable values
        original.vcfFreq = 0.75f;
        original.resonance = 0.5f;
        original.attack = 0.1f;
        original.decay = 0.2f;
        original.chorus1 = true;
        original.hpfFreq = 2;
        original.dcoRange = 0; // 4'
        original.vcaMode = 0; // ENV (in original internal mapping: 0=ENV, 1=GATE)

        JunoSysExEngine engine;
        auto msg = engine.makePatchDump(0, original);
        
        SynthParams recovered;
        engine.handleIncomingSysEx(msg, recovered);

        expect(recovered.isSamePatch(original), "Recovered patch should match original (within SysEx precision)");
    }
};
