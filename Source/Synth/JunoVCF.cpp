#include <JuceHeader.h>
#include "JunoVCF.h"
#include "../Core/CalibrationSettings.h"
#include <cmath>
#include <algorithm>

JunoVCF::JunoVCF()
{
    reset();
}

void JunoVCF::reset()
{
    s.fill (0.0f);
    lastOutput = 0.0f;
}

void JunoVCF::setSampleRate (double sr)
{
    jassert (sr > 0.0);
    sampleRate    = sr;
    invSampleRate = 1.0f / (float) sr;
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
    // Escalar parámetros a los rangos de la MCU
    uint16_t vcfCutoff = static_cast<uint16_t>(cutoff01 * 16256.0f); // 0x0000 - 0x3F80
    
    // LFO (LFO Amount * LFO Val)
    float lfoMod = lfoAmount * lfoVal;
    bool lfoPolarity = lfoMod < 0.0f;
    uint16_t vcfLfoSignal = static_cast<uint16_t>(std::abs(lfoMod) * 16383.0f);
    
    // Bender: Emulando la CPU/firmware de 16 bits del Juno-106 con limitación del ~25% del DAC (máx 4064 en ea).
    uint8_t vcfBendSens = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, benderToVCF) * 255.0f);
    uint8_t bendVal = static_cast<uint8_t>(std::abs(benderValue) * 255.0f);
    uint16_t vcfBendAmt = static_cast<uint16_t>((static_cast<uint16_t>(vcfBendSens) * bendVal) >> 4);
    bool bendPolarity = benderValue < 0.0f;

    // Env: Recuperar el valor real del slider de la UI dividiendo por el rango de calibración
    float envRange = cal != nullptr ? cal->getValue("vcfEnvRange") : 2.0f;
    float sliderEnvAmount = envAmount / (envRange > 0.0f ? envRange : 2.0f);
    uint8_t vcfEnvMod = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, sliderEnvAmount) * 254.0f);
    uint16_t envelope = static_cast<uint16_t>(juce::jlimit(0.0f, 1.0f, envVal) * 16383.0f);

    // Keytracking
    uint8_t vcfKeyTrack = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, kybdTrack) * 254.0f);
    
    // Pitch a punto fijo 8.8 (MIDI Note * 256)
    float midiNote = std::log2(std::max(1.0f, currentFreqHz) / 440.0f) * 12.0f + 69.0f;
    uint16_t pitch = static_cast<uint16_t>(std::max(0.0f, midiNote) * 256.0f);

    // -- Paso 1: Base (Cutoff ± LFO ± Bend)
    uint16_t ea = vcfCutoff;
    bool overflow = false;

    if (!lfoPolarity) ea = vcf_add(ea, vcfLfoSignal, overflow);
    else              ea = vcf_sub(ea, vcfLfoSignal, overflow);

    if (!bendPolarity) ea = vcf_add(ea, vcfBendAmt, overflow);
    else               ea = vcf_sub(ea, vcfBendAmt, overflow);

    // -- Paso 2: Envelope
    uint16_t scaledEnv = mul8x16_hi(vcfEnvMod, envelope);
    scaledEnv = static_cast<uint16_t>(juce::jlimit(0.0f, 65535.0f, (float)scaledEnv * envRange));
    if (!envInverted) ea = vcf_add(ea, scaledEnv, overflow);
    else              ea = vcf_sub(ea, scaledEnv, overflow);

    // -- Paso 3: Keytracking
    uint16_t pScaled = (pitch >> 2) + (pitch >> 3); // pitch * 0.375
    static constexpr uint16_t MIDDLE_C_SCALED = 0x1680; // MIDI 60 * 256 * 0.375

    if (pScaled > MIDDLE_C_SCALED) {
        uint16_t keyDelta = mul8x16_hi(vcfKeyTrack, pScaled - MIDDLE_C_SCALED);
        ea = vcf_add(ea, keyDelta, overflow);
    } else {
        uint16_t keyDelta = mul8x16_hi(vcfKeyTrack, MIDDLE_C_SCALED - pScaled);
        ea = vcf_sub(ea, keyDelta, overflow);
    }

    // -- Paso 4: Clamp a 14-bits
    ea = vcf_clamp(ea, overflow);

    // -- Paso 5: DAC To Hz con J106DACHzTable
    // Aplicar width trim y frequency trim (C2 Fix)
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
// k < 4.0 → filtrado normal
// k > 4.0 → autooscilación (igual que el 80017A)
// ------------------------------------------------------------
float JunoVCF::computeResonanceFeedback (float res01, float selfOscThreshold, float selfOscInt) const
{
    if (res01 < selfOscThreshold)
    {
        // Escala lineal hasta el umbral
        // 4.0 = límite teórico de oscilación en ladder de 4 polos
        return res01 * (4.0f / selfOscThreshold);
    }

    // Superación suave del umbral → autooscilación natural
    const float excess = (res01 - selfOscThreshold)
                       / (1.0f - selfOscThreshold);

    // [Build 29] Calibrated Self-Osc Intensity
    return 4.0f + excess * selfOscInt;
}

// ------------------------------------------------------------
// Núcleo TPT (Trapezoidal Piecewise) de 4 etapas OTA
//
// El pre-warping con tan(π·fc/fs) corrige el error de frecuencia
// de los filtros digitales bilineales a cutoffs altos (>5 kHz),
// replicando el comportamiento del circuito continuo del IR3109.
//
// Saturación por etapa + en la entrada con feedback modelan
// el carácter no lineal del OTA BA662 del 80017A.
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
    // 1. Frecuencia de corte modulada usando MCU aritmética
    float cutoffHz = computeCutoffHz (cutoff01, envAmount, envVal, envInverted, lfoAmount, lfoVal,
                                      kybdTrack, currentFreqHz, benderValue, benderToVCF, vcfWidth, vcfFrqTrim, cal);
    
    // Limitar para evitar aliasing masivo o inestabilidad
    float safeMaxHz = (float)sampleRate * 0.45f;
    cutoffHz = juce::jlimit(10.0f, safeMaxHz, cutoffHz);

    // 2. Coeficiente de frecuencia normalizado a Nyquist
    float frq = cutoffHz / (sampleRate * 0.5f);

    // Recuperar parámetros de calibración personalizados
    float resPolK = cal != nullptr ? cal->getValue("vcfResPolK") : 1.24f;
    float fbScaleVal = cal != nullptr ? cal->getValue("vcfFbScale") : 4.20f;

    // 3. Feedback de resonancia usando hardware ResK_J106 polinómico y calibración dinámica
    float k = resPolK * computeResonanceFeedback (resonance, selfOscThreshold, selfOscInt);
    
    // Limitar resonancia a frecuencias altas para evitar inestabilidad
    if (frq > 0.5f) {
        k *= std::max(1.f - (frq - 0.5f) * 1.f, 0.5f);
    }

    // 4. FreqCompensationClamped
    float freqComp = FreqCompensationClamped(k, frq * 0.25f);

    // 5. Coeficiente g (bilinear transform pre-warping)
    float frqClamped = std::min(frq, 0.85f);
    float g = std::tan(frqClamped * juce::MathConstants<float>::pi * 0.5f);
    g *= freqComp;

    float g1 = g / (1.f + g);
    float G = g1 * g1 * g1 * g1;

    // 6. Zero-Delay Feedback (ZDF) Resolution
    // Señal proyectada (S) a través de las 4 etapas de integración
    float S = s[0] * g1 * g1 * g1 + s[1] * g1 * g1 + s[2] * g1 + s[3];

    // Q/input compensation
    float comp = InputComp(k, frq);

    // BA662 feedback saturation
    float kFbScale = fbScaleVal * std::clamp((k - 2.5f) * 1.0f, 0.3f, 1.f);
    float fbSig = OTASat(S * kFbScale) / kFbScale;

    // Solve loop para la entrada de la etapa 0 (u)
    float u = (input * comp - k * fbSig) / (1.f + k * G);

    // 7. Resolver etapas no lineales usando Newton-Raphson
    float stateAmp = std::abs(s[3]);
    float dfGain = 1.f / std::sqrt(1.f + 0.6f * stateAmp * stateAmp);
    dfGain = std::max(dfGain, 0.65f);

    float hfFade = juce::jlimit(0.f, 1.f, (0.12f - frq) * 25.f);
    dfGain = 1.f - hfFade * (1.f - dfGain);

    float g1NL = g1 / dfGain;
    g1NL = std::min(g1NL, 0.98f);
    float gNL = g1NL / (1.f - g1NL);

    float otaScale = OTAScaleForFreq(frq, resonance);

    float lp1 = NLStage(s[0], u, gNL, g1NL, otaScale);
    float lp2 = NLStage(s[1], lp1, gNL, g1NL, otaScale);
    float lp3 = NLStage(s[2], lp2, gNL, g1NL, otaScale);
    float lp4 = NLStage(s[3], lp3, gNL, g1NL, otaScale);

    // Eliminar denormales de los estados integradores
    for (auto& st : s) {
        if (std::abs(st) < 1e-15f) {
            st = 0.f;
        }
    }

    // Compensación de ganancia de salida
    float frqFactor = juce::jlimit(0.f, 1.f, (frq - 0.02f) * 20.f);
    float resFactor = juce::jlimit(0.f, 1.f, (k - 2.f) * 0.5f);
    float outputScale = 1.f - frqFactor * resFactor * 0.5f;

    lastOutput = lp4 * 3.22f * outputScale * (saturationScale > 0.0f ? saturationScale : 1.0f);
    return lastOutput;
}
