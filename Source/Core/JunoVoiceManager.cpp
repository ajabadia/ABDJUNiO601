#include "JunoVoiceManager.h"

namespace ABD {
JunoVoiceManager::JunoVoiceManager() {
    currentActiveVoices.store(8);
    for (auto& ts : voiceTimestamps) ts.store(0);
}

void JunoVoiceManager::prepare(double sampleRate, int maxBlockSize) {
    allocator.prepare(sampleRate, maxBlockSize);
    auto& voices = allocator.getVoices();
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].setVoiceIndex(i); // [Fidelidad] Assign physical index for Unison Detune
    }
}

void JunoVoiceManager::updateParams(const SynthParams& params) {
    int requestedVoices = params.numVoices;
    
    // [Modern] Logic: If 6+ voices, ensure even count for balanced stereo spread
    if (requestedVoices >= 6 && (requestedVoices % 2 != 0)) {
        requestedVoices--;
    }
    
    currentActiveVoices.store(juce::jlimit(1, (int)MAX_VOICES, requestedVoices));
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) {
        voice.updateParams(params);
    }
}

void JunoVoiceManager::forceUpdate() {
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) {
        voice.forceUpdate();
    }
}

void JunoVoiceManager::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer) {
    const juce::ScopedLock sl(lock);
    auto& voices = allocator.getVoices();
    int solo = soloVoice.load();

    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive()) {
            // [Service Mode] Skip voice if soloing a different one
            if (solo >= 0 && solo != i) continue;

            float neighborOut = voices[(i + 1) % currentActiveVoices].lastActiveOutputLevel(); 
            
            // Set dynamic state for agnostic render
            voices[i].setLfoBuffer(&lfoBuffer);
            voices[i].setCrosstalk(neighborOut);
            voices[i].setUnisonCount(currentActiveVoices.load());
            
            voices[i].renderNextBlock(buffer, startSample, numSamples);
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
    auto& voices = allocator.getVoices();
    
    // UNISON (Mode 3): Trigger all voices within the limit
    if (polyMode == 3) {
        bool isLegatoTransition = isAnyNoteHeld();
        for (int i = 0; i < currentActiveVoices; ++i) {
            voices[i].noteOn(midiNote, velocity, isLegatoTransition, currentActiveVoices);
            voiceTimestamps[i].store(currentTimestamp.load());
        }
        lastAllocatedVoiceIndex.store(0);
        return;
    }

    // POLY (Mode 1 & 2): Allocation logic for single voice
    // 1. Buscar si la nota ya está sonando para hacer Retrigger
    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive() && voices[i].getCurrentNote() == midiNote) {
            voices[i].noteOn(midiNote, velocity, false, 1); 
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
        if (voiceIndex != -1) voices[voiceIndex].prepareForStealing();
    }
    
    if (voiceIndex != -1) {
        voices[voiceIndex].noteOn(midiNote, velocity, false, 1);
        voiceTimestamps[voiceIndex].store(currentTimestamp.load());
        lastAllocatedVoiceIndex.store(voiceIndex);
    }
}

void JunoVoiceManager::noteOff(int /*midiChannel*/, int midiNote, float /*velocity*/) {
    const juce::ScopedLock sl(lock);
    auto& voices = allocator.getVoices();

    if (polyMode == 3) { // UNISON
        for (int i = 0; i < currentActiveVoices; ++i) {
             if (voices[i].getCurrentNote() == midiNote) voices[i].noteOff();
        }
        return;
    }

    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive() && voices[i].getCurrentNote() == midiNote) {
            voices[i].noteOff();
        }
    }
}

int JunoVoiceManager::findFreeVoiceIndex() {
    auto& voices = allocator.getVoices();
    static const int voiceOrder[MAX_VOICES] = {0, 2, 4, 1, 3, 5};
    if (polyMode == 1) {
        int startIndex = nextPoly1Index;
        for (int i = 0; i < currentActiveVoices; ++i) {
            int currentPatternIdx = (startIndex + i) % currentActiveVoices;
            int voiceIdx = voiceOrder[currentPatternIdx];
            if (!voices[voiceIdx].isActive()) {
                nextPoly1Index = (currentPatternIdx + 1) % currentActiveVoices; 
                return voiceIdx;
            }
        }
    } else if (polyMode == 2 || polyMode == 3) {
         for (int i = 0; i < currentActiveVoices; ++i) {
            if (!voices[i].isActive()) return i;
        }
    }
    return -1;
}

int JunoVoiceManager::findVoiceToSteal() {
    auto& voices = allocator.getVoices();
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
    int oldestIndex = -1;
    uint64_t minTimestamp = UINT64_MAX;
    for (int i = 0; i < currentActiveVoices; ++i) {
        if (voices[i].isActive() && !voices[i].isGateOnActive()) { 
             if (voiceTimestamps[i] < minTimestamp) { minTimestamp = voiceTimestamps[i]; oldestIndex = i; }
        }
    }
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
    auto& voices = allocator.getVoices();
    juce::String state;
    for (int i = 0; i < currentActiveVoices; ++i) {
        state += "[" + juce::String(i) + ":" + (voices[i].isActive() ? juce::String(voices[i].getCurrentNote()) : ".") + "] ";
    }
    DBG("Voices: " << state);
}

void JunoVoiceManager::setAllNotesOff() {
    allocator.allNotesOff();
}

bool JunoVoiceManager::anyVoiceActive() const {
    return allocator.getNumActiveVoices() > 0;
}

void JunoVoiceManager::setBenderAmount(float v) {
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) voice.setBender(v);
}
void JunoVoiceManager::setPortamentoEnabled(bool b) {
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) voice.setPortamentoEnabled(b);
}
void JunoVoiceManager::setPortamentoTime(float v) {
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) voice.setPortamentoTime(v);
}
void JunoVoiceManager::setPortamentoLegato(bool b) {
    auto& voices = allocator.getVoices();
    for (auto& voice : voices) voice.setPortamentoLegato(b);
}
} // namespace ABD
