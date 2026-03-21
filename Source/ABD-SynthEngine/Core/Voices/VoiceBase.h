// ABD-SynthEngine/Core/Voices/VoiceBase.h
#pragma once
#include <JuceHeader.h>

namespace ABD {

/**
 *  Clase base para cualquier voz de sintetizador.
 *  JunoVoice, CZVoice, ProphecyVoice heredan de aquí.
 */
class VoiceBase
{
public:
    virtual ~VoiceBase() = default;

    //── Ciclo de vida ─────────────────────────────────────────────────────────
    virtual void prepare(double sampleRate, int blockSize)
    {
        sr        = sampleRate;
        blockSz   = blockSize;
        onPrepare();
    }

    virtual void reset() { isActive_ = false; onReset(); }

    //── Activación ────────────────────────────────────────────────────────────
    virtual void noteOn(int midiNote, float velocity)
    {
        note_     = midiNote;
        velocity_ = velocity;
        isActive_ = true;
        onNoteOn(midiNote, velocity);
    }

    virtual void noteOff(float velocity)
    {
        onNoteOff(velocity);
        // isActive_ se pone a false cuando el envelope llega a silencio
    }

    //── Render ────────────────────────────────────────────────────────────────
    /**
     * Cada voz acumula (+= ) su salida en buffer para mezcla correcta.
     */
    virtual void renderNextBlock(juce::AudioBuffer<float>& buffer,
                                  int startSample,
                                  int numSamples) = 0;

    //── Estado ────────────────────────────────────────────────────────────────
    bool  isActive()  const noexcept { return isActive_; }
    int   getNote()   const noexcept { return note_; }
    float getVelocity() const noexcept { return velocity_; }

    /** La voz se marca inactiva cuando el envelope termina el release */
    void markInactive() noexcept { isActive_ = false; }

protected:
    double sr      { 44100.0 };
    int    blockSz { 512 };
    int    note_   { -1 };
    float  velocity_ { 0.f };
    bool   isActive_ { false };

    virtual void onPrepare() {}
    virtual void onReset()   {}
    virtual void onNoteOn (int note, float vel) = 0;
    virtual void onNoteOff(float vel)           = 0;
};

} // namespace ABD
