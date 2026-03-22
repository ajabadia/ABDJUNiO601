#pragma once

#include <JuceHeader.h>
#include "../Synth/Voice.h"
#include "../ABD-SynthEngine/Core/Voices/VoiceAllocator.h"
#include "SynthParams.h"
#include <array>
#include <atomic>

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
    void forceUpdate(); // [Fix] Instant parameter update for patch load
    
    void setPolyMode(int mode); 
    int getLastTriggeredVoiceIndex() const { return lastAllocatedVoiceIndex; }
    void setAllNotesOff();
    
    void setBenderAmount(float v);
    void setPortamentoEnabled(bool b);
    void setPortamentoTime(float v);
    void setPortamentoLegato(bool b);
    void setTuningTable(const float* table) {
        for (auto& v : allocator.getVoices()) v.setTuningTable(table);
    }
    
    void resetAllVoices() {
        allocator.reset();
        setAllNotesOff();
    }

    void setSoloVoice(int voiceIndex) const { soloVoice.store(voiceIndex); }

    float getTotalEnvelopeLevel() const {
        float sum = 0.0f;
        auto& voices = allocator.getVoices();
        for (const auto& v : voices) if (v.isActive()) sum += v.lastActiveOutputLevel();
        return sum;
    }

    int getActiveVoiceCount() const {
        return allocator.getNumActiveVoices();
    }

    bool hideIsAnyNoteHeld() const { // internal helper
        auto& voices = allocator.getVoices();
        for (const auto& v : voices) if (v.isGateOnActive()) return true;
        return false;
    }

    bool isAnyNoteHeld() const {
        return hideIsAnyNoteHeld();
    }

private:
    static constexpr int MAX_VOICES = 16;
    std::atomic<int> currentActiveVoices;
    
    ABD::VoiceAllocator<Voice, MAX_VOICES> allocator;
    
    // For legacy tracking until full migration to VoiceAllocator internals
    std::array<std::atomic<uint64_t>, MAX_VOICES> voiceTimestamps;
    std::atomic<uint64_t> currentTimestamp {0};
    
    std::atomic<int> lastAllocatedVoiceIndex {-1}; 
    std::atomic<int> polyMode {1}; 
    
    bool anyVoiceActive() const;
    int findFreeVoiceIndex();
    int findVoiceToSteal();

    mutable std::atomic<int> soloVoice{ -1 }; // [Fidelidad] -1 = all voices, 0-5 = solo specific voice

    juce::CriticalSection lock;
    int nextPoly1Index = 0; // [Fidelidad] Authentic Cyclic allocation state
};

