#pragma once

#include <JuceHeader.h>
#include "../Synth/Voice.h"
#include "SynthParams.h"
#include <array>

/**
 * JunoVoiceManager
 * 
 * Handles the allocation and lifecycle of 6 fixed voices.
 */
class JunoVoiceManager {
public:
    JunoVoiceManager();
    
    void prepare(double sampleRate, int maxBlockSize);
    
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer);
    
    void noteOn(int midiChannel, int midiNote, float velocity);
    void noteOff(int midiChannel, int midiNote, float velocity);
    void outputActiveVoiceInfo(); 
    
    void updateParams(const SynthParams& params);
    
    void setPolyMode(int mode); 
    int getLastTriggeredVoiceIndex() const { return lastAllocatedVoiceIndex; }
    void setAllNotesOff();
    
    void setBenderAmount(float v);
    void setPortamentoEnabled(bool b);
    void setPortamentoTime(float v);
    void setPortamentoLegato(bool b);
    
    void resetAllVoices() {
        for (auto& v : voices) v.forceStop();
        setAllNotesOff();
    }

    // [Fidelidad] Helper to check if any key is physically held down
    bool isAnyNoteHeld() const {
        for (const auto& v : voices) if (v.isGateOnActive()) return true;
        return false;
    }

private:
    static constexpr int MAX_VOICES = 6;
    std::array<Voice, MAX_VOICES> voices;
    
    std::array<uint64_t, MAX_VOICES> voiceTimestamps;
    uint64_t currentTimestamp = 0;
    
    int lastAllocatedVoiceIndex = -1; 
    int polyMode = 1; 
    
    bool anyVoiceActive() const;
    int findFreeVoiceIndex();
    int findVoiceToSteal();
};
