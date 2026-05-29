#include "ChorusBBD.h"
#include "../Core/JunoConstants.h"

ChorusBBD::ChorusBBD()
{
    reset();
}

void ChorusBBD::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;
    
    lineI_L.init(MAX_DELAY_SAMPLES);
    lineI_R.init(MAX_DELAY_SAMPLES);
    lineII_L.init(MAX_DELAY_SAMPLES);
    lineII_R.init(MAX_DELAY_SAMPLES);

    filterI_L.prepare(sr, calFilterCutoff);
    filterI_R.prepare(sr, calFilterCutoff);
    filterII_L.prepare(sr, calFilterCutoff);
    filterII_R.prepare(sr, calFilterCutoff);

    wetNoiseL.Init((float)sr);
    wetNoiseL.mPinkEnabled = true;
    wetNoiseR.Init((float)sr);
    wetNoiseR.mPinkEnabled = true;
    wetNoiseL.mSeed = 0x12345678u;
    wetNoiseR.mSeed = 0x87654321u;
    wetNoiseL.SetHighShelf(3000.0f, 6.0f, (float)sr);
    wetNoiseR.SetHighShelf(3000.0f, 6.0f, (float)sr);

    wetRipple.SetMainsHz(60.0f, (float)sr);
    wetRipple.SetAmplitudes(7.9e-5f, 2.2e-5f, 9.8e-6f);

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

    wetRipple.Reset();
    lfoPhase = 0.0;
}

void ChorusBBD::process(juce::AudioBuffer<float>& buffer)
{
    if (mode == Mode::Off) return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float* L = buffer.getWritePointer(0);
    float* R = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    // Scale modulation depth by mode to match KR-106 hardware specs:
    // Chorus I: 2.13ms (ref), Chorus II: 1.71ms (~0.8028x), Chorus Both: 0.236ms (~0.1108x)
    float modeScale = 1.0f;
    if (mode == Mode::ChorusII)   modeScale = 0.8028f;
    else if (mode == Mode::ChorusBoth) modeScale = 0.1108f;
    const float modAmp = (calModDepth * m_depth * 0.001f) * (float)sr * modeScale;
    const float currentRate = (mode == Mode::ChorusBoth) ? calBothRate : lfoRate;
    const double phaseInc = juce::MathConstants<double>::twoPi * currentRate / sr;

    const float baseI = (calDelayI * 0.001f) * (float)sr;
    const float baseII = (calDelayII * 0.001f) * (float)sr;

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = L[s];
        const float inR = R ? R[s] : inL;

        float outL = inL;
        float outR = inR;

        // --- Chorus I or Both (Both uses 8Hz mono-line logic) ---
        if (mode == Mode::ChorusI || mode == Mode::ChorusBoth)
        {
            float lfo_L, lfo_R;
            if (mode == Mode::ChorusBoth)
            {
                lfo_L = (float)std::sin(lfoPhase);
                lfo_R = (float)std::sin(lfoPhase + juce::MathConstants<double>::pi);
            }
            else
            {
                double norm = lfoPhase / juce::MathConstants<double>::twoPi;
                lfo_L = 1.0f - 4.0f * (float)std::abs(norm - 0.5);
                lfo_R = -lfo_L;
            }

            float del_L = baseI + modAmp * lfo_L;
            float del_R = baseI + modAmp * lfo_R;

            lineI_L.write(saturate(inL));
            lineI_R.write(saturate(inR));
            float wet_L = filterI_L.process(lineI_L.read(del_L));
            float wet_R = filterI_R.process(lineI_R.read(del_R));

            float dryMix = 1.0f - wetMix * (1.0f - calGainDry);
            float wetMixAmount = wetMix * calGainWet;

            outL = dryMix * inL + wetMixAmount * wet_L;
            outR = dryMix * inR + wetMixAmount * wet_R;
        }
        else if (mode == Mode::ChorusII)
        {
            double norm = lfoPhase / juce::MathConstants<double>::twoPi;
            float lfo_L = 1.0f - 4.0f * (float)std::abs(norm - 0.5);
            float lfo_R = -lfo_L;

            float del_L = baseII + modAmp * lfo_L;
            float del_R = baseII + modAmp * lfo_R;

            lineII_L.write(saturate(inL));
            lineII_R.write(saturate(inR));
            float wet_L = filterII_L.process(lineII_L.read(del_L));
            float wet_R = filterII_R.process(lineII_R.read(del_R));

            float dryMix = 1.0f - wetMix * (1.0f - calGainDry);
            float wetMixAmount = wetMix * calGainWet;

            outL = dryMix * inL + wetMixAmount * wet_L;
            outR = dryMix * inR + wetMixAmount * wet_R;
        }

        float nL = wetNoiseL.Process() * 0.0018f * hissMultiplier;
        float nR = wetNoiseR.Process() * 0.0018f * hissMultiplier;
        float ripple = wetRipple.Process() * hissMultiplier;
        outL += nL + ripple;
        outR += nR + ripple;

        L[s] = outL;
        if (R) R[s] = outR;

        lfoPhase += phaseInc;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;
    }
}
