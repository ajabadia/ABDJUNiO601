#include <JuceHeader.h>
#include "JunoVCF.h"
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
// Curva de cutoff: exponencial con modulaciones y kbd tracking
// CV se suma antes de la exponencial para replicar V/oct del OTA
// ------------------------------------------------------------
float JunoVCF::computeCutoffHz (float cutoff01,
                                 float envMod,
                                 float lfoMod,
                                 float kybdTrack,
                                 float noteHz,
                                 float minHz,
                                 float maxHz,
                                 float trackCenterHz,
                                 float vcfWidth) const
{
    // Suma de todas las fuentes de CV en el dominio normalizado
    float cv = juce::jlimit (0.0f, 1.0f, cutoff01 + envMod + lfoMod);

    // [vcfWidth] Escalado de la curva exponencial
    // Simula el trim pot de Width que ajusta el rango efectivo de CV
    cv *= vcfWidth;

    // Ley exponencial: minHz a maxHz
    const float ratio = maxHz / std::max(1.0f, minHz);
    float cutoffHz = minHz * std::pow (ratio, cv);

    // Keyboard tracking: V/oct centered at trackCenterHz
    if (kybdTrack > 0.001f)
    {
        const float trackRatio = noteHz / std::max(1.0f, trackCenterHz);
        const float trackMult  = 1.0f + kybdTrack * (trackRatio - 1.0f);
        cutoffHz *= std::max (trackMult, 0.01f);
    }

    return juce::jlimit (minHz, maxHz, cutoffHz);
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
                               float envMod,
                               float lfoMod,
                               float kybdTrack,
                               float noteHz,
                               float minHz,
                               float maxHz,
                               float selfOscThreshold,
                               float saturationScale,
                               float selfOscInt,
                               float trackCenterHz,
                               float vcfWidth)
{
    // 1. Frecuencia de corte con todas las modulaciones
    const float cutoffHz = computeCutoffHz (cutoff01, envMod, lfoMod,
                                            kybdTrack, noteHz, minHz, maxHz, trackCenterHz, vcfWidth);

    // 2. Pre-warping TPT: g = tan(π·fc/fs)
    //    Compensa la contracción de frecuencia del dominio discreto
    float g = std::tan (juce::MathConstants<float>::pi
                        * cutoffHz * invSampleRate);

    // Límite práctico: evita inestabilidad cerca de Nyquist
    g = juce::jlimit (0.0001f, 0.999f, g);

    // Factor de integración trapezoidal por etapa
    const float G = g / (1.0f + g);

    // 3. Coeficiente de resonancia (feedback)
    const float k = computeResonanceFeedback (resonance, selfOscThreshold, selfOscInt);

    // 4. Señal de entrada con feedback saturado
    //    Modela el overload OTA del IR3109 cuando hay señal grande
    const float inputWithFeedback = stageSaturate (input - k * lastOutput, saturationScale);
    
    // 5. Cuatro etapas TPT con saturación OTA por etapa
    //    Cada etapa: v = G * (sat(x) - state)
    //                y = v + state
    //                s_next = y + v  (integrador trapezoidal)
    auto processStage = [&] (float x, float& state) -> float
    {
        const float v = G * (stageSaturate (x, saturationScale) - state);
        const float y = v + state;
        state = y + v;
        return y;
    };

    const float y0 = processStage (inputWithFeedback, s[0]);
    const float y1 = processStage (y0,                s[1]);
    const float y2 = processStage (y1,                s[2]);
    const float y3 = processStage (y2,                s[3]);

    // 6. Salida = 4ª etapa = LP 24 dB/oct
    lastOutput = y3;

    return lastOutput;
}
