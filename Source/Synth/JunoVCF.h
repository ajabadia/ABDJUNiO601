#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <JuceHeader.h>

// ============================================================
// Inline polyphase IIR resampler (from Laurent de Soras)
// ============================================================
static constexpr int kNumResamplerCoefs = 12;

struct JunoUpsampler2x
{
    float coef[kNumResamplerCoefs] = {};
    float x[kNumResamplerCoefs] = {};
    float y[kNumResamplerCoefs] = {};

    void set_coefs(const double c[kNumResamplerCoefs])
    {
        for (int i = 0; i < kNumResamplerCoefs; i++)
            coef[i] = static_cast<float>(c[i]);
    }

    void clear_buffers()
    {
        std::memset(x, 0, sizeof(x));
        std::memset(y, 0, sizeof(y));
    }

    void process_sample(float& out_0, float& out_1, float input)
    {
        float even = input;
        float odd = input;
        for (int i = 0; i < kNumResamplerCoefs; i += 2)
        {
            float t0 = (even - y[i]) * coef[i] + x[i];
            float t1 = (odd - y[i + 1]) * coef[i + 1] + x[i + 1];
            x[i] = even;   x[i + 1] = odd;
            y[i] = t0;     y[i + 1] = t1;
            even = t0;     odd = t1;
        }
        out_0 = even;
        out_1 = odd;
    }
};

struct JunoDownsampler2x
{
    float coef[kNumResamplerCoefs] = {};
    float x[kNumResamplerCoefs] = {};
    float y[kNumResamplerCoefs] = {};

    void set_coefs(const double c[kNumResamplerCoefs])
    {
        for (int i = 0; i < kNumResamplerCoefs; i++)
            coef[i] = static_cast<float>(c[i]);
    }

    void clear_buffers()
    {
        std::memset(x, 0, sizeof(x));
        std::memset(y, 0, sizeof(y));
    }

    float process_sample(const float in[2])
    {
        float spl_0 = in[1];
        float spl_1 = in[0];
        for (int i = 0; i < kNumResamplerCoefs; i += 2)
        {
            float t0 = (spl_0 - y[i]) * coef[i] + x[i];
            float t1 = (spl_1 - y[i + 1]) * coef[i + 1] + x[i + 1];
            x[i] = spl_0;   x[i + 1] = spl_1;
            y[i] = t0;       y[i + 1] = t1;
            spl_0 = t0;     spl_1 = t1;
        }
        return 0.5f * (spl_0 + spl_1);
    }
};

/**
 * JunoVCF - OTA Ladder Filter (IR3109 / 80017A model)
 */
class JunoVCF
{
public:
    JunoVCF();

    void reset();
    void setSampleRate (double sr);
    void setOversample (int factor);
    void setModelAndResCurve(bool j106Res, bool otaSaturation);

    /**
     * Processes a single audio sample through the filter.
     */
    float processSample (float input,
                         float cutoff01,
                         float resonance,
                         float envAmount = 0.0f,
                         float envVal = 0.0f,
                         bool envInverted = false,
                         float lfoAmount = 0.0f,
                         float lfoVal = 0.0f,
                         float kybdTrack = 0.0f,
                         float currentFreqHz = 440.0f,
                         float benderValue = 0.0f,
                         float benderToVCF = 0.0f,
                         float selfOscThreshold = 0.95f,
                         float saturationScale = 1.0f,
                         float selfOscInt = 1.0f,
                         float vcfWidth = 1.0f,
                         float vcfFrqTrim = 0.0f,
                         class CalibrationSettings* cal = nullptr);

private:
    float processSampleInternal (float input, float frq, float res, float k);
    float process2x (float input, float frq, float res, float k);
    float process4x (float input, float frq, float res, float k);

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

    // ResK_J6 - Juno-6 hardware peak response
    static inline float ResK_J6 (float res) noexcept
    {
        static constexpr float kShape = 2.128f;
        static constexpr float kNorm = 0.811f;
        return kNorm * (std::exp(kShape * res) - 1.f);
    }

    // Soft-clip resonance above k=3.0 (OTA gain compression)
    static inline float SoftClipK (float k) noexcept
    {
        if (k > 3.0f)
        {
            float excess = k - 3.0f;
            k = 3.0f + excess / (1.0f + excess * 0.2f);
        }
        return std::min(k, 6.6f);
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
    std::array<float, 4> s {};
    float lastOutput = 0.0f;

    double sampleRate    = 44100.0;
    float  invSampleRate = 1.0f / 44100.0f;

    // Config conmutable
    bool mJ106Res = true;
    bool mOTASaturation = true;
    int mOversample = 4;

    // Seeding de ruido térmico adaptativo
    uint32_t mNoiseSeed = 123456789u;
    float mInputEnv = 0.f;
    float mEnvDecay = 0.999f;
    float mFreqComp = 1.f;

    // Resamplers polifase
    JunoUpsampler2x mUp1, mUp2;
    JunoDownsampler2x mDown1, mDown2;
};
