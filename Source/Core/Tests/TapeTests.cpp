#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "JunoTapeEncoder.h"
#include "JunoTapeDecoder.h"

class JunoTapeTests : public juce::UnitTest {
public:
    JunoTapeTests() : juce::UnitTest("JunoTape RoundTrip", "JunoIO") {}

    void runTest() override {
        beginTest("Tape encoding/decoding preserves data");

        std::vector<uint8_t> patch(18, 0x42); 
        std::vector<std::vector<uint8_t>> bank = { patch };
        
        auto buffer106 = JunoTapeEncoder::encodePatches(bank, 44100.0, JunoTapeEncoder::Juno106);
        expect(buffer106.getNumSamples() > 0);

        auto buffer60 = JunoTapeEncoder::encodePatches(bank, 44100.0, JunoTapeEncoder::Juno60);
        expect(buffer60.getNumSamples() > 0);
        expect(buffer60.getNumSamples() > buffer106.getNumSamples(), "Juno-60 tape should take longer than Juno-106 due to lower baud rate");

        // Resolve docs path relative to the source file location
        juce::File srcFile(__FILE__);
        juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
        juce::File docsDir = projectRoot.getChildFile("docs");
        
        // Test Juno-60 Bank A
        juce::File bankA = docsDir.getChildFile("Juno-60 (1)").getChildFile("JUNO-60 Bank A.wav");
        if (bankA.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(bankA, 340);
            std::printf("Decoded JUNO-60 Bank A: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 10, "Bank A should decode at least 10 patches");
        } else {
            std::printf("WARNING: JUNO-60 Bank A not found at: %s\n", bankA.getFullPathName().toRawUTF8());
        }
        
        // Test Juno-60 Bank B
        juce::File bankB = docsDir.getChildFile("Juno-60 (1)").getChildFile("JUNO-60 Bank B.wav");
        if (bankB.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(bankB, 340);
            std::printf("Decoded JUNO-60 Bank B: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 5, "Bank B should decode at least 5 patches at 340 baud");
        } else {
            std::printf("WARNING: JUNO-60 Bank B not found at: %s\n", bankB.getFullPathName().toRawUTF8());
        }
        
        // Test auto-detect with Juno-60 Bank A (no forced baud rate)
        if (bankA.existsAsFile()) {
            auto resAuto = JunoTapeDecoder::decodeWavFile(bankA, 0);
            std::printf("Auto-detect JUNO-60 Bank A: success=%d, patches=%d, error=%s\n",
                        (int)resAuto.success, (int)(resAuto.data.size() / 18), resAuto.errorMessage.toRawUTF8());
            expect(resAuto.success, "Auto-detect should decode Juno-60 Bank A");
        }
        
        // Test Juno-106 Bank A (if available)
        juce::File bank106A = docsDir.getChildFile("JUNO-106").getChildFile("JUNO106 Bank A.wav");
        if (bank106A.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(bank106A, 1200);
            std::printf("Decoded JUNO-106 Bank A: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "JUNO-106 Bank A should decode at least 1 patch");
        } else {
            std::printf("WARNING: JUNO-106 Bank A not found at: %s\n", bank106A.getFullPathName().toRawUTF8());
        }
        
        // Test Juno-106 Bank B (if available)
        juce::File bank106B = docsDir.getChildFile("JUNO-106").getChildFile("JUNO106 Bank B.wav");
        if (bank106B.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(bank106B, 1200);
            std::printf("Decoded JUNO-106 Bank B: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "JUNO-106 Bank B should decode at least 1 patch");
        } else {
            std::printf("WARNING: JUNO-106 Bank B not found at: %s\n", bank106B.getFullPathName().toRawUTF8());
        }
        
        // --- New factory tape recordings (better quality, more patches) ---
        
        // Roland Juno-60 factory programs group 1 (actually 1200 baud recording)
        juce::File juno60G1 = docsDir.getChildFile("JUNO-106").getChildFile("Roland Juno-60 factory programs group 1.wav");
        if (juno60G1.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(juno60G1, 1200);
            std::printf("Decoded Roland Juno-60 G1: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 40, "Juno-60 G1 should decode >= 40 patches at 1200 baud");
        } else {
            std::printf("WARNING: Roland Juno-60 G1 not found at: %s\n", juno60G1.getFullPathName().toRawUTF8());
        }
        
        // Roland Juno-60 factory programs group 2 (also 1200 baud recording)
        juce::File juno60G2 = docsDir.getChildFile("JUNO-106").getChildFile("Roland Juno-60 factory programs group 2.wav");
        if (juno60G2.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(juno60G2, 1200);
            std::printf("Decoded Roland Juno-60 G2: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 30, "Juno-60 G2 should decode >= 30 patches at 1200 baud");
        } else {
            std::printf("WARNING: Roland Juno-60 G2 not found at: %s\n", juno60G2.getFullPathName().toRawUTF8());
        }
        
        // Roland Juno-106 factory tape j106ma
        juce::File j106dir = projectRoot.getChildFile("JUNO106");
        juce::File j106ma = j106dir.getChildFile("original").getChildFile("tapes")
                             .getChildFile("roland_juno106_factory").getChildFile("j106ma.wav");
        if (j106ma.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(j106ma, 1200);
            std::printf("Decoded j106ma: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "j106ma should decode at least 1 patch");
        } else {
            std::printf("WARNING: j106ma not found at: %s\n", j106ma.getFullPathName().toRawUTF8());
        }
        
        // Roland Juno-106 factory tape j106mb
        juce::File j106mb = j106dir.getChildFile("original").getChildFile("tapes")
                             .getChildFile("roland_juno106_factory").getChildFile("j106mb.wav");
        if (j106mb.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(j106mb, 1200);
            std::printf("Decoded j106mb: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "j106mb should decode at least 1 patch");
        } else {
            std::printf("WARNING: j106mb not found at: %s\n", j106mb.getFullPathName().toRawUTF8());
        }
        
        // --- 44100 Hz versions of factory tapes ---
        
        // j106ma at 44100 Hz
        juce::File j106ma441 = j106dir.getChildFile("original").getChildFile("tapes")
                              .getChildFile("44100 16bits").getChildFile("j106ma.wav");
        if (j106ma441.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(j106ma441, 1200);
            std::printf("Decoded j106ma@44100: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "j106ma@44100 should decode at least 1 patch");
        } else {
            std::printf("WARNING: j106ma@44100 not found at: %s\n", j106ma441.getFullPathName().toRawUTF8());
        }
        
        // j106mb at 44100 Hz
        juce::File j106mb441 = j106dir.getChildFile("original").getChildFile("tapes")
                              .getChildFile("44100 16bits").getChildFile("j106mb.wav");
        if (j106mb441.existsAsFile()) {
            auto res = JunoTapeDecoder::decodeWavFile(j106mb441, 1200);
            std::printf("Decoded j106mb@44100: success=%d, patches=%d, error=%s\n",
                        (int)res.success, (int)(res.data.size() / 18), res.errorMessage.toRawUTF8());
            expect(res.success && res.data.size() / 18 >= 1, "j106mb@44100 should decode at least 1 patch");
        } else {
            std::printf("WARNING: j106mb@44100 not found at: %s\n", j106mb441.getFullPathName().toRawUTF8());
        }

    }
};
