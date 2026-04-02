#pragma once
#include <array>

/**
 * JunoVCF - OTA Ladder Filter (IR3109 / 80017A model)
 *
 * Topology: 4-pole OTA ladder, 24 dB/oct Low-Pass.
 * Saturation: Padé 3/3 approximation per stage (OTA character, non-BJT).
 * Pre-warping: tan(π·fc/fs) TPT trapezoidal integration per stage.
 * Self-oscillation: Natural oscillation when resonance feedback k > 4.0.
 * Keyboard tracking: Exponential V/oct scaling centered around A4 (440Hz).
 *
 * Hardware Reference: Roland 80017A (containing IR3109 and BA662 clones).
 */
class JunoVCF
{
public:
    JunoVCF();

    void reset();
    void setSampleRate (double sr);

    /**
     * Processes a single audio sample through the filter.
     * 
     * @param input             Pre-VCF audio signal (mixed DCO, Sub, and Noise).
     * @param cutoff01          Normalized cutoff frequency (0.0 to 1.0).
     * @param resonance         Normalized resonance level (0.0 to 1.0).
     * @param envMod            Envelope modulation intensity, pre-scaled [-1.0 to 1.0].
     * @param lfoMod            LFO modulation intensity, pre-scaled [-1.0 to 1.0].
     * @param kybdTrack         Keyboard tracking intensity (0.0 to 1.0).
     * @param noteHz            The fundamental frequency of the currently playing note.
     * @param minHz             The minimum allowed cutoff frequency (calibration).
     * @param maxHz             The maximum allowed cutoff frequency (calibration).
     * @param selfOscThreshold  The resonance level where oscillation begins.
     * @param saturationScale   Multiplier for the OTA stage saturation intensity.
     * @param selfOscInt        Intensity of the self-oscillation feedback loop.
     * @param trackCenterHz     The frequency pivot point for keyboard tracking.
     * @param vcfWidth          Scaling accuracy for V/oct tracking.
     * @return                  The filtered audio sample.
     */
    float processSample (float input, 
                         float cutoff01, float resonance,
                         float envMod, float lfoMod, 
                         float kybdTrack, float noteHz,
                         float minHz = 18.0f,
                         float maxHz = 18000.0f,
                         float selfOscThreshold = 0.92f,
                         float saturationScale = 1.0f,
                         float selfOscInt = 0.5f,
                         float trackCenterHz = 440.0f,
                         float vcfWidth = 1.0f);

private:
    float computeCutoffHz (float cutoff01,
                           float envMod,
                           float lfoMod,
                           float kybdTrack,
                           float noteHz,
                           float minHz,
                           float maxHz,
                           float trackCenterHz,
                           float vcfWidth) const;

    float computeResonanceFeedback (float res01, float selfOscThreshold, float selfOscInt) const;

    // Padé 3/3 Saturation: Approximates tanh characteristics of OTA stages.
    static inline float stageSaturate (float x, float scale = 1.0f) noexcept;

    // Estado de las 4 etapas TPT
    std::array<float, 4> s {};   // integrador por etapa
    float lastOutput = 0.0f;

    double sampleRate    = 44100.0;
    float  invSampleRate = 1.0f / 44100.0f;

    // [Build 29] Ranges are now dynamic via CalibrationManager

    // Resonancia: umbral de autooscilación (HW: ~0.92)
    // float kSelfOscThreshold = 0.92f; // Moved to dynamic parameter
};
