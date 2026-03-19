#include "JunoVoiceManager.h"

JunoVoiceManager::JunoVoiceManager() {
    currentActiveVoices.store(8);
    for (auto& ts : voiceTimestamps) ts.store(0);
}

void JunoVoiceManager::prepare(double sampleRate, int maxBlockSize) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].prepare(sampleRate, maxBlockSize);
        voices[i].setVoiceIndex(i); // [Fidelidad] Assign physical index for Unison Detune
    }
}

void JunoVoiceManager::updateParams(const SynthParams& params) {
    currentActiveVoices.store(juce::jlimit(1, (int)MAX_VOICES, params.numVoices));
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
    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive()) {
            float neighborOut = voices[(i + 1) % currentActiveVoices].lastActiveOutputLevel(); 
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
    
    // UNISON (Mode 3): Trigger all voices within the limit
    if (polyMode == 3) {
        bool isLegatoTransition = isAnyNoteHeld();
        for (int i = 0; i < currentActiveVoices; ++i) {
            voices[i].noteOn(midiNote, velocity, isLegatoTransition);
            voiceTimestamps[i].store(currentTimestamp.load());
        }
        lastAllocatedVoiceIndex.store(0);
        return;
    }

    // POLY (Mode 1 & 2): Allocation logic for single voice
    // 1. Buscar si la nota ya está sonando para hacer Retrigger
    for (int i = 0; i < currentActiveVoices; ++i) {
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
        for (int i = 0; i < currentActiveVoices; ++i) {
             if (voices[i].getCurrentNote() == midiNote) voices[i].noteOff();
        }
        return;
    }

    for (int i = 0; i < currentActiveVoices; ++i) {
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
        int startIndex = nextPoly1Index;
        for (int i = 0; i < currentActiveVoices; ++i) {
            int currentPatternIdx = (startIndex + i) % currentActiveVoices;
            int voiceIdx = voiceOrder[currentPatternIdx];
            
            if (!voices[voiceIdx].isActive()) {
                nextPoly1Index = (currentPatternIdx + 1) % currentActiveVoices; 
                return voiceIdx;
            }
        }
    }
    // Poly 2: Static Allocation (Lowest Available Index)
    else if (polyMode == 2 || polyMode == 3) {
         for (int i = 0; i < currentActiveVoices; ++i) {
            if (!voices[i].isActive()) return i;
        }
    }
    return -1;
}

int JunoVoiceManager::findVoiceToSteal() {
    // Poly 2: Low Note Priority (Steal Highest Note to preserve Bass)
    if (polyMode == 2) {
        int highestNote = -1;
        int stealIdx = -1;
        
        for (int i=0; i < currentActiveVoices; ++i) {
             if (voices[i].isActive()) {
                 int note = voices[i].getCurrentNote();
                 if (note > highestNote) { highestNote = note; stealIdx = i; }
             }
        }
        if (stealIdx != -1) return stealIdx;
    }

    // Poly 1 & Unison: Steal Oldest (Standard)
    int oldestIndex = -1;
    uint64_t minTimestamp = UINT64_MAX;
    
    // Priority 1: Steal Release phase first
    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive() && !voices[i].isGateOnActive()) { 
             if (voiceTimestamps[i] < minTimestamp) { minTimestamp = voiceTimestamps[i]; oldestIndex = i; }
        }
    }
    
    // Priority 2: Steal Oldest Active
    if (oldestIndex == -1) {
         for (int i = 0; i < currentActiveVoices; ++i) {
            if (voices[i].isActive()) { 
                 if (voiceTimestamps[i] < minTimestamp) { minTimestamp = voiceTimestamps[i]; oldestIndex = i; }
            }
        }
    }
    return oldestIndex;
}


void JunoVoiceManager::outputActiveVoiceInfo() {
    juce::String state;
    for (int i = 0; i < currentActiveVoices; ++i) state += "[" + juce::String(i) + ":" + (voices[i].isActive() ? juce::String(voices[i].getCurrentNote()) : ".") + "] ";
    DBG("Voices: " << state);
}

void JunoVoiceManager::setAllNotesOff() { for (auto& voice : voices) voice.noteOff(); }

bool JunoVoiceManager::anyVoiceActive() const {
    for (const auto& v : voices) if (v.isActive()) return true;
    return false;
}

void JunoVoiceManager::setBenderAmount(float v) { for (auto& voice : voices) voice.setBender(v); }
void JunoVoiceManager::setPortamentoEnabled(bool b) { for (auto& voice : voices) voice.setPortamentoEnabled(b); }
void JunoVoiceManager::setPortamentoTime(float v) { for (auto& voice : voices) voice.setPortamentoTime(v); }
void JunoVoiceManager::setPortamentoLegato(bool b) { for (auto& voice : voices) voice.setPortamentoLegato(b); }
