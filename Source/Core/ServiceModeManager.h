#pragma once

#include <JuceHeader.h>
#include <atomic>

class ABDSimpleJuno106AudioProcessor;

/**
 * ServiceModeManager - Diagnostic and calibration routines.
 * Aligned with 0002.txt Service Mode requirements.
 */
class ServiceModeManager
{
public:
    ServiceModeManager(ABDSimpleJuno106AudioProcessor& p);
    
    // Diagnostic Controls
    void setVoiceSolo(int voiceIndex); // -1 for normal poly
    void clearVoiceSolo() { soloVoice = -1; }
    
    void triggerDCOReference(int midiNote, int rangeIndex);
    void startVCFSweep();
    void startHpfCycle();
    void startChorusCycle();
    void startTestScale();
    void startAutoVcfTune();
    void stopAllTests();

    // State Access
    int getSoloVoice() const { return soloVoice; }
    bool isVcfSweepActive() const { return vcfSweepActive.load(); }
    float getVcfSweepCutoff() const { return vcfSweepValue.load(); }
    bool isTestScaleActive() const { return testScaleActive.load(); }
    bool isAutoTuning() const { return autoVcfTuneActive.load(); }
    
    int getHpfCyclePos() const { return hpfCycleActive.load() ? hpfCycleValue.load() : -1; }
    int getChorusCycleMode() const { return chorusCycleActive.load() ? chorusCycleValue.load() : -1; }

    // Processing (called from PluginProcessor::processBlock)
    void update(double sampleRate, int numSamples);

private:
    ABDSimpleJuno106AudioProcessor& processor;
    
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
    
    // HPF Cycle State
    std::atomic<bool> hpfCycleActive {false};
    std::atomic<int> hpfCycleValue {0};
    double hpfCycleTimer = 0.0;

    // Chorus Cycle State
    std::atomic<bool> chorusCycleActive {false};
    std::atomic<int> chorusCycleValue {0};
    double chorusCycleTimer = 0.0;

    // Auto-Tuning State
    std::atomic<bool> autoVcfTuneActive {false};
    float autoTuneStep = 0.0f;
    int autoTuneCycle = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServiceModeManager)
};
