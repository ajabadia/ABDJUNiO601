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
// Saturación Padé 3/3
// Aproxima tanh sin llamadas a libm; simétrica, suave, estable
// Error < 0.5% en [-2, 2]
// ------------------------------------------------------------
inline float JunoVCF::stageSaturate (float x, float scale) noexcept
{
    x = juce::jlimit (-4.0f, 4.0f, x * scale);
    const float x2 = x * x;
    return (x * (27.0f + x2) / (27.0f + 9.0f * x2)) / std::max(0.001f, scale);
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
                                 CalibrationSettings* cal) const
{
    // Escalar parámetros a los rangos de la MCU
    uint16_t vcfCutoff = static_cast<uint16_t>(cutoff01 * 16256.0f); // 0x0000 - 0x3F80
    
    // LFO (LFO Amount * LFO Val)
    float lfoMod = lfoAmount * lfoVal;
    bool lfoPolarity = lfoMod < 0.0f;
    uint16_t vcfLfoSignal = static_cast<uint16_t>(std::abs(lfoMod) * 16383.0f);
    
    // Bender
    float bendMod = benderValue * benderToVCF;
    bool bendPolarity = bendMod < 0.0f;
    uint16_t vcfBendAmt = static_cast<uint16_t>(std::abs(bendMod) * 16383.0f);

    // Env
    uint8_t vcfEnvMod = static_cast<uint8_t>(juce::jlimit(0.0f, 1.0f, envAmount) * 254.0f);
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
    float frqTrim = 0.0f;
    float cv_lin = static_cast<float>(ea) * vcfWidth + frqTrim;
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
                              CalibrationSettings* cal)
{
    // 1. Frecuencia de corte modulada usando MCU aritmética
    float cutoffHz = computeCutoffHz (cutoff01, envAmount, envVal, envInverted, lfoAmount, lfoVal,
                                      kybdTrack, currentFreqHz, benderValue, benderToVCF, vcfWidth, cal);
    
    // Limitar para evitar aliasing masivo o inestabilidad
    float safeMaxHz = (float)sampleRate * 0.45f;
    cutoffHz = juce::jlimit(10.0f, safeMaxHz, cutoffHz);

    // 2. Coeficiente G (TPT: tan(pi*fc/fs) / (1 + tan(pi*fc/fs)))
    float g = std::tan (juce::MathConstants<float>::pi * cutoffHz * invSampleRate);
    g = juce::jlimit (0.0001f, 0.999f, g); 
    const float G = g / (1.0f + g);

    // 3. Coeficiente de resonancia (feedback)
    const float k = computeResonanceFeedback (resonance, selfOscThreshold, selfOscInt);

    // 4. Zero-Delay Feedback (ZDF) Resolution
    // Calculamos la "señal proyectada" (S) a través de las 4 etapas de integración
    float S = (G * G * G * s[0]) + (G * G * s[1]) + (G * s[2]) + s[3];
    
    // Resolución de la realimentación: y = (G^4 * x + S) / (1 + k * G^4)
    const float G2 = G * G;
    const float G3 = G2 * G;
    const float G4 = G3 * G;
    
    // Entrada saturada (modelando clipping del BA662 OTA)
    float inputSat = stageSaturate(input, saturationScale);
    float y3 = (G4 * inputSat + S) / (1.0f + k * G4);

    // 5. Actualización de estados del integrador TPT (4 etapas saturadas)
    // [Fidelity] Every stage MUST saturate to keep k > 4.0 stable!
    float u = inputSat - k * y3;
    
    auto updateStage = [&] (float x, float& state) -> float {
        float v = (x - state) * G;
        float y = v + state;
        float ySat = stageSaturate(y, saturationScale);
        state = ySat + v;
        return ySat;
    };

    float y0 = updateStage (u,  s[0]);
    float y1 = updateStage (y0, s[1]);
    float y2 = updateStage (y1, s[2]);
    float y3_final = updateStage (y2, s[3]);

    lastOutput = y3_final;
    return lastOutput;
}
