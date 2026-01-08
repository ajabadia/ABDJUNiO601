// ============================================================================
// CRITICAL FIX #1: Retrigger Logic in JunoVoiceManager
// ============================================================================
// PROBLEMA: Cuando repites la misma nota rápidamente sin soltar, 
// ADSR no se resetea y la nota suena débil o desaparece.
//
// CAUSA: isLegato=true hace que el envelope continue desde su estado anterior
// en lugar de ir a ATTACK de nuevo.

// FILE: JunoVoiceManager.cpp
// FUNCIÓN: noteOn()

// ❌ ANTES (INCORRECTO):
void JunoVoiceManager::noteOn(int midiChannel, int midiNote, float velocity) {
    currentTimestamp++;
    
    // Check retrigger for same note
    for (int i = 0; i < MAXVOICES; i++) {
        if (voicesi.isActive && voicesi.getCurrentNote() == midiNote) {
            voicesi.noteOn(midiNote, velocity, anyVoiceActive);  // ❌ isLegato = true
            voiceTimestampsi = currentTimestamp;
            lastAllocatedVoiceIndex = i;
            return;
        }
    }
    
    // ... resto ...
}

// ✅ DESPUÉS (CORRECTO):
void JunoVoiceManager::noteOn(int midiChannel, int midiNote, float velocity) {
    currentTimestamp++;
    
    // Calcula si hay OTRAS voces activas (no esta nota)
    bool anyOtherVoiceActive = false;
    for (int i = 0; i < MAXVOICES; i++) {
        if (voicesi.isActive && voicesi.getCurrentNote() != midiNote) {
            anyOtherVoiceActive = true;
            break;
        }
    }
    
    // Check retrigger for same note
    for (int i = 0; i < MAXVOICES; i++) {
        if (voicesi.isActive && voicesi.getCurrentNote() == midiNote) {
            // RETRIGGER SIEMPRE resetea envelope
            // nunca entra en legato para la misma nota
            voicesi.noteOn(midiNote, velocity, false);  // ✅ isLegato = false SIEMPRE
            voiceTimestampsi = currentTimestamp;
            lastAllocatedVoiceIndex = i;
            return;
        }
    }
    
    // Allocate new voice (original code)
    int voiceIndex = findFreeVoiceIndex();
    if (voiceIndex == -1) {
        voiceIndex = findVoiceToSteal();
    }
    
    if (voiceIndex != -1) {
        voicesi.noteOn(midiNote, velocity, anyOtherVoiceActive);  // ✅ Legato solo si hay OTRAS notas
        voiceTimestampsi = currentTimestamp;
        lastAllocatedVoiceIndex = voiceIndex;
    }
}

// ============================================================================
// CRITICAL FIX #2: Tape Decoder - Auto-detect Patch Count
// ============================================================================
// PROBLEMA: Asume siempre 128 patches, pero WAV puede tener 16, 32, 64, o 128
//
// ARCHIVO: JunoTapeDecoder.h
// FUNCIÓN: decodeWavFile()

// ❌ ANTES (INCORRECTO):
while (patchCount < 128) {
    offset = dataPayloadStart + (patchCount * 18);
    if (offset + 18 > intresult.data.size()) break;
    
    const unsigned char patchBytes[18] = ...;
    patchCount++;
}

// ✅ DESPUÉS (CORRECTO):
// Primero, calcula automáticamente cuántos patches hay
const int autoDetectedPatches = intresult.data.size() / 18;

if (result.data.size() % 18 != 0) {
    // No es múltiplo de 18 - datos incompletos
    result.errorMessage = "Incomplete patch. Data: " + 
        juce::String(intresult.data.size()) + 
        " bytes (need multiple of 18)";
    result.success = false;
    return result;
}

if (autoDetectedPatches == 0) {
    result.errorMessage = "No valid patches found in WAV";
    result.success = false;
    return result;
}

// Cap at 128 (Juno no tiene más)
int patchCount = juce::jmin(autoDetectedPatches, 128);

// Ahora decodifica
for (int p = 0; p < patchCount; p++) {
    offset = dataPayloadStart + (p * 18);
    if (offset + 18 > intresult.data.size()) break;
    
    const unsigned char patchBytes[18] = ...;
    // ... createPresetFromJunoBytes() ...
}

// Reporta lo que encontró
result.success = (patchCount > 0);
result.errorMessage = "Loaded " + juce::String(patchCount) + " patches";

if (patchCount == 16) {
    result.errorMessage += " (Single bank 1x8x2)";
} else if (patchCount == 32) {
    result.errorMessage += " (Two banks)";
} else if (patchCount == 64) {
    result.errorMessage += " (Full group A or B)";
} else if (patchCount == 128) {
    result.errorMessage += " (Both groups A+B)";
}

// ============================================================================
// CRITICAL FIX #3: Voice ADSR Timeout (Stuck Note Prevention)
// ============================================================================
// PROBLEMA: Una voice puede quedar en Release indefinidamente
// SOLUCIÓN: Fuerza idle después de tiempo máximo de release

// ARCHIVO: SynthVoice.h

class SynthVoice {
    // ... existing members ...
    
private:
    int releaseCounter = 0;
    int sampleRate = 44100;
    static constexpr int kReleaseTimeoutMs = 30000;  // 30 segundos máximo
    
    // ... rest of class ...
};

// ARCHIVO: SynthVoice.cpp
// FUNCIÓN: prepareToPlay()

void SynthVoice::prepareToPlay(double sr, int blockSize) {
    sampleRate = static_cast<int>(sr);
    envelope.setSampleRate(sr);
    // ... rest of preparation ...
}

// FUNCIÓN: renderNextBlock()

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                 int startSample, int numSamples) {
    if (!isActive) return;
    
    // ... render audio ...
    
    // ✅ NUEVO: Monitorea timeout en Release
    if (envelope.getCurrentStage() == JunoADSR::Stage::Release) {
        releaseCounter += numSamples;
        
        // Calcula timeout en samples
        int timeoutSamples = (kReleaseTimeoutMs * sampleRate) / 1000;
        
        if (releaseCounter > timeoutSamples) {
            // Fuerza idle si está stuck
            juce::Logger::outputDebugString(
                "WARNING: Voice stuck in Release. Force idle. Note=" 
                + juce::String(getCurrentNote())
            );
            isActive = false;
            envelope.reset();
            releaseCounter = 0;
        }
    } else {
        // Reset counter cuando salimos de Release
        releaseCounter = 0;
    }
}

// FUNCIÓN: noteOn()

void SynthVoice::noteOn(int midiNote, float velocity, bool isLegato) {
    currentNote = midiNote;
    currentVelocity = velocity;
    
    // ✅ Reset timeout cuando entra una nota nueva
    releaseCounter = 0;
    
    if (!isLegato) {
        envelope.noteOn();  // Reset a ATTACK
    }
    
    isActive = true;
}

// FUNCIÓN: noteOff()

void SynthVoice::noteOff(bool allowTail) {
    // ✅ Reset timeout cuando hace note-off
    releaseCounter = 0;
    
    if (allowTail) {
        envelope.noteOff();
    } else {
        isActive = false;
        envelope.reset();
    }
}

// ============================================================================
// CRITICAL FIX #4: PerformanceState Synchronization
// ============================================================================
// PROBLEMA: Bender y portamento se actualizan pero no se propagan a voces

// ARCHIVO: PluginProcessor.cpp
// FUNCIÓN: processBlock()

void SimpleJuno106AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages) {
    auto numSamples = buffer.getNumSamples();
    
    // Actualiza parámetros del APVTS
    updateParamsFromAPVTS();
    
    // ✅ NUEVO: Sincroniza performance state con voice manager
    float benderAmount = currentParams.benderValue;  // -1.0 a 1.0
    
    // Propaga bender a VoiceManager
    voiceManager.setBenderAmount(benderAmount);
    
    // Propaga portamento
    voiceManager.setPortamentoEnabled(currentParams.portamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);
    
    // Procesa MIDI
    for (const auto metadata : midiMessages) {
        if (metadata.getMessage().isNoteOn()) {
            int note = metadata.getMessage().getNoteNumber();
            int velocity = metadata.getMessage().getVelocity();
            voiceManager.noteOn(0, note, velocity / 127.0f);
        } else if (metadata.getMessage().isNoteOff()) {
            int note = metadata.getMessage().getNoteNumber();
            voiceManager.noteOff(note, true);
        }
    }
    
    // Limpia buffer y renderiza
    buffer.clear();
    voiceManager.renderNextBlock(buffer, 0, numSamples);
    
    // Aplica master volume
    buffer.applyGain(currentParams.vcaLevel);
}

// ============================================================================
// TEST: Validar que los fixes funcionan
// ============================================================================
// En PluginEditor o test harness:

void testRetriggerFix() {
    // Simula: C4 NOTE_ON, C4 NOTE_ON (rápido), C4 NOTE_OFF
    
    processor.voiceManager.noteOn(0, 60, 0.7f);  // C4 ON
    // AUDIO RENDERS a 10ms
    processor.voiceManager.noteOn(0, 60, 0.7f);  // C4 ON AGAIN
    // AUDIOS SHOULD HEAR ATTACK AGAIN, not weak sustain
    
    // Assertion: Last voice should be in ATTACK stage
    // (not DECAY or SUSTAIN)
}

void testTapeLoaderFix() {
    juce::File tapeFile("path/to/16patch.wav");
    // o ("path/to/64patch.wav"), etc.
    
    auto result = processor.getPresetManager().loadTape(tapeFile);
    
    // Assertion: result.success == true
    // Assertion: "Single bank" in errorMessage (if 16 patches)
    // Assertion: "Full group" in errorMessage (if 64 patches)
}

void testTimeoutFix() {
    // Mantén una nota durante >30 segundos sin release
    processor.voiceManager.noteOn(0, 60, 0.7f);
    
    // Simula 31 segundos
    for (int i = 0; i < (31 * 44100 / blockSize); i++) {
        processor.processBlock(buffer, midiBuffer);
    }
    
    // Assertion: Voice debe estar en IDLE, no stuck en RELEASE
}
