#include "../Synth/JunoADSR.h"
#include <JuceHeader.h>

// Tiempos del envelope segun las service notes del Juno-106 (Roland, 31-JUL-1984):
//   Attack 1.5 ms .. 3 s   |   Decay 1.5 ms .. 12 s   |   Release 1.5 ms .. 12 s
class EnvelopeTimingTests : public juce::UnitTest
{
public:
    EnvelopeTimingTests()
        : juce::UnitTest("Envelope Timing", "JunoDSP") {}

    static void configure(JunoADSR& env, float attackRaw, float decayRaw, float releaseRaw)
    {
        env.setSampleRate(44100.0);
        env.setMode(ADSRMode::kJ106);
        env.setMcuRate(4.2335f); // valor por defecto de la calibracion
        env.setDacSteps(1024.0f);
        env.setSlewMs(0.0f);
        env.setGateMode(false);
        env.setAttackRaw(attackRaw);
        env.setDecayRaw(decayRaw);
        env.setSustain(1.0f);
        env.setReleaseRaw(releaseRaw);
    }

    static float timeToReach(JunoADSR& env, float target, bool below, int maxSamples)
    {
        for (int i = 0; i < maxSamples; ++i)
        {
            const float v = env.getNextSample();
            if (below ? (v < target) : (v > target))
                return (float)i / 44100.0f;
        }
        return -1.0f;
    }

    static float releaseSeconds(float slider)
    {
        JunoADSR env;
        configure(env, 0.0f, 0.0f, slider);
        env.noteOn();

        for (int i = 0; i < 44100; ++i)
            if (env.getNextSample() > 0.95f) break;

        env.noteOff();
        return timeToReach(env, 0.001f, true, 44100 * 40);
    }

    static float decaySeconds(float slider)
    {
        JunoADSR env;
        configure(env, 0.0f, slider, 0.0f);
        env.setSustain(0.0f);
        env.noteOn();

        for (int i = 0; i < 44100; ++i)
            if (env.getNextSample() > 0.95f) break;

        return timeToReach(env, 0.001f, true, 44100 * 40);
    }

    static float attackSeconds(float slider)
    {
        JunoADSR env;
        configure(env, slider, 0.0f, 0.0f);
        env.noteOn();
        return timeToReach(env, 0.95f, false, 44100 * 40);
    }

    void runTest() override
    {
        beginTest("Release cubre 1.5 ms .. 12 s");
        const float r0   = releaseSeconds(0.0f);
        const float r50  = releaseSeconds(0.5f);
        const float r100 = releaseSeconds(1.0f);

        expect(r0 >= 0.0f && r0 < 0.1f,
               "release minimo fuera de rango: " + juce::String(r0) + " s");
        expect(r100 > 8.0f && r100 < 16.0f,
               "release maximo fuera de rango: " + juce::String(r100) + " s (r50=" + juce::String(r50) + " s)");
        expect(r50 > r0 && r100 > r50,
               "release debe ser monotono: r0=" + juce::String(r0) + " r50=" + juce::String(r50) + " r100=" + juce::String(r100));

        beginTest("Attack cubre 1.5 ms .. 3 s");
        const float a0   = attackSeconds(0.0f);
        const float a100 = attackSeconds(1.0f);

        expect(a0 >= 0.0f && a0 < 0.1f,
               "attack minimo fuera de rango: " + juce::String(a0) + " s");
        expect(a100 > 2.0f && a100 < 4.5f,
               "attack maximo fuera de rango: " + juce::String(a100) + " s");

        beginTest("Decay cubre 1.5 ms .. 12 s");
        const float d0   = decaySeconds(0.0f);
        const float d100 = decaySeconds(1.0f);

        expect(d0 >= 0.0f && d0 < 0.1f,
               "decay minimo fuera de rango: " + juce::String(d0) + " s");
        expect(d100 > 8.0f && d100 < 16.0f,
               "decay maximo fuera de rango: " + juce::String(d100) + " s");
    }
};

static EnvelopeTimingTests envelopeTimingTestsInstance;
