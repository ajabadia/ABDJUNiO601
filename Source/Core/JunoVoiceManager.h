#pragma once

#include <JuceHeader.h>
#include "../Synth/Voice.h"
#include "SynthParams.h"
#include <array>
#include <atomic>

/**
 * JunoVoiceManager
 * Handles the allocation and lifecycle of voices.
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
    void forceUpdate(); 
    
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

    float getTotalEnvelopeLevel() const {
        float sum = 0.0f;
        for (int i = 0; i < currentActiveVoices; ++i) if (voices[i].isActive()) sum += voices[i].lastActiveOutputLevel();
        return sum;
    }

    int getActiveVoiceCount() const {
        int count = 0;
        for (int i = 0; i < currentActiveVoices; ++i) if (voices[i].isActive()) count++;
        return count;
    }

    bool isAnyNoteHeld() const {
        for (int i = 0; i < currentActiveVoices; ++i) if (voices[i].isGateOnActive()) return true;
        return false;
    }

private:
    static constexpr int MAX_VOICES = 16;
    std::atomic<int> currentActiveVoices;
    std::array<Voice, MAX_VOICES> voices;
    
    std::array<std::atomic<uint64_t>, MAX_VOICES> voiceTimestamps;
    std::atomic<uint64_t> currentTimestamp {0};
    
    std::atomic<int> lastAllocatedVoiceIndex {-1}; 
    std::atomic<int> polyMode {1}; 
    
    bool anyVoiceActive() const;
    int findFreeVoiceIndex();
    int findVoiceToSteal();

    juce::CriticalSection lock;
    int nextPoly1Index = 0; 
};
