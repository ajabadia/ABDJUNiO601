// ABD-SynthEngine/Core/Voices/VoiceAllocator.h
#pragma once
#include "VoiceBase.h"
#include <array>
#include <functional>

namespace ABD {

enum class PolyMode { Poly1, Poly2, Unison };

template <typename VoiceType, int MAX_VOICES>
class VoiceAllocator
{
    static_assert(std::is_base_of_v<VoiceBase, VoiceType>,
                  "VoiceType must inherit from VoiceBase");
public:
    using VoiceArray = std::array<VoiceType, MAX_VOICES>;

    VoiceAllocator() = default;

    void prepare(double sampleRate, int blockSize)
    {
        for (auto& v : voices_)
            v.prepare(sampleRate, blockSize);
    }

    void reset()
    {
        for (auto& v : voices_) v.reset();
        roundRobinIdx_ = 0;
    }

    void setPolyMode(PolyMode m) noexcept { mode_ = m; }
    void setPortamento(float time, bool legato) noexcept
    {
        portaTime_   = time;
        portaLegato_ = legato;
    }

    //── NoteOn ────────────────────────────────────────────────────────────────
    void noteOn(int note, float velocity)
    {
        switch (mode_)
        {
            case PolyMode::Poly1:   allocPoly1(note, velocity); break;
            case PolyMode::Poly2:   allocPoly2(note, velocity); break;
            case PolyMode::Unison:  allocUnison(note, velocity); break;
        }
    }

    //── NoteOff ───────────────────────────────────────────────────────────────
    void noteOff(int note, float velocity)
    {
        for (auto& v : voices_)
            if (v.isActive() && v.getNote() == note)
                v.noteOff(velocity);
    }

    void allNotesOff()
    {
        for (auto& v : voices_)
            if (v.isActive()) v.noteOff(0.f);
    }

    //── Render ────────────────────────────────────────────────────────────────
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                          int startSample, int numSamples)
    {
        for (auto& v : voices_)
            if (v.isActive())
                v.renderNextBlock(buffer, startSample, numSamples);
    }

    //── Acceso directo ────────────────────────────────────────────────────────
    VoiceArray&       getVoices()       noexcept { return voices_; }
    const VoiceArray& getVoices() const noexcept { return voices_; }
    int getNumActiveVoices() const noexcept
    {
        int n = 0;
        for (auto& v : voices_) if (v.isActive()) ++n;
        return n;
    }

private:
    VoiceArray voices_;
    PolyMode   mode_          { PolyMode::Poly1 };
    int        roundRobinIdx_ { 0 };
    float      portaTime_     { 0.f };
    bool       portaLegato_   { false };

    //── Algoritmos de asignación ──────────────────────────────────────────────
    void allocPoly1(int note, float vel)  // Round-robin
    {
        // Si la nota ya está sonando, reutilizar esa voz
        for (auto& v : voices_)
            if (v.isActive() && v.getNote() == note)
            { v.noteOn(note, vel); return; }

        // Siguiente voz libre en round-robin
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            int idx = (roundRobinIdx_ + i) % MAX_VOICES;
            if (!voices_[idx].isActive())
            {
                voices_[idx].noteOn(note, vel);
                roundRobinIdx_ = (idx + 1) % MAX_VOICES;
                return;
            }
        }

        // Voice stealing: robar la más antigua (round-robin actual)
        voices_[roundRobinIdx_].noteOn(note, vel);
        roundRobinIdx_ = (roundRobinIdx_ + 1) % MAX_VOICES;
    }

    void allocPoly2(int note, float vel)  // Lowest-free (estático)
    {
        for (auto& v : voices_)
            if (!v.isActive())
            { v.noteOn(note, vel); return; }

        // Stealing: la voz más silenciosa (índice 0 como fallback)
        voices_[0].noteOn(note, vel);
    }

    void allocUnison(int note, float vel)  // Todas las voces
    {
        for (auto& v : voices_)
            v.noteOn(note, vel);
    }
};

} // namespace ABD
