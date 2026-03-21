// ABD-SynthEngine/Core/LFO/LFOGeneric.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

namespace ABD {

class LFOGeneric
{
public:
    enum class Shape { Sine, Triangle, Square, SawUp, SawDown, SampleHold };

    void prepare(double sampleRate) noexcept
    {
        sr_    = sampleRate;
        phase_ = 0.0;
        sampleHoldValue_ = 0.f;
        reset();
    }

    void reset() noexcept { phase_ = 0.0; delayCounter_ = 0; fadeLevel_ = 0.f; }

    void setRate (float hz)   noexcept { rate_  = juce::jlimit(0.01f, 50.f, hz); }
    void setShape(Shape s)    noexcept { shape_ = s; }
    void setDepth(float d)    noexcept { depth_ = juce::jlimit(0.f, 1.f, d); }

    /**
     *  Delay + fade-in: igual que el Juno-106 hardware.
     *  delaySeconds: tiempo antes de que el LFO empiece a actuar.
     *  fadeSeconds:  tiempo de fundido desde 0 hasta profundidad plena.
     */
    void setDelayFade(float delaySeconds, float fadeSeconds) noexcept
    {
        delaySamples_ = (int)(delaySeconds * sr_);
        fadeSamples_  = (int)(fadeSeconds  * sr_);
    }

    /** Sincronización de fase en NoteOn (comportamiento Juno-106) */
    void syncPhase() noexcept
    {
        phase_        = 0.0;
        delayCounter_ = 0;
        fadeLevel_    = 0.f;
    }

    /** Procesa un sample, devuelve valor modulado en [-1, +1] */
    float process() noexcept
    {
        // Delay
        if (delayCounter_ < delaySamples_)
        {
            ++delayCounter_;
            return 0.f;
        }

        // Fade-in
        if (fadeSamples_ > 0 && fadeLevel_ < 1.f)
        {
            fadeLevel_ += 1.f / (float)fadeSamples_;
            fadeLevel_  = std::min(fadeLevel_, 1.f);
        }
        else
        {
            fadeLevel_ = 1.f;
        }

        // Generar forma de onda
        float raw = generateSample();

        // Avance de fase
        phase_ += rate_ / sr_;
        if (phase_ >= 1.0) phase_ -= 1.0;

        return raw * depth_ * fadeLevel_;
    }

    float getPhase() const noexcept { return (float)phase_; }

private:
    Shape  shape_  { Shape::Sine };
    double phase_  { 0.0 };
    double sr_     { 44100.0 };
    float  rate_   { 0.5f };
    float  depth_  { 1.0f };

    // Delay / fade
    int   delaySamples_ { 0 };
    int   fadeSamples_  { 0 };
    int   delayCounter_ { 0 };
    float fadeLevel_    { 0.f };

    // Sample & Hold
    float sampleHoldValue_ { 0.f };
    double prevPhase_      { 0.0 };

    float generateSample() noexcept
    {
        float p = (float)phase_;

        switch (shape_)
        {
        case Shape::Sine:
            return std::sin(p * juce::MathConstants<float>::twoPi);

        case Shape::Triangle:
            return p < 0.5f ? (4.f * p - 1.f) : (3.f - 4.f * p);

        case Shape::Square:
            return p < 0.5f ? 1.f : -1.f;

        case Shape::SawUp:
            return 2.f * p - 1.f;

        case Shape::SawDown:
            return 1.f - 2.f * p;

        case Shape::SampleHold:
            // Nuevo ciclo → nuevo valor aleatorio
            if (phase_ < prevPhase_)
                sampleHoldValue_ = juce::Random::getSystemRandom()
                                   .nextFloat() * 2.f - 1.f;
            prevPhase_ = phase_;
            return sampleHoldValue_;
        }
        return 0.f;
    }
};

} // namespace ABD
