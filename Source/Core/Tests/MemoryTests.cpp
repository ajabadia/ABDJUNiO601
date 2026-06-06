#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "PresetManager.h"

class JunoMemoryTests : public juce::UnitTest {
public:
    JunoMemoryTests() : juce::UnitTest("Juno Memory Tests", "JunoIO") {}

    void runTest() override {
        beginTest("Internal RAM Initialization");
        {
            PresetManager pm;
            int ramIdx = pm.getLibraryIndex("C - Internal RAM");
            expect(ramIdx >= 0, "C - Internal RAM should be present after initialization");
            
            if (ramIdx >= 0) {
                const auto& ram = pm.getLibrary(ramIdx);
                expect(ram.patches.size() == PresetManager::kMaxPatchesPerLibrary,
                       juce::String("Internal RAM should contain ") + juce::String(PresetManager::kMaxPatchesPerLibrary) + " slots");
                expect(ram.patches[0].category == "RAM",
                       "RAM patches should be tagged as RAM");
            }
        }

        beginTest("Write to Internal Slot");
        {
            PresetManager pm;
            int ramIdx = pm.getLibraryIndex("C - Internal RAM");
            expect(ramIdx >= 0, "Internal RAM must exist for write test");
            if (ramIdx < 0) return;

            juce::ValueTree newState("Parameters");
            newState.setProperty("vcfFreq", 0.123f, nullptr);

            // Write to Group 0, Bank 1, Patch 1 (Index 0)
            juce::Result res = pm.writeToInternalSlot(0, 1, 1, newState);
            expect(res.wasOk(), "Write to slot should succeed: " + res.getErrorMessage());

            if (res.wasOk()) {
                const auto& updatedPatch = pm.getLibrary(ramIdx).patches[0];
                float vcfFreq = (float)updatedPatch.state.getProperty("vcfFreq", -1.0f);
                expect(vcfFreq == 0.123f,
                       juce::String("State should be updated in memory (got ") + juce::String(vcfFreq) + ")");
            }
        }
    }
};
