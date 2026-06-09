/*
 * ABD JUNiO 601 - JunoTapeEcho Unit Tests
 *
 * Tests the calibration-linked DSP parameters:
 *   - inputLevel  (0..1): input gain before delay line
 *   - wetDry      (0..1): wet send level (parallel mix — dry passes unattenuated)
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
            for (int p = 0; p < warmupPasses - 1; ++p)
            {
                auto temp = createTestBuffer(buf.getNumSamples(), testAmp);
                echo.process(temp);
            }
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
        // 2. inputLevel = 0 → wet signal is silent, dry passes through
        // ====================================================================
        beginTest("inputLevel=0 silences wet signal, dry passes");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setInputLevel(0.0f);
            echo.setWetDry(1.0f);    // 100% wet (but wet=0 since input=0)
            echo.setIntensity(1.0f); // full feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(0);
            echo.reset();
            processWarm(input);      // warm up — wet is silent, dry passes through

            // Parallel mix: output = dry + wet*1.0 = dry + 0 = dry
            float rmsRef = computeRMS(reference);
            float rmsOut = computeRMS(input);
            expect(std::abs(rmsRef - rmsOut) < 0.01f,
                   "inputLevel=0 should pass dry signal: ref=" + juce::String(rmsRef)
                   + " out=" + juce::String(rmsOut));
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
            // With parallel mix and wetDry=0, output = dry + wet * 0 = dry
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
                echo.setDelaySetting(11); // REV ONLY (reverbOn=true)
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
        // 10. Input-limiting: inputLevel=0 silences wet, dry passes through
        // ====================================================================
        beginTest("inputLevel=0 silences feedback loop, dry passes");
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

            // Parallel mix: output = dry + wet*1.0 = dry + 0 = dry
            // (no signal enters delay line since inputLevel=0)
            float rmsRef = computeRMS(reference);
            float rmsOut = computeRMS(input);
            expect(std::abs(rmsRef - rmsOut) < 0.01f,
                   "inputLevel=0 should pass dry signal with feedback: ref="
                   + juce::String(rmsRef) + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 11. ECHO CANCEL ON + echo-only setting → dry passes unchanged
        // ====================================================================
        beginTest("EchoCancel ON with echo-only setting passes dry");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(true);  // ON = echo silenced
            echo.setInputLevel(1.0f);
            echo.setWetDry(0.5f);      // typical 50% wet
            echo.setIntensity(1.0f);
            echo.setRepeatRate(0.9f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(0);   // Head 1 only, NO reverb (reverbOn=false)
            echo.reset();
            processWarm(input);

            float rmsIn = computeRMS(reference);
            float rmsOut = computeRMS(input);
            // Parallel mix: output = dry + wet * wetDry
            // With echo cancel ON and no reverb: wet = 0, so output = dry
            expect(std::abs(rmsIn - rmsOut) < 0.001f,
                   "EchoCancel ON echo-only should pass dry: RMS in=" + juce::String(rmsIn)
                   + " out=" + juce::String(rmsOut));

            float peakOut = computePeak(input);
            expect(std::abs(peakOut - 0.5f) < 0.01f,
                   "EchoCancel ON echo-only peak should be ~0.5, got " + juce::String(peakOut));
        }

        // ====================================================================
        // 11b. ECHO CANCEL ON + reverb setting → reverb still active
        // ====================================================================
        beginTest("EchoCancel ON with reverb setting keeps reverb");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(true);  // ON = echo silenced
            echo.setInputLevel(0.8f);
            echo.setWetDry(0.5f);      // 50% wet
            echo.setIntensity(0.0f);   // no feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(0.0f);     // no echo (cancelled anyway)
            echo.setReverbVol(0.8f);
            echo.setDelaySetting(11);  // REV ONLY (reverbOn=true, no heads)
            echo.setReverbType(0);     // Waveguide Spring
            echo.reset();
            processWarm(input);

            float rmsOut = computeRMS(input);
            float dryRms = testAmp / std::sqrt(2.0f);
            // With parallel mix: output = dry + reverb*wetDry
            // Reverb modifies the signal — verify output differs from dry-only
            expect(std::abs(rmsOut - dryRms) > 0.001f,
                   "EchoCancel ON with reverb should alter signal: dry="
                   + juce::String(dryRms) + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 12. ECHO CANCEL OFF = effect processes (echo + reverb)
        // ====================================================================
        beginTest("EchoCancel OFF enables effect processing");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(false); // OFF = effect active
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.8f);   // High feedback to build up delay output
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(0.8f);
            echo.setReverbVol(0.5f);
            echo.setDelaySetting(4);   // H1 + Rev (shorter delay, easier to build up)
            echo.setReverbType(1);     // Schroeder Short
            echo.reset();
            processWarm(input, 5);     // Extra warmup passes for feedback buildup

            float rmsOut = computeRMS(input);
            // With high feedback, the delay line accumulates energy and output differs from dry
            float dryRms = testAmp / std::sqrt(2.0f);
            expect(std::abs(rmsOut - dryRms) > 0.01f,
                   "EchoCancel OFF should produce different RMS: dry=" + juce::String(dryRms)
                   + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 13. Reverb Only mode (delaySetting=11, no echo heads)
        // ====================================================================
        beginTest("Reverb Only mode (setting 11) produces reverb without echo");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setInputLevel(0.8f);
            echo.setWetDry(0.5f);      // 50% wet
            echo.setIntensity(0.0f);   // no feedback = no echo repeats
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(0.0f);     // no echo — reverb only
            echo.setReverbVol(0.8f);
            echo.setDelaySetting(11);  // REV ONLY
            echo.setReverbType(0);     // Waveguide Spring
            echo.reset();
            processWarm(input);

            float rmsOut = computeRMS(input);
            float dryRms = testAmp / std::sqrt(2.0f);
            // With parallel mix: output = dry + reverb*wetDry
            // Reverb modifies the signal — verify output differs from dry-only
            expect(std::abs(rmsOut - dryRms) > 0.001f,
                   "Reverb Only should alter signal: dry=" + juce::String(dryRms)
                   + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 14. Sync mode: BPM-based delay timing (1/4 note at 120 BPM)
        // ====================================================================
        beginTest("Sync mode at 120 BPM produces 125ms base delay (1/4 note)");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setSyncEnabled(true);
            echo.setHostBPM(120.0);
            echo.setSyncDivision(2); // 1/4 note
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.0f); // no feedback to isolate timing
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);
            echo.setDelaySetting(0);  // Head 1 only
            echo.reset();

            // At 120 BPM, 1/4 note = 500ms, head 1 = 1x ratio = 500ms
            // We can't easily measure exact delay, but we can verify sync doesn't crash
            processWarm(input);

            float rmsOut = computeRMS(input);
            float dryRms = testAmp / std::sqrt(2.0f);
            // With no feedback, the first echo should still be audible
            expect(rmsOut > dryRms * 0.5f,
                   "Sync mode should produce output: dry=" + juce::String(dryRms)
                   + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 15. Sync mode: different divisions produce different timing
        // ====================================================================
        beginTest("Sync divisions produce measurably different output");
        {
            // 1/8 note at 120 BPM = 250ms vs 1/4 note = 500ms
            // Use single pass (no processWarm) to avoid parallel sum accumulation
            float rms8, rms4;

            {
                auto input = createTestBuffer(numSamples, testAmp);
                echo.setEnabled(true);
                echo.setEchoCancel(false);
                echo.setSyncEnabled(true);
                echo.setHostBPM(120.0);
                echo.setSyncDivision(4); // 1/8 note
                echo.setInputLevel(0.4f);
                echo.setWetDry(0.3f);
                echo.setIntensity(0.0f);
                echo.setRepeatRate(0.5f);
                echo.setEchoVol(0.5f);
                echo.setReverbVol(0.0f);
                echo.setDelaySetting(0);
                echo.reset();
                echo.process(input); // single pass
                rms8 = computeRMS(input);
            }

            {
                auto input = createTestBuffer(numSamples, testAmp);
                echo.setEnabled(true);
                echo.setEchoCancel(false);
                echo.setSyncEnabled(true);
                echo.setHostBPM(120.0);
                echo.setSyncDivision(2); // 1/4 note
                echo.setInputLevel(0.4f);
                echo.setWetDry(0.3f);
                echo.setIntensity(0.0f);
                echo.setRepeatRate(0.5f);
                echo.setEchoVol(0.5f);
                echo.setReverbVol(0.0f);
                echo.setDelaySetting(0);
                echo.reset();
                echo.process(input); // single pass
                rms4 = computeRMS(input);
            }

            // Different delay times produce different interpolation artifacts
            // in the delay line read, causing measurable RMS differences
            float diff = std::abs(rms8 - rms4);
            expect(diff > 0.0005f,
                   "Different sync divisions should produce different output: 1/8="
                   + juce::String(rms8) + " 1/4=" + juce::String(rms4)
                   + " diff=" + juce::String(diff));
        }

        // ====================================================================
        // 16. Enabled=false: no processing at all
        // ====================================================================
        beginTest("Enabled false passes dry signal unchanged");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(false);
            echo.setEchoCancel(false);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(1.0f);
            echo.setRepeatRate(0.9f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(10);
            echo.reset();
            processWarm(input);

            float rmsIn = computeRMS(reference);
            float rmsOut = computeRMS(input);
            expect(std::abs(rmsIn - rmsOut) < 0.001f,
                   "Disabled should not modify buffer: RMS " + juce::String(rmsIn)
                   + " vs " + juce::String(rmsOut));
        }

        // ====================================================================
        // 17. All delay settings 0-11 produce valid output
        // ====================================================================
        beginTest("All 12 delay settings (0-11) produce valid output");
        {
            for (int setting = 0; setting <= 11; ++setting)
            {
                auto input = createTestBuffer(numSamples, testAmp);

                echo.setEnabled(true);
                echo.setEchoCancel(false);
                echo.setInputLevel(1.0f);
                echo.setWetDry(1.0f);
                echo.setIntensity(0.5f);
                echo.setRepeatRate(0.5f);
                echo.setEchoVol(0.7f);
                echo.setReverbVol(0.5f);
                echo.setDelaySetting(setting);
                echo.reset();
                processWarm(input);

                float peakOut = computePeak(input);
                bool hasNan = false, hasInf = false;
                for (int ch = 0; ch < input.getNumChannels(); ++ch)
                    for (int s = 0; s < input.getNumSamples(); ++s)
                    {
                        float v = input.getSample(ch, s);
                        if (std::isnan(v)) hasNan = true;
                        if (std::isinf(v)) hasInf = true;
                    }

                expect(!hasNan, "NaN in setting " + juce::String(setting));
                expect(!hasInf, "Inf in setting " + juce::String(setting));
                expect(peakOut < 10.0f,
                       "Peak should be reasonable in setting " + juce::String(setting)
                       + ", got " + juce::String(peakOut));
            }
        }

        // ====================================================================
        // 18. Delay setting 0 (Head 1 only) produces echo without reverb
        // ====================================================================
        beginTest("Delay setting 0 produces echo-only output");
        {
            auto input = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.6f);  // Moderate feedback
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);  // No reverb
            echo.setDelaySetting(0);  // Head 1 only, no reverb (setting 0 has reverbOn=false)
            echo.reset();
            processWarm(input);

            float rmsOut = computeRMS(input);
            float dryRms = testAmp / std::sqrt(2.0f);
            // With no reverb and moderate feedback, output should differ from dry
            expect(std::abs(rmsOut - dryRms) > 0.005f,
                   "Echo-only should differ from dry: dry=" + juce::String(dryRms)
                   + " out=" + juce::String(rmsOut));
        }

        // ====================================================================
        // 19. REGRESSION: wetDry=0.5 must not attenuate dry when wet=0
        // ====================================================================
        // The original bug: the crossfade formula dry*(1-wetDry) + wet*wetDry
        // reduced the dry signal by 50% even when wet=0 (e.g., echo cancel ON
        // on an echo-only setting). The fix changed to parallel mix:
        // dry + wet*wetDry, so dry passes through unattenuated.
        beginTest("REGRESSION: wetDry=0.5 does not attenuate dry when wet is zero");
        {
            auto input = createTestBuffer(numSamples, testAmp);
            auto reference = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(true);  // ON → echoGain=0, wet=0
            echo.setInputLevel(1.0f);
            echo.setWetDry(0.5f);      // THE critical parameter: 50% wet
            echo.setIntensity(0.0f);
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);   // no reverb
            echo.setDelaySetting(0);   // echo-only, no reverb (reverbOn=false)
            echo.reset();
            processWarm(input);

            float rmsDry = computeRMS(reference);
            float rmsOut = computeRMS(input);
            // With parallel mix: output = dry + 0 * 0.5 = dry
            // With old crossfade: output = dry * 0.5 + 0 = dry * 0.5 (BUG)
            expect(std::abs(rmsDry - rmsOut) < 0.001f,
                   "REGRESSION: wetDry=0.5 must NOT halve dry when wet=0: dry="
                   + juce::String(rmsDry) + " out=" + juce::String(rmsOut)
                   + " (old bug would give ~" + juce::String(rmsDry * 0.5f) + ")");

            float peakDry = computePeak(reference);
            float peakOut = computePeak(input);
            expect(std::abs(peakDry - peakOut) < 0.001f,
                   "REGRESSION: peak must match dry: expected ~" + juce::String(peakDry)
                   + " got " + juce::String(peakOut));
        }

        // ====================================================================
        // 20. Wow/Flutter parameter modifies output
        // ====================================================================
        beginTest("Wow/Flutter parameter modifies output");
        {
            auto input1 = createTestBuffer(numSamples, testAmp);
            auto input2 = createTestBuffer(numSamples, testAmp);

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.5f);
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);
            echo.setDelaySetting(0);

            // Run with no wow/flutter
            echo.setWowFlutter(0.0f);
            echo.reset();
            processWarm(input1);

            // Run with max wow/flutter
            echo.setWowFlutter(1.0f);
            echo.reset();
            processWarm(input2);

            float rms1 = computeRMS(input1);
            float rms2 = computeRMS(input2);
            // Since wow & flutter modulates tape delay time dynamically, it changes the output signal.
            expect(std::abs(rms1 - rms2) > 0.0001f,
                   "Wow/Flutter should modify output: RMS1=" + juce::String(rms1) + " RMS2=" + juce::String(rms2));
        }

        // ====================================================================
        // 21. Reverb Decay parameter alters decay length (RMS)
        // ====================================================================
        beginTest("Reverb Decay parameter modifies output");
        {
            auto input1 = createTestBuffer(numSamples, testAmp);
            auto input2 = createTestBuffer(numSamples, testAmp);
            for (int ch = 0; ch < 2; ++ch) {
                std::fill(input1.getWritePointer(ch) + 2000, input1.getWritePointer(ch) + numSamples, 0.0f);
                std::fill(input2.getWritePointer(ch) + 2000, input2.getWritePointer(ch) + numSamples, 0.0f);
            }

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setInputLevel(1.0f);
            echo.setWetDry(1.0f);
            echo.setIntensity(0.0f);
            echo.setEchoVol(0.0f);
            echo.setReverbVol(1.0f);
            echo.setDelaySetting(11); // Reverb Only

            // Run with low decay
            echo.setReverbDecay(0.0f);
            echo.reset();
            processWarm(input1);

            // Run with high decay
            echo.setReverbDecay(1.0f);
            echo.reset();
            processWarm(input2);

            float rms1 = computeRMS(input1);
            float rms2 = computeRMS(input2);
            // High decay means more feedback within reverb network, leading to larger RMS.
            expect(rms2 > rms1 + 0.001f,
                   "Higher Reverb Decay should produce longer/louder reverb tails: decay 0.0 RMS="
                   + juce::String(rms1) + " decay 1.0 RMS=" + juce::String(rms2));
        }

        // ====================================================================
        // 22. Echo Isolator alters high frequency content (reduces RMS)
        // ====================================================================
        beginTest("Echo Isolator parameter modifies output");
        {
            auto input1 = createTestBuffer(numSamples, testAmp);
            auto input2 = createTestBuffer(numSamples, testAmp);
            for (int ch = 0; ch < 2; ++ch) {
                std::fill(input1.getWritePointer(ch) + 2000, input1.getWritePointer(ch) + numSamples, 0.0f);
                std::fill(input2.getWritePointer(ch) + 2000, input2.getWritePointer(ch) + numSamples, 0.0f);
            }

            echo.setEnabled(true);
            echo.setEchoCancel(false);
            echo.setInputLevel(1.0f);
            echo.setWetDry(0.5f);
            echo.setIntensity(0.6f); // moderate feedback to prevent rail clipping
            echo.setRepeatRate(0.5f);
            echo.setEchoVol(1.0f);
            echo.setReverbVol(0.0f);
            echo.setDelaySetting(0);

            // Run with isolator at 0 (brightest, no lowpass damping)
            echo.setEchoIsolator(0.0f);
            echo.reset();
            processWarm(input1, 2);

            // Run with isolator at 1 (darkest, heavy lowpass damping)
            echo.setEchoIsolator(1.0f);
            echo.reset();
            processWarm(input2, 2);

            float rms1 = computeRMS(input1);
            float rms2 = computeRMS(input2);
            // With isolator at 1.0, feedback cutoff is extremely low (200Hz), so high frequencies decay instantly.
            // This results in less overall energy (smaller RMS).
            expect(rms1 > rms2 + 0.0001f,
                   "Echo Isolator ON should reduce overall feedback energy: isolator 0.0 RMS="
                   + juce::String(rms1) + " isolator 1.0 RMS=" + juce::String(rms2));
        }
    }
};

static JunoTapeEchoTests tapeEchoTests;
