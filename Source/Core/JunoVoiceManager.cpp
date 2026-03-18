#include "JunoVoiceManager.h"

JunoVoiceManager::JunoVoiceManager() {
    for (auto& ts : voiceTimestamps) ts.store(0);
}

void JunoVoiceManager::prepare(double sampleRate, int maxBlockSize) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].prepare(sampleRate, maxBlockSize);
        voices[i].setVoiceIndex(i); // [Fidelidad] Assign physical index for Unison Detune
    }
}

void JunoVoiceManager::updateParams(const SynthParams& params) {
    for (auto& voice : voices) {
        voice.updateParams(params);
    }
}

void JunoVoiceManager::forceUpdate() {
    for (auto& voice : voices) {
        voice.forceUpdate();
    }
}

void JunoVoiceManager::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer) {
    static bool firstRender = true;
    if (firstRender) { DBG("JunoVoiceManager::renderNextBlock FIRST CALL"); firstRender = false; }
    
    const juce::ScopedLock sl(lock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive()) {
            float neighborOut = voices[(i + 1) % MAX_VOICES].lastActiveOutputLevel(); 
            voices[i].renderNextBlock(buffer, startSample, numSamples, lfoBuffer, neighborOut);
        }
    }
}

void JunoVoiceManager::setPolyMode(int mode) {
    if (polyMode != mode) {
        polyMode = mode;
        resetAllVoices(); // [Fidelidad] Limpiar todo al cambiar modo
    }
}

void JunoVoiceManager::noteOn(int /*midiChannel*/, int midiNote, float velocity) {
    const juce::ScopedLock sl(lock);
    currentTimestamp++;
    
    // UNISON (Mode 3): Trigger all 6 voices
    if (polyMode == 3) {
        bool isLegatoTransition = isAnyNoteHeld();
        for (int i = 0; i < MAX_VOICES; ++i) {
            voices[i].noteOn(midiNote, velocity, isLegatoTransition);
            voiceTimestamps[i].store(currentTimestamp.load());
        }
        lastAllocatedVoiceIndex.store(0);
        return;
    }

    // POLY (Mode 1 & 2): Allocation logic for single voice
    // 1. Buscar si la nota ya está sonando para hacer Retrigger
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive() && voices[i].getCurrentNote() == midiNote) {
            voices[i].noteOn(midiNote, velocity, false); 
            voiceTimestamps[i].store(currentTimestamp.load());
            lastAllocatedVoiceIndex.store(i);
            return;
        }
    }

    // 2. Buscar una voz libre
    int voiceIndex = findFreeVoiceIndex();

    // 3. Si no hay libres, robar la más antigua
    if (voiceIndex == -1) {
        voiceIndex = findVoiceToSteal();
    }
    
    if (voiceIndex != -1) {
        voices[voiceIndex].noteOn(midiNote, velocity, false);
        voiceTimestamps[voiceIndex].store(currentTimestamp.load());
        lastAllocatedVoiceIndex.store(voiceIndex);
    }
}

void JunoVoiceManager::noteOff(int /*midiChannel*/, int midiNote, float /*velocity*/) {
    const juce::ScopedLock sl(lock);
    if (polyMode == 3) { // UNISON
        for (int i = 0; i < MAX_VOICES; ++i) {
             if (voices[i].getCurrentNote() == midiNote) voices[i].noteOff();
        }
        return;
    }

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive() && voices[i].getCurrentNote() == midiNote) {
            voices[i].noteOff();
            return;
        }
    }
}

// [Fidelidad] Static counter REMOVED in favor of instance member 'nextPoly1Index'

int JunoVoiceManager::findFreeVoiceIndex() {
    // Poly 1: Round Robin (Cyclic Fixed Pattern)
    // [Fidelidad] Hardware 8253 Pattern: {0, 2, 4, 1, 3, 5}
    static const int voiceOrder[MAX_VOICES] = {0, 2, 4, 1, 3, 5};

    if (polyMode == 1) {
        // [Audit Fix] Cyclic search pattern independent of last allocation
        // We start searching from the next pattern index.
        int startIndex = nextPoly1Index;
        for (int i = 0; i < MAX_VOICES; ++i) {
            int currentPatternIdx = (startIndex + i) % MAX_VOICES;
            int voiceIdx = voiceOrder[currentPatternIdx];
            
            if (!voices[voiceIdx].isActive()) {
                // If found, we advance the cycle for the NEXT allocation
                nextPoly1Index = (currentPatternIdx + 1) % MAX_VOICES; 
                return voiceIdx;
            }
        }
        // If all full, nextPoly1Index stays or advances? 
        // Hardware maintains the cycle. Optimization: Don't advance on fail? 
        // Actually, if we fail here, we go to stealing. 
        // Stealing might pick ANY voice. The cycle should probably continue from where it left off.
        // We leave nextPoly1Index as is.
    }
    // Poly 2: Static Allocation (Lowest Available Index)
    else if (polyMode == 2 || polyMode == 3) {
         for (int i = 0; i < MAX_VOICES; ++i) {
            if (!voices[i].isActive()) return i;
        }
    }
    // Fallback or Mono modes (handled by Poly 2 logic effectively for allocation)
    return -1;
}

int JunoVoiceManager::findVoiceToSteal() {
    // Poly 2: Low Note Priority (Steal Highest Note to preserve Bass)
    // Actually "Last Note Priority" usually means steal the oldest? 
    // But Audit Req: "Low-Note Priority (Steal Highest)". 
    if (polyMode == 2) {
        int highestNote = -1;
        int stealIdx = -1;
        
        // Find highest note to kill (preserve bass)
        for (int i=0; i<MAX_VOICES; ++i) {
             // Only considering active voices
             if (voices[i].isActive()) {
                 int note = voices[i].getCurrentNote();
                 // If notes are equal, fallback to age?
                 if (note > highestNote) {
                     highestNote = note;
                     stealIdx = i;
                 }
             }
        }
        if (stealIdx != -1) return stealIdx;
    }

    // Poly 1 & Unison: Steal Oldest (Standard)
    int oldestIndex = -1;
    uint64_t minTimestamp = UINT64_MAX;
    
    // Priority 1: Steal Release phase first
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive() && !voices[i].isGateOnActive()) { 
             if (voiceTimestamps[i] < minTimestamp) {
                minTimestamp = voiceTimestamps[i];
                oldestIndex = i;
            }
        }
    }
    
    // Priority 2: Steal Oldest Active
    if (oldestIndex == -1) {
         for (int i = 0; i < MAX_VOICES; ++i) {
            if (voices[i].isActive()) { 
                 if (voiceTimestamps[i] < minTimestamp) {
                    minTimestamp = voiceTimestamps[i];
                    oldestIndex = i;
                }
            }
        }
    }
    
    // [Fidelidad] If we steal for Poly 1, should we update nextPoly1Index?
    // The cycle logic is for *finding free*. Stealing is separate.
    return oldestIndex;
}


void JunoVoiceManager::outputActiveVoiceInfo() {
    juce::String state;
    for (int i = 0; i < MAX_VOICES; ++i) {
        state += "[" + juce::String(i) + ":" + (voices[i].isActive() ? juce::String(voices[i].getCurrentNote()) : ".") + "] ";
    }
    DBG("Voices: " << state);
}

void JunoVoiceManager::setAllNotesOff() {
    for (auto& voice : voices) voice.noteOff();
}

bool JunoVoiceManager::anyVoiceActive() const {
    for (const auto& v : voices) if (v.isActive()) return true;
    return false;
}

void JunoVoiceManager::setBenderAmount(float v) {
    for (auto& voice : voices) voice.setBender(v);
}
void JunoVoiceManager::setPortamentoEnabled(bool b) {
    for (auto& voice : voices) voice.setPortamentoEnabled(b);
}
void JunoVoiceManager::setPortamentoTime(float v) {
    for (auto& voice : voices) voice.setPortamentoTime(v);
}
void JunoVoiceManager::setPortamentoLegato(bool b) {
    for (auto& voice : voices) voice.setPortamentoLegato(b);
}
