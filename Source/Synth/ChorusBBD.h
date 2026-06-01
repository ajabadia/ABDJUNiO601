#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>
#include "BBDFilter.h"

struct RailRipple
{
    float mPhase = 0.f;
    float mInc = 0.f;
    float mA1 = 0.f, mA2 = 0.f, mA3 = 0.f;

    void SetMainsHz(float mainsHz, float sampleRate)
    {
        mInc = (2.f * mainsHz) / sampleRate; // full-wave = 2× mains
    }

    void SetAmplitudes(float a1, float a2, float a3)
    {
        mA1 = a1;
        mA2 = a2;
        mA3 = a3;
    }

    void Reset() { mPhase = 0.f; }

    float Process()
    {
        mPhase += mInc;
        if (mPhase >= 1.f) mPhase -= 1.f;
        const float tp = 2.f * juce::MathConstants<float>::pi * mPhase;
        return mA1 * std::sin(tp) + mA2 * std::sin(2.f * tp) + mA3 * std::sin(3.f * tp);
    }
};

struct AnalogFloorNoise
{
    uint32_t mSeed = 0x12345678u;
    float mLpState = 0.f;
    float mLpCoeff = 0.f;
    float mShelfState = 0.f;
    float mShelfCoeff = 0.f;
    float mShelfGain = 0.f;
    float mPink0=0, mPink1=0, mPink2=0, mPink3=0, mPink4=0, mPink5=0, mPink6=0;
    bool mPinkEnabled = false;

    void Init(float sampleRate, float cutoffHz = 20000.f)
    {
        const float fc = std::min(cutoffHz, sampleRate * 0.45f);
        mLpCoeff = 1.f - std::exp(-2.f * juce::MathConstants<float>::pi * fc / sampleRate);
        mShelfCoeff = 0.f;
        mShelfGain = 0.f;
    }

    void SetHighShelf(float shelfFcHz, float hfBoostDb, float sampleRate)
    {
        const float fc = std::min(shelfFcHz, sampleRate * 0.45f);
        mShelfCoeff = 1.f - std::exp(-2.f * juce::MathConstants<float>::pi * fc / sampleRate);
        mShelfGain  = std::pow(10.f, hfBoostDb / 20.f) - 1.f;
    }

    float Process(float whiteToPink = 0.0f)
    {
        mSeed = mSeed * 196314165u + 907633515u;
        float white = (2.f * static_cast<float>(mSeed) / static_cast<float>(0xFFFFFFFFu)) - 1.f;

        float noiseOut = white;
        if (mPinkEnabled)
        {
            mPink0 = 0.99886f * mPink0 + white * 0.0555179f;
            mPink1 = 0.99332f * mPink1 + white * 0.0750759f;
            mPink2 = 0.96900f * mPink2 + white * 0.1538520f;
            mPink3 = 0.86650f * mPink3 + white * 0.3104856f;
            mPink4 = 0.55000f * mPink4 + white * 0.5329522f;
            mPink5 = -0.7616f * mPink5 - white * 0.0168980f;
            float pink = mPink0 + mPink1 + mPink2 + mPink3 + mPink4 + mPink5 + mPink6 + white * 0.5362f;
            mPink6 = white * 0.115926f;

            float pinkOut = pink * 0.11f;
            noiseOut = pinkOut * (1.f - whiteToPink) + white * whiteToPink;
        }
        else
        {
            mLpState += mLpCoeff * (white - mLpState);
            noiseOut = mLpState;
        }

        if (mShelfGain != 0.f)
        {
            mShelfState += mShelfCoeff * (noiseOut - mShelfState);
            return noiseOut + mShelfGain * (noiseOut - mShelfState);
        }
        return noiseOut;
    }
};

struct ClickRing
{
    float mFreqCoeff = 0.f;
    float mDamp = 0.f;
    float mLow = 0.f, mBand = 0.f;

    void Init(float resHz, float Q, float sampleRate)
    {
        mFreqCoeff = 2.f * std::sin(juce::MathConstants<float>::pi * resHz / sampleRate);
        mDamp = 1.f / Q;
        Reset();
    }

    void Reset() { mLow = 0.f; mBand = 0.f; }

    float Process(float input)
    {
        mLow  += mFreqCoeff * mBand;
        float high = input - mLow - mDamp * mBand;
        mBand += mFreqCoeff * high;
        return mLow;
    }
};

struct LeakNoise
{
    uint32_t mSeed = 0xDEADBEEFu;
    float mHpState = 0.f;
    float mHpCoeff = 0.f;
    float mLpState = 0.f;
    float mLpCoeff = 0.f;

    void Init(float sampleRate, float hpCornerHz = 1500.f)
    {
        float piVal = juce::MathConstants<float>::pi;
        mHpCoeff = 1.f - std::exp(-2.f * piVal * hpCornerHz / sampleRate);
        mLpCoeff = 1.f - std::exp(-2.f * piVal * 4000.f / sampleRate);
    }
  
    float Process()
    {
        mSeed = mSeed * 196314165u + 907633515u;
        float white = (2.f * static_cast<float>(mSeed) / static_cast<float>(0xFFFFFFFFu)) - 1.f;
        mHpState += mHpCoeff * (white - mHpState);
        float hp = white - mHpState;
        mLpState += mLpCoeff * (hp - mLpState);
        return mLpState;
    }
};

struct BBDClick
{
    int mCounter = -1;
    int mDurationSamples = 0;
    bool mWasInZone = false;
    static constexpr float kTriggerThreshold = 0.95f;
    static constexpr float kDurationMs = 180.f;

    void Init(float sampleRate)
    {
        mDurationSamples = static_cast<int>(kDurationMs * 0.001f * sampleRate);
    }

    void Reset() { mCounter = -1; mWasInZone = false; }

    void Suppress() { mCounter = -1; mWasInZone = true; }

    float Process(float lfoForLine)
    {
        bool inZone = (lfoForLine > kTriggerThreshold);
        if (inZone && !mWasInZone) mCounter = 0;
        mWasInZone = inZone;

        if (mCounter < 0 || mCounter >= mDurationSamples)
        {
            mCounter = -1;
            return 0.f;
        }

        constexpr float kAsymPoint       = 0.20f;
        constexpr float kSecondLobeScale = 0.85f;
        constexpr float kLeadDecayRate   = 4.0f;
        constexpr float kTrailDecayRate  = 8.0f;

        float t = static_cast<float>(mCounter) / static_cast<float>(mDurationSamples);
        mCounter++;

        float out;
        float piVal = juce::MathConstants<float>::pi;
        if (t < kAsymPoint)
        {
            float u = t / kAsymPoint;
            out = -std::exp(-kLeadDecayRate * u) * std::sin(piVal * u);
        }
        else
        {
            float u = (t - kAsymPoint) / (1.f - kAsymPoint);
            out = kSecondLobeScale * std::exp(-kTrailDecayRate * u) * std::sin(piVal * u);
        }
        return out;
    }
};

struct BBDLine
{
    static constexpr int kNumStages = 256;

    std::vector<float> mBuf;
    int mMask = 0;
    int mWPos = 0;

    BBDFilter mPreFilter;
    BBDFilter mPostFilter;

    float mSatDrive = 0.12f;
    float mSatDriveSmooth = 0.12f;
    float mSampleRate = 44100.f;

    void Init(float sampleRate, float biquadFc)
    {
        mSampleRate = sampleRate;

        int minLen = static_cast<int>(sampleRate * 0.020f) + 4;
        int len = 1;
        while (len < minLen) len <<= 1;
        mBuf.assign(static_cast<size_t>(len), 0.f);
        mMask = len - 1;
        mWPos = 0;

        mPreFilter.Init(sampleRate, biquadFc);
        mPostFilter.Init(sampleRate, biquadFc);
    }

    void Clear()
    {
        std::fill(mBuf.begin(), mBuf.end(), 0.f);
        mWPos = 0;
        mPreFilter.Reset();
        mPostFilter.Reset();
    }

    static float Hermite(float frac, float y0, float y1, float y2, float y3)
    {
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    float ReadHermite(float delaySamples) const
    {
        float rPos = static_cast<float>(mWPos) - delaySamples;
        if (rPos < 0.f) rPos += static_cast<float>(mMask + 1);

        int i1 = static_cast<int>(rPos);
        float frac = rPos - static_cast<float>(i1);

        return Hermite(frac,
            mBuf[(i1 - 1) & mMask],
            mBuf[i1 & mMask],
            mBuf[(i1 + 1) & mMask],
            mBuf[(i1 + 2) & mMask]);
    }

    float Process(float input, float delaySamples, float injectedNoise = 0.f)
    {
        float withNoise = input + injectedNoise;
        float filtered = mPreFilter.Process(withNoise);

        mSatDriveSmooth += (mSatDrive - mSatDriveSmooth) * 0.001f;
        float sd = mSatDriveSmooth;
        float sat = (sd > 0.01f) ? std::tanh(filtered * sd) / sd : filtered;

        mBuf[mWPos & mMask] = sat;
        float wet = ReadHermite(delaySamples);
        mWPos = (mWPos + 1) & mMask;
        float out = mPostFilter.Process(wet);

        return out;
    }
};

/**
 *  JUNiO 601 — Chorus BBD
 *
 *  Emula el chip MN3009 (utilizado en el coro del Juno-106):
 *  - Dos líneas de retardo por modo (L+R).
 *  - Chorus I: ~3.3ms delay base, LFO ~0.514Hz.
 *  - Chorus II: ~3.3ms delay base, LFO ~0.842Hz.
 *  - Chorus I+II: Modo vibrato rápido combinando ambos.
 */
class ChorusBBD
{
public:
    enum class Mode { Off, ChorusI, ChorusII, ChorusBoth };
    enum class ChorusModel { J60 = 0, J106 };

    ChorusBBD();
    ~ChorusBBD() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    void setMode(Mode m);
    void setChorusModel(ChorusModel model) { chorusModel = model; }
    ChorusModel getChorusModel() const { return chorusModel; }
    void setRate(float hz)      { lfoRate = juce::jlimit(0.1f, 15.0f, hz); }
    void setDepth(float d)      { m_depth = juce::jlimit(0.0f, 1.0f, d); }
    void setMix(float w)        { wetMix = juce::jlimit(0.0f, 1.0f, w); }
    void setHissLevel(float db) { hissLvlDb = juce::jlimit(-96.0f, -40.0f, db); }
    void setHissColor(float color) { calHissColor = color; }
    
    // [Build 29] Calibration Overrides
    void setCalibrationParams(float dI, float dII, float dryGain, float wetGain, float depth, float sat, float cutoff, float bothRate) {
        calDelayI = dI;
        calDelayII = dII;
        calGainDry = dryGain;
        calGainWet = wetGain;
        calModDepth = depth;
        calSatBoost = sat;
        calFilterCutoff = cutoff;
        calBothRate = bothRate;
    }

    void setHissMultiplier(float m) { hissMultiplier = juce::jlimit(0.0f, 2.0f, m); }
    void setNoiseWear(float wear) { setHissMultiplier(wear); } // BBD Noise/Wear slider

    /** Procesa buffer estéreo in-place */
    void process(juce::AudioBuffer<float>& buffer);

private:
    ChorusModel chorusModel { ChorusModel::J106 };

    //-- Parameters ---------------------------------------------------------
    Mode  mode     { Mode::Off };
    float lfoRate  { 0.513f };
    float m_depth    { 0.65f  };
    float wetMix   { 0.50f  };
    float hissLvlDb { -52.0f };
    double sr      { 44100.0 };

    // [Build 29] Calibration Values
    float calDelayI { 3.3f };
    float calDelayII { 3.3f };
    float calGainDry { 0.863f };
    float calGainWet { 1.257f };
    float calModDepth { 1.5f };
    float calSatBoost { 1.2f };
    float calFilterCutoff { 9661.0f };
    float calBothRate { 7.7f };

    //── Delay lines ────────────────────────────────────────────────────────
    BBDLine lineL, lineR;

    //── Physical Modeling Glitches & Noise ────────────────────────────────
    BBDClick clickL_Pri, clickR_Pri;
    BBDClick clickL_Slo, clickR_Slo;
    ClickRing clickRingL, clickRingR;
    LeakNoise leakNoiseL, leakNoiseR;

    //── LFO ────────────────────────────────────────────────────────────────
    double lfoPhase { 0.0 };
    Mode prevMode { Mode::Off };

    //── Wet Noise & Ripple ────────────────────────────────────────────────
    AnalogFloorNoise wetNoiseL, wetNoiseR;
    RailRipple wetRipple;
    float hissMultiplier { 1.0f };
    float calHissColor { 0.4f };
};
