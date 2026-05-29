#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

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

    float Process()
    {
        mSeed = mSeed * 196314165u + 907633515u;
        float white = (2.f * static_cast<float>(mSeed) / static_cast<float>(0xFFFFFFFFu)) - 1.f;

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

            if (mShelfGain != 0.f)
            {
                mShelfState += mShelfCoeff * (pinkOut - mShelfState);
                return pinkOut + mShelfGain * (pinkOut - mShelfState);
            }
            return pinkOut;
        }
        else
        {
            mLpState += mLpCoeff * (white - mLpState);
            if (mShelfGain != 0.f)
            {
                mShelfState += mShelfCoeff * (mLpState - mShelfState);
                return mLpState + mShelfGain * (mLpState - mShelfState);
            }
            return mLpState;
        }
    }
};

/**
 *  JUNiO 601 — Chorus BBD
 *
 *  Emula el chip MN3009 (utilizado en el coro del Juno-106):
 *  - Dos líneas de retardo por modo (L+R).
 *  - Chorus I: ~3.2ms delay base, LFO ~0.5Hz.
 *  - Chorus II: ~6.4ms delay base, LFO ~0.8Hz.
 *  - Chorus I+II: Modo combinado.
 *
 *  Características de fidelidad:
 *  - Interpolación cúbica Hermite.
 *  - Saturación NE570 estilo Juno (tanh).
 *  - Filtros de reconstrucción post-BBD (8kHz).
 *  - LFO en cuadratura (0, 180, 90, 270 grados).
 */
class ChorusBBD
{
public:
    enum class Mode { Off, ChorusI, ChorusII, ChorusBoth };

    ChorusBBD();
    ~ChorusBBD() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    void setMode(Mode m)        { mode = m; }
    void setRate(float hz)      { lfoRate = juce::jlimit(0.1f, 8.0f, hz); }
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

    /** Procesa buffer estéreo in-place */
    void process(juce::AudioBuffer<float>& buffer);

private:
    //-- Parameters ---------------------------------------------------------
    Mode  mode     { Mode::Off };
    float lfoRate  { 0.513f };
    float m_depth    { 0.65f  };
    float wetMix   { 0.50f  };
    float hissLvlDb { -52.0f };
    double sr      { 44100.0 };

    // [Build 29] Calibration Values
    float calDelayI { 3.2f };
    float calDelayII { 3.3f };
    float calGainDry { 0.863f };
    float calGainWet { 1.257f };
    float calModDepth { 1.5f };
    float calSatBoost { 1.2f };
    float calFilterCutoff { 8000.0f };
    float calBothRate { 7.7f };

    //── Delay lines ────────────────────────────────────────────────────────
    static constexpr int MAX_DELAY_SAMPLES = 8192; // 100ms+ a 48k para seguridad
    static constexpr float DELAY_I_MS  = 3.2f;
    static constexpr float DELAY_II_MS = 6.4f;
    static constexpr float MOD_DEPTH_MS = 1.5f;

    struct DelayLine
    {
        std::vector<float> buf;
        int writePos { 0 };

        void init(int size) { buf.assign(size, 0.0f); writePos = 0; }
        void reset() { std::fill(buf.begin(), buf.end(), 0.0f); writePos = 0; }

        void write(float sample)
        {
            if (buf.empty()) return;
            buf[writePos] = sample;
            if (++writePos >= (int)buf.size()) writePos = 0;
        }

        float read(float delaySamples) const
        {
            if (buf.empty()) return 0.0f;
            int size = (int)buf.size();
            float rPos = (float)writePos - delaySamples;
            while (rPos < 0.0f) rPos += (float)size;
            while (rPos >= (float)size) rPos -= (float)size;

            int i1 = (int)rPos;
            int i0 = (i1 - 1 + size) % size;
            int i2 = (i1 + 1) % size;
            int i3 = (i1 + 2) % size;

            float frac = rPos - (float)i1;
            
            // Hermite cúbico
            float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
            float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
            float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float a2 = -0.5f * y0 + 0.5f * y2;
            float a3 = y1;

            return ((a0 * frac + a1) * frac + a2) * frac + a3;
        }
    };

    DelayLine lineI_L, lineI_R;
    DelayLine lineII_L, lineII_R;

    //── LFO ────────────────────────────────────────────────────────────────
    double lfoPhase { 0.0 };

    //── Post-filter (1-polo LP ~8kHz) ──────────────────────────────────────
    struct OnePoleLP
    {
        float b0 { 1.f }, a1 { 0.f }, z1 { 0.f };
        void prepare(double sampleRate, float cutoffHz)
        {
            float w = std::tan(juce::MathConstants<float>::pi * cutoffHz / (float)sampleRate);
            b0 = w / (1.f + w);
            a1 = (w - 1.f) / (w + 1.f);
        }
        void reset() { z1 = 0.f; }
        float process(float x)
        {
            float y = b0 * x + b0 * z1 - a1 * z1;
            z1 = y;
            return y;
        }
    };

    OnePoleLP filterI_L, filterI_R, filterII_L, filterII_R;

    //── Saturation & Noise ────────────────────────────────────────────────
    inline float saturate(float x) const { return std::tanh(x * calSatBoost); }

    //── Wet Noise & Ripple ────────────────────────────────────────────────
    AnalogFloorNoise wetNoiseL, wetNoiseR;
    RailRipple wetRipple;
    float hissMultiplier { 1.0f };
    float calHissColor { 0.4f };
};
