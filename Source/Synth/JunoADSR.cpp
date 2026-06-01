#include <JuceHeader.h>
#include "JunoADSR.h"
#include <cmath>
#include <algorithm>
#include "../Core/JunoConstants.h"

// ============================================================================
// JunoADSR Implementation
// ============================================================================

JunoADSR::JunoADSR()
{
    reset();
}

void JunoADSR::setSampleRate(double sr)
{
    if (sr > 0.0) {
        sampleRate = sr;
        calculateRates();
    }
}

void JunoADSR::setMode(ADSRMode newMode)
{
    mode = newMode;
}

static constexpr std::array<uint16_t, 128> GenerateDecRelTable()
{
    constexpr int kCounts[] = {  4,      1,      10,     28,     22,     58,     4     };
    constexpr uint16_t kSteps[] = { 0x2000, 0x1000, 0x0800, 0x0080, 0x000C, 0x0004, 0x0001 };
    std::array<uint16_t, 128> t{};
    uint16_t val = 0x1000;
    int i = 0;
    t[i++] = val;
    for (int seg = 0; seg < 7; ++seg)
        for (int n = 0; n < kCounts[seg]; ++n)
            t[i++] = (val = static_cast<uint16_t>(val + kSteps[seg]));
    return t;
}
static constexpr auto kDecRelTable = GenerateDecRelTable();

static uint16_t AttackIncFromSlider(float slider)
{
    float s = std::clamp(slider, 0.f, 1.f);
    if (s < 0.003937f) return 0x3FFF;
    if (s <= 0.500000f)
        return static_cast<uint16_t>(8192.f / (s * 127.f) + 0.5f);
    if (s <= 0.681102f)
        return static_cast<uint16_t>(305.03f - 352.26f * s + 0.5f);
    if (s <= 0.846457f)
        return static_cast<uint16_t>(194.74f - 190.50f * s + 0.5f);
    if (s <= 0.956693f)
        return static_cast<uint16_t>(86.37f - 62.52f * s + 0.5f);
    return static_cast<uint16_t>(std::max(
        static_cast<int>(148.f - 127.f * s + 0.5f), 1));
}

// J6 attack tau (seconds) from 11-point log-interpolated hardware measurement
float JunoADSR::AttackTauJ6(float slider)
{
    static constexpr float kAttackTau[11] = {
        0.000558f, 0.001674f, 0.008762f, 0.029468f, 0.064015f, 0.120998f,
        0.238481f, 0.495993f, 0.607950f, 1.392486f, 1.674332f
    };
    float s = slider * 10.f;
    int idx = std::min(static_cast<int>(s), 9);
    float frac = s - idx;
    return std::exp(std::log(kAttackTau[idx])
          + frac * (std::log(kAttackTau[idx + 1]) - std::log(kAttackTau[idx])));
}

// J6 decay/release tau (seconds) from exponential fit of hardware measurements
float JunoADSR::DecRelTauJ6(float slider)
{
    return 0.003577f * std::exp(12.9460f * slider - 5.0638f * slider * slider);
}

void JunoADSR::reset()
{
    stage = Stage::Idle;
    currentValue = 0.0f;
    gateEnv = 0.0f;
    mEnvInt = 0;
    mTickAccum = 0.0f;
    mEnvPrev = 0.0f;
    mEnvNext = 0.0f;
    smoothedValue = 0.0f;
}

void JunoADSR::setAttackRaw(float slider)
{
    if (mode == ADSRMode::kJ106)
    {
        mAtkInc = AttackIncFromSlider(slider);
    }
    else // kJ6 or kJ60
    {
        float tau = AttackTauJ6(slider);
        mAttackCoeff = 1.f - std::exp(-1.f / (tau * mTimeScale * sampleRate));
    }
}

void JunoADSR::setDecayRaw(float slider)
{
    if (mode == ADSRMode::kJ106)
    {
        int index = std::clamp(static_cast<int>(slider * 127.f + 0.5f), 0, 127);
        mDecMul = kDecRelTable[index];
    }
    else // kJ6 or kJ60
    {
        float tau = DecRelTauJ6(slider);
        mDecayCoeff = 1.f - std::exp(-1.f / (tau * mTimeScale * sampleRate));
    }
}

void JunoADSR::setReleaseRaw(float slider)
{
    if (mode == ADSRMode::kJ106)
    {
        int index = std::clamp(static_cast<int>(slider * 127.f + 0.5f), 0, 127);
        mRelMul = kDecRelTable[index];
    }
    else // kJ6 or kJ60
    {
        float tau = DecRelTauJ6(slider);
        mReleaseCoeff = 1.f - std::exp(-1.f / (tau * mTimeScale * sampleRate));
    }
}

void JunoADSR::setAttack(float seconds)
{
    attackTime = juce::jlimit(0.0015f, 3.0f, seconds);
    float raw = (attackTime - 0.0015f) / (3.0f - 0.0015f);
    raw = std::pow(std::max(0.0f, raw), 1.0f / 2.2f);
    setAttackRaw(raw);
}

void JunoADSR::setDecay(float seconds)
{
    decayTime = juce::jlimit(0.0015f, 12.0f, seconds);
    float raw = (decayTime - 0.0015f) / (12.0f - 0.0015f);
    raw = std::pow(std::max(0.0f, raw), 1.0f / 2.2f);
    setDecayRaw(raw);
}

void JunoADSR::setSustain(float level)
{
    sustainLevel = juce::jlimit(0.0f, 1.0f, level);
    mSusInt = static_cast<uint16_t>(sustainLevel * 0x3F80);
}

void JunoADSR::setRelease(float seconds)
{
    releaseTime = juce::jlimit(0.0015f, 12.0f, seconds);
    float raw = (releaseTime - 0.0015f) / (12.0f - 0.0015f);
    raw = std::pow(std::max(0.0f, raw), 1.0f / 2.2f);
    setReleaseRaw(raw);
}

void JunoADSR::setGateMode(bool enabled) { gateMode = enabled; }
void JunoADSR::setSlewMs(float ms) { slewMs = juce::jlimit(0.1f, 10.0f, ms); }
void JunoADSR::setAttackFactor(float factor) { attackFactor = juce::jlimit(0.1f, 1.0f, factor); calculateRates(); }

void JunoADSR::noteOn()
{
    stage = Stage::Attack;
    if (mode == ADSRMode::kJ106)
    {
        mEnvPrev = static_cast<float>(mEnvInt) / JunoADSR::kEnvMax;
        tick106();
        mEnvNext = static_cast<float>(mEnvInt) / JunoADSR::kEnvMax;
    }
    else
    {
        // kJ6/kJ60: starts from current value for smooth retrigger
        gateEnv = 0.0f;
    }
}

void JunoADSR::noteOff()
{
    if (stage != Stage::Idle) {
        stage = Stage::Release;
    }
}

uint16_t JunoADSR::calcDecay(uint16_t value, uint16_t coeff)
{
    uint8_t vh = value >> 8;
    uint8_t vl = value & 0xFF;
    uint8_t ch = coeff >> 8;
    uint8_t cl = coeff & 0xFF;
    return static_cast<uint16_t>(vh * ch)
         + static_cast<uint16_t>((vh * cl) >> 8)
         + static_cast<uint16_t>((vl * ch) >> 8);
}

void JunoADSR::tick106()
{
    switch (stage)
    {
        case Stage::Attack: {
            uint32_t sum = static_cast<uint32_t>(mEnvInt) + mAtkInc;
            if (sum >= JunoADSR::kEnvMax)
            {
                mEnvInt = JunoADSR::kEnvMax;
                stage = Stage::Decay;
            }
            else
                mEnvInt = static_cast<uint16_t>(sum);
            break;
        }
        case Stage::Decay:
            if (mEnvInt > mSusInt)
            {
                uint16_t diff = mEnvInt - mSusInt;
                diff = calcDecay(diff, mDecMul);
                mEnvInt = diff + mSusInt;
            }
            else
                mEnvInt = mSusInt;
            break;
        case Stage::Release:
            mEnvInt = calcDecay(mEnvInt, mRelMul);
            if (mEnvInt == 0)
                stage = Stage::Idle;
            break;
        case Stage::Idle:
        default:
            break;
    }
}

float JunoADSR::getNextSample()
{
    if (gateMode) {
        float target = (stage == Stage::Release || stage == Stage::Idle) ? 0.0f : 0.97f;
        currentValue += (target - currentValue) * 0.03f;
        if (stage == Stage::Release && currentValue < 0.0001f) {
            currentValue = 0.0f;
            stage = Stage::Idle;
        }
        return currentValue;
    }

    if (mode == ADSRMode::kJ106)
    {
        mTickAccum += mTickStep;
        while (mTickAccum >= 1.f)
        {
            mTickAccum -= 1.f;
            mEnvPrev = mEnvNext;
            tick106();
            mEnvNext = static_cast<float>(mEnvInt) / JunoADSR::kEnvMax;
        }
        currentValue = mEnvPrev + (mEnvNext - mEnvPrev) * mTickAccum;
    }
    else // kJ6 or kJ60
    {
        switch (stage)
        {
            case Stage::Attack:
                currentValue += (kAttackTarget - currentValue) * mAttackCoeff;
                gateEnv += kGateSlope;
                if (currentValue >= 1.0f)
                {
                    currentValue = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                currentValue += (sustainLevel - currentValue) * mDecayCoeff;
                gateEnv += kGateSlope;
                break;

            case Stage::Release:
                gateEnv -= kGateSlope;
                currentValue += (kReleaseTarget - currentValue) * mReleaseCoeff;
                if (currentValue < kSilence)
                {
                    currentValue = 0.0f;
                    stage = Stage::Idle;
                }
                break;

            case Stage::Idle:
            default:
                gateEnv -= kGateSlope;
                currentValue = 0.0f;
                break;
        }
        gateEnv = std::clamp(gateEnv, 0.0f, 1.0f);
    }
    
    float quantized = currentValue;
    if (dacSteps > 0.0f) {
        quantized = std::floor(currentValue * (dacSteps - 0.01f)) / (dacSteps - 1.0f);
    }
    
    if (slewMs > 0.1f) {
        float alpha = 1.0f - std::exp(-1.0f / (slewMs * 0.001f * (float)sampleRate));
        smoothedValue += (quantized - smoothedValue) * alpha;
        return smoothedValue;
    }
    
    smoothedValue = quantized;
    return quantized;
}

void JunoADSR::calculateRates()
{
    if (sampleRate <= 0.0) return;
    
    float tickRate = 1000.0f / mcuRateFactor;
    mTickStep = (tickRate * mTimeScale) / (float)sampleRate;
}
