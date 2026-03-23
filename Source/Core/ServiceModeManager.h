#pragma once

#include <JuceHeader.h>
#include <atomic>

class SimpleJuno106AudioProcessor;

/**
 * ServiceModeManager - Diagnostic and calibration routines.
 * Aligned with 0002.txt Service Mode requirements.
 */
class ServiceModeManager
{
public:
    ServiceModeManager(SimpleJuno106AudioProcessor& p);
    
    // Diagnostic Controls
    void setVoiceSolo(int voiceIndex); // -1 for normal poly
    void clearVoiceSolo() { soloVoice = -1; }
    
    void triggerDCOReference(int midiNote, int rangeIndex);
    void startVCFSweep();
    void startTestScale();
    void stopAllTests();

    // State Access
    int getSoloVoice() const { return soloVoice; }
    bool isVcfSweepActive() const { return vcfSweepActive.load(); }
    float getVcfSweepCutoff() const { return vcfSweepValue.load(); }
    bool isTestScaleActive() const { return testScaleActive.load(); }

    // Processing (called from PluginProcessor::processBlock)
    void update(double sampleRate, int numSamples);

private:
    SimpleJuno106AudioProcessor& processor;
    
    std::atomic<int> soloVoice;
    
    // VCF Sweep State
    std::atomic<bool> vcfSweepActive;
    std::atomic<float> vcfSweepValue;
    double vcfSweepPhase = 0.0;
    
    // DCO Reference State
    std::atomic<bool> dcoRefActive;
    int dcoRefNote = 69;

    // Test Scale State
    std::atomic<bool> testScaleActive;
    std::atomic<float> scaleTimer;
    std::atomic<int> currentScaleNoteIdx;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServiceModeManager)
};
