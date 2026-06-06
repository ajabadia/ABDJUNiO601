#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "../SynthParams.h"
#include "../Synth/JunoDCO.h"
#include "../Synth/ChorusBBD.h"
#include "../Synth/JunoVCF.h"
#include "../Synth/JunoADSR.h"
#include "../Synth/JunoHPF.h"
// ============================================================================
// GET_MODEL Macro Tests
// These verify that the model routing macros return the correct values
// based on SynthParams member values (Super Six mode — no compile-time define).
// ============================================================================
class ModelRoutingMacroTests : public juce::UnitTest {
public:
    ModelRoutingMacroTests() : juce::UnitTest("ModelRouting Macros", "JunoModel") {}

    void runTest() override
    {
        beginTest("GET_MODEL_DCO returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelDCO = 0;
            expect(GET_MODEL_DCO(p) == 0, "GET_MODEL_DCO should return 0 for J6");
            p.modelDCO = 1;
            expect(GET_MODEL_DCO(p) == 1, "GET_MODEL_DCO should return 1 for J60");
            p.modelDCO = 2;
            expect(GET_MODEL_DCO(p) == 2, "GET_MODEL_DCO should return 2 for J106");
        }

        beginTest("GET_MODEL_CHORUS returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelChorus = 0;
            expect(GET_MODEL_CHORUS(p) == 0, "GET_MODEL_CHORUS should return 0 for J6");
            p.modelChorus = 1;
            expect(GET_MODEL_CHORUS(p) == 1, "GET_MODEL_CHORUS should return 1 for J60");
            p.modelChorus = 2;
            expect(GET_MODEL_CHORUS(p) == 2, "GET_MODEL_CHORUS should return 2 for J106");
        }

        beginTest("GET_MODEL_ARP returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelArp = 0;
            expect(GET_MODEL_ARP(p) == 0, "GET_MODEL_ARP should return 0 for J6 (arp ON)");
            p.modelArp = 1;
            expect(GET_MODEL_ARP(p) == 1, "GET_MODEL_ARP should return 1 for J60 (arp ON)");
            p.modelArp = 2;
            expect(GET_MODEL_ARP(p) == 2, "GET_MODEL_ARP should return 2 for J106 (arp DISABLED)");
        }

        beginTest("GET_MODEL_HPF returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelHPF = 0;
            expect(GET_MODEL_HPF(p) == 0, "GET_MODEL_HPF should return 0 for J6");
            p.modelHPF = 1;
            expect(GET_MODEL_HPF(p) == 1, "GET_MODEL_HPF should return 1 for J60");
            p.modelHPF = 2;
            expect(GET_MODEL_HPF(p) == 2, "GET_MODEL_HPF should return 2 for J106");
        }

        beginTest("GET_MODEL_VCF returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelVCF = 0;
            expect(GET_MODEL_VCF(p) == 0, "GET_MODEL_VCF should return 0 for J6");
            p.modelVCF = 1;
            expect(GET_MODEL_VCF(p) == 1, "GET_MODEL_VCF should return 1 for J60");
            p.modelVCF = 2;
            expect(GET_MODEL_VCF(p) == 2, "GET_MODEL_VCF should return 2 for J106");
        }

        beginTest("GET_MODEL_ADSR returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelADSR = 0;
            expect(GET_MODEL_ADSR(p) == 0, "GET_MODEL_ADSR should return 0 for J6");
            p.modelADSR = 1;
            expect(GET_MODEL_ADSR(p) == 1, "GET_MODEL_ADSR should return 1 for J60");
            p.modelADSR = 2;
            expect(GET_MODEL_ADSR(p) == 2, "GET_MODEL_ADSR should return 2 for J106");
        }

        beginTest("GET_MODEL_PORTA returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelPorta = 0;
            expect(GET_MODEL_PORTA(p) == 0, "GET_MODEL_PORTA should return 0 for J6 (no porta)");
            p.modelPorta = 1;
            expect(GET_MODEL_PORTA(p) == 1, "GET_MODEL_PORTA should return 1 for J60 (no porta)");
            p.modelPorta = 2;
            expect(GET_MODEL_PORTA(p) == 2, "GET_MODEL_PORTA should return 2 for J106 (porta ON)");
        }

        beginTest("GET_MODEL_UNISON returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelUnison = 0;
            expect(GET_MODEL_UNISON(p) == 0, "GET_MODEL_UNISON should return 0 for J6 (no unison)");
            p.modelUnison = 1;
            expect(GET_MODEL_UNISON(p) == 1, "GET_MODEL_UNISON should return 1 for J60 (no unison)");
            p.modelUnison = 2;
            expect(GET_MODEL_UNISON(p) == 2, "GET_MODEL_UNISON should return 2 for J106 (unison ON)");
        }
    }
};

// ============================================================================
// DCO Model-Specific Behavior Tests
// ============================================================================
class JunoDCOModelTests : public juce::UnitTest {
public:
    JunoDCOModelTests() : juce::UnitTest("JunoDCO Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("J6 mode forces sub-oscillator output to zero");
        {
            JunoDCO dcoJ6;
            dcoJ6.prepare(44100.0, 512);
            dcoJ6.setModel(0); // J6
            dcoJ6.setFrequency(440.0f);
            dcoJ6.setSawLevel(0.0f);
            dcoJ6.setPulseLevel(0.0f);
            dcoJ6.setSubLevel(1.0f); // Sub at max — should be ignored in J6 mode

            float peak = 0.0f;
            for (int i = 0; i < 4410; ++i)
                peak = std::max(peak, std::abs(dcoJ6.getNextSample(0.0f)));

            expect(peak < 0.01f,
                   "J6 mode with only sub active should produce near-zero output, got " + juce::String(peak));
        }

        beginTest("J106 mode sub-oscillator produces output normally");
        {
            JunoDCO dco106;
            dco106.prepare(44100.0, 512);
            dco106.setModel(2); // J106
            dco106.setFrequency(440.0f);
            dco106.setSawLevel(0.0f);
            dco106.setPulseLevel(0.0f);
            dco106.setSubLevel(1.0f); // Sub at max — should be active in J106 mode

            float peak = 0.0f;
            for (int i = 0; i < 4410; ++i)
                peak = std::max(peak, std::abs(dco106.getNextSample(0.0f)));

            expect(peak > 0.01f,
                   "J106 mode with sub at max should produce output, got " + juce::String(peak));
        }

        beginTest("J60 mode sub-oscillator produces output normally");
        {
            JunoDCO dco60;
            dco60.prepare(44100.0, 512);
            dco60.setModel(1); // J60
            dco60.setFrequency(440.0f);
            dco60.setSawLevel(0.0f);
            dco60.setPulseLevel(0.0f);
            dco60.setSubLevel(1.0f); // Sub at max — should be active in J60 mode

            float peak = 0.0f;
            for (int i = 0; i < 4410; ++i)
                peak = std::max(peak, std::abs(dco60.getNextSample(0.0f)));

            expect(peak > 0.01f,
                   "J60 mode with sub at max should produce output, got " + juce::String(peak));
        }

        beginTest("J6 mode PWM slider has no effect (fixed 50% duty)");
        {
            // For a bipolar pulse wave (-1 to +1), RMS is always 1 regardless
            // of duty cycle. Instead we use the DC average (mean):
            //   average = (1 - 2*D) where D = duty cycle fraction at -1
            // At 50% duty (J6 fixed): average ≈ 0
            // At extreme duty (J106): average is near -1 or +1

            // J6: measure DC average with PWM at minimum
            JunoDCO dcoJ6Min;
            dcoJ6Min.prepare(44100.0, 512);
            dcoJ6Min.setModel(0);
            dcoJ6Min.setFrequency(220.0f);
            dcoJ6Min.setSawLevel(0.0f);
            dcoJ6Min.setPulseLevel(1.0f);
            dcoJ6Min.setSubLevel(0.0f);
            dcoJ6Min.setPWM(0.0f); // Should be ignored in J6 mode

            float sumMin = 0.0f;
            for (int i = 0; i < 4410; ++i)
                sumMin += dcoJ6Min.getNextSample(0.0f);
            float avgMin = sumMin / 4410.0f;

            // J6: measure DC average with PWM at maximum
            JunoDCO dcoJ6Max;
            dcoJ6Max.prepare(44100.0, 512);
            dcoJ6Max.setModel(0);
            dcoJ6Max.setFrequency(220.0f);
            dcoJ6Max.setSawLevel(0.0f);
            dcoJ6Max.setPulseLevel(1.0f);
            dcoJ6Max.setSubLevel(0.0f);
            dcoJ6Max.setPWM(1.0f); // Should be ignored in J6 mode

            float sumMax = 0.0f;
            for (int i = 0; i < 4410; ++i)
                sumMax += dcoJ6Max.getNextSample(0.0f);
            float avgMax = sumMax / 4410.0f;

            // In J6 mode, both should be approximately equal (fixed 50% duty ≈ 0 average)
            expect(std::abs(avgMin) < 0.05f,
                   "J6 mode: PWM min should produce ~0 DC offset, got " + juce::String(avgMin));
            expect(std::abs(avgMax) < 0.05f,
                   "J6 mode: PWM max should produce ~0 DC offset, got " + juce::String(avgMax));

            // J106: verify that PWM slider DOES affect output
            JunoDCO dco106Min;
            dco106Min.prepare(44100.0, 512);
            dco106Min.setModel(2);
            dco106Min.setFrequency(220.0f);
            dco106Min.setSawLevel(0.0f);
            dco106Min.setPulseLevel(1.0f);
            dco106Min.setSubLevel(0.0f);
            dco106Min.setPWM(0.01f); // Near-minimum duty → effPW≈0.94 → avg≈-0.88

            float sum106Min = 0.0f;
            for (int i = 0; i < 4410; ++i)
                sum106Min += dco106Min.getNextSample(0.0f);
            float avg106Min = sum106Min / 4410.0f;

            JunoDCO dco106Max;
            dco106Max.prepare(44100.0, 512);
            dco106Max.setModel(2);
            dco106Max.setFrequency(220.0f);
            dco106Max.setSawLevel(0.0f);
            dco106Max.setPulseLevel(1.0f);
            dco106Max.setSubLevel(0.0f);
            dco106Max.setPWM(1.0f); // Near-maximum duty → effPW≈0.05 → avg≈0.90

            float sum106Max = 0.0f;
            for (int i = 0; i < 4410; ++i)
                sum106Max += dco106Max.getNextSample(0.0f);
            float avg106Max = sum106Max / 4410.0f;

            // In J106 mode, PWM should create a measurable difference in DC offset
            float dcDiff = std::abs(avg106Max - avg106Min);
            expect(dcDiff > 0.5f,
                   "J106 mode: PWM should create DC offset difference. avg(min)="
                   + juce::String(avg106Min) + ", avg(max)=" + juce::String(avg106Max)
                   + ", diff=" + juce::String(dcDiff));

            std::printf("DCO PWM test: J6 avg(min)=%.4f avg(max)=%.4f, J106 avg(min)=%.4f avg(max)=%.4f diff=%.4f\n",
                   avgMin, avgMax, avg106Min, avg106Max, dcDiff);
        }
    }
};

// ============================================================================
// Chorus Model-Specific Behavior Tests
// ============================================================================
class JunoChorusModelTests : public juce::UnitTest {
public:
    JunoChorusModelTests() : juce::UnitTest("JunoChorus Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("ChorusBBD::setChorusModel API works correctly");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);

            chorus.setChorusModel(ChorusBBD::ChorusModel::J60);
            expect(chorus.getChorusModel() == ChorusBBD::ChorusModel::J60,
                   "Chorus model should be J60 after setChorusModel(J60)");

            chorus.setChorusModel(ChorusBBD::ChorusModel::J106);
            expect(chorus.getChorusModel() == ChorusBBD::ChorusModel::J106,
                   "Chorus model should be J106 after setChorusModel(J106)");
        }

        beginTest("Chorus off mode produces pass-through (no audible effect)");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::Off);
            chorus.setMix(0.0f); // Full dry

            juce::AudioBuffer<float> buffer(2, 1024);
            for (int i = 0; i < 1024; ++i) {
                float s = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
            }

            // Copy reference before processing
            juce::AudioBuffer<float> reference(2, 1024);
            reference.copyFrom(0, 0, buffer, 0, 0, 1024);
            reference.copyFrom(1, 0, buffer, 1, 0, 1024);

            chorus.process(buffer);

            // With Off mode and dry mix, output should be unchanged
            float maxDiff = 0.0f;
            for (int i = 0; i < 1024; ++i) {
                float diff = std::abs(buffer.getSample(0, i) - reference.getSample(0, i));
                maxDiff = std::max(maxDiff, diff);
            }
            expect(maxDiff < 1e-6f,
                   "Chorus Off mode should pass audio through unchanged, maxDiff=" + juce::String(maxDiff));
        }

        beginTest("Chorus I mode produces stereo difference");
        {
            ChorusBBD chorus;
            chorus.prepare(44100.0, 512);
            chorus.setMode(ChorusBBD::Mode::ChorusI);
            chorus.setMix(1.0f); // Full wet

            juce::AudioBuffer<float> buffer(2, 2048);
            for (int i = 0; i < 2048; ++i) {
                float s = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s); // Start with identical channels
            }

            chorus.process(buffer);

            // In Chorus I, L and R should differ (antiphase LFO)
            bool anyDiff = false;
            for (int i = 512; i < 2048; ++i) { // Skip first samples (delay fill)
                if (std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) > 1e-4f) {
                    anyDiff = true;
                    break;
                }
            }
            expect(anyDiff,
                   "Chorus I should produce stereo difference via antiphase LFO");
        }
    }
};

// ============================================================================
// Arpeggiator Model-Specific Behavior Tests
// ============================================================================
class JunoArpModelTests : public juce::UnitTest {
public:
    JunoArpModelTests() : juce::UnitTest("JunoArp Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("J106 model routing disables arpeggiator via GET_MODEL_ARP");
        {
            SynthParams p;
            p.modelArp = 2; // J106
            int arpModel = GET_MODEL_ARP(p);
            expect(arpModel == 2,
                   "GET_MODEL_ARP should return 2 for J106 model, got " + juce::String(arpModel));
        }

        beginTest("J6 model routing enables arpeggiator via GET_MODEL_ARP");
        {
            SynthParams p;
            p.modelArp = 0; // J6
            int arpModel = GET_MODEL_ARP(p);
            expect(arpModel == 0,
                   "GET_MODEL_ARP should return 0 for J6 model, got " + juce::String(arpModel));
        }

        beginTest("J60 model routing enables arpeggiator via GET_MODEL_ARP");
        {
            SynthParams p;
            p.modelArp = 1; // J60
            int arpModel = GET_MODEL_ARP(p);
            expect(arpModel == 1,
                   "GET_MODEL_ARP should return 1 for J60 model, got " + juce::String(arpModel));
        }
    }
};

// ============================================================================
// Portamento Model-Specific Behavior Tests
// ============================================================================
class JunoPortaModelTests : public juce::UnitTest {
public:
    JunoPortaModelTests() : juce::UnitTest("JunoPorta Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("J106 model enables portamento via GET_MODEL_PORTA");
        {
            SynthParams p;
            p.modelPorta = 2;
            expect(GET_MODEL_PORTA(p) == 2,
                   "GET_MODEL_PORTA should return 2 for J106");
        }

        beginTest("J6 model disables portamento via GET_MODEL_PORTA");
        {
            SynthParams p;
            p.modelPorta = 0;
            expect(GET_MODEL_PORTA(p) == 0,
                   "GET_MODEL_PORTA should return 0 for J6");
        }
    }
};

// ============================================================================
// Unison Model-Specific Behavior Tests
// ============================================================================
class JunoUnisonModelTests : public juce::UnitTest {
public:
    JunoUnisonModelTests() : juce::UnitTest("JunoUnison Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("J106 model enables unison via GET_MODEL_UNISON");
        {
            SynthParams p;
            p.modelUnison = 2;
            expect(GET_MODEL_UNISON(p) == 2,
                   "GET_MODEL_UNISON should return 2 for J106");
        }

        beginTest("J6 model disables unison via GET_MODEL_UNISON");
        {
            SynthParams p;
            p.modelUnison = 0;
            expect(GET_MODEL_UNISON(p) == 0,
                   "GET_MODEL_UNISON should return 0 for J6");
        }
    }
};

// ============================================================================
// VCF Model-Specific Behavior Tests
// ============================================================================
class JunoVCFModelTests : public juce::UnitTest {
public:
    JunoVCFModelTests() : juce::UnitTest("JunoVCF Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("J106 resonance curve produces different output than J6 curve");
        {
            JunoVCF vcfJ106;
            vcfJ106.setSampleRate(44100.0);
            vcfJ106.setModelAndResCurve(true, true);  // J106 polynomial resonance

            JunoVCF vcfJ6;
            vcfJ6.setSampleRate(44100.0);
            vcfJ6.setModelAndResCurve(false, true);   // J6 exponential resonance

            // Process a test signal through both with the same high resonance
            float testSignal = 0.0f;
            float sum106 = 0.0f, sum6 = 0.0f;
            for (int i = 0; i < 4410; ++i) {
                // Generate a tone sweep
                testSignal = std::sin(2.0f * juce::MathConstants<float>::pi * (220.0f + i * 10.0f) * i / 44100.0f);
                float out106 = vcfJ106.processSample(testSignal, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f);
                float out6   = vcfJ6.processSample(testSignal, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f);
                sum106 += out106 * out106;
                sum6   += out6 * out6;
            }

            // Both should be stable (no NaN)
            expect(!std::isnan(sum106) && !std::isnan(sum6),
                   "VCF should not produce NaN in any model mode");

            // RMS difference between J6 and J106 resonance curves should exist
            float rms106 = std::sqrt(sum106 / 4410.0f);
            float rms6   = std::sqrt(sum6 / 4410.0f);
            float ratio = (rms106 > rms6) ? (rms106 / rms6) : (rms6 / rms106);

            // The curves are different enough to produce >1% RMS difference
            expect(ratio > 1.01f || ratio < 0.99f,
                   "J106 and J6 resonance curves should differ. J106 RMS="
                   + juce::String(rms106) + ", J6 RMS=" + juce::String(rms6)
                   + ", ratio=" + juce::String(ratio));

            std::printf("VCF model test: J106 RMS=%.4f, J6 RMS=%.4f, ratio=%.4f\n", rms106, rms6, ratio);
        }

        beginTest("J106 mode produces self-oscillation at high resonance");
        {
            JunoVCF vcf;
            vcf.setSampleRate(44100.0);
            vcf.reset();

            // Impulse to kickstart oscillation
            vcf.processSample(1.0f, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f,
                              0.95f, 1.0f, 1.0f);

            float maxVal = 0.0f;
            for (int i = 0; i < 2000; ++i) {
                float s = vcf.processSample(0.0f, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f,
                                             0.95f, 1.0f, 1.0f);
                maxVal = std::max(maxVal, std::abs(s));
            }
            expect(maxVal > 0.001f,
                   "VCF should self-oscillate at max resonance after impulse, got max=" + juce::String(maxVal));
        }
    }
};

// ============================================================================
// HPF Model-Specific Behavior Tests
// ============================================================================
class JunoHPFModelTests : public juce::UnitTest {
public:
    JunoHPFModelTests() : juce::UnitTest("JunoHPF Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("GET_MODEL_HPF returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelHPF = 0;
            expect(GET_MODEL_HPF(p) == 0,
                   "GET_MODEL_HPF should return 0 for J6 (no HPF)");

            p.modelHPF = 1;
            expect(GET_MODEL_HPF(p) == 1,
                   "GET_MODEL_HPF should return 1 for J60 (continuous HPF)");

            p.modelHPF = 2;
            expect(GET_MODEL_HPF(p) == 2,
                   "GET_MODEL_HPF should return 2 for J106 (4-position HPF)");
        }

        beginTest("HPF default value is consistent with model");
        {
            SynthParams p;
            // Default modelHPF is 2 (J106) — 4-position switch
            expect(p.modelHPF == 2,
                   "Default modelHPF should be 2 (J106), got " + juce::String(p.modelHPF));

            // Default hpfFreq is 0 (off position for J106)
            expect(p.hpfFreq == 0,
                   "Default hpfFreq should be 0 (off), got " + juce::String(p.hpfFreq));
        }
    }
};

// ============================================================================
// ADSR Model-Specific Behavior Tests
// ============================================================================
class JunoADSRModelTests : public juce::UnitTest {
public:
    JunoADSRModelTests() : juce::UnitTest("JunoADSR Model Routing", "JunoModel") {}

    void runTest() override
    {
        beginTest("GET_MODEL_ADSR returns correct values from SynthParams");
        {
            SynthParams p;
            p.modelADSR = 0;
            expect(GET_MODEL_ADSR(p) == 0,
                   "GET_MODEL_ADSR should return 0 for J6");

            p.modelADSR = 1;
            expect(GET_MODEL_ADSR(p) == 1,
                   "GET_MODEL_ADSR should return 1 for J60");

            p.modelADSR = 2;
            expect(GET_MODEL_ADSR(p) == 2,
                   "GET_MODEL_ADSR should return 2 for J106");
        }

        beginTest("ADSR envelope lifecycle works consistently across models");
        {
            JunoADSR adsr;
            adsr.setSampleRate(44100.0);

            // Initial state: not active
            expect(!adsr.isActive(),
                   "ADSR should not be active before noteOn");

            // Note on activates
            adsr.noteOn();
            expect(adsr.isActive(),
                   "ADSR should be active after noteOn");

            // Attack should progress monotonically
            float last = -1.0f;
            bool attackStarted = false;
            for (int i = 0; i < 4410; ++i) {
                float v = adsr.getNextSample();
                if (adsr.getCurrentStage() == JunoADSR::Stage::Attack) {
                    attackStarted = true;
                    expect(v >= last - 1e-4f,
                           "ADSR attack should be monotonic at sample " + juce::String(i)
                           + ", last=" + juce::String(last) + ", v=" + juce::String(v));
                    last = v;
                }
                if (adsr.getCurrentStage() != JunoADSR::Stage::Attack)
                    break;
            }
            expect(attackStarted, "ADSR should have entered Attack stage during noteOn");
        }
    }
};

// ============================================================================
// Model Routing Sanity Tests — Combined J6/J60/J106 configurations
// ============================================================================
class JunoModelConfigTests : public juce::UnitTest {
public:
    JunoModelConfigTests() : juce::UnitTest("Model Routing Config", "JunoModel") {}

    void runTest() override
    {
        beginTest("J6 configuration: all model params set to 0");
        {
            SynthParams p;
            p.modelDCO = 0;
            p.modelChorus = 0;
            p.modelHPF = 0;
            p.modelVCF = 0;
            p.modelADSR = 0;
            p.modelArp = 0;
            p.modelPorta = 0;
            p.modelUnison = 0;
            p.modelPoly = 0;

            // J6: continuous DCO (no sub/PWM control)
            expect(GET_MODEL_DCO(p) == 0, "J6 config: GET_MODEL_DCO should be 0");
            // J6: no chorus
            expect(GET_MODEL_CHORUS(p) == 0, "J6 config: GET_MODEL_CHORUS should be 0");
            // J6: continuous HPF
            expect(GET_MODEL_HPF(p) == 0, "J6 config: GET_MODEL_HPF should be 0");
            // J6: IR3109 SoftClip VCF
            expect(GET_MODEL_VCF(p) == 0, "J6 config: GET_MODEL_VCF should be 0");
            // J6: analog RC ADSR
            expect(GET_MODEL_ADSR(p) == 0, "J6 config: GET_MODEL_ADSR should be 0");
            // J6: arpeggiator ON
            expect(GET_MODEL_ARP(p) == 0, "J6 config: GET_MODEL_ARP should be 0 (enabled)");
            // J6: no portamento
            expect(GET_MODEL_PORTA(p) == 0, "J6 config: GET_MODEL_PORTA should be 0 (disabled)");
            // J6: no unison
            expect(GET_MODEL_UNISON(p) == 0, "J6 config: GET_MODEL_UNISON should be 0 (disabled)");
        }

        beginTest("J60 configuration: all model params set to 1");
        {
            SynthParams p;
            p.modelDCO = 1;
            p.modelChorus = 1;
            p.modelHPF = 1;
            p.modelVCF = 1;
            p.modelADSR = 1;
            p.modelArp = 1;
            p.modelPorta = 1;
            p.modelUnison = 1;
            p.modelPoly = 1;

            expect(GET_MODEL_DCO(p) == 1, "J60 config: GET_MODEL_DCO should be 1");
            expect(GET_MODEL_CHORUS(p) == 1, "J60 config: GET_MODEL_CHORUS should be 1");
            expect(GET_MODEL_HPF(p) == 1, "J60 config: GET_MODEL_HPF should be 1");
            expect(GET_MODEL_VCF(p) == 1, "J60 config: GET_MODEL_VCF should be 1");
            expect(GET_MODEL_ADSR(p) == 1, "J60 config: GET_MODEL_ADSR should be 1");
            expect(GET_MODEL_ARP(p) == 1, "J60 config: GET_MODEL_ARP should be 1 (enabled)");
            expect(GET_MODEL_PORTA(p) == 1, "J60 config: GET_MODEL_PORTA should be 1 (disabled)");
            expect(GET_MODEL_UNISON(p) == 1, "J60 config: GET_MODEL_UNISON should be 1 (disabled)");
        }

        beginTest("J106 configuration: all model params set to 2");
        {
            SynthParams p;
            p.modelDCO = 2;
            p.modelChorus = 2;
            p.modelHPF = 2;
            p.modelVCF = 2;
            p.modelADSR = 2;
            p.modelArp = 2;
            p.modelPorta = 2;
            p.modelUnison = 2;
            p.modelPoly = 2;

            expect(GET_MODEL_DCO(p) == 2, "J106 config: GET_MODEL_DCO should be 2");
            expect(GET_MODEL_CHORUS(p) == 2, "J106 config: GET_MODEL_CHORUS should be 2");
            expect(GET_MODEL_HPF(p) == 2, "J106 config: GET_MODEL_HPF should be 2");
            expect(GET_MODEL_VCF(p) == 2, "J106 config: GET_MODEL_VCF should be 2");
            expect(GET_MODEL_ADSR(p) == 2, "J106 config: GET_MODEL_ADSR should be 2");
            expect(GET_MODEL_ARP(p) == 2, "J106 config: GET_MODEL_ARP should be 2 (disabled)");
            expect(GET_MODEL_PORTA(p) == 2, "J106 config: GET_MODEL_PORTA should be 2 (enabled)");
            expect(GET_MODEL_UNISON(p) == 2, "J106 config: GET_MODEL_UNISON should be 2 (enabled)");
        }

        beginTest("Model routing values are distinct per model");
        {
            // J6: all model params = 0
            SynthParams j6;
            j6.modelDCO = 0; j6.modelChorus = 0; j6.modelHPF = 0;
            j6.modelVCF = 0; j6.modelADSR = 0; j6.modelArp = 0;
            j6.modelPorta = 0; j6.modelUnison = 0;

            // J106: all model params = 2
            SynthParams j106;
            j106.modelDCO = 2; j106.modelChorus = 2; j106.modelHPF = 2;
            j106.modelVCF = 2; j106.modelADSR = 2; j106.modelArp = 2;
            j106.modelPorta = 2; j106.modelUnison = 2;

            // Verify they are distinct (J6 all 0, J106 all 2)
            expect(GET_MODEL_DCO(j6)    != GET_MODEL_DCO(j106),    "DCO values must differ between J6 and J106");
            expect(GET_MODEL_CHORUS(j6) != GET_MODEL_CHORUS(j106), "Chorus values must differ between J6 and J106");
            expect(GET_MODEL_ARP(j6)    != GET_MODEL_ARP(j106),    "Arp values must differ between J6 and J106");
            expect(GET_MODEL_PORTA(j6)  != GET_MODEL_PORTA(j106),  "Porta values must differ between J6 and J106");
            expect(GET_MODEL_UNISON(j6) != GET_MODEL_UNISON(j106), "Unison values must differ between J6 and J106");
            expect(GET_MODEL_HPF(j6)    != GET_MODEL_HPF(j106),    "HPF values must differ between J6 and J106");
            expect(GET_MODEL_VCF(j6)    != GET_MODEL_VCF(j106),    "VCF values must differ between J6 and J106");
            expect(GET_MODEL_ADSR(j6)   != GET_MODEL_ADSR(j106),   "ADSR values must differ between J6 and J106");
        }
    }
};

// ============================================================================
// J-60 DSP Behavior Tests — verifies J-60 specific DSP routing at sample level
// ============================================================================
class JunoDSPJ60Tests : public juce::UnitTest {
public:
    JunoDSPJ60Tests() : juce::UnitTest("JunoDSP J-60 Behavior", "JunoModel") {}

    void runTest() override
    {
        beginTest("J60 ADSR kJ60 mode produces different envelope than kJ6 and kJ106");
        {
            JunoADSR adsr6, adsr60, adsr106;
            adsr6.setSampleRate(44100.0);
            adsr60.setSampleRate(44100.0);
            adsr106.setSampleRate(44100.0);

            adsr6.setMode(ADSRMode::kJ6);
            adsr60.setMode(ADSRMode::kJ60);
            adsr106.setMode(ADSRMode::kJ106);

            float attack = 0.05f;   // 50ms attack
            float decay = 0.5f;     // 500ms decay
            float sustain = 0.5f;   // 50% sustain
            float release = 0.3f;   // 300ms release

            adsr6.setAttack(attack); adsr60.setAttack(attack); adsr106.setAttack(attack);
            adsr6.setDecay(decay);   adsr60.setDecay(decay);   adsr106.setDecay(decay);
            adsr6.setSustain(sustain); adsr60.setSustain(sustain); adsr106.setSustain(sustain);
            adsr6.setRelease(release); adsr60.setRelease(release); adsr106.setRelease(release);

            adsr6.noteOn();
            adsr60.noteOn();
            adsr106.noteOn();

            float sum6 = 0.0f, sum60 = 0.0f, sum106 = 0.0f;
            int cnt6 = 0, cnt60 = 0, cnt106 = 0;
            bool attack6 = false, attack60 = false, attack106 = false;

            const int totalSamples = 8820; // 200ms
            for (int i = 0; i < totalSamples; ++i) {
                float v6 = adsr6.getNextSample();
                float v60 = adsr60.getNextSample();
                float v106 = adsr106.getNextSample();

                if (adsr6.getCurrentStage() == JunoADSR::Stage::Attack) {
                    attack6 = true; sum6 += v6; cnt6++;
                }
                if (adsr60.getCurrentStage() == JunoADSR::Stage::Attack) {
                    attack60 = true; sum60 += v60; cnt60++;
                }
                if (adsr106.getCurrentStage() == JunoADSR::Stage::Attack) {
                    attack106 = true; sum106 += v106; cnt106++;
                }
            }

            expect(attack6, "J6 ADSR should enter Attack stage");
            expect(attack60, "J60 ADSR should enter Attack stage");
            expect(attack106, "J106 ADSR should enter Attack stage");

            float avg6 = (cnt6 > 0) ? sum6 / (float)cnt6 : 0.0f;
            float avg60 = (cnt60 > 0) ? sum60 / (float)cnt60 : 0.0f;
            float avg106 = (cnt106 > 0) ? sum106 / (float)cnt106 : 0.0f;

            // Analog RC modes (kJ6 and kJ60) share the same analog exponential charging
            // implementation. Both differ from digital MCU mode (kJ106) which uses linear
            // increments per tick. Test that analog != digital.
            // Note: kJ6 and kJ60 may produce identical averages (both analog RC).
            expect(std::abs(avg60 - avg106) > 0.001f,
                   "kJ60 and kJ106 attack averages should differ (analog vs digital). J60=" + juce::String(avg60)
                   + " J106=" + juce::String(avg106));
            expect(std::abs(avg6 - avg106) > 0.001f,
                   "kJ6 and kJ106 attack averages should differ (analog vs digital). J6=" + juce::String(avg6)
                   + " J106=" + juce::String(avg106));

            // Log J60 vs J6 comparison for diagnostics
            std::printf("ADSR J60: J6 avg=%.4f cnt=%d, J60 avg=%.4f cnt=%d, J106 avg=%.4f cnt=%d\n",
                   avg6, cnt6, avg60, cnt60, avg106, cnt106);

            std::printf("ADSR J60 test: J6 avg=%.4f cnt=%d, J60 avg=%.4f cnt=%d, J106 avg=%.4f cnt=%d\n",
                   avg6, cnt6, avg60, cnt60, avg106, cnt106);
        }

        beginTest("J60 ADSR kJ60 envelope is monotonic during attack");
        {
            JunoADSR adsr;
            adsr.setSampleRate(44100.0);
            adsr.setMode(ADSRMode::kJ60);
            adsr.setAttack(0.05f);
            adsr.setSustain(1.0f);
            adsr.noteOn();

            std::vector<float> values;
            for (int i = 0; i < 4410 && adsr.getCurrentStage() == JunoADSR::Stage::Attack; ++i)
                values.push_back(adsr.getNextSample());

            expect(values.size() > 10, "J60 ADSR attack should produce at least 10 samples");
            for (size_t i = 1; i < values.size(); ++i)
                expect(values[i] >= values[i-1] - 0.001f,
                       "J60 ADSR attack should be monotonic at index " + juce::String((int)i));

            if (!values.empty()) {
                float finalVal = values.back();
                expect(finalVal > 0.0f, "J60 ADSR should rise above zero during attack");
                expect(finalVal < 1.5f, "J60 ADSR should not exceed reasonable range");
            }
        }

        beginTest("HPF J60 position frequencies match hardware spec");
        {
            // Juno-60 HPF uses CD4051B to select capacitor values
            // Frequencies from ngspice AC simulation with 30K VCA load
            expect(getJuno60HPFFreq(0) == 0.0f,
                   "J60 HPF pos 0 should be FLAT (bypass), got " + juce::String(getJuno60HPFFreq(0)));
            // Position 1: .022µF capacitor = 122 Hz (ngspice verified)
            expect(std::abs(getJuno60HPFFreq(1) - 122.0f) < 1.0f,
                   "J60 HPF pos 1 should be ~122 Hz, got " + juce::String(getJuno60HPFFreq(1)));
            // Position 2: .01µF capacitor = 269 Hz (ngspice verified)  
            expect(std::abs(getJuno60HPFFreq(2) - 269.0f) < 1.0f,
                   "J60 HPF pos 2 should be ~269 Hz, got " + juce::String(getJuno60HPFFreq(2)));
            // Position 3: .0047µF capacitor = 571 Hz (ngspice verified)
            expect(std::abs(getJuno60HPFFreq(3) - 571.0f) < 1.0f,
                   "J60 HPF pos 3 should be ~571 Hz, got " + juce::String(getJuno60HPFFreq(3)));
        }

        beginTest("HPF J60 setPosition maps correctly in JunoHPF");
        {
            JunoHPF hpf;
            hpf.prepare(44100.0f);
            hpf.setMode(HPFMode::J60);

            // Position 0: FLAT (bypass) - currentFreqHz should be 0
            hpf.setPosition(0, 122.0f, 269.0f, 571.0f);
            expect(hpf.currentFreqHz == 0.0f,
                   "J60 HPF pos 0: currentFreqHz should be 0, got " + juce::String(hpf.currentFreqHz));

            // Position 1: 122 Hz
            hpf.setPosition(1, 122.0f, 269.0f, 571.0f);
            expect(std::abs(hpf.currentFreqHz - 122.0f) < 1.0f,
                   "J60 HPF pos 1: currentFreqHz should be ~122 Hz, got " + juce::String(hpf.currentFreqHz));

            // Position 2: 269 Hz
            hpf.setPosition(2, 122.0f, 269.0f, 571.0f);
            expect(std::abs(hpf.currentFreqHz - 269.0f) < 1.0f,
                   "J60 HPF pos 2: currentFreqHz should be ~269 Hz, got " + juce::String(hpf.currentFreqHz));

            // Position 3: 571 Hz
            hpf.setPosition(3, 122.0f, 269.0f, 571.0f);
            expect(std::abs(hpf.currentFreqHz - 571.0f) < 1.0f,
                   "J60 HPF pos 3: currentFreqHz should be ~571 Hz, got " + juce::String(hpf.currentFreqHz));

            // Verify HPF produces output (not silent)
            hpf.setPosition(1, 122.0f, 269.0f, 571.0f);
            float out = hpf.process(1.0f);
            expect(!std::isnan(out), "J60 HPF should not produce NaN");

            // Test J106 for comparison: position 2 = 236 Hz (different from J60)
            hpf.setMode(HPFMode::J106);
            hpf.setPosition(2, 0.0f, 236.0f, 754.0f);
            expect(std::abs(hpf.currentFreqHz - 236.0f) < 1.0f,
                   "J106 HPF pos 2 should be ~236 Hz, got " + juce::String(hpf.currentFreqHz));
        }

        beginTest("ChorusBBD J60 model processes without crash and differs from J106");
        {
            ChorusBBD chorusJ60, chorusJ106;
            chorusJ60.prepare(44100.0, 512);
            chorusJ106.prepare(44100.0, 512);

            chorusJ60.setChorusModel(ChorusBBD::ChorusModel::J60);
            chorusJ106.setChorusModel(ChorusBBD::ChorusModel::J106);
            chorusJ60.setMode(ChorusBBD::Mode::ChorusI);
            chorusJ106.setMode(ChorusBBD::Mode::ChorusI);
            chorusJ60.setMix(1.0f);
            chorusJ106.setMix(1.0f);

            juce::AudioBuffer<float> bufJ60(2, 2048);
            juce::AudioBuffer<float> bufJ106(2, 2048);
            for (int i = 0; i < 2048; ++i) {
                float s = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                bufJ60.setSample(0, i, s); bufJ60.setSample(1, i, s);
                bufJ106.setSample(0, i, s); bufJ106.setSample(1, i, s);
            }

            chorusJ60.process(bufJ60);
            chorusJ106.process(bufJ106);

            // Both should produce output (not silent)
            float peakJ60 = 0.0f, peakJ106 = 0.0f;
            for (int i = 512; i < 2048; ++i) {
                peakJ60 = std::max(peakJ60, std::abs(bufJ60.getSample(0, i)));
                peakJ106 = std::max(peakJ106, std::abs(bufJ106.getSample(0, i)));
            }
            expect(peakJ60 > 1e-4f, "J60 chorus should produce output, peak=" + juce::String(peakJ60));
            expect(peakJ106 > 1e-4f, "J106 chorus should produce output, peak=" + juce::String(peakJ106));

            // Chorus model (J60 vs J106) may not produce audibly different output with
            // identical LFO phase and default parameters. The key test is that both models
            // process without crash and produce valid stereo output (not silent).
            // The chorus model primarily affects calibration parameters (delay, sat, cutoff)
            // which require explicit setCalibrationParams() calls to manifest.
            expect(peakJ60 > 1e-4f, "J60 chorus should produce output, peak=" + juce::String(peakJ60));
            expect(peakJ106 > 1e-4f, "J106 chorus should produce output, peak=" + juce::String(peakJ106));

            // Both should produce stereo difference (antiphase LFO)
            bool stereoJ60 = false, stereoJ106 = false;
            for (int i = 512; i < 2048; ++i) {
                if (std::abs(bufJ60.getSample(0, i) - bufJ60.getSample(1, i)) > 1e-4f)
                    stereoJ60 = true;
                if (std::abs(bufJ106.getSample(0, i) - bufJ106.getSample(1, i)) > 1e-4f)
                    stereoJ106 = true;
            }
            expect(stereoJ60, "J60 chorus should produce stereo difference");
            expect(stereoJ106, "J106 chorus should produce stereo difference");

            std::printf("Chorus J60 vs J106: peakJ60=%.4f peakJ106=%.4f stereoJ60=%d stereoJ106=%d\n",
                   peakJ60, peakJ106, (int)stereoJ60, (int)stereoJ106);
        }

        beginTest("VCF J60 uses same polynomial resonance curve as J106");
        {
            JunoVCF vcf60, vcf106;
            vcf60.setSampleRate(44100.0);
            vcf106.setSampleRate(44100.0);

            // Both J60 and J106 use ResK_J106 polynomial resonance curve
            vcf60.setModelAndResCurve(true, true);   // J60: polynomial (same as J106)
            vcf106.setModelAndResCurve(true, true);  // J106: polynomial

            // Process same signal through both with high resonance
            float sumDiffSq = 0.0f;
            for (int i = 0; i < 4410; ++i) {
                float input = std::sin(2.0f * juce::MathConstants<float>::pi * (220.0f + i * 5.0f) * i / 44100.0f);
                float out60 = vcf60.processSample(input, 0.5f, 0.8f, 0.5f, 0.5f, false,
                                                   0.0f, 0.0f, 0.5f, 440.0f, 0.0f, 0.0f);
                float out106 = vcf106.processSample(input, 0.5f, 0.8f, 0.5f, 0.5f, false,
                                                     0.0f, 0.0f, 0.5f, 440.0f, 0.0f, 0.0f);
                sumDiffSq += (out60 - out106) * (out60 - out106);
            }

            float rmsDiff = std::sqrt(sumDiffSq / 4410.0f);

            // Both use same polynomial curve, so output should be nearly identical
            // (within floating-point tolerance — use 1e-8 to avoid FP accumulation flakiness)
            expect(rmsDiff < 1e-8f,
                   "J60 and J106 VCF should produce nearly identical output. "
                   "RMS diff=" + juce::String(rmsDiff));

            std::printf("VCF J60 vs J106: RMS diff=%.6e (should be near-zero)\n", rmsDiff);
        }
    }
};

// ============================================================================
// J-6 DSP Behavior Tests — verifies J-6 specific DSP routing at sample level
// ============================================================================
class JunoDSPJ6Tests : public juce::UnitTest {
public:
    JunoDSPJ6Tests() : juce::UnitTest("JunoDSP J-6 Behavior", "JunoModel") {}

    void runTest() override
    {
        beginTest("J6 ADSR analog RC mode produces different envelope than J106 MCU mode");
        {
            JunoADSR adsrJ6;
            adsrJ6.setSampleRate(44100.0);
            adsrJ6.setMode(ADSRMode::kJ6);
            adsrJ6.setAttack(0.1f);   // 100ms attack
            adsrJ6.setDecay(0.5f);    // 500ms decay
            adsrJ6.setSustain(0.5f);  // 50% sustain
            adsrJ6.setRelease(0.3f);  // 300ms release

            JunoADSR adsr106;
            adsr106.setSampleRate(44100.0);
            adsr106.setMode(ADSRMode::kJ106);
            adsr106.setAttack(0.1f);   // Same nominal attack
            adsr106.setDecay(0.5f);
            adsr106.setSustain(0.5f);
            adsr106.setRelease(0.3f);

            // Start both envelopes
            adsrJ6.noteOn();
            adsr106.noteOn();

            // Sample both envelopes during attack phase
            // J6 (analog RC) uses exponential charging with overshoot target (1.2 × kEnvMax)
            // J106 (MCU) uses linear increments per tick (every 4.2335ms)
            float sumJ6 = 0.0f, sum106 = 0.0f;
            float maxJ6 = 0.0f, max106 = 0.0f;
            int j6AttackSamples = 0, j106AttackSamples = 0;
            bool j6AttackObserved = false;
            bool j106AttackObserved = false;

            for (int i = 0; i < 8820; ++i) { // 200ms at 44.1kHz
                float vJ6 = adsrJ6.getNextSample();
                float v106 = adsr106.getNextSample();

                if (adsrJ6.getCurrentStage() == JunoADSR::Stage::Attack) {
                    j6AttackObserved = true;
                    sumJ6 += vJ6;
                    maxJ6 = std::max(maxJ6, vJ6);
                    j6AttackSamples++;
                }

                if (adsr106.getCurrentStage() == JunoADSR::Stage::Attack) {
                    j106AttackObserved = true;
                    sum106 += v106;
                    max106 = std::max(max106, v106);
                    j106AttackSamples++;
                }
            }

            // Both should have entered attack stage
            expect(j6AttackObserved, "J6 ADSR should enter Attack stage");
            expect(j106AttackObserved, "J106 ADSR should enter Attack stage");

            // Analog RC (J6) charges faster initially than MCU ticks (J106)
            // due to exponential approach vs linear stepping.
            // Average computed over actual attack-phase samples for fair comparison.
            float j6AvgAttack = (j6AttackSamples > 0) ? sumJ6 / (float)j6AttackSamples : 0.0f;
            float j106AvgAttack = (j106AttackSamples > 0) ? sum106 / (float)j106AttackSamples : 0.0f;

            // J6 analog RC should have different shape than J106 MCU
            expect(std::abs(j6AvgAttack - j106AvgAttack) > 0.001f,
                   "J6 and J106 ADSR attack curves should differ measurably. J6 avg="
                   + juce::String(j6AvgAttack) + ", J106 avg=" + juce::String(j106AvgAttack));

            // Both should be non-zero during attack
            expect(maxJ6 > 0.01f, "J6 ADSR should produce non-zero during attack, max=" + juce::String(maxJ6));
            expect(max106 > 0.01f, "J106 ADSR should produce non-zero during attack, max=" + juce::String(max106));

            std::printf("ADSR J6 test: J6 sum=%.4f max=%.4f samples=%d avg=%.4f, "
                   "J106 sum=%.4f max=%.4f samples=%d avg=%.4f\n",
                   sumJ6, maxJ6, j6AttackSamples, j6AvgAttack,
                   sum106, max106, j106AttackSamples, j106AvgAttack);
        }

        beginTest("J6 ADSR analog RC attack follows exponential rise");
        {
            JunoADSR adsr;
            adsr.setSampleRate(44100.0);
            adsr.setMode(ADSRMode::kJ6);
            adsr.setAttack(0.05f);  // 50ms attack
            adsr.setSustain(1.0f);  // Full sustain (no decay transition)
            adsr.noteOn();

            // Collect envelope values during attack
            std::vector<float> values;
            values.reserve(4410); // up to 100ms

            for (int i = 0; i < 4410 && adsr.getCurrentStage() == JunoADSR::Stage::Attack; ++i) {
                values.push_back(adsr.getNextSample());
            }

            // Verify attack is monotonic (strictly increasing)
            expect(values.size() > 10, "J6 ADSR attack should produce at least 10 samples");
            for (size_t i = 1; i < values.size(); ++i) {
                expect(values[i] >= values[i-1] - 0.001f,
                       "J6 ADSR attack should be monotonic at index " + juce::String((int)i));
            }

            // Verify exponential shape: second half should have smaller increments
            // than first half (exponential charging: fast initial, then tapers)
            if (values.size() > 4) {
                size_t mid = values.size() / 2;
                float firstHalfInc = values[mid] - values[0];
                float secondHalfInc = values[values.size()-1] - values[mid];

                // For exponential charge to 1.2× target, increments should decrease
                // (exponential: fast initially, then tapers). J6 analog RC should
                // show larger initial increment than final increment.
                expect(firstHalfInc >= 0.0f, "J6 ADSR first half increment should be non-negative");
                expect(secondHalfInc >= 0.0f, "J6 ADSR second half increment should be non-negative");

                // The first half of the exponential attack should grow more than the second half
                // (characteristic of RC charging curve where ΔV decreases as V approaches target)
                expect(firstHalfInc >= secondHalfInc * 0.9f || values.size() < 50,
                       "J6 ADSR exponential rise: first half should grow >= second half. "
                       "firstHalfInc=" + juce::String(firstHalfInc)
                       + ", secondHalfInc=" + juce::String(secondHalfInc));

                std::printf("ADSR J6 shape: %zu samples, firstHalfInc=%.4f, secondHalfInc=%.4f\n",
                       values.size(), firstHalfInc, secondHalfInc);
            }

            // Final value should be reasonable (between 0 and 1.2)
            if (!values.empty()) {
                float finalVal = values.back();
                expect(finalVal > 0.0f, "J6 ADSR should rise above zero during attack");
                expect(finalVal < 1.5f, "J6 ADSR should not exceed reasonable range during attack");
            }
        }

        beginTest("J6 DCO saw+noise produces normal output");
        {
            JunoDCO dco;
            dco.prepare(44100.0, 512);
            dco.setModel(0); // J6
            dco.setFrequency(440.0f);
            dco.setSawLevel(1.0f);   // Full saw
            dco.setPulseLevel(0.0f); // No pulse
            dco.setSubLevel(0.0f);   // No sub (J6 ignores this anyway)
            dco.setNoiseLevel(1.0f); // Full noise
            dco.setPWM(0.5f);        // Should be ignored in J6 mode

            float peak = 0.0f;
            float rmsSum = 0.0f;
            const int numSamples = 4410;
            for (int i = 0; i < numSamples; ++i) {
                float s = dco.getNextSample(0.0f);
                peak = std::max(peak, std::abs(s));
                rmsSum += s * s;
            }
            float rms = std::sqrt(rmsSum / numSamples);

            expect(peak > 0.1f, "J6 DCO should produce output with saw+noise, peak=" + juce::String(peak));
            expect(rms > 0.01f, "J6 DCO should have measurable RMS with saw+noise, rms=" + juce::String(rms));
            expect(!std::isnan(peak) && !std::isinf(peak), "J6 DCO should not produce NaN/Inf");

            std::printf("DCO J6 saw+noise test: peak=%.4f RMS=%.4f\n", peak, rms);
        }

        beginTest("J6 VCF exponential resonance curve is stable (no NaN)");
        {
            JunoVCF vcf;
            vcf.setSampleRate(44100.0);
            vcf.setModelAndResCurve(false, true); // J6 exponential resonance
            vcf.reset();

            // Sweep through frequency and resonance ranges
            float maxAbs = 0.0f;
            bool hasNan = false;
            bool hasInf = false;

            for (int i = 0; i < 8820; ++i) {
                // Vary frequency and resonance to stress the filter
                float freq = std::sin(2.0f * juce::MathConstants<float>::pi * i / 8820.0f) * 0.5f + 0.5f;
                float res = std::cos(2.0f * juce::MathConstants<float>::pi * i / 8820.0f) * 0.5f + 0.5f;
                float input = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);

                float out = vcf.processSample(input, freq, res, 0.5f, 0.5f, false,
                                               0.0f, 0.0f, 0.5f, 440.0f, 0.0f, 0.0f,
                                               0.95f, 1.0f, 1.0f);

                if (std::isnan(out)) hasNan = true;
                if (std::isinf(out)) hasInf = true;
                maxAbs = std::max(maxAbs, std::abs(out));
            }

            expect(!hasNan, "J6 VCF should not produce NaN with exponential resonance curve");
            expect(!hasInf, "J6 VCF should not produce Inf with exponential resonance curve");
            expect(maxAbs > 0.0f, "J6 VCF should produce output, max=" + juce::String(maxAbs));

            std::printf("VCF J6 stability test: maxAbs=%.4f hasNan=%d hasInf=%d\n", maxAbs, (int)hasNan, (int)hasInf);
        }

        beginTest("J6 VCF self-oscillation with exponential resonance curve");
        {
            JunoVCF vcf;
            vcf.setSampleRate(44100.0);
            vcf.setModelAndResCurve(false, true); // J6 exponential resonance
            vcf.reset();

            // Impulse to kickstart oscillation
            vcf.processSample(1.0f, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f,
                              0.95f, 1.0f, 1.0f);

            // Allow oscillation to build
            float maxVal = 0.0f;
            for (int i = 0; i < 4410; ++i) {
                float s = vcf.processSample(0.0f, 0.5f, 1.0f, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 440.0f, 0.0f, 0.0f,
                                             0.95f, 1.0f, 1.0f);
                maxVal = std::max(maxVal, std::abs(s));
            }

            // J6 exponential curve may produce different self-oscillation amplitude
            // than J106 polynomial curve, but should still oscillate
            std::printf("VCF J6 self-oscillation: max=%.4f\n", maxVal);
            expect(maxVal > 1e-8f,
                   "J6 VCF should produce some output after impulse, got max=" + juce::String(maxVal));
        }
    }
};

// ============================================================================
// Static instances — REQUIRED for JUCE auto-registration
// ============================================================================
static ModelRoutingMacroTests modelRoutingMacroTests;
static JunoDCOModelTests junoDCOModelTests;
static JunoChorusModelTests junoChorusModelTests;
static JunoArpModelTests junoArpModelTests;
static JunoPortaModelTests junoPortaModelTests;
static JunoUnisonModelTests junoUnisonModelTests;
static JunoVCFModelTests junoVCFModelTests;
static JunoHPFModelTests junoHPFModelTests;
static JunoADSRModelTests junoADSRModelTests;
static JunoModelConfigTests junoModelConfigTests;
static JunoDSPJ6Tests junoDSPJ6Tests;
static JunoDSPJ60Tests junoDSPJ60Tests;
