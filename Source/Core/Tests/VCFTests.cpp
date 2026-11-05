#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/JunoVCF.h"

class JunoVCFTests : public juce::UnitTest
{
public:
    JunoVCFTests()
        : juce::UnitTest("JunoVCF Tests", "JunoDSP") {}

    void runTest() override
    {
        beginTest("Exponential Mapping");
        JunoVCF vcf;
        vcf.setSampleRate(44100.0);

        // Manual verification of internal method computeCutoffHz would be ideal
        // but it's private. We'll test via processSample stability.
        float out = vcf.processSample(0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 440.0f);
        expect(!std::isnan(out));

        beginTest("Auto-oscillation stability");
        vcf.reset();
        // Provide a tiny impulse to kickstart the feedback loop
        vcf.processSample(1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 440.0f);

        // High resonance should produce audio even with zero input after kickstart
        float maxVal = 0.0f;
        for (int i = 0; i < 2000; ++i)
        {
            float s = vcf.processSample(0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 440.0f);
            maxVal  = std::max(maxVal, std::abs(s));
        }
        expect(maxVal > 0.001f, "VCF should auto-oscillate at max resonance after kickstart");

        beginTest("Cutoff tracks the slider without calibration");
        // Regresion: sin CalibrationSettings el VCF devolvia 1000 Hz fijos y el
        // cutoff no hacia nada en ningun preset. Un tono de 8 kHz debe pasar
        // mucho mas con el cutoff alto que con el bajo.
        auto peakOf8k = [](float cutoff01) {
            JunoVCF v;
            v.setSampleRate(44100.0);
            v.reset();
            const double inc = juce::MathConstants<double>::twoPi * 8000.0 / 44100.0;
            double phase     = 0.0;
            float peak       = 0.0f;
            for (int i = 0; i < 8192; ++i)
            {
                const float in = (float)std::sin(phase);
                phase += inc;
                const float out = v.processSample(in, cutoff01, 0.0f);
                if (i > 4096) peak = std::max(peak, std::abs(out));
            }
            return peak;
        };

        const float lowCut  = peakOf8k(0.15f);
        const float highCut = peakOf8k(0.95f);
        expect(highCut > lowCut * 4.0f,
               "8 kHz must pass much more with cutoff at 0.95 than at 0.15 (low=" + juce::String(lowCut) + ", high=" + juce::String(highCut) + ")");
        beginTest("VCF Saturation adds harmonics to the OTA stage");
        {
            // Un tono puro entra al filtro abierto: si el drive de la etapa OTA
            // funciona, el contenido armonico tiene que subir con saturationScale.
            auto harmonics = [](float satScale) {
                JunoVCF v;
                v.setSampleRate(44100.0);
                v.reset();

                const int N      = 8192;
                const double sr  = 44100.0;
                const double f0  = sr / 512.0;
                const double inc = juce::MathConstants<double>::twoPi * f0 / sr;

                std::vector<float> out((size_t)N, 0.0f);
                double phase = 0.0;
                for (int n = 0; n < N; ++n)
                {
                    const float in = 4.0f * (float)std::sin(phase);
                    phase += inc;
                    out[(size_t)n] = v.processSample(in, 0.90f, 0.3f,
                                                     0.0f, 0.0f, false,
                                                     0.0f, 0.0f, 0.0f,
                                                     440.0f, 0.0f, 0.0f,
                                                     0.95f, satScale);
                }

                auto mag = [&out, N](int k) {
                    double re = 0.0, im = 0.0;
                    for (int n = 0; n < N; ++n)
                    {
                        const double a = -2.0 * juce::MathConstants<double>::pi * k * n / N;
                        re += out[(size_t)n] * std::cos(a);
                        im += out[(size_t)n] * std::sin(a);
                    }
                    return std::sqrt(re * re + im * im);
                };

                // f0 = sr/512 con N = 8192 -> cada armonico cae en k = 16*h
                const double h1 = mag(16);
                double rest     = 0.0;
                for (int h = 2; h <= 5; ++h)
                {
                    const double m = mag(16 * h);
                    rest += m * m;
                }
                return (h1 > 0.0) ? (float)(std::sqrt(rest) / h1) : 0.0f;
            };

            const float thdLow  = harmonics(0.5f); // drive minimo (clamp 0.5)
            const float thdMid  = harmonics(1.2f); // neutro (drive 1.0)
            const float thdHigh = harmonics(2.0f); // drive maximo

            expect(thdLow > 0.0f && !std::isnan(thdLow), "low drive must produce output");
            expect(thdHigh > thdLow * 1.2f,
                   "drive must add harmonics (low=" + juce::String(thdLow) + ", high=" + juce::String(thdHigh) + ")");
            expect(thdHigh >= thdMid * 0.99f && thdMid >= thdLow * 0.99f,
                   "harmonics must grow with saturation (low=" + juce::String(thdLow) + ", mid=" + juce::String(thdMid) + ", high=" + juce::String(thdHigh) + ")");
            expect(thdHigh > 0.02f,
                   "max drive must be audibly nonlinear, thd=" + juce::String(thdHigh));
        }
    }
};

static JunoVCFTests junoVCFTests;
