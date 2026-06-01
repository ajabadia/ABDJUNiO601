#include "ChorusBBD.h"
#include "../Core/JunoConstants.h"

ChorusBBD::ChorusBBD()
{
    reset();
}

void ChorusBBD::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;
    
    lineL.Init((float)sr, calFilterCutoff);
    lineR.Init((float)sr, calFilterCutoff);

    clickL_Pri.Init((float)sr);
    clickR_Pri.Init((float)sr);
    clickL_Slo.Init((float)sr);
    clickR_Slo.Init((float)sr);

    leakNoiseL.Init((float)sr, 800.0f);
    leakNoiseR.Init((float)sr, 800.0f);

    clickRingL.Init(30.0f, 18.0f, (float)sr);
    clickRingR.Init(30.0f, 18.0f, (float)sr);

    wetNoiseL.Init((float)sr);
    wetNoiseL.mPinkEnabled = true;
    wetNoiseR.Init((float)sr);
    wetNoiseR.mPinkEnabled = true;
    wetNoiseL.mSeed = 0x12345678u;
    wetNoiseR.mSeed = 0x87654321u;
    wetNoiseL.SetHighShelf(3000.0f, 6.0f, (float)sr);
    wetNoiseR.SetHighShelf(3000.0f, 6.0f, (float)sr);

    wetRipple.SetMainsHz(60.0f, (float)sr);
    wetRipple.SetAmplitudes(7.9e-5f, 2.2e-5f, 9.8e-6f);

    reset();
}

void ChorusBBD::reset()
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

void ChorusBBD::setMode(Mode m)
{
    if (m != mode)
    {
        prevMode = mode;
        mode = m;
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

void ChorusBBD::process(juce::AudioBuffer<float>& buffer)
{
    if (mode == Mode::Off) return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float* L = buffer.getWritePointer(0);
    float* R = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    // Map base depths in ms for the physical chorus
    float baseDepthMs = 2.13f;
    if (mode == Mode::ChorusII)        baseDepthMs = 1.71f;
    else if (mode == Mode::ChorusBoth) baseDepthMs = 0.236f;

    // Scale by calModDepth (default 1.5ms) and user depth
    float delayDepth = baseDepthMs * (calModDepth / 1.5f) * m_depth;

    const float centerDelayMs = (mode == Mode::ChorusII) ? calDelayII : calDelayI;
    const float currentRate = (mode == Mode::ChorusBoth) ? calBothRate : lfoRate;
    const double phaseInc = juce::MathConstants<double>::twoPi * currentRate / sr;

    // Apply sat boost to line saturation
    float drive = 0.1f * calSatBoost;
    lineL.mSatDrive = drive;
    lineR.mSatDrive = drive;

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = L[s];
        const float inR = R ? R[s] : inL;

        float outL = inL;
        float outR = inR;

        // Calculate LFO value
        float lfo;
        if (mode == Mode::ChorusBoth)
        {
            lfo = (float)std::sin(lfoPhase);
        }
        else
        {
            double norm = lfoPhase / juce::MathConstants<double>::twoPi;
            lfo = 1.0f - 4.0f * (float)std::abs(norm - 0.5);
        }

        // Delay-domain modulation with BBD Clock network tolerances
        constexpr float kBBDClockTrim = 0.015f;
        float delay0Ms = (centerDelayMs + delayDepth * lfo) * (1.f - kBBDClockTrim);
        float delay1Ms = (centerDelayMs - delayDepth * lfo) * (1.f + kBBDClockTrim);

        constexpr float kMinDelayMs = 0.1f;
        delay0Ms = std::max(delay0Ms, kMinDelayMs);
        delay1Ms = std::max(delay1Ms, kMinDelayMs);

        float delay0samp = delay0Ms * 0.001f * (float)sr;
        float delay1samp = delay1Ms * 0.001f * (float)sr;

        // Clock rates in Hz for CTE-loss gain compensation
        constexpr float kMinClockHz = 5000.f;
        float clock0 = 256.f / (2.f * delay0Ms * 0.001f);
        float clock1 = 256.f / (2.f * delay1Ms * 0.001f);
        clock0 = std::max(clock0, kMinClockHz);
        clock1 = std::max(clock1, kMinClockHz);

        // BBD Leakage Noise
        constexpr float kLeakMinFrac = 0.0126f;
        float invDepth = (delayDepth > 1e-9f) ? 1.f / delayDepth : 0.f;
        float lfo0 = (delay0Ms - centerDelayMs) * invDepth;
        float lfo1 = (delay1Ms - centerDelayMs) * invDepth;

        float lfoNorm0 = (lfo0 + 1.f) * 0.5f;
        float lfoNorm1 = (lfo1 + 1.f) * 0.5f;

        float leakAmount0 = delayDepth * (kLeakMinFrac + (1.f - kLeakMinFrac) * lfoNorm0);
        float leakAmount1 = delayDepth * (kLeakMinFrac + (1.f - kLeakMinFrac) * lfoNorm1);

        float noiseN0 = leakNoiseL.Process();
        float noiseN1 = leakNoiseR.Process();

        float baseNoiseGain = std::pow(10.f, hissLvlDb / 20.f);
        float wetPink0 = wetNoiseL.Process(calHissColor) * baseNoiseGain * hissMultiplier;
        float wetPink1 = wetNoiseR.Process(calHissColor) * baseNoiseGain * hissMultiplier;

        // BBD clicks
        float gainModScale = delayDepth / 2.13f;
        constexpr float kBBDClickGain = 0.11f;
        constexpr float kBBDSlowClickGain = 0.022f;

        float click0Pri = clickL_Pri.Process(-lfo) * kBBDClickGain * gainModScale;
        float click1Pri = clickR_Pri.Process(lfo) * kBBDClickGain * gainModScale;
        float click0Slo = clickL_Slo.Process(lfo) * kBBDSlowClickGain * gainModScale;
        float click1Slo = clickR_Slo.Process(-lfo) * kBBDSlowClickGain * gainModScale;

        constexpr float kBBDLeakageGain = 8.8e-3f;
        float bbdIn0 = (wetPink0 + noiseN0 * kBBDLeakageGain * leakAmount0 + click0Pri - click0Slo) * hissMultiplier;
        float bbdIn1 = (wetPink1 + noiseN1 * kBBDLeakageGain * leakAmount1 + click1Pri - click1Slo) * hissMultiplier;

        // Process through the Hermite BBD Line with pre/post reconstruction filters
        float wet0 = lineL.Process(inL, delay0samp, bbdIn0);
        float wet1 = lineR.Process(inR, delay1samp, bbdIn1);

        // BBD charge-transfer efficiency loss gain modulation
        constexpr float kBBDGainTrim = 0.04f;
        constexpr float kBBDCTELossCoeff = 4468.f;
        constexpr float kBBDInvClockCenter = 1.f / 40000.f;

        float gain0 = (1.f + kBBDGainTrim) * (1.f - kBBDCTELossCoeff * (1.f / clock0 - kBBDInvClockCenter));
        float gain1 = (1.f - kBBDGainTrim) * (1.f - kBBDCTELossCoeff * (1.f / clock1 - kBBDInvClockCenter));
        wet0 *= gain0;
        wet1 *= gain1;

        // Per-channel LF ringing excited by clicks
        constexpr float kClickRingGain = 0.06f;
        wet0 += clickRingL.Process(click0Pri) * kClickRingGain * hissMultiplier;
        wet1 += clickRingR.Process(click1Pri) * kClickRingGain * hissMultiplier;

        // Mains ripple
        float ripple = wetRipple.Process() * hissMultiplier;
        wet0 += ripple;
        wet1 += ripple;

        // IC6 Inverting summer dry/wet mix
        float dryMix = 1.f - wetMix * (1.f - calGainDry);
        float wetMixAmount = wetMix * calGainWet;

        outL = dryMix * inL + wetMixAmount * wet0;
        outR = dryMix * inR + wetMixAmount * wet1;

        L[s] = outL;
        if (R) R[s] = outR;

        lfoPhase += phaseInc;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;
    }
}
