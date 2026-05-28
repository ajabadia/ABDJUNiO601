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
     * [Phase 2] Now uses MCU 14-bit arithmetic and J106DACHzTable for cutoff.
     * 
     * @param input             Pre-VCF audio signal (mixed DCO, Sub, and Noise).
     * @param cutoff01          Normalized cutoff frequency (0.0 to 1.0).
     * @param resonance         Normalized resonance level (0.0 to 1.0).
     * @param envAmount         Envelope modulation amount (0.0 to 1.0).
     * @param envVal            Current envelope value (0.0 to 1.0).
     * @param envInverted       True if envelope polarity is inverted.
     * @param lfoAmount         LFO modulation amount (0.0 to 1.0).
     * @param lfoVal            Current LFO value (-1.0 to 1.0).
     * @param kybdTrack         Keyboard tracking intensity (0.0 to 1.0).
     * @param currentFreqHz     The fundamental frequency of the currently playing note.
     * @param benderValue       Pitch bender position (-1.0 to 1.0).
     * @param benderToVCF       Bender to VCF modulation amount.
     * @param selfOscThreshold  The resonance level where oscillation begins.
     * @param saturationScale   Multiplier for the OTA stage saturation intensity.
     * @param selfOscInt        Intensity of the self-oscillation feedback loop.
     * @param vcfWidth          Scaling accuracy for V/oct tracking.
     * @param cal               Pointer to CalibrationSettings for DAC table lookup.
     * @return                  The filtered audio sample.
     */
    float processSample (float input,
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
                         class CalibrationSettings* cal);

private:
    // ------------------------------------------------------------
    // Curva de cutoff: MCU Aritmética 14-bits y tabla J106DACHzTable
    // ------------------------------------------------------------
    float computeCutoffHz (float cutoff01,
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
                           class CalibrationSettings* cal) const;

    float computeResonanceFeedback (float res01, float selfOscThreshold, float selfOscInt) const;

    // --- Funciones de Aritmética MCU (uPD7811G) ---
    static inline uint16_t mul8x16_hi(uint8_t coeff, uint16_t value) {
        return static_cast<uint16_t>(static_cast<uint32_t>(coeff) * value >> 8);
    }
    static inline uint16_t vcf_add(uint16_t ea, uint16_t bc, bool& overflow) {
        uint32_t result = static_cast<uint32_t>(ea) + bc;
        if (result > 0xFFFF) overflow = false;
        return static_cast<uint16_t>(result);
    }
    static inline uint16_t vcf_sub(uint16_t ea, uint16_t bc, bool& overflow) {
        if (bc > ea) overflow = true;
        return ea - bc;
    }
    static inline uint16_t vcf_clamp(uint16_t ea, bool underflow) {
        if (ea > 0x3FFF) return underflow ? 0x0000 : 0x3FFF;
        return ea;
    }

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
