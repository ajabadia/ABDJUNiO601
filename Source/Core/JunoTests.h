#pragma once

#include <JuceHeader.h>

class PresetManager;

/**
 * [Sprint 10 Robustness] Juno-106 Fidelity Tests
 */
namespace JunoTests
{
    // Test: ValueTree -> Bytes -> ValueTree (18-byte roundtrip)
    void runJunoPatchRoundtripTest (PresetManager& pm);
    void runJunoPatchRoundtripTest (PresetManager& pm, int& failuresOut);
    void runJunoPatchRoundtripTest (PresetManager& pm, int& failuresOut, juce::StringArray& failedNamesOut);

    // Test: SynthParams -> makePatchDump -> applyPatchDump -> SynthParams (SysEx roundtrip)
    void runSysExPatchDumpRoundtripTest();
    void runSysExPatchDumpRoundtripTest (bool& okOut);

    // Test: Library -> JSON -> Library (Structure & ValueTree roundtrip)
    void runPresetJsonRoundtripTest (PresetManager& pm);
    void runPresetJsonRoundtripTest (PresetManager& pm, bool& okOut);
}
