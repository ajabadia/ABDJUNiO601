#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "JunoTapeDecoder.h"
#include "JunoTapeEncoder.h"
#include "Importers/JunoFormatConverter.h"
#include "JunoConstants.h"

class JunoFormatConverterTests : public juce::UnitTest {
public:
    JunoFormatConverterTests() : juce::UnitTest("JunoFormatConverter Tests", "JunoIO") {}

    void runTest() override {
        beginTest("Juno-60 DCB: model routing, chorus, sub osc, and pwm defaults");

        // Build a Juno-60 DCB patch with:
        // - SW1 bit 5 = 0 (Sub Osc OFF), bit 6 = 0 (PWM OFF)
        // - Sub level (byte 15) = 0x7F, PWM depth (byte 3) = 0x7F
        // These should be forced to 0 by the switch states.
        uint8_t juno60Patch[18] = {0};
        juno60Patch[15] = 0x7F;  // Sub level at max
        juno60Patch[3]  = 0x7F;  // PWM depth at max
        // SW1: bit1=1 (8' range), bit3=1 (Saw ON), bit4=0 (Pulse OFF)
        //      bit5=0 (Sub Osc OFF), bit6=0 (PWM OFF), bit7=0 (PWM LFO)
        juno60Patch[16] = 0x0A; // 0b00001010
        // SW2: bit0=0 (VCA ENV), bit1=0 (VCF POS), bits3-4=11 (HPF pos 0/FLAT)
        juno60Patch[17] = 0x18; // 0b00011000

        auto vt60 = ABD::JunoFormatConverter::juno60BytesToValueTree(juno60Patch);

        // 1. Model routing must be 1 (Juno-60) for all modules
        expect((int)vt60.getProperty("modelHPF", -1) == 1,
               "Juno-60 DCB: modelHPF should be 1");
        expect((int)vt60.getProperty("modelDCO", -1) == 1,
               "Juno-60 DCB: modelDCO should be 1");
        expect((int)vt60.getProperty("modelVCF", -1) == 1,
               "Juno-60 DCB: modelVCF should be 1");
        expect((int)vt60.getProperty("modelADSR", -1) == 1,
               "Juno-60 DCB: modelADSR should be 1");
        expect((int)vt60.getProperty("modelChorus", -1) == 1,
               "Juno-60 DCB: modelChorus should be 1");

        // 2. No chorus
        expect(!(bool)vt60.getProperty("chorus1", true),
               "Juno-60 DCB: chorus1 should be false");
        expect(!(bool)vt60.getProperty("chorus2", true),
               "Juno-60 DCB: chorus2 should be false");

        // 3. Sub Osc OFF forces level to 0 even though byte 15 is 0x7F
        expect((float)vt60.getProperty("subOsc", 1.0f) == 0.0f,
               "Juno-60 DCB: Sub Osc OFF should force subOsc=0");

        // 4. PWM OFF forces depth to 0 even though byte 3 is 0x7F
        expect((float)vt60.getProperty("pwm", 1.0f) == 0.0f,
               "Juno-60 DCB: PWM OFF should force pwm=0");

        // 5. Basic slider decoding still works
        expect((int)vt60.getProperty("dcoRange", -1) == 1,
               "Juno-60 DCB: dcoRange should be 1 (8')");
        expect((bool)vt60.getProperty("sawOn", false) == true,
               "Juno-60 DCB: sawOn should be true");
        expect((bool)vt60.getProperty("pulseOn", true) == false,
               "Juno-60 DCB: pulseOn should be false");
        expect((int)vt60.getProperty("vcaMode", -1) == 0,
               "Juno-60 DCB: vcaMode should be 0 (ENV)");
        expect((int)vt60.getProperty("vcfPolarity", -1) == 0,
               "Juno-60 DCB: vcfPolarity should be 0 (POS)");
        expect((int)vt60.getProperty("hpfFreq", -1) == 0,
               "Juno-60 DCB: hpfFreq should be 0 (FLAT)");

        beginTest("Juno-60 DCB: Sub Osc ON preserves level");

        // Now test with Sub Osc ON (SW1 bit 5 = 1)
        uint8_t juno60Patch2[18] = {0};
        juno60Patch2[15] = 0x40;  // Sub level at ~50%
        juno60Patch2[16] = 0x0A | 0x20; // 0x2A: same as before + bit5=1 (Sub Osc ON)
        juno60Patch2[17] = 0x18;

        auto vt60b = ABD::JunoFormatConverter::juno60BytesToValueTree(juno60Patch2);
        float subLevel = (float)vt60b.getProperty("subOsc", 0.0f);
        expect(subLevel > 0.0f,
               "Juno-60 DCB: Sub Osc ON should preserve level > 0");
        expect(std::abs(subLevel - (64.0f / 127.0f)) < 0.01f,
               "Juno-60 DCB: Sub Osc level should be ~0.504");

        beginTest("Juno-60 DCB: PWM ON preserves depth");

        // Now test with PWM ON (SW1 bit 6 = 1), PWM LFO (bit 7 = 0)
        uint8_t juno60Patch3[18] = {0};
        juno60Patch3[3]  = 0x7F;  // PWM depth at max
        juno60Patch3[16] = 0x0A | 0x40; // 0x4A: same as before + bit6=1 (PWM ON)
        juno60Patch3[17] = 0x18;

        auto vt60c = ABD::JunoFormatConverter::juno60BytesToValueTree(juno60Patch3);
        float pwmLevel = (float)vt60c.getProperty("pwm", 0.0f);
        expect(pwmLevel > 0.0f,
               "Juno-60 DCB: PWM ON should preserve depth > 0");
        expect((int)vt60c.getProperty("pwmMode", -1) == 0,
               "Juno-60 DCB: pwmMode should be 0 (LFO)");

        beginTest("Juno-60 DCB: PWM Manual mode");

        // PWM ON + PWM Manual mode (bit 7 = 1)
        uint8_t juno60Patch4[18] = {0};
        juno60Patch4[3]  = 0x40;
        juno60Patch4[16] = 0x0A | 0x40 | 0x80; // 0xCA: PWM ON + PWM Manual
        juno60Patch4[17] = 0x18;

        auto vt60d = ABD::JunoFormatConverter::juno60BytesToValueTree(juno60Patch4);
        expect((int)vt60d.getProperty("pwmMode", -1) == 1,
               "Juno-60 DCB: pwmMode should be 1 (Manual)");

        beginTest("Juno-60 DCB: VCA Gate mode and VCF NEG polarity");

        uint8_t juno60Patch5[18] = {0};
        // SW2: bit0=1 (VCA GATE), bit1=1 (VCF NEG)
        juno60Patch5[17] = 0x18 | 0x01 | 0x02; // 0x1B: VCA Gate + VCF Neg + HPF FLAT

        auto vt60e = ABD::JunoFormatConverter::juno60BytesToValueTree(juno60Patch5);
        expect((int)vt60e.getProperty("vcaMode", -1) == 1,
               "Juno-60 DCB: vcaMode should be 1 (GATE)");
        expect((int)vt60e.getProperty("vcfPolarity", -1) == 1,
               "Juno-60 DCB: vcfPolarity should be 1 (NEG)");

        beginTest("Juno-60 DCB: HPF position encoding");

        // Test all 4 HPF positions (SW2 bits 3-4)
        for (int pos = 0; pos < 4; ++pos) {
            uint8_t patch[18] = {0};
            // SW2 bits 3-4: 11→pos0, 10→pos1, 01→pos2, 00→pos3
            int hwBits = 3 - pos;
            patch[17] = (uint8_t)((hwBits & 0x03) << 3);

            auto vt = ABD::JunoFormatConverter::juno60BytesToValueTree(patch);
            int hpfOut = (int)vt.getProperty("hpfFreq", -1);
            expect(hpfOut == pos,
                   "Juno-60 DCB: HPF position " + juce::String(pos)
                   + " should decode to hpfFreq=" + juce::String(pos)
                   + " but got " + juce::String(hpfOut));
        }

        beginTest("quickDetectFormat(): Juno-60 Bank A returns 340");

        juce::File srcFile(__FILE__);
        juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
        juce::File docsDir = projectRoot.getChildFile("docs");
        juce::File bank60A = docsDir.getChildFile("Juno-60 (1)").getChildFile("JUNO-60 Bank A.wav");

        if (bank60A.existsAsFile()) {
            int detected = JunoTapeDecoder::quickDetectFormat(bank60A);
            std::printf("quickDetectFormat(JUNO-60 Bank A) = %d (expected 340)\n", detected);
            expect(detected == 340,
                   "quickDetectFormat should return 340 for Juno-60 Bank A, got " + juce::String(detected));
        } else {
            std::printf("WARNING: JUNO-60 Bank A not found at: %s\n", bank60A.getFullPathName().toRawUTF8());
        }

        beginTest("quickDetectFormat(): JUNO106 Bank A returns 1200");

        juce::File bank106A = docsDir.getChildFile("JUNO-106").getChildFile("JUNO106 Bank A.wav");
        if (bank106A.existsAsFile()) {
            int detected = JunoTapeDecoder::quickDetectFormat(bank106A);
            std::printf("quickDetectFormat(JUNO106 Bank A) = %d (expected 1200)\n", detected);
            expect(detected == 1200,
                   "quickDetectFormat should return 1200 for JUNO106 Bank A, got " + juce::String(detected));
        } else {
            std::printf("WARNING: JUNO106 Bank A not found at: %s\n", bank106A.getFullPathName().toRawUTF8());
        }

        beginTest("Juno-60 DCB: encoder produces valid FSK audio at 340 baud");

        {
            // Verify that the encoder produces audio with the expected characteristics
            uint8_t patch[18] = {0};
            patch[0] = 0x40;
            patch[16] = 0x0A; // SW1: 8', Saw ON
            patch[17] = 0x18; // SW2: ENV, POS, HPF flat

            std::vector<std::vector<uint8_t>> bank = {
                std::vector<uint8_t>(patch, patch + 18)
            };

            auto buf60 = JunoTapeEncoder::encodePatches(bank, 44100.0, JunoTapeEncoder::Juno60);
            expect(buf60.getNumSamples() > 1000,
                   "J60 encoder should produce > 1000 samples, got " + juce::String(buf60.getNumSamples()));

            auto buf106 = JunoTapeEncoder::encodePatches(bank, 44100.0, JunoTapeEncoder::Juno106);
            expect(buf106.getNumSamples() > 1000,
                   "J106 encoder should produce > 1000 samples");

            // Juno-60 at 340 baud → longer audio than Juno-106 at 1200 baud
            expect(buf60.getNumSamples() > buf106.getNumSamples(),
                   "Juno-60 tape should be longer than Juno-106 tape: "
                   + juce::String(buf60.getNumSamples()) + " vs " + juce::String(buf106.getNumSamples()));

            std::printf("J60 round-trip: Juno60 audio=%d samples, Juno106 audio=%d samples\n",
                       buf60.getNumSamples(), buf106.getNumSamples());
        }

        beginTest("Juno-60 DCB: decode real Bank A tape → convert (validates DCB pipeline)");

        {
            // Test the full Juno-60 DCB pipeline using the REAL JUNO-60 Bank A recording:
            // 1. The existing JunoTapeTests already verifies that decodeWavFile succeeds
            // 2. We add the conversion step via juno60BytesToValueTree
            // 3. We verify the Juno-60-specific parameter outputs
            // This tests the complete pipeline: tape → decoder → DCB converter
            juce::File srcFile(__FILE__);
            juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
            juce::File docsDir = projectRoot.getChildFile("docs");
            juce::File bank60A = docsDir.getChildFile("Juno-60 (1)").getChildFile("JUNO-60 Bank A.wav");

            if (bank60A.existsAsFile()) {
                // Decode with forced 340 baud
                auto res = JunoTapeDecoder::decodeWavFile(bank60A, 340);
                expect(res.success,
                       "Bank A decode should succeed: " + res.errorMessage);
                expect(res.data.size() >= 18,
                       "Bank A should decode >= 1 patch, got " + juce::String((int)(res.data.size() / 18)) + " patches");

                if (res.success && res.data.size() >= 18) {
                    std::printf("Bank A round-trip: decoded %zu patches (using %s baud)\n",
                               res.data.size() / 18,
                               (res.detectedBaudRate == 340) ? "340" : "1200");

                    // Convert first patch via Juno-60 DCB converter
                    auto vt = ABD::JunoFormatConverter::juno60BytesToValueTree(res.data.data());

                    // Verify Juno-60-specific properties
                    expect(!(bool)vt.getProperty("chorus1", true),
                           "J60 round-trip: chorus1 should be false");
                    expect(!(bool)vt.getProperty("chorus2", true),
                           "J60 round-trip: chorus2 should be false");
                    expect((int)vt.getProperty("modelHPF", -1) == 1,
                           "J60 round-trip: modelHPF should be 1");
                    expect((int)vt.getProperty("modelDCO", -1) == 1,
                           "J60 round-trip: modelDCO should be 1");
                    expect((int)vt.getProperty("modelVCF", -1) == 1,
                           "J60 round-trip: modelVCF should be 1");
                    expect((int)vt.getProperty("modelChorus", -1) == 1,
                           "J60 round-trip: modelChorus should be 1");

                    // Also verify the first patch decodes to sensible Juno-60 values
                    // (Specific values depend on the actual tape content)
                    std::printf("Bank A round-trip: patch 0 params: dcoRange=%d sawOn=%d pulseOn=%d "
                               "vcaMode=%d vcfPol=%d hpfFreq=%d chorus1=%d chorus2=%d "
                               "modelDCO=%d modelChorus=%d\n",
                               (int)vt.getProperty("dcoRange", -1),
                               (bool)vt.getProperty("sawOn", false) ? 1 : 0,
                               (bool)vt.getProperty("pulseOn", false) ? 1 : 0,
                               (int)vt.getProperty("vcaMode", -1),
                               (int)vt.getProperty("vcfPolarity", -1),
                               (int)vt.getProperty("hpfFreq", -1),
                               (bool)vt.getProperty("chorus1", true) ? 1 : 0,
                               (bool)vt.getProperty("chorus2", true) ? 1 : 0,
                               (int)vt.getProperty("modelDCO", -1),
                               (int)vt.getProperty("modelChorus", -1));

                    std::printf("Bank A round-trip: ALL CHECKS PASSED (real tape → DCB converter)\n");
                }
            } else {
                std::printf("WARNING: JUNO-60 Bank A not found at: %s\n", bank60A.getFullPathName().toRawUTF8());
            }
        }

        beginTest("Juno-60 DCB: synthetic round-trip at 340 baud (encode → decodeFSK → convert → verify)");

        {
            // Full synthetic round-trip test at 340 baud:
            // 1. Encode a patch with JunoTapeEncoder::Juno60
            // 2. Decode the audio buffer with JunoTapeDecoder::decodeFSK at 340 baud
            // 3. Validate checksums with validatePatches
            // 4. Convert via juno60BytesToValueTree
            // 5. Verify all parameters match the original

            // Build a patch with distinctive Juno-60 bytes
            // SW1: 0x7A = bit1(8') + bit3(Saw) + bit4(Pulse) + bit5(Sub) + bit6(PWM)
            // SW2 bits 3-4 encode HPF position: 11=pos0(FLAT), 10=pos1, 01=pos2, 00=pos3
            // For hpfFreq=1 (pos1): bits 3-4 = 10 → (2 << 3) = 0x10
            // 0x13 = 0x10 + bit0(GATE=1) + bit1(NEG=1)
            uint8_t patch[18] = {0};
            patch[0]  = 0x40;  // DCO range: 0=4', 1=8', 2=16'
            patch[15] = 0x7F;  // Sub level max
            patch[3]  = 0x7F;  // PWM depth max
            patch[16] = 0x7A;  // SW1: 8'+Saw+Pulse+Sub+PWM
            patch[17] = 0x13;  // SW2: GATE+NEG+HPF bits=10→hpfFreq=1

            // Encode to audio via JunoTapeEncoder::Juno60
            std::vector<std::vector<uint8_t>> bank = {
                std::vector<uint8_t>(patch, patch + 18)
            };
            auto buffer = JunoTapeEncoder::encodePatches(bank, 44100.0, JunoTapeEncoder::Juno60);
            expect(buffer.getNumSamples() > 1000,
                   "J60 encoder should produce > 1000 samples, got " + juce::String(buffer.getNumSamples()));

            // Decode directly from audio buffer at 340 baud
            const float* samples = buffer.getReadPointer(0);
            int numSamples = buffer.getNumSamples();

            auto rawBytes = JunoTapeDecoder::decodeFSK(samples, numSamples, 44100.0, 340.0);
            std::printf("J60 round-trip: decodeFSK returned %zu raw bytes\n", rawBytes.size());

            // Validate patches (checksum verification)
            auto validated = JunoTapeDecoder::validatePatches(rawBytes);
            size_t numPatches = validated.size() / 18;
            std::printf("J60 round-trip: validatePatches returned %zu patches\n", numPatches);

            expect(numPatches >= 1,
                   "J60 round-trip: should decode >= 1 patch, got " + juce::String(numPatches));

            if (numPatches >= 1) {
                // DEBUG: show decoded bytes vs original
                std::printf("J60 round-trip: decoded bytes vs original:\n");
                for (int i = 0; i < 18; ++i) {
                    std::printf("  [%02d] decoded=0x%02X original=0x%02X%s\n",
                               i, validated[i], patch[i],
                               (validated[i] == patch[i]) ? "" : " MISMATCH");
                }

                // Convert via Juno-60 DCB converter
                auto vt = ABD::JunoFormatConverter::juno60BytesToValueTree(validated.data());

                // Show all relevant parameter values
                std::printf("J60 round-trip: params: dcoRange=%d sawOn=%d pulseOn=%d "
                           "vcaMode=%d vcfPol=%d hpfFreq=%d "
                           "chorus1=%d chorus2=%d "
                           "modelHPF=%d modelDCO=%d modelVCF=%d modelChorus=%d "
                           "subOsc=%.3f pwm=%.3f\n",
                           (int)vt.getProperty("dcoRange", -1),
                           (bool)vt.getProperty("sawOn", false) ? 1 : 0,
                           (bool)vt.getProperty("pulseOn", false) ? 1 : 0,
                           (int)vt.getProperty("vcaMode", -1),
                           (int)vt.getProperty("vcfPolarity", -1),
                           (int)vt.getProperty("hpfFreq", -1),
                           (bool)vt.getProperty("chorus1", true) ? 1 : 0,
                           (bool)vt.getProperty("chorus2", true) ? 1 : 0,
                           (int)vt.getProperty("modelHPF", -1),
                           (int)vt.getProperty("modelDCO", -1),
                           (int)vt.getProperty("modelVCF", -1),
                           (int)vt.getProperty("modelChorus", -1),
                           (float)vt.getProperty("subOsc", 0.0f),
                           (float)vt.getProperty("pwm", 0.0f));

                // Verify Juno-60-specific properties
                expect(!(bool)vt.getProperty("chorus1", true),
                       "J60 round-trip: chorus1 should be false");
                expect(!(bool)vt.getProperty("chorus2", true),
                       "J60 round-trip: chorus2 should be false");
                expect((int)vt.getProperty("modelHPF", -1) == 1,
                       "J60 round-trip: modelHPF should be 1");
                expect((int)vt.getProperty("modelDCO", -1) == 1,
                       "J60 round-trip: modelDCO should be 1");
                expect((int)vt.getProperty("modelVCF", -1) == 1,
                       "J60 round-trip: modelVCF should be 1");
                expect((int)vt.getProperty("modelChorus", -1) == 1,
                       "J60 round-trip: modelChorus should be 1");

                // Verify switch decode: SW1=0x7A → dcoRange=1 (8'), sawOn=1, pulseOn=1, sub enabled, pwm enabled
                // SW2=0x13 → vcaMode=1 (GATE), vcfPolarity=1 (NEG), hpfFreq=1 (pos1)
                expect((int)vt.getProperty("dcoRange", -1) == 1,
                       "J60 round-trip: dcoRange should be 1 (8')");
                expect((bool)vt.getProperty("sawOn", false) == true,
                       "J60 round-trip: sawOn should be true");
                expect((bool)vt.getProperty("pulseOn", false) == true,
                       "J60 round-trip: pulseOn should be true");
                expect((int)vt.getProperty("vcaMode", -1) == 1,
                       "J60 round-trip: vcaMode should be 1 (GATE)");
                expect((int)vt.getProperty("vcfPolarity", -1) == 1,
                       "J60 round-trip: vcfPolarity should be 1 (NEG)");
                expect((int)vt.getProperty("hpfFreq", -1) == 1,
                       "J60 round-trip: hpfFreq should be 1 (pos1)");

                // Sub Osc ON and PWM ON should preserve their levels
                float subLevel = (float)vt.getProperty("subOsc", 0.0f);
                expect(subLevel > 0.0f,
                       "J60 round-trip: Sub Osc ON should preserve level > 0, got " + juce::String(subLevel));

                float pwmLevel = (float)vt.getProperty("pwm", 0.0f);
                expect(pwmLevel > 0.0f,
                       "J60 round-trip: PWM ON should preserve depth > 0, got " + juce::String(pwmLevel));

                std::printf("J60 round-trip: ALL CHECKS PASSED (encode → decodeFSK → convert → verify)\n");
            }
        }
    }
};

class JunoDcbCorrectorTests : public juce::UnitTest {
public:
    JunoDcbCorrectorTests() : juce::UnitTest("JunoDCB Corrector Tests", "JunoIO") {}

    void runTest() override {
        beginTest("SW2 reserved bits cleared for Juno-60");
        {
            // Build a patch with ALL SW2 bits set to 1
            // Juno-60: bits 2,5,6,7 are reserved → should be cleared to 0
            // Valid bits 0,1,3,4 should be preserved
            uint8_t patch[18] = {0};
            patch[17] = 0xFF; // SW2 = all bits 1
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 340);
            
            expect(corrected.size() == 18, "Should return 18 bytes");
            uint8_t sw2 = corrected[17];
            expect((sw2 & 0xE4) == 0,
                   "Juno-60 SW2 reserved bits 2,5,6,7 should be 0, got 0x" + juce::String::toHexString((int)sw2));
            // Valid bits (0,1,3,4) should be preserved
            expect((sw2 & 0x1B) == 0x1B,
                   "Juno-60 SW2 valid bits 0,1,3,4 should be preserved, got 0x" + juce::String::toHexString((int)sw2));
        }

        beginTest("SW2 reserved bits cleared for Juno-106");
        {
            // Juno-106: bits 5,6,7 are reserved → should be cleared to 0
            // Valid bits 0-4 should be preserved
            uint8_t patch[18] = {0};
            patch[17] = 0xFF; // SW2 = all bits 1
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 1200);
            
            expect(corrected.size() == 18, "Should return 18 bytes");
            uint8_t sw2 = corrected[17];
            expect((sw2 & 0xE0) == 0,
                   "Juno-106 SW2 reserved bits 5,6,7 should be 0, got 0x" + juce::String::toHexString((int)sw2));
            // Valid bits (0-4) should be preserved
            expect((sw2 & 0x1F) == 0x1F,
                   "Juno-106 SW2 valid bits 0-4 should be preserved, got 0x" + juce::String::toHexString((int)sw2));
        }

        beginTest("SW1 bit 7 cleared for Juno-106");
        {
            // Use a valid single range (bit 1 = 8') so the range corrector doesn't interfere.
            // SW1 with bit 7 set + valid 8' range = 0x80 | 0x02 = 0x82
            // After correction: bit 7 cleared → 0x02, range (bits 0-2 = 010) is valid → unchanged
            uint8_t patch[18] = {0};
            patch[16] = 0x82; // SW1 = bit 7 + 8' range (valid single range)
            patch[17] = 0x00;
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 1200);
            
            uint8_t sw1 = corrected[16];
            expect((sw1 & 0x80) == 0,
                   "Juno-106 SW1 bit 7 should be 0, got 0x" + juce::String::toHexString((int)sw1));
            // Bits 0-6 should be preserved (8' range = bit 1 = 0x02)
            expect(sw1 == 0x02,
                   "Juno-106 SW1 should be 0x02 (bit 7 cleared, 8' range preserved), got 0x" + juce::String::toHexString((int)sw1));
        }

        beginTest("SW1 range bits fixed when NONE set (Juno-60 → defaults to 8')");
        {
            uint8_t patch[18] = {0};
            patch[16] = 0x00; // SW1 = no range bits (invalid for Juno-60)
            patch[17] = 0x00;
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 340);
            
            uint8_t sw1 = corrected[16];
            expect((sw1 & 0x07) == (1 << 1),
                   "Juno-60 SW1 no range → should default to 8' (bit 1), got 0x" + juce::String::toHexString((int)sw1 & 0x07));
        }

        beginTest("SW1 range bits fixed when MULTIPLE set (Juno-60 → defaults to 8')");
        {
            uint8_t patch[18] = {0};
            patch[16] = 0x07; // SW1 = 16'+8'+4' (all ranges, invalid!)
            patch[17] = 0x00;
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 340);
            
            uint8_t sw1 = corrected[16];
            expect((sw1 & 0x07) == (1 << 1),
                   "Juno-60 SW1 multiple ranges → should default to 8' (bit 1), got 0x" + juce::String::toHexString((int)sw1 & 0x07));
        }

        beginTest("SW1 range bits preserved when VALID (Juno-60)");
        {
            // Test each valid range: 16', 8', 4'
            for (int r = 0; r < 3; ++r) {
                uint8_t patch[18] = {0};
                patch[16] = (uint8_t)(1 << r); // Only ONE range bit set
                patch[17] = 0x00;
                
                std::vector<uint8_t> patches(patch, patch + 18);
                auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 340);
                
                uint8_t sw1 = corrected[16];
                expect((sw1 & 0x07) == (uint8_t)(1 << r),
                       "Juno-60 SW1 valid range should be preserved (bit " + juce::String(r)
                       + "), got 0x" + juce::String::toHexString((int)sw1 & 0x07));
            }
        }

        beginTest("Patches with valid bits pass through unchanged");
        {
            // Build a typical Juno-60 patch with valid switch bytes
            uint8_t patch[18] = {0};
            patch[0]  = 0x40;
            patch[15] = 0x7F;
            patch[16] = 0x0A; // SW1: 8' range (bit 1) + Saw (bit 3)
            patch[17] = 0x18; // SW2: HPF=pos0 (bits 3-4=11)
            
            std::vector<uint8_t> patches(patch, patch + 18);
            auto corrected = JunoTapeDecoder::correctDcbFormat(patches, 340);
            
            expect(corrected.size() == 18, "Should return 18 bytes");
            expect(corrected[16] == 0x0A, "SW1 should be unchanged");
            expect(corrected[17] == 0x18, "SW2 should be unchanged");
            // Slider bytes should never be modified
            expect(corrected[0] == 0x40, "Slider bytes should be unchanged");
            expect(corrected[15] == 0x7F, "Slider bytes should be unchanged");
        }

        beginTest("Empty input returns empty");
        {
            std::vector<uint8_t> empty;
            auto corrected = JunoTapeDecoder::correctDcbFormat(empty, 340);
            expect(corrected.empty(), "Empty input should return empty");
        }

        beginTest("Multiple patches corrected independently");
        {
            uint8_t patches[36] = {0};
            // Patch 0: SW2 all-bits-set (Juno-60)
            patches[17] = 0xFF;
            // Patch 1: SW2 all-bits-set (Juno-60)
            patches[35] = 0xFF;
            
            std::vector<uint8_t> input(patches, patches + 36);
            auto corrected = JunoTapeDecoder::correctDcbFormat(input, 340);
            
            expect(corrected.size() == 36, "Should return 36 bytes for 2 patches");
            expect((corrected[17] & 0xE4) == 0, "Patch 0 SW2 reserved bits cleared");
            expect((corrected[35] & 0xE4) == 0, "Patch 1 SW2 reserved bits cleared");
        }
    }
};
