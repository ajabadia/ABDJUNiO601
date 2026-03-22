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
    void stopAllTests();

    // State Access
    int getSoloVoice() const { return soloVoice; }
    bool isVcfSweepActive() const { return vcfSweepActive; }
    float getVcfSweepCutoff() const { return vcfSweepValue; }

    // Processing (called from PluginProcessor::processBlock)
    void update(double sampleRate, int numSamples);

private:
    SimpleJuno106AudioProcessor& processor;
    
    std::atomic<int> soloVoice{ -1 };
    
    // VCF Sweep State
    bool vcfSweepActive = false;
    float vcfSweepValue = 0.0f;
    double vcfSweepPhase = 0.0;
    
    // DCO Reference State
    bool dcoRefActive = false;
    int dcoRefNote = 69;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServiceModeManager)
};
