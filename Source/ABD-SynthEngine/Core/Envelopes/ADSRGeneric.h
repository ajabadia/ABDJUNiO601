// ABD-SynthEngine/Core/Envelopes/ADSRGeneric.h
#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace ABD {

/**
 *  ADSR exponencial configurable.
 *  - curveExp: 0 = lineal, 1 = completamente exponencial
 *  - Usado por JunoVoice, CZVoice, ProphecyVoice sin cambios
 */
class ADSRGeneric
{
public:
    struct Params
    {
        float attackSecs  { 0.01f };
        float decaySecs   { 0.3f  };
        float sustainLevel{ 0.7f  };
        float releaseSecs { 0.5f  };
        float curveExp    { 0.7f  }; // 0=lineal, 1=expo pura
    };

    void prepare(double sampleRate) noexcept
    {
        sr_ = sampleRate;
        reset();
    }

    void setParams(const Params& p) noexcept { params_ = p; }
    Params getParams() const noexcept { return params_; }

    void noteOn()  noexcept { stage_ = Stage::Attack; }
    void noteOff() noexcept { if (stage_ != Stage::Idle) stage_ = Stage::Release; }
    void reset()   noexcept { stage_ = Stage::Idle; level_ = 0.f; }

    bool isActive() const noexcept { return stage_ != Stage::Idle; }

    float process() noexcept
    {
        switch (stage_)
        {
        case Stage::Attack:
            level_ += attackInc();
            if (level_ >= 1.f)
            { level_ = 1.f; stage_ = Stage::Decay; }
            break;

        case Stage::Decay:
            level_ -= decayInc();
            if (level_ <= params_.sustainLevel)
            { level_ = params_.sustainLevel; stage_ = Stage::Sustain; }
            break;

        case Stage::Sustain:
            level_ = params_.sustainLevel;
            break;

        case Stage::Release:
            level_ -= releaseInc();
            if (level_ <= 0.001f)
            { level_ = 0.f; stage_ = Stage::Idle; }
            break;

        case Stage::Idle:
            level_ = 0.f;
            break;
        }

        // Curvatura exponencial
        return params_.curveExp > 0.f
            ? std::pow(level_, 1.f + params_.curveExp * 2.f)
            : level_;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    Params params_;
    Stage  stage_ { Stage::Idle };
    float  level_ { 0.f };
    double sr_    { 44100.0 };

    float attackInc()  const noexcept
    { return (float)(1.0 / (params_.attackSecs  * sr_)); }
    float decayInc()   const noexcept
    { return (float)((1.f - params_.sustainLevel) / (juce::jmax(0.001f, params_.decaySecs) * sr_)); }
    float releaseInc() const noexcept
    { return (float)(params_.sustainLevel / (juce::jmax(0.001f, params_.releaseSecs) * sr_)); }
};

} // namespace ABD
