#include "ChorusBBD.h"

ChorusBBD::ChorusBBD()
{
    reset();
}

void ChorusBBD::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;
    
    // Inicializar líneas de retardo
    lineI_L.init(MAX_DELAY_SAMPLES);
    lineI_R.init(MAX_DELAY_SAMPLES);
    lineII_L.init(MAX_DELAY_SAMPLES);
    lineII_R.init(MAX_DELAY_SAMPLES);

    // Filtros post-BBD (reconstrucción ~8kHz)
    constexpr float POST_LP_HZ = 8000.0f;
    filterI_L.prepare(sr, POST_LP_HZ);
    filterI_R.prepare(sr, POST_LP_HZ);
    filterII_L.prepare(sr, POST_LP_HZ);
    filterII_R.prepare(sr, POST_LP_HZ);

    reset();
}

void ChorusBBD::reset()
{
    lineI_L.reset();
    lineI_R.reset();
    lineII_L.reset();
    lineII_R.reset();

    filterI_L.reset();
    filterI_R.reset();
    filterII_L.reset();
    filterII_R.reset();

    lfoPhase = 0.0;
}

void ChorusBBD::process(juce::AudioBuffer<float>& buffer)
{
    if (mode == Mode::Off) return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float* L = buffer.getWritePointer(0);
    float* R = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    const double phaseInc = juce::MathConstants<double>::twoPi * lfoRate / sr;
    
    const float baseI = (DELAY_I_MS * 0.001f) * (float)sr;
    const float baseII = (DELAY_II_MS * 0.001f) * (float)sr;
    const float modAmp = (MOD_DEPTH_MS * depth * 0.001f) * (float)sr;

    const bool doI  = (mode == Mode::ChorusI  || mode == Mode::ChorusBoth);
    const bool doII = (mode == Mode::ChorusII || mode == Mode::ChorusBoth);

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = L[s];
        const float inR = R ? R[s] : inL;

        float outL = inL;
        float outR = inR;

        //── Chorus I ─────────────────────────────────────────────────────────
        if (doI)
        {
            // LFO I: L=0°, R=180°
            float lfo_L = (float)std::sin(lfoPhase);
            float lfo_R = (float)std::sin(lfoPhase + juce::MathConstants<double>::pi);

            float del_L = baseI + modAmp * lfo_L;
            float del_R = baseI + modAmp * lfo_R;

            lineI_L.write(saturate(inL));
            lineI_R.write(saturate(inR));

            float wet_L = filterI_L.process(lineI_L.read(del_L));
            float wet_R = filterI_R.process(lineI_R.read(del_R));

            // Mezcla wet/dry del Chorus I
            outL += wetMix * (wet_L - inL);
            outR += wetMix * (wet_R - inR);
        }

        //── Chorus II ────────────────────────────────────────────────────────
        if (doII)
        {
            // LFO II: L=90°, R=270° (Cuadratura respecto a Chorus I)
            float lfo_L = (float)std::sin(lfoPhase + juce::MathConstants<double>::halfPi);
            float lfo_R = (float)std::sin(lfoPhase + 3.0 * juce::MathConstants<double>::halfPi);

            float del_L = baseII + modAmp * lfo_L;
            float del_R = baseII + modAmp * lfo_R;

            lineII_L.write(saturate(inL));
            lineII_R.write(saturate(inR));

            float wet_L = filterII_L.process(lineII_L.read(del_L));
            float wet_R = filterII_R.process(lineII_R.read(del_R));

            // Mezcla wet/dry (acumulativa si ChorusBoth)
            outL += wetMix * (wet_L - inL);
            outR += wetMix * (wet_R - inR);
        }

        L[s] = outL;
        if (R) R[s] = outR;

        lfoPhase += phaseInc;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;
    }
}
