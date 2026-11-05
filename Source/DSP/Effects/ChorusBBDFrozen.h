/*
  ==============================================================================

    ChorusBBDFrozen.h
    REFERENCIA CONGELADA del coro BBD de JUNiO601. NO LA USA EL PRODUCTO.

    Que es. Una transcripcion literal de lo que era el coro del Juno antes de
    extraccionse al modulo compartido ABDSharedCode::DspEffects: el fichero
    Source/Synth/ChorusBBD.{h,cpp} mas Source/Synth/BBDFilter.h, con las seis
    estructuras (BBDFilter, BbdLine, BbdClick, ClickRing, LeakNoise,
    AnalogFloorNoise, RailRipple) y el bucle de proceso tal cual estaban.

    POR QUE ESTA AQUI Y NO EN EL PRODUCTO. El producto ya usa el motor
    compartido. Este fichero existe para que Tests/ChorusBBDParityTest.cpp pueda
    comparar las dos implementaciones MUESTRA A MUESTRA. Si el motor se toca,
    este test dice si cambio la salida y en que muestra. Es el unico testigo de
    que la extraccion fue una extraccion y no una reescritura.

    Y LA REGLA QUE HACE POSIBLE EL TEST: ESTE FICHERO NO INCLUYE JunoBBD.h.
    Cero dependencia del codigo bajo prueba. La referencia se apoya solo en
    DspCore.h para las transcendentales deterministas, y el motor tiene que
    coincidir con ella sin que nada los ate.

    LO UNICO QUE SE CAMBIO AL TRANSCRIBIRLO, y hay que decirlo porque es la
    diferencia entre "identico" y "identico salvo esto":

      std::sin   -> abd::dsp::sin        (DspCore/DspMath.h)
      std::tanh  -> abd::dsp::tanh
      std::pow   -> abd::dsp::pow
      std::tan   -> tanDet   = sin(x) / cos(x)
      std::exp   -> expDet   = exp2(y * log2(e))

    El original usaba la libm de la plataforma. Este modulo es libm-free, y
    sustituir la libm por una implementacion determinista es un cambio de sonido
    DELIBERADO (el motor es pre-1.0) que ademas es lo que cierra la paridad
    nativa <-> WASM. Mismo criterio y mismo texto que ABDNeural/Tests/
    DspEffectsParityTest.cpp, y por el mismo motivo: este test ya no prueba ESE
    cambio, seria circular. Lo que prueba es que la extraccion fue fiel con la
    aritmetica puesta. El error de las identidades esta medido y va en
    DspEffects/JunoBBD.h (`tanDet`, `expDet`): <= 6.6e-07 relativo.

    Arithmetica de la mezcla y del LFO: se dejan tal cual, includedo el factor
    `lfoPhase / twoPi` y los `abs`. No se "optimiza" nada, porque una referencia
    que se parece al original pero no es el original no freezes nada.

  ==============================================================================
*/

#pragma once

#include "DspCore/DspCore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace juno_frozen
{

using abd::dsp::dspmath_detail::kPi;

//--- Las dos identidades, escritas aqui y no importadas del motor, a proposito -
constexpr float kLog2E = 1.4426950408889634f;

inline float tanDet(float x) noexcept
{
    return abd::dsp::sin(x) / abd::dsp::cos(x);
}
inline float expDet(float y) noexcept
{
    return abd::dsp::exp2(y * kLog2E);
}

using abd::dsp::jlimit;
using abd::dsp::jmax;
using abd::dsp::jmin;

//==============================================================================
// --- Source/Synth/BBDFilter.h, verbatim -------------------------------------
struct BBDFilter
{
    static constexpr float kBiquadFc = 9661.0f;
    static constexpr float kBiquadQ  = 0.7071f;
    static constexpr float kPoleFc   = 20000.0f;

    struct SVFBiquad
    {
        float mIC1eq = 0.0f;
        float mIC2eq = 0.0f;

        void Reset() { mIC1eq = mIC2eq = 0.0f; }
        void SetState(float v)
        {
            mIC1eq = 0.0f;
            mIC2eq = v;
        }

        float Process(float input, float g, float a1)
        {
            float v3 = input - mIC2eq;
            float v1 = a1 * mIC1eq + a1 * g * v3;
            float v2 = mIC2eq + g * v1;
            mIC1eq   = 2.0f * v1 - mIC1eq;
            mIC2eq   = 2.0f * v2 - mIC2eq;
            return v2;
        }
    };

    struct TPT1
    {
        float mS = 0.0f;

        void Reset() { mS = 0.0f; }
        void SetState(float v) { mS = v; }

        float Process(float input, float g)
        {
            const float v  = (input - mS) * g / (1.0f + g);
            const float lp = mS + v;
            mS             = lp + v;
            return lp;
        }
    };

    SVFBiquad mBiquad;
    TPT1 mPole;
    float mG_bq = 0.0f, mA1_bq = 0.0f, mG_p = 0.0f;

    void Init(float sampleRate, float biquadFc = kBiquadFc)
    {
        const float fc = std::min(biquadFc, sampleRate * 0.45f);
        mG_bq          = tanDet(kPi * fc / sampleRate);
        mA1_bq         = 1.0f / (1.0f + mG_bq / kBiquadQ + mG_bq * mG_bq);

        const float fcP = std::min(kPoleFc, sampleRate * 0.45f);
        mG_p            = tanDet(kPi * fcP / sampleRate);

        Reset();
    }

    void Reset()
    {
        mBiquad.Reset();
        mPole.Reset();
    }
    void SetState(float v)
    {
        mBiquad.SetState(v);
        mPole.SetState(v);
    }

    float Process(float input)
    {
        float x = mBiquad.Process(input, mG_bq, mA1_bq);
        x       = mPole.Process(x, mG_p);
        return x;
    }
};

//==============================================================================
// --- Source/Synth/ChorusBBD.h, las cinco estructuras restantes, verbatim -----
struct RailRipple
{
    float mPhase = 0.0f;
    float mInc   = 0.0f;
    float mA1 = 0.0f, mA2 = 0.0f, mA3 = 0.0f;

    void SetMainsHz(float mainsHz, float sampleRate) { mInc = (2.0f * mainsHz) / sampleRate; }
    void SetAmplitudes(float a1, float a2, float a3)
    {
        mA1 = a1;
        mA2 = a2;
        mA3 = a3;
    }
    void Reset() { mPhase = 0.0f; }

    float Process()
    {
        mPhase += mInc;
        if (mPhase >= 1.0f) mPhase -= 1.0f;
        const float tp = 2.0f * kPi * mPhase;
        return mA1 * abd::dsp::sin(tp) + mA2 * abd::dsp::sin(2.0f * tp) + mA3 * abd::dsp::sin(3.0f * tp);
    }
};

struct AnalogFloorNoise
{
    std::uint32_t mSeed = 0x12345678u;
    float mLpState = 0.0f, mLpCoeff = 0.0f;
    float mShelfState = 0.0f, mShelfCoeff = 0.0f, mShelfGain = 0.0f;
    float mPink0 = 0.0f, mPink1 = 0.0f, mPink2 = 0.0f, mPink3 = 0.0f;
    float mPink4 = 0.0f, mPink5 = 0.0f, mPink6 = 0.0f;
    bool mPinkEnabled = false;

    void Init(float sampleRate, float cutoffHz = 20000.0f)
    {
        const float fc = std::min(cutoffHz, sampleRate * 0.45f);
        mLpCoeff       = 1.0f - expDet(-2.0f * kPi * fc / sampleRate);
        mShelfCoeff    = 0.0f;
        mShelfGain     = 0.0f;
    }

    void SetHighShelf(float shelfFcHz, float hfBoostDb, float sampleRate)
    {
        const float fc = std::min(shelfFcHz, sampleRate * 0.45f);
        mShelfCoeff    = 1.0f - expDet(-2.0f * kPi * fc / sampleRate);
        mShelfGain     = abd::dsp::pow(10.0f, hfBoostDb / 20.0f) - 1.0f;
    }

    float Process(float whiteToPink = 0.0f)
    {
        mSeed             = mSeed * 196314165u + 907633515u;
        const float white = (2.0f * static_cast<float>(mSeed) / static_cast<float>(0xFFFFFFFFu)) - 1.0f;

        float noiseOut;
        if (mPinkEnabled)
        {
            mPink0           = 0.99886f * mPink0 + white * 0.0555179f;
            mPink1           = 0.99332f * mPink1 + white * 0.0750759f;
            mPink2           = 0.96900f * mPink2 + white * 0.1538520f;
            mPink3           = 0.86650f * mPink3 + white * 0.3104856f;
            mPink4           = 0.55000f * mPink4 + white * 0.5329522f;
            mPink5           = -0.7616f * mPink5 - white * 0.0168980f;
            const float pink = mPink0 + mPink1 + mPink2 + mPink3 + mPink4 + mPink5 + mPink6 + white * 0.5362f;
            mPink6           = white * 0.115926f;
            noiseOut         = (pink * 0.11f) * (1.0f - whiteToPink) + white * whiteToPink;
        }
        else
        {
            mLpState += mLpCoeff * (white - mLpState);
            noiseOut = mLpState;
        }

        if (mShelfGain != 0.0f)
        {
            mShelfState += mShelfCoeff * (noiseOut - mShelfState);
            return noiseOut + mShelfGain * (noiseOut - mShelfState);
        }
        return noiseOut;
    }
};

struct ClickRing
{
    float mFreqCoeff = 0.0f, mDamp = 0.0f, mLow = 0.0f, mBand = 0.0f;

    void Init(float resHz, float Q, float sampleRate)
    {
        mFreqCoeff = 2.0f * abd::dsp::sin(kPi * resHz / sampleRate);
        mDamp      = 1.0f / Q;
        Reset();
    }

    void Reset()
    {
        mLow  = 0.0f;
        mBand = 0.0f;
    }

    float Process(float input)
    {
        mLow += mFreqCoeff * mBand;
        const float high = input - mLow - mDamp * mBand;
        mBand += mFreqCoeff * high;
        return mLow;
    }
};

struct LeakNoise
{
    std::uint32_t mSeed = 0xDEADBEEFu;
    float mHpState = 0.0f, mHpCoeff = 0.0f, mLpState = 0.0f, mLpCoeff = 0.0f;

    void Init(float sampleRate, float hpCornerHz = 1500.0f)
    {
        mHpCoeff = 1.0f - expDet(-2.0f * kPi * hpCornerHz / sampleRate);
        mLpCoeff = 1.0f - expDet(-2.0f * kPi * 4000.0f / sampleRate);
    }

    float Process()
    {
        mSeed             = mSeed * 196314165u + 907633515u;
        const float white = (2.0f * static_cast<float>(mSeed) / static_cast<float>(0xFFFFFFFFu)) - 1.0f;
        mHpState += mHpCoeff * (white - mHpState);
        const float hp = white - mHpState;
        mLpState += mLpCoeff * (hp - mLpState);
        return mLpState;
    }
};

struct BBDClick
{
    int mCounter                             = -1;
    int mDurationSamples                     = 0;
    bool mWasInZone                          = false;
    static constexpr float kTriggerThreshold = 0.95f;
    static constexpr float kDurationMs       = 180.0f;

    void Init(float sampleRate)
    {
        mDurationSamples = static_cast<int>(kDurationMs * 0.001f * sampleRate);
    }

    void Reset()
    {
        mCounter   = -1;
        mWasInZone = false;
    }
    void Suppress()
    {
        mCounter   = -1;
        mWasInZone = true;
    }

    float Process(float lfoForLine)
    {
        const bool inZone = (lfoForLine > kTriggerThreshold);
        if (inZone && !mWasInZone) mCounter = 0;
        mWasInZone = inZone;

        if (mCounter < 0 || mCounter >= mDurationSamples)
        {
            mCounter = -1;
            return 0.0f;
        }

        constexpr float kAsymPoint       = 0.20f;
        constexpr float kSecondLobeScale = 0.85f;
        constexpr float kLeadDecayRate   = 4.0f;
        constexpr float kTrailDecayRate  = 8.0f;

        const float t = static_cast<float>(mCounter) / static_cast<float>(mDurationSamples);
        mCounter++;

        float out;
        if (t < kAsymPoint)
        {
            const float u = t / kAsymPoint;
            out           = -expDet(-kLeadDecayRate * u) * abd::dsp::sin(kPi * u);
        }
        else
        {
            const float u = (t - kAsymPoint) / (1.0f - kAsymPoint);
            out           = kSecondLobeScale * expDet(-kTrailDecayRate * u) * abd::dsp::sin(kPi * u);
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

    float mSatDrive       = 0.12f;
    float mSatDriveSmooth = 0.12f;
    float mSampleRate     = 44100.0f;

    void Init(float sampleRate, float biquadFc)
    {
        mSampleRate = sampleRate;

        const int minLen = static_cast<int>(sampleRate * 0.020f) + 4;
        int len          = 1;
        while (len < minLen) len <<= 1;
        mBuf.assign(static_cast<size_t>(len), 0.0f);
        mMask = len - 1;
        mWPos = 0;

        mPreFilter.Init(sampleRate, biquadFc);
        mPostFilter.Init(sampleRate, biquadFc);
    }

    void Clear()
    {
        std::fill(mBuf.begin(), mBuf.end(), 0.0f);
        mWPos = 0;
        mPreFilter.Reset();
        mPostFilter.Reset();
    }

    static float Hermite(float frac, float y0, float y1, float y2, float y3)
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    float ReadHermite(float delaySamples) const
    {
        float rPos = static_cast<float>(mWPos) - delaySamples;
        if (rPos < 0.0f) rPos += static_cast<float>(mMask + 1);

        const int i1     = static_cast<int>(rPos);
        const float frac = rPos - static_cast<float>(i1);

        return Hermite(frac,
                       mBuf[(i1 - 1) & mMask], mBuf[i1 & mMask],
                       mBuf[(i1 + 1) & mMask], mBuf[(i1 + 2) & mMask]);
    }

    float Process(float input, float delaySamples, float injectedNoise = 0.0f)
    {
        const float withNoise = input + injectedNoise;
        const float filtered  = mPreFilter.Process(withNoise);

        mSatDriveSmooth += (mSatDrive - mSatDriveSmooth) * 0.001f;
        const float sd  = mSatDriveSmooth;
        const float sat = (sd > 0.01f) ? abd::dsp::tanh(filtered * sd) / sd : filtered;

        mBuf[mWPos & mMask] = sat;
        const float wet     = ReadHermite(delaySamples);
        mWPos               = (mWPos + 1) & mMask;
        return mPostFilter.Process(wet);
    }
};

//==============================================================================
// --- ChorusBBD, verbatim -----------------------------------------------------
enum class Mode
{
    Off,
    ChorusI,
    ChorusII,
    ChorusBoth
};
enum class HissColour
{
    Pink,
    White,
    Dark
};

class ChorusBBDFrozen
{
public:
    void prepare(double sampleRate)
    {
        sr            = sampleRate;
        const float s = static_cast<float>(sr);

        lineL.Init(s, calFilterCutoff);
        lineR.Init(s, calFilterCutoff);

        clickL_Pri.Init(s);
        clickR_Pri.Init(s);
        clickL_Slo.Init(s);
        clickR_Slo.Init(s);

        leakNoiseL.Init(s, 800.0f);
        leakNoiseR.Init(s, 800.0f);

        clickRingL.Init(30.0f, 18.0f, s);
        clickRingR.Init(30.0f, 18.0f, s);

        wetNoiseL.Init(s);
        wetNoiseL.mPinkEnabled = true;
        wetNoiseR.Init(s);
        wetNoiseR.mPinkEnabled = true;
        wetNoiseL.mSeed        = 0x12345678u;
        wetNoiseR.mSeed        = 0x87654321u;
        wetNoiseL.SetHighShelf(3000.0f, 6.0f, s);
        wetNoiseR.SetHighShelf(3000.0f, 6.0f, s);

        wetRipple.SetMainsHz(60.0f, s);
        wetRipple.SetAmplitudes(7.9e-5f, 2.2e-5f, 9.8e-6f);

        reset();
    }

    void reset()
    {
        lineL.Clear();
        lineR.Clear();
        clickL_Pri.Reset();
        clickR_Pri.Reset();
        clickL_Slo.Reset();
        clickR_Slo.Reset();
        clickRingL.Reset();
        clickRingR.Reset();
        wetRipple.Reset();
        lfoPhase = 0.0;
    }

    void setMode(Mode m)
    {
        if (m != mode)
        {
            prevMode = mode;
            mode     = m;
            if (mode != Mode::Off)
            {
                clickL_Pri.Suppress();
                clickR_Pri.Suppress();
                clickL_Slo.Suppress();
                clickR_Slo.Suppress();
                clickRingL.Reset();
                clickRingR.Reset();
            }
        }
    }

    void setRate(float hz) { lfoRate = jlimit(0.1f, 15.0f, hz); }
    void setDepth(float d) { m_depth = jlimit(0.0f, 1.0f, d); }
    void setMix(float w) { wetMix = jlimit(0.0f, 1.0f, w); }
    void setHissLevel(float d) { hissLvlDb = jlimit(-96.0f, -40.0f, d); }
    void setHissColour(float c) { calHissColor = c; }
    void setHissMultiplier(float m) { hissMultiplier = jlimit(0.0f, 2.0f, m); }

    void setCalibrationParams(float dI, float dII, float dryGain, float wetGain,
                              float depth, float sat, float cutoff, float bothRate)
    {
        calDelayI       = dI;
        calDelayII      = dII;
        calGainDry      = dryGain;
        calGainWet      = wetGain;
        calModDepth     = depth;
        calSatBoost     = sat;
        calFilterCutoff = cutoff;
        calBothRate     = bothRate;
    }

    /** Un marco stereo. El original procesa un AudioBuffer; por bloque y por
        marco dan igual porque no hay estado a nivel de bloque. */
    void process(float inL, float inR, float& outL, float& outR)
    {
        if (mode == Mode::Off)
        {
            outL = inL;
            outR = inR;
            return;
        }

        const float s = static_cast<float>(sr);

        float baseDepthMs = 2.13f;
        if (mode == Mode::ChorusII)
            baseDepthMs = 1.71f;
        else if (mode == Mode::ChorusBoth)
            baseDepthMs = 0.236f;

        const float delayDepth = baseDepthMs * (calModDepth / 1.5f) * m_depth;

        const float centerDelayMs = (mode == Mode::ChorusII) ? calDelayII : calDelayI;
        const float currentRate   = (mode == Mode::ChorusBoth) ? calBothRate : lfoRate;
        const double phaseInc     = kPi * 2.0 * currentRate / sr;

        const float drive = 0.1f * calSatBoost;
        lineL.mSatDrive   = drive;
        lineR.mSatDrive   = drive;

        float lfo;
        if (mode == Mode::ChorusBoth)
        {
            lfo = abd::dsp::sin(static_cast<float>(lfoPhase));
        }
        else
        {
            const double norm = lfoPhase / (kPi * 2.0);
            lfo               = 1.0f - 4.0f * static_cast<float>(std::abs(norm - 0.5));
        }

        constexpr float kBBDClockTrim = 0.015f;
        float delay0Ms                = (centerDelayMs + delayDepth * lfo) * (1.0f - kBBDClockTrim);
        float delay1Ms                = (centerDelayMs - delayDepth * lfo) * (1.0f + kBBDClockTrim);

        constexpr float kMinDelayMs = 0.1f;
        delay0Ms                    = std::max(delay0Ms, kMinDelayMs);
        delay1Ms                    = std::max(delay1Ms, kMinDelayMs);

        const float delay0samp = delay0Ms * 0.001f * s;
        const float delay1samp = delay1Ms * 0.001f * s;

        constexpr float kMinClockHz = 5000.0f;
        float clock0                = 256.0f / (2.0f * delay0Ms * 0.001f);
        float clock1                = 256.0f / (2.0f * delay1Ms * 0.001f);
        clock0                      = std::max(clock0, kMinClockHz);
        clock1                      = std::max(clock1, kMinClockHz);

        constexpr float kLeakMinFrac = 0.0126f;
        const float invDepth         = (delayDepth > 1.0e-9f) ? 1.0f / delayDepth : 0.0f;
        const float lfo0             = (delay0Ms - centerDelayMs) * invDepth;
        const float lfo1             = (delay1Ms - centerDelayMs) * invDepth;
        const float lfoNorm0         = (lfo0 + 1.0f) * 0.5f;
        const float lfoNorm1         = (lfo1 + 1.0f) * 0.5f;
        const float leakAmount0      = delayDepth * (kLeakMinFrac + (1.0f - kLeakMinFrac) * lfoNorm0);
        const float leakAmount1      = delayDepth * (kLeakMinFrac + (1.0f - kLeakMinFrac) * lfoNorm1);

        const float noiseN0 = leakNoiseL.Process();
        const float noiseN1 = leakNoiseR.Process();

        const float baseNoiseGain = abd::dsp::pow(10.0f, hissLvlDb / 20.0f);
        const float wetPink0      = wetNoiseL.Process(calHissColor) * baseNoiseGain * hissMultiplier;
        const float wetPink1      = wetNoiseR.Process(calHissColor) * baseNoiseGain * hissMultiplier;

        const float gainModScale          = delayDepth / 2.13f;
        constexpr float kBBDClickGain     = 0.11f;
        constexpr float kBBDSlowClickGain = 0.022f;

        const float click0Pri = clickL_Pri.Process(-lfo) * kBBDClickGain * gainModScale;
        const float click1Pri = clickR_Pri.Process(lfo) * kBBDClickGain * gainModScale;
        const float click0Slo = clickL_Slo.Process(lfo) * kBBDSlowClickGain * gainModScale;
        const float click1Slo = clickR_Slo.Process(-lfo) * kBBDSlowClickGain * gainModScale;

        constexpr float kBBDLeakageGain = 8.8e-3f;
        const float bbdIn0              = (wetPink0 + noiseN0 * kBBDLeakageGain * leakAmount0 + click0Pri - click0Slo) * hissMultiplier;
        const float bbdIn1              = (wetPink1 + noiseN1 * kBBDLeakageGain * leakAmount1 + click1Pri - click1Slo) * hissMultiplier;

        float wet0 = lineL.Process(inL, delay0samp, bbdIn0);
        float wet1 = lineR.Process(inR, delay1samp, bbdIn1);

        constexpr float kBBDGainTrim       = 0.04f;
        constexpr float kBBDCTELossCoeff   = 4468.0f;
        constexpr float kBBDInvClockCenter = 1.0f / 40000.0f;

        float gain0 = (1.0f + kBBDGainTrim) * (1.0f - kBBDCTELossCoeff * (1.0f / clock0 - kBBDInvClockCenter));
        float gain1 = (1.0f - kBBDGainTrim) * (1.0f - kBBDCTELossCoeff * (1.0f / clock1 - kBBDInvClockCenter));
        wet0 *= gain0;
        wet1 *= gain1;

        constexpr float kClickRingGain = 0.06f;
        wet0 += clickRingL.Process(click0Pri) * kClickRingGain * hissMultiplier;
        wet1 += clickRingR.Process(click1Pri) * kClickRingGain * hissMultiplier;

        const float ripple = wetRipple.Process() * hissMultiplier;
        wet0 += ripple;
        wet1 += ripple;

        const float dryMix       = 1.0f - wetMix * (1.0f - calGainDry);
        const float wetMixAmount = wetMix * calGainWet;

        outL = dryMix * inL + wetMixAmount * wet0;
        outR = dryMix * inR + wetMixAmount * wet1;

        lfoPhase += phaseInc;
        if (lfoPhase >= kPi * 2.0) lfoPhase -= kPi * 2.0;
    }

private:
    Mode mode{Mode::Off};
    float lfoRate   = 0.513f;
    float m_depth   = 0.65f;
    float wetMix    = 0.50f;
    float hissLvlDb = -52.0f;
    double sr       = 44100.0;

    float calDelayI = 3.3f, calDelayII = 3.3f;
    float calGainDry = 0.863f, calGainWet = 1.257f;
    float calModDepth = 1.5f, calSatBoost = 1.2f;
    float calFilterCutoff = 9661.0f, calBothRate = 7.7f;
    float calHissColor   = 0.4f;
    float hissMultiplier = 1.0f;

    BBDLine lineL, lineR;
    BBDClick clickL_Pri, clickR_Pri, clickL_Slo, clickR_Slo;
    ClickRing clickRingL, clickRingR;
    LeakNoise leakNoiseL, leakNoiseR;
    AnalogFloorNoise wetNoiseL, wetNoiseR;
    RailRipple wetRipple;

    double lfoPhase = 0.0;
    Mode prevMode{Mode::Off};
};

} // namespace juno_frozen
