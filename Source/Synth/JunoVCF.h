#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * JunoVCF - OTA Ladder Filter (IR3109 / 80017A model)
 *
 * Topología: 4 polos OTA ladder, 24 dB/oct LP
 * Saturación: Padé 3/3 por etapa (carácter OTA, no BJT)
 * Pre-warping: tan(π·fc/fs) TPT trapezoidal por etapa
 * Autooscilación: natural cuando k > 4.0
 * Keyboard tracking: exponencial V/oct centrado en A4
 *
 * Referencia hardware: Roland 80017A (IR3109 + BA662)
 */
class JunoVCF
{
public:
    JunoVCF();

    void reset();
    void setSampleRate (double sr);

    /**
     * Procesa una muestra
     * @param input       señal pre-VCF (DCO + noise mezclados)
     * @param cutoff01    frecuencia de corte normalizada 0–1
     * @param resonance   resonancia normalizada 0–1
     * @param envMod      modulación de envolvente, ya escalada [-1..+1]
     * @param lfoMod      modulación LFO, ya escalada [-1..+1]
     * @param kybdTrack   keyboard tracking 0–1
     * @param noteHz      frecuencia fundamental de la voz en Hz
     * @return            muestra filtrada
     */
    float processSample (float input,
                         float cutoff01,
                         float resonance,
                         float envMod,
                         float lfoMod,
                         float kybdTrack,
                         float noteHz,
                         float selfOscThreshold = 0.92f,
                         float saturationScale = 1.0f);

private:
    float computeCutoffHz (float cutoff01,
                           float envMod,
                           float lfoMod,
                           float kybdTrack,
                           float noteHz) const;

    float computeResonanceFeedback (float res01, float selfOscThreshold) const;

    // Saturación Padé 3/3: aproximación de tanh, simétrica, cara OTA
    static inline float stageSaturate (float x, float scale = 1.0f) noexcept;

    // Estado de las 4 etapas TPT
    std::array<float, 4> s {};   // integrador por etapa
    float lastOutput = 0.0f;

    double sampleRate    = 44100.0;
    float  invSampleRate = 1.0f / 44100.0f;

    // Rango de corte del 80017A: ~18 Hz – ~18 kHz (~10 octavas)
    static constexpr float kMinHz = 18.0f;
    static constexpr float kMaxHz = 18000.0f;
    static constexpr float kRatio = 1000.0f; // kMaxHz / kMinHz

    // Resonancia: umbral de autooscilación (HW: ~0.92)
    // float kSelfOscThreshold = 0.92f; // Moved to dynamic parameter
};
