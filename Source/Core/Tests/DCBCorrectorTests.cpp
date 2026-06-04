#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "JunoTapeDecoder.h"

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
