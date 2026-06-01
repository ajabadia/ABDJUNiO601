#include <JuceHeader.h>
#include "JunoVCF.h"
#include "../Core/CalibrationSettings.h"
#include <cmath>
#include <algorithm>

static constexpr double kResamplerCoefs2x[12] = {
    0.036681502163648017, 0.13654762463195794, 0.27463175937945444,
    0.42313861743656711, 0.56109869787919531, 0.67754004997416184,
    0.76974183386322703, 0.83988962484963892, 0.89226081800387902,
    0.9315419599631839,  0.96209454837808417, 0.98781637073289585
};

JunoVCF::JunoVCF()
{
    mUp1.set_coefs(kResamplerCoefs2x);
    mUp2.set_coefs(kResamplerCoefs2x);
    mDown1.set_coefs(kResamplerCoefs2x);
    mDown2.set_coefs(kResamplerCoefs2x);
    reset();
}

void JunoVCF::reset()
{
    s.fill (0.0f);
    lastOutput = 0.0f;
    mUp1.clear_buffers();
    mUp2.clear_buffers();
    mDown1.clear_buffers();
    mDown2.clear_buffers();
    mInputEnv = 0.0f;
}

void JunoVCF::setSampleRate (double sr)
{
    jassert (sr > 0.0);
    sampleRate    = sr;
    invSampleRate = 1.0f / (float) sr;
    mEnvDecay = std::exp(-1.f / (0.022f * (float)sampleRate * static_cast<float>(mOversample)));
}

void JunoVCF::setOversample(int factor)
{
    int prev = mOversample;
    mOversample = (factor <= 1) ? 1 : (factor == 2) ? 2 : 4;
    mEnvDecay = std::exp(-1.f / (0.022f * (float)sampleRate * static_cast<float>(mOversample)));
    if (mOversample == 4 && prev == 2)
    {
        mUp2.clear_buffers();
        mDown2.clear_buffers();
    }
}

void JunoVCF::setModelAndResCurve(bool j106Res, bool otaSaturation)
{
    mJ106Res = j106Res;
    mOTASaturation = otaSaturation;
}

// ------------------------------------------------------------
// Curva de cutoff: MCU Aritmética 14-bits y tabla J106DACHzTable
// ------------------------------------------------------------
float JunoVCF::computeCutoffHz (float cutoff01,
                                 float envAmount,
                                 float envVal,
                                 bool envInverted,
                                 float lfoAmount,
                                 float lfoVal,
                                 float kybdTrack,
                                 float currentFreqHz,
                                 float benderValue,
                                 float benderToVCF,
                                 float vcfWidth,
                                 float vcfFrqTrim,
                                 CalibrationSettings* cal) const
{
    uint16_t vcfCutoff = static_cast<uint16_t>(cutoff01 * 16256.0f); // 0x0000 - 0x3F80
    
    float lfoMod = lfoAmount * lfoVal;
    bool lfoPolarity = lfoMod < 0.0f;
    uint16_t vcfLfoSignal = static_cast<uint16_t>(std::abs(lfoMod) * 16383.0f);
    
    uint8_t vcfBendSens = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, benderToVCF) * 255.0f);
    uint8_t bendVal = static_cast<uint8_t>(std::abs(benderValue) * 255.0f);
    uint16_t vcfBendAmt = static_cast<uint16_t>((static_cast<uint16_t>(vcfBendSens) * bendVal) >> 4);
    bool bendPolarity = benderValue < 0.0f;

    float envRange = cal != nullptr ? cal->getValue("vcfEnvRange") : 2.0f;
    float sliderEnvAmount = envAmount / (envRange > 0.0f ? envRange : 2.0f);
    uint8_t vcfEnvMod = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, sliderEnvAmount) * 254.0f);
    uint16_t envelope = static_cast<uint16_t>(juce::jlimit(0.0f, 1.0f, envVal) * 16383.0f);

    uint8_t vcfKeyTrack = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, kybdTrack) * 254.0f);
    
    float midiNote = std::log2(std::max(1.0f, currentFreqHz) / 440.0f) * 12.0f + 69.0f;
    uint16_t pitch = static_cast<uint16_t>(std::max(0.0f, midiNote) * 256.0f);

    uint16_t ea = vcfCutoff;
    bool overflow = false;

    if (!lfoPolarity) ea = vcf_add(ea, vcfLfoSignal, overflow);
    else              ea = vcf_sub(ea, vcfLfoSignal, overflow);

    if (!bendPolarity) ea = vcf_add(ea, vcfBendAmt, overflow);
    else               ea = vcf_sub(ea, vcfBendAmt, overflow);

    uint16_t scaledEnv = mul8x16_hi(vcfEnvMod, envelope);
    scaledEnv = static_cast<uint16_t>(juce::jlimit(0.0f, 65535.0f, (float)scaledEnv * envRange));
    if (!envInverted) ea = vcf_add(ea, scaledEnv, overflow);
    else              ea = vcf_sub(ea, scaledEnv, overflow);

    uint16_t pScaled = (pitch >> 2) + (pitch >> 3); // pitch * 0.375
    static constexpr uint16_t MIDDLE_C_SCALED = 0x1680; // MIDI 60 * 256 * 0.375

    if (pScaled > MIDDLE_C_SCALED) {
        uint16_t keyDelta = mul8x16_hi(vcfKeyTrack, pScaled - MIDDLE_C_SCALED);
        ea = vcf_add(ea, keyDelta, overflow);
    } else {
        uint16_t keyDelta = mul8x16_hi(vcfKeyTrack, MIDDLE_C_SCALED - pScaled);
        ea = vcf_sub(ea, keyDelta, overflow);
    }

    ea = vcf_clamp(ea, overflow);

    float cv_lin = static_cast<float>(ea) * vcfWidth + vcfFrqTrim;
    int internal_cv = static_cast<int>(cv_lin + 0.5f);
    if (internal_cv < 0) internal_cv = 0;
    if (internal_cv > 16383) internal_cv = 16383;
    
    int code = internal_cv >> 2; // 12-bit DAC chip code (0..4095)
    
    float cutoffHz = 1000.0f;
    if (cal) {
        cutoffHz = cal->getDacHz(code);
    }

    return cutoffHz;
}

// ------------------------------------------------------------
// Feedback de resonancia con autooscilación suave
// ------------------------------------------------------------
float JunoVCF::computeResonanceFeedback (float res01, float selfOscThreshold, float selfOscInt) const
{
    if (res01 < selfOscThreshold)
    {
        return res01 * (4.0f / selfOscThreshold);
    }

    const float excess = (res01 - selfOscThreshold) / (1.0f - selfOscThreshold);
    return 4.0f + excess * selfOscInt;
}

// ------------------------------------------------------------
// Proceso ZDF VCF principal
// ------------------------------------------------------------
float JunoVCF::processSample (float input,
                              float cutoff01,
                              float resonance,
                              float envAmount,
                              float envVal,
                              bool envInverted,
                              float lfoAmount,
                              float lfoVal,
                              float kybdTrack,
                              float currentFreqHz,
                              float benderValue,
                              float benderToVCF,
                              float selfOscThreshold,
                              float saturationScale,
                              float selfOscInt,
                              float vcfWidth,
                              float vcfFrqTrim,
                              CalibrationSettings* cal)
{
    float cutoffHz = computeCutoffHz (cutoff01, envAmount, envVal, envInverted, lfoAmount, lfoVal,
                                      kybdTrack, currentFreqHz, benderValue, benderToVCF, vcfWidth, vcfFrqTrim, cal);
    
    float safeMaxHz = (float)sampleRate * 0.45f;
    cutoffHz = juce::jlimit(10.0f, safeMaxHz, cutoffHz);

    float frq = cutoffHz / (sampleRate * 0.5f);

    float resPolK = cal != nullptr ? cal->getValue("vcfResPolK") : 1.24f;
    float fbScaleVal = cal != nullptr ? cal->getValue("vcfFbScale") : 4.20f;

    float k = mJ106Res ? (resPolK * computeResonanceFeedback (resonance, selfOscThreshold, selfOscInt))
                       : ResK_J6 (resonance);
    if (!mJ106Res) k = SoftClipK (k);

    if (frq > 0.5f) {
        k *= std::max(1.f - (frq - 0.5f) * 1.f, 0.5f);
    }

    mFreqComp = FreqCompensationClamped(k, frq * 0.25f);

    if (mOversample == 4)
        lastOutput = process4x (input, frq, resonance) * (saturationScale > 0.0f ? saturationScale : 1.0f);
    else if (mOversample == 2)
        lastOutput = process2x (input, frq, resonance) * (saturationScale > 0.0f ? saturationScale : 1.0f);
    else
        lastOutput = processSampleInternal (input, frq, resonance) * (saturationScale > 0.0f ? saturationScale : 1.0f);

    return lastOutput;
}

float JunoVCF::process2x (float input, float frq, float res)
{
    float up[2], down[2];
    mUp1.process_sample(up[0], up[1], input);

    float frq2x = frq * 0.5f;
    down[0] = processSampleInternal(up[0], frq2x, res);
    down[1] = processSampleInternal(up[1], frq2x, res);

    return mDown1.process_sample(down);
}

float JunoVCF::process4x (float input, float frq, float res)
{
    float frq4x = frq * 0.25f;

    float up2x[2];
    mUp1.process_sample(up2x[0], up2x[1], input);

    float down4x[2], down2x[2];

    float up4x_a[2];
    mUp2.process_sample(up4x_a[0], up4x_a[1], up2x[0]);
    down4x[0] = processSampleInternal(up4x_a[0], frq4x, res);
    down4x[1] = processSampleInternal(up4x_a[1], frq4x, res);
    down2x[0] = mDown2.process_sample(down4x);

    float up4x_b[2];
    mUp2.process_sample(up4x_b[0], up4x_b[1], up2x[1]);
    down4x[0] = processSampleInternal(up4x_b[0], frq4x, res);
    down4x[1] = processSampleInternal(up4x_b[1], frq4x, res);
    down2x[1] = mDown2.process_sample(down4x);

    return mDown1.process_sample(down2x);
}

float JunoVCF::processSampleInternal (float input, float frq, float res)
{
    mNoiseSeed = mNoiseSeed * 196314165u + 907633515u;
    float white = static_cast<float>(mNoiseSeed) / static_cast<float>(0xFFFFFFFFu) * 2.f - 1.f;
    mInputEnv = std::max(std::abs(input), mInputEnv * mEnvDecay);
    float stateEnergy = std::abs(s[0]) + std::abs(s[1]) + std::abs(s[2]) + std::abs(s[3]);
    float energy = std::max(mInputEnv, stateEnergy);
    float noiseLevel = 1e-2f / (static_cast<float>(mOversample) * (1.f + energy * 1000.f));
    input += white * noiseLevel;

    float k = mJ106Res ? ResK_J106(res) : ResK_J6(res);
    if (!mJ106Res) k = SoftClipK(k);
    if (frq > 0.5f)
        k *= std::max(1.f - (frq - 0.5f) * 1.f, 0.5f);

    float frqUnclamped = frq;
    frq = std::min(frq, 0.85f);
    float g = std::tan(frq * juce::MathConstants<float>::pi * 0.5f);
    g *= mFreqComp;

    float g1 = g / (1.f + g);
    float G = g1 * g1 * g1 * g1;

    float S = s[0] * g1 * g1 * g1 + s[1] * g1 * g1 + s[2] * g1 + s[3];

    float comp = InputComp(k, frq);

    float kFbScale = 4.20f * std::clamp((k - 2.5f) * 1.0f, 0.3f, 1.f);
    float fbSig = OTASat(S * kFbScale) / kFbScale;

    float u = (input * comp - k * fbSig) / (1.f + k * G);

    float lp4;
    if (mOTASaturation)
    {
        float stateAmp = std::abs(s[3]);
        float dfGain = 1.f / std::sqrt(1.f + 0.6f * stateAmp * stateAmp);
        dfGain = std::max(dfGain, 0.65f);

        float hfFade = juce::jlimit(0.f, 1.f, (0.12f - frq) * 25.f);
        dfGain = 1.f - hfFade * (1.f - dfGain);
        float g1NL = g1 / dfGain;
        g1NL = std::min(g1NL, 0.98f);

        float gNL = g1NL / (1.f - g1NL);
        float ota = OTAScaleForFreq(frq, res);

        float lp1 = NLStage(s[0], u, gNL, g1NL, ota);
        float lp2 = NLStage(s[1], lp1, gNL, g1NL, ota);
        float lp3 = NLStage(s[2], lp2, gNL, g1NL, ota);
        lp4 = NLStage(s[3], lp3, gNL, g1NL, ota);
    }
    else
    {
        float g1L = g1 * (1.f + k * 0.0003f);
        float v, st;
        st = s[0]; v = g1L * (u - st);   s[0] = st + 2.f * v; float lp1 = st + v;
        st = s[1]; v = g1L * (lp1 - st); s[1] = st + 2.f * v; float lp2 = st + v;
        st = s[2]; v = g1L * (lp2 - st); s[2] = st + 2.f * v; float lp3 = st + v;
        st = s[3]; v = g1L * (lp3 - st); s[3] = st + 2.f * v; lp4       = st + v;
    }

    for (auto& st : s) {
        if (std::abs(st) < 1e-15f) {
            st = 0.f;
        }
    }

    float frqFactor = juce::jlimit(0.f, 1.f, (frqUnclamped - 0.02f) * 20.f);
    float resFactor = juce::jlimit(0.f, 1.f, (k - 2.f) * 0.5f);
    float outputScale = 1.f - frqFactor * resFactor * 0.5f;
    return lp4 * 3.22f * outputScale;
}
