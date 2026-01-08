#include "JunoVoiceManager.h"

JunoVoiceManager::JunoVoiceManager() {
    voiceTimestamps.fill(0);
}

void JunoVoiceManager::prepare(double sampleRate, int maxBlockSize) {
    for (auto& voice : voices) {
        voice.prepare(sampleRate, maxBlockSize);
    }
}

void JunoVoiceManager::updateParams(const SynthParams& params) {
    for (auto& voice : voices) {
        voice.updateParams(params);
    }
}

void JunoVoiceManager::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float lfoValue) {
    for (auto& voice : voices) {
        if (voice.isActive()) {
            voice.renderNextBlock(buffer, startSample, numSamples, lfoValue);
        }
    }
}

void JunoVoiceManager::setPolyMode(int mode) {
    if (polyMode != mode) {
        polyMode = mode;
        resetAllVoices(); 
    }
}

void JunoVoiceManager::noteOn(int /*midiChannel*/, int midiNote, float velocity) {
    currentTimestamp++;
    
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive() && voices[i].getCurrentNote() == midiNote) {
            // [Fidelidad] Si la nota ya suena, siempre re-atacar (isLegato = false)
            voices[i].noteOn(midiNote, velocity, false); 
            voiceTimestamps[i] = currentTimestamp;
            lastAllocatedVoiceIndex = i;
            return;
        }
    }

    int voiceIndex = findFreeVoiceIndex();
    if (voiceIndex == -1) voiceIndex = findVoiceToSteal();
    
    if (voiceIndex != -1) {
        // [Fidelidad] El legato solo ocurre en UNISON si hay teclas FÍSICAMENTE pulsadas.
        // Si solo hay colas de release, la nueva nota debe atacar de nuevo.
        bool isLegatoTransition = (polyMode == 3) && isAnyNoteHeld();
        
        voices[voiceIndex].noteOn(midiNote, velocity, isLegatoTransition);
        voiceTimestamps[voiceIndex] = currentTimestamp;
        lastAllocatedVoiceIndex = voiceIndex;
    }
}

void JunoVoiceManager::noteOff(int /*midiChannel*/, int midiNote, float /*velocity*/) {
    if (polyMode == 3) { 
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

int JunoVoiceManager::findFreeVoiceIndex() {
    if (polyMode == 1) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            int idx = (lastAllocatedVoiceIndex + 1 + i) % MAX_VOICES;
            if (!voices[idx].isActive()) return idx;
        }
    }
    else {
         for (int i = 0; i < MAX_VOICES; ++i) {
            if (!voices[i].isActive()) return i;
        }
    }
    return -1;
}

int JunoVoiceManager::findVoiceToSteal() {
    int oldestIndex = -1;
    uint64_t minTimestamp = UINT64_MAX;
    
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].isActive() && !voices[i].isGateOnActive()) { 
             if (voiceTimestamps[i] < minTimestamp) {
                minTimestamp = voiceTimestamps[i];
                oldestIndex = i;
            }
        }
    }
    
    if (oldestIndex == -1) {
        minTimestamp = UINT64_MAX;
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (voiceTimestamps[i] < minTimestamp) {
                minTimestamp = voiceTimestamps[i];
                oldestIndex = i;
            }
        }
    }
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
