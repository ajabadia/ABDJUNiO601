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
                         float vcfFrqTrim,
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
                           float vcfFrqTrim,
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

    // OTASat - Pade 3/3 approximation of tanh(x)
    static inline float OTASat (float x) noexcept
    {
        if (x > 3.f) return 1.f;
        if (x < -3.f) return -1.f;
        float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }

    // OTASatDeriv - Derivative of OTASat for Newton-Raphson solver
    static inline float OTASatDeriv (float x) noexcept
    {
        if (x > 3.f || x < -3.f) return 0.f;
        float x2 = x * x;
        float d = 27.f + 9.f * x2;
        return 27.f * (27.f - 3.f * x2) / (d * d);
    }

    // NLStage - Non-linear stage solver via Newton-Raphson
    static inline float NLStage (float& s, float x, float g, float g1, float otaScale) noexcept
    {
        float y = s + g1 * (x - s);
        float diff = x - y;
        float sd = diff * otaScale;
        float t = OTASat(sd) / otaScale;
        float f = y - s - g * t;
        float df = 1.f + g * OTASatDeriv(sd);
        y -= f / df;
        s = 2.f * y - s;
        return y;
    }

    // ResK_J106 - Polynomial resonance curve fit from hardware measurements
    static inline float ResK_J106 (float res) noexcept
    {
        float r = res;
        float r2 = r * r;
        float r3 = r2 * r;
        float r4 = r2 * r2;
        return 1.24f * (4.7116f * r - 6.5743f * r2 + 13.4633f * r3 - 8.2197f * r4);
    }

    // FreqCompensationClamped - Cutoff compensation
    static inline float FreqCompensationClamped (float k, float frq) noexcept
    {
        float lowQ = std::max(1.0f, 0.42f * std::pow(std::max(frq, 1e-6f), -0.12f));
        float logdist = std::log(std::max(frq, 1e-6f) / 0.012f);
        lowQ += 0.20f * std::exp(-logdist * logdist / 1.0f);
        float blend = std::min(k * k * 0.0625f, 1.f);
        return lowQ + blend * (1.f - lowQ);
    }

    // InputComp - Q compensation counteracting passband drop
    static inline float InputComp (float k, float frq) noexcept
    {
        float qComp = 0.379f + 0.087f * k;
        float freqGain = std::pow(std::max(frq, 1e-6f) * (1.f / 0.00445f), -0.10f);
        freqGain = juce::jlimit(0.65f, 1.2f, freqGain);
        return qComp * freqGain;
    }

    static constexpr float kOTAScaleBase = 0.35f;

    // OTAScaleForFreq - Scaling factor for OTA saturation based on frequency/resonance
    static inline float OTAScaleForFreq (float frq, float res = 0.f) noexcept
    {
        float scale = kOTAScaleBase;
        if (frq < 0.005f)
        {
            float blend = std::max(frq / 0.005f, 0.15f);
            scale *= blend;
        }
        if (res > 0.f)
        {
            float resK = ResK_J106(res);
            float resBlend = std::min(resK * resK * 0.0625f, 1.f);
            scale = scale + resBlend * (kOTAScaleBase - scale);
        }
        return scale;
    }

    // Estado de las 4 etapas TPT
    std::array<float, 4> s {};   // integrador por etapa
    float lastOutput = 0.0f;

    double sampleRate    = 44100.0;
    float  invSampleRate = 1.0f / 44100.0f;

    // [Build 29] Ranges are now dynamic via CalibrationManager

    // Resonancia: umbral de autooscilación (HW: ~0.92)
    // float kSelfOscThreshold = 0.92f; // Moved to dynamic parameter
};
