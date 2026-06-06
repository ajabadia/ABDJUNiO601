/*
 * ABD JUNiO 601 - JunoTapeEcho Unit Tests
 *
 * Tests the calibration-linked DSP parameters:
 *   - inputLevel  (0..1): input gain before delay line
 *   - wetDry      (0..1): dry/wet crossfade mix
 *   - reverbType  (0..2): three reverb algorithms
 *   - enabled/disabled
 *   - getActiveHeads (all 11 settings)
 *
 * IMPORTANT: The delay line minimum length is ~2205 samples (50ms at 44.1kHz).
 * Tests that need wet output use processWarm() to fill the delay line first,
 * then measure the output on subsequent passes.
 */

#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include "JunoTapeEcho.h"

// ============================================================================
// JunoTapeEchoTests
// ============================================================================
class JunoTapeEchoTests : public juce::UnitTest
{
public:
    JunoTapeEchoTests() : juce::UnitTest("JunoTapeEcho", "Synth") {}

    // Helper: create a stereo buffer filled with a sine wave
    static juce::AudioBuffer<float> createTestBuffer(int numSamples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < numSamples; ++s)
                buffer.setSample(ch, s,
                    amplitude * std::sin(2.0f * juce::MathConstants<float>::pi * 200.0f * s / 44100.0f));
        return buffer;
    }

    // Helper: compute RMS of a buffer
    static float computeRMS(const juce::AudioBuffer<float>& buffer)
    {
        double sumSq = 0.0;
        int count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int s = 0; s < buffer.getNumSamples(); ++s)
            {
                float v = buffer.getSample(ch, s);
                sumSq += (double)(v * v);
                ++count;
            }
        return (count > 0) ? std::sqrt((float)(sumSq / count)) : 0.0f;
    }

    // Helper: compute peak absolute value of a buffer
    static float computePeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, s)));
        return peak;
    }

    void runTest() override
    {
        const double sampleRate = 44100.0;
        // Use a large buffer (~1.1s) so delay line fills up even at max repeatRate
        const int numSamples = 48000;
        const float testAmp = 0.5f;

        JunoTapeEcho echo;
        echo.prepare(sampleRate, 2, numSamples);

        // Helper: process the buffer multiple times to warm up the delay line,
        // then return the final output for measurement.
        // This ensures the delay line has enough content to produce wet output.
        auto processWarm = [&](juce::AudioBuffer<float>& buf, int warmupPasses = 3) {
            for (int p = 0; p < warmupPasses; ++p)
                echo.process(buf);
        };

        // ====================================================================
        // 1. Disabled: process() must not modify the buffer
        // ====================================================================
        beginTest("Disabled produces no output");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(false);
            echo.process(input);

            float rmsIn = computeRMS(reference);
            float rmsOut = computeRMS(input);
            expect(std::abs(rmsIn - rmsOut) < 0.001f,
                   "Disabled should not modify buffer: RMS " + juce::String(rmsIn)
                   + " vs " + juce::String(rmsOut));
        }

        // ====================================================================
        // 2. inputLevel = 0 → wet signal should be silent
        // ====================================================================
        beginTest("inputLevel=0 produces silent wet signal");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setInputLevel(0.0f);
            echo.setWetDry(1.0f);    // 100% wet
            echo.setIntensity(1.0f); // full feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(0);
            echo.reset();
            processWarm(input);      // warm up — should still be silent since input=0

            float peakOut = computePeak(input);
            expect(peakOut < 0.001f,
                   "inputLevel=0 should produce near-silent output, peak=" + juce::String(peakOut));
        }

        // ====================================================================
        // 3. inputLevel = 1, wetDry = 1 → wet signal present
        // ====================================================================
        beginTest("inputLevel=1 with 100% wet produces signal");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.3f); // moderate feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.3f); // some reverb
            echo.setDelaySetting(0);
            echo.reset();
            processWarm(input);

            float rmsOut = computeRMS(input);
            expect(rmsOut > 0.001f,
                   "inputLevel=1, wetDry=1 should produce signal, RMS=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 4. wetDry = 0 → output should approximately match dry
        // ====================================================================
        beginTest("wetDry=0 approximates dry signal");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto dryReference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setInputLevel(1.0f);
            echo.setWetDry(0.0f);
            echo.setIntensity(0.0f);
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);
            echo.setDelaySetting(0);
            echo.reset();
            processWarm(input);

            float rmsDry = computeRMS(dryReference);
            float rmsOut = computeRMS(input);
            // With wetDry=0, output = dry * 1.0 + wet * 0 = dry
            expect(std::abs(rmsDry - rmsOut) < 0.01f,
                   "wetDry=0 should match dry: RMS dry=" + juce::String(rmsDry)
                   + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 5. wetDry monotonically increases RMS
        // ====================================================================
        beginTest("wetDry monotonic RMS increase");
        {
            float prevRms = -1.0f;
            for (int step = 0; step <= 4; ++step)
            {
                auto input = createTestBuffer(numSamples, testAmp);
                float wd = step * 0.25f;

                echo.setEnabled(true);
                echo.setInputLevel(1.0f);
                echo.setWetDry(wd);
                echo.setIntensity(1.0f); // full feedback for strong wet signal
                echo.setRepeatRate(0.5f);
                echo.setEchoVol(1.0f);
                echo.setReverbVol(0.5f);
                echo.setDelaySetting(0);
                echo.reset();
                processWarm(input);

                float rms = computeRMS(input);
                if (prevRms >= 0.0f)
                    expect(rms >= prevRms * 0.9f,
                           "wetDry should increase RMS: step=" + juce::String(step)
                           + " rms=" + juce::String(rms) + " prev=" + juce::String(prevRms));
                prevRms = rms;
            }
        }

        // ====================================================================
        // 6. All reverb types produce different output
        // ====================================================================
        beginTest("Reverb types produce different output");
        {
            float rmsTypes[3] = { 0.0f, 0.0f, 0.0f };

            for (int rt = 0; rt < 3; ++rt)
            {
                auto input = createTestBuffer(numSamples, testAmp);

                echo.setEnabled(true);
                echo.setInputLevel(1.0f);
                echo.setWetDry(1.0f); // 100% wet to isolate reverb
                echo.setReverbType(rt);
                echo.setReverbVol(1.0f);
                echo.setEchoVol(0.0f); // no echo, just reverb
                echo.setIntensity(0.0f);
                echo.setRepeatRate(0.5f);
                echo.setBass(0.5f);
                echo.setTreble(0.5f);
                echo.setDelaySetting(0);
                echo.reset();
                processWarm(input);

                rmsTypes[rt] = computeRMS(input);
                expect(rmsTypes[rt] > 0.001f,
                       "reverbType=" + juce::String(rt) + " should produce output, RMS="
                       + juce::String(rmsTypes[rt]));
            }

            // At least one pair of types should differ
            float diff01 = std::abs(rmsTypes[0] - rmsTypes[1]);
            float diff02 = std::abs(rmsTypes[0] - rmsTypes[2]);
            float diff12 = std::abs(rmsTypes[1] - rmsTypes[2]);
            expect(diff01 > 0.001f || diff02 > 0.001f || diff12 > 0.001f,
                   "At least one reverb type pair should differ, diffs: 0-1="
                   + juce::String(diff01) + " 0-2=" + juce::String(diff02)
                   + " 1-2=" + juce::String(diff12));
        }

        // ====================================================================
        // 7. No NaN/Inf output for extreme parameter combinations
        // ====================================================================
        beginTest("No NaN/Inf for extreme parameter combinations");
        {
            for (int rt = 0; rt < 3; ++rt)
            {
                for (float il : { 0.0f, 1.0f })
                {
                    for (float wd : { 0.0f, 1.0f })
                    {
                        auto input = createTestBuffer(numSamples, 1.0f); // high amplitude

                        echo.setEnabled(true);
                        echo.setInputLevel(il);
                        echo.setWetDry(wd);
                        echo.setReverbType(rt);
                        echo.setIntensity(1.0f);
                        echo.setRepeatRate(1.0f);
                        echo.setEchoVol(1.0f);
                        echo.setReverbVol(1.0f);
                        echo.setBass(1.0f);
                        echo.setTreble(1.0f);
                        echo.setDelaySetting(10);
                        echo.reset();
                        processWarm(input);

                        bool hasNan = false;
                        bool hasInf = false;
                        for (int ch = 0; ch < input.getNumChannels(); ++ch)
                            for (int s = 0; s < input.getNumSamples(); ++s)
                            {
                                float v = input.getSample(ch, s);
                                if (std::isnan(v)) hasNan = true;
                                if (std::isinf(v)) hasInf = true;
                            }

                        expect(!hasNan, "NaN detected: rt=" + juce::String(rt)
                               + " il=" + juce::String(il) + " wd=" + juce::String(wd));
                        expect(!hasInf, "Inf detected: rt=" + juce::String(rt)
                               + " il=" + juce::String(il) + " wd=" + juce::String(wd));
                    }
                }
            }
        }

        // ====================================================================
        // 8. getActiveHeads: all 11 settings produce valid head combinations
        // ====================================================================
        beginTest("getActiveHeads all 11 settings");
        {
            for (int s = 0; s <= 10; ++s)
            {
                auto heads = JunoTapeEcho::getActiveHeads(s);
                int activeCount = (heads[0] ? 1 : 0) + (heads[1] ? 1 : 0) + (heads[2] ? 1 : 0);
                expect(activeCount >= 1 && activeCount <= 3,
                       "Setting " + juce::String(s) + " should have 1-3 active heads, got "
                       + juce::String(activeCount));
                expect(heads[0] || heads[1] || heads[2],
                       "Setting " + juce::String(s) + " must have at least one active head");
            }
        }

        // ====================================================================
        // 9. reset clears internal state
        // ====================================================================
        beginTest("Reset clears internal state");
        {
            // Run with high feedback to build up internal state
            auto input1 = createTestBuffer(numSamples, testAmp);
            echo.setEnabled(true);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(1.0f);
            echo.setRepeatRate(0.9f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(6);
            echo.reset();
            processWarm(input1);

            // Reset and run again with same settings — should give same result
            auto input2 = createTestBuffer(numSamples, testAmp);
            echo.reset();
            processWarm(input2);

            float rms1 = computeRMS(input1);
            float rms2 = computeRMS(input2);
            expect(std::abs(rms1 - rms2) < 0.001f,
                   "Reset should produce repeatable output: RMS1=" + juce::String(rms1)
                   + " RMS2=" + juce::String(rms2));
        }

        // ====================================================================
        // 10. Input-limiting: inputLevel=0 silences even with feedback
        // ====================================================================
        beginTest("inputLevel=0 silences feedback loop");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setInputLevel(0.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(1.0f); // max feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(6);
            echo.reset();
            processWarm(input);

            float peakOut = computePeak(input);
            // With inputLevel=0, no signal enters the delay line or reverb,
            // so even with max feedback the output stays silent.
            expect(peakOut < 0.001f,
                   "inputLevel=0 should silence even with full feedback, peak="
                   + juce::String(peakOut));
        }
    }
};

static JunoTapeEchoTests tapeEchoTests;
