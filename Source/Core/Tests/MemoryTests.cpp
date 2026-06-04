#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "PresetManager.h"

class JunoMemoryTests : public juce::UnitTest {
public:
    JunoMemoryTests() : juce::UnitTest("Juno Memory Tests", "JunoIO") {}

    void runTest() override {
        beginTest("Internal RAM Initialization");
        PresetManager pm;
        int ramIdx = pm.getLibraryIndex("INTERNAL RAM");
        expect(ramIdx >= 0, "INTERNAL RAM should be created on initialization");
        
        const auto& ram = pm.getLibrary(ramIdx);
        expect(ram.patches.size() == 128, "INTERNAL RAM should contain 128 slots");
        expect(ram.patches[0].category == "Internal", "RAM patches should be tagged as Internal");

        beginTest("Write to Internal Slot");
        juce::ValueTree newState("Parameters");
        newState.setProperty("vcfFreq", 0.123f, nullptr);
        
        // Write to Group 0, Bank 1, Patch 1 (Index 0)
        auto res = pm.writeToInternalSlot(0, 1, 1, newState);
        expect(res.wasOk(), "Write to slot should succeed");
        
        const auto& updatedPatch = pm.getLibrary(ramIdx).patches[0];
        expect((float)updatedPatch.state.getProperty("vcfFreq") == 0.123f, "State should be updated in memory");
    }
};
