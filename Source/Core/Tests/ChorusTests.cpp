#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../Synth/ChorusBBD.h"

class ChorusBBDTests : public juce::UnitTest
{
public:
    ChorusBBDTests()
        : juce::UnitTest("ChorusBBD Tests", "JunoDSP") {}

    void runTest() override
    {
        // Use a dynamic signal (rising saw) so that different L/R delays
        // produce different output samples — constant/DC input makes
        // Hermite interpolation return the same value regardless of delay.
        auto makeDynamicSignal = [](juce::AudioBuffer<float>& buf) {
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample(0, i, std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f));
            // Copy to R so we start with identical channels
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample(1, i, buf.getSample(0, i));
        };

        beginTest("Stereo Phase Relationship");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::ChorusI);
            chorus.setMix(1.0f);

            juce::AudioBuffer<float> buffer(2, 2048);
            buffer.clear();
            makeDynamicSignal(buffer);

            chorus.process(buffer);

            // In Chorus I, L/R should be different (antiphase LFO produces
            // different delay modulation on each channel)
            bool anyDifference = false;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) > 1e-4f)
                {
                    anyDifference = true;
                    break;
                }
            }
            expect(anyDifference, "Chorus I should produce stereo difference via antiphase LFO");
        }

        beginTest("Saturation & Filtering");
        {
            // La saturacion del BBD se comprueba como compresion: al multiplicar
            // la entrada por 10 la salida debe crecer mucho menos que 10 veces.
            auto peakFor = [](float amplitude) {
                ChorusBBD chorus;
                chorus.prepare(44100.0, 512);
                chorus.setMode(ChorusBBD::Mode::ChorusI);
                chorus.setMix(1.0f);
                // El sumador IC6 siempre suma algo de seco: se anula para medir el BBD.
                chorus.setCalibrationParams(3.2f, 3.3f, /*dry*/ 0.0f, /*wet*/ 1.257f,
                                            1.5f, 1.2f, 9661.0f, 7.7f);

                juce::AudioBuffer<float> buffer(1, 2048);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample(0, i, amplitude);

                chorus.process(buffer);

                float peak = 0.0f;
                for (int i = 200; i < buffer.getNumSamples(); ++i) // skip first samples (delay line filling)
                    peak = std::max(peak, std::abs(buffer.getSample(0, i)));
                return peak;
            };

            const float p10  = peakFor(10.0f);
            const float p100 = peakFor(100.0f);

            expect(p10 > 0.1f, "Chorus should still produce output (not silent)");
            expect(p100 > p10, "A hotter input must still be louder");
            expect(p100 < p10 * 4.0f, "BBD should compress hot signals (x10 input must not be x10 out)");
        }

        beginTest("Mix 0 anula el coro: la salida es la entrada intacta");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::ChorusI);
            chorus.setMix(0.0f);

            juce::AudioBuffer<float> buffer(1, 1024);
            for (int i = 0; i < 1024; ++i)
                buffer.setSample(0, i, std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f));

            juce::AudioBuffer<float> reference(1, 1024);
            reference.copyFrom(0, 0, buffer, 0, 0, 1024);

            chorus.process(buffer);

            float maxDiff = 0.0f;
            for (int i = 0; i < 1024; ++i)
                maxDiff = std::max(maxDiff, std::abs(buffer.getSample(0, i) - reference.getSample(0, i)));

            expect(maxDiff < 1e-6f,
                   "con Mix 0 el IC6 solo suma seco: maxDiff=" + juce::String(maxDiff));
        }

        beginTest("La linea BBD satura por si sola (sin sumador de seco)");
        {
            // Aqui no hay IC6: se mide el tanh de la linea MN3009, que es donde
            // vive la saturacion. 10x de entrada no puede dar 10x de salida.
            auto rmsFor = [](float amplitude) {
                BBDLine line;
                line.Init(44100.f, 9661.f);
                line.mSatDrive       = 0.12f;
                line.mSatDriveSmooth = 0.12f;

                const float delaySamples = 3.2f * 0.001f * 44100.f;
                double acc               = 0.0;
                int n                    = 0;

                for (int i = 0; i < 4096; ++i)
                {
                    const float in  = amplitude * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                    const float out = line.Process(in, delaySamples);

                    if (i > 1024)
                    {
                        acc += (double)out * out;
                        ++n;
                    }
                }

                return (float)std::sqrt(acc / std::max(1, n));
            };

            const float r10  = rmsFor(10.0f);
            const float r100 = rmsFor(100.0f);

            expect(r10 > 1e-4f, "la linea BBD debe dar salida, rms=" + juce::String(r10));
            expect(r100 > r10, "mas entrada sigue dando mas salida");
            expect(r100 < r10 * 3.0f,
                   "tanh de la linea: 10x entrada no debe dar 10x salida (r10=" + juce::String(r10) + ", r100=" + juce::String(r100) + ")");
        }
        beginTest("El sumador IC6 es fiel: seco 0.863 + humedo 1.257, no crossfade");
        {
            // En el Juno el coro es una SUMA de brazos (IC6), no un crossfade:
            // con Mix 1.0 el seco sigue presente con su ganancia de hardware.
            auto dcOut = [](float dryGain, float wetGain) {
                ChorusBBD chorus;
                chorus.prepare(44100.0, 512);
                chorus.setMode(ChorusBBD::Mode::ChorusI);
                chorus.setMix(1.0f);
                chorus.setCalibrationParams(3.2f, 3.3f, dryGain, wetGain,
                                            1.5f, 1.2f, 9661.0f, 7.7f);

                juce::AudioBuffer<float> buffer(1, 8192);
                buffer.clear();
                // El brazo seco es instantaneo (no tiene estado): para leerlo hay
                // que mantener la DC durante toda la ventana.
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample(0, i, 0.02f); // DC suave, lejos del tanh

                chorus.process(buffer);
                return buffer.getSample(0, 8000);
            };

            const float dryOnly     = dcOut(0.863f, 0.0f);
            const float dryPlus     = dcOut(0.863f, 1.257f);
            const float expectedDry = 0.863f * 0.02f;

            expect(std::abs(dryOnly - expectedDry) < expectedDry * 0.25f,
                   "el brazo seco debe valer calGainDry (esperado=" + juce::String(expectedDry) + ", medido=" + juce::String(dryOnly) + ")");
            expect(std::abs(dryPlus - dryOnly) > expectedDry * 0.1f,
                   "el brazo humedo debe sumarse al seco (dry=" + juce::String(dryOnly) + ", dry+wet=" + juce::String(dryPlus) + ")");
            expect(dryPlus > expectedDry,
                   "Mix 1.0 conserva el seco: es la suma de hardware (out=" + juce::String(dryPlus) + ")");
        }
    }
};

static ChorusBBDTests chorusBBDTestsInstance;
