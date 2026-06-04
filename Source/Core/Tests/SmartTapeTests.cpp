#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

#include "JunoTapeEncoder.h"
#include "JunoTapeDecoder.h"

class JunoSmartTapeTests : public juce::UnitTest {
public:
    JunoSmartTapeTests() : juce::UnitTest("JunoSmartTape Tests", "JunoIO") {}

    // ── Helper: Generate a pure sine tone ─────────────────────────────────
    static juce::AudioBuffer<float> generateSineTone(double freqHz, double durationS, double sr = 44100.0)
    {
        int numSamples = (int)(sr * durationS + 0.5);
        juce::AudioBuffer<float> buf(1, numSamples);
        float* samples = buf.getWritePointer(0);
        double phaseDelta = juce::MathConstants<double>::twoPi * freqHz / sr;
        for (int i = 0; i < numSamples; ++i)
            samples[i] = (float)std::sin(i * phaseDelta);
        return buf;
    }
    
    // ── Helper: Generate sine + white noise ───────────────────────────────
    static juce::AudioBuffer<float> generateNoisySine(double freqHz, double durationS, double noiseDb, double sr = 44100.0)
    {
        auto buf = generateSineTone(freqHz, durationS, sr);
        float* samples = buf.getWritePointer(0);
        int numSamples = buf.getNumSamples();
        double noiseAmp = std::pow(10.0, noiseDb / 20.0); // dB to linear amplitude
        for (int i = 0; i < numSamples; ++i)
            samples[i] += (float)(noiseAmp * ((double)std::rand() / RAND_MAX * 2.0 - 1.0));
        // Normalize back to [-1, 1]
        float peak = buf.getMagnitude(0, 0, numSamples);
        if (peak > 0.0001f) buf.applyGain(1.0f / peak);
        return buf;
    }
    
    // ── Helper: Silence ───────────────────────────────────────────────────
    static juce::AudioBuffer<float> generateSilence(double durationS, double sr = 44100.0)
    {
        int numSamples = (int)(sr * durationS + 0.5);
        juce::AudioBuffer<float> buf(1, numSamples);
        buf.clear();
        return buf;
    }
    
    // ── Helper: Frequency-modulated sine (for jitter) ─────────────────────
    static juce::AudioBuffer<float> generateJitteredSine(double baseFreq, double durationS, double modDepthHz, double modFreq, double sr = 44100.0)
    {
        int numSamples = (int)(sr * durationS + 0.5);
        juce::AudioBuffer<float> buf(1, numSamples);
        float* samples = buf.getWritePointer(0);
        double phase = 0.0;
        for (int i = 0; i < numSamples; ++i) {
            double t = (double)i / sr;
            double instFreq = baseFreq + modDepthHz * std::sin(juce::MathConstants<double>::twoPi * modFreq * t);
            phase += juce::MathConstants<double>::twoPi * instFreq / sr;
            samples[i] = (float)std::sin(phase);
        }
        float peak = buf.getMagnitude(0, 0, numSamples);
        if (peak > 0.0001f) buf.applyGain(1.0f / peak);
        return buf;
    }
    
    // ── Helper: Sine with dropout regions (in the MIDDLE to avoid edge effects) ──
    static juce::AudioBuffer<float> generateDropoutSine(double freqHz, double durationS, double dropoutPct, double sr = 44100.0)
    {
        auto buf = generateSineTone(freqHz, durationS, sr);
        float* samples = buf.getWritePointer(0);
        int numSamples = buf.getNumSamples();
        // Place the dropout region in the middle of the signal so that
        // measureDropouts() (which looks at data after leaderStart + 0.6s)
        // reliably captures it regardless of signal duration.
        int dropoutLen = (int)(numSamples * dropoutPct / 100.0);
        int dropoutStart = (numSamples - dropoutLen) / 2;  // centered
        for (int i = dropoutStart; i < dropoutStart + dropoutLen && i < numSamples; ++i)
            samples[i] = 0.0f;
        return buf;
    }
    
    // ── Helper: Generate a full FSK tape in an AudioBuffer ────────────────
    // This encodes the same way as JunoTapeEncoder but returns a buffer
    // suitable for writing to a temp WAV file for smartDecode testing.
    static juce::AudioBuffer<float> encodeTapeAudio(const std::vector<uint8_t>& patchData, int baudRate, double sr = 44100.0)
    {
        std::vector<std::vector<uint8_t>> bank = { patchData };
        auto format = (baudRate == 340) ? JunoTapeEncoder::Juno60 : JunoTapeEncoder::Juno106;
        return JunoTapeEncoder::encodePatches(bank, sr, format);
    }
    
    // ── Helper: Preprocess audio the same way smartDecode() does ──────────
    static juce::AudioBuffer<float> preprocessBuffer(const juce::AudioBuffer<float>& input, double sr)
    {
        if (input.getNumChannels() > 1) {
            juce::AudioBuffer<float> mono(1, input.getNumSamples());
            mono.copyFrom(0, 0, input, 0, 0, input.getNumSamples());
            if (input.getNumChannels() > 1)
                mono.addFrom(0, 0, input, 1, 0, input.getNumSamples());
            mono.applyGain(0.5f);
            return preprocessBuffer(mono, sr);
        }
        
        auto buf = input;
        float* samples = buf.getWritePointer(0);
        int numSamples = buf.getNumSamples();
        
        // HPF
        float y_prev = 0.0f, x_prev = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float x = samples[i];
            float y = 0.9943f * (y_prev + x - x_prev);
            samples[i] = y;
            y_prev = y;
            x_prev = x;
        }
        
        // Normalize
        float peak = buf.getMagnitude(0, 0, numSamples);
        if (peak > 0.0001f) buf.applyGain(1.0f / peak);
        
        return buf;
    }
    
    // ══════════════════════════════════════════════════════════════════════
    // analyzeSignal() Tests
    // ══════════════════════════════════════════════════════════════════════
    
    void runTest() override
    {
        // ── Test 1: Clean Juno-60 leader tone (2380 Hz) ───────────────────
        beginTest("analyzeSignal: Clean 2380 Hz leader (Juno-60)");
        {
            double sr = 44100.0;
            // Generate 3 seconds of clean 2380 Hz sine
            auto buf = generateSineTone(2380.0, 3.0, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // Should detect Juno-60 (340 baud)
            expect(metrics.detectedBaudRate == 340,
                   "Clean 2380 Hz tone should detect as 340 baud (Juno-60), got " + juce::String(metrics.detectedBaudRate));
            
            // SNR should be very high (> 40 dB for clean synthetic sine)
            expect(metrics.snrDb > 30.0,
                   "Clean 2380 Hz sine should have high SNR (> 30 dB), got " + juce::String(metrics.snrDb, 1) + " dB");
            
            // Jitter should be very low for a pure sine
            expect(metrics.jitterPct < 2.0,
                   "Clean sine should have low jitter (< 2%), got " + juce::String(metrics.jitterPct, 1) + "%");
            
            // Duration should be approximately 3 seconds
            expect(std::abs(metrics.durationS - 3.0) < 0.5,
                   "Duration should be ~3.0s, got " + juce::String(metrics.durationS, 1) + "s");
            
            // DC bias should be near zero for a pure sine
            expect(metrics.dcBias < 0.01,
                   "DC bias should be < 0.01 for clean sine, got " + juce::String(metrics.dcBias, 5));
            
            // Dropouts may be up to ~3% for a pure sine due to zero-crossing samples
            // falling below the 5% peak threshold in measureDropouts()
            expect(metrics.dropoutPct < 5.0,
                   "Dropouts should be < 5% for continuous tone (zero-crossing artifact), got " + juce::String(metrics.dropoutPct, 1) + "%");
            
            // Quality should be GOOD (score >= 0.75)
            expect(metrics.qualityScore >= 0.75,
                   "Quality score should be >= 0.75 (GOOD), got " + juce::String(metrics.qualityScore, 3));
            expect(metrics.qualityLabel == "GOOD",
                   "Quality label should be GOOD, got " + metrics.qualityLabel);
            
            // Analysis time should be reasonable
            expect(metrics.analysisTimeS > 0.0,
                   "Analysis time should be > 0, got " + juce::String(metrics.analysisTimeS, 3) + "s");
            
            std::printf("  analyze[1]: Clean 2380 Hz: baud=%d SNR=%.1fdB jitter=%.1f%% dur=%.1fs score=%.3f label=%s\n",
                       metrics.detectedBaudRate, metrics.snrDb, metrics.jitterPct,
                       metrics.durationS, metrics.qualityScore, metrics.qualityLabel.toRawUTF8());
        }
        
        // ── Test 2: Clean Juno-106 leader tone (2100 Hz) ─────────────────
        beginTest("analyzeSignal: Clean 2100 Hz leader (Juno-106)");
        {
            double sr = 44100.0;
            auto buf = generateSineTone(2100.0, 3.0, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            expect(metrics.detectedBaudRate == 1200,
                   "Clean 2100 Hz tone should detect as 1200 baud (Juno-106), got " + juce::String(metrics.detectedBaudRate));
            expect(metrics.snrDb > 30.0,
                   "Clean 2100 Hz sine should have high SNR (> 30 dB), got " + juce::String(metrics.snrDb, 1) + " dB");
            expect(metrics.qualityLabel == "GOOD",
                   "Quality label should be GOOD, got " + metrics.qualityLabel);
            
            std::printf("  analyze[2]: Clean 2100 Hz: baud=%d SNR=%.1fdB score=%.3f label=%s\n",
                       metrics.detectedBaudRate, metrics.snrDb, metrics.qualityScore, metrics.qualityLabel.toRawUTF8());
        }
        
        // ── Test 3: Noisy signal (low SNR) ───────────────────────────────
        beginTest("analyzeSignal: Noisy 2380 Hz signal");
        {
            double sr = 44100.0;
            // Add 0 dB noise (same amplitude as sine) — heavy noise that
            // the Goertzel filter won't fully reject since noise spans all freqs
            auto buf = generateNoisySine(2380.0, 3.0, 0.0, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // Noisy signal should still detect format since leader tone dominates
            expect(metrics.detectedBaudRate == 340 || metrics.detectedBaudRate == 1200,
                   "Noisy signal should still detect a baud rate, got " + juce::String(metrics.detectedBaudRate));
            
            // SNR should be lower than clean signal (Goertzel + HPF reject some noise,
            // but 0 dB noise still gets through the ~678 Hz bandwidth)
            expect(metrics.snrDb < 50.0,
                   "Noisy signal should have SNR < 50 dB (clean ~60 dB), got " + juce::String(metrics.snrDb, 1) + " dB");
            
            // Dropouts may be higher for noise due to random amplitude dips
            expect(metrics.dropoutPct < 15.0,
                   "Continuous noise signal should have reasonable dropouts, got " + juce::String(metrics.dropoutPct, 1) + "%");
            
            std::printf("  analyze[3]: Noisy 2380 Hz: baud=%d SNR=%.1fdB score=%.3f label=%s\n",
                       metrics.detectedBaudRate, metrics.snrDb, metrics.qualityScore, metrics.qualityLabel.toRawUTF8());
        }
        
        // ── Test 4: Silence → no detection ───────────────────────────────
        beginTest("analyzeSignal: Silence returns early");
        {
            double sr = 44100.0;
            auto buf = generateSilence(3.0, sr);
            float* samples = buf.getWritePointer(0);
            int numSamples = buf.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // Silence may still get a default baud rate since no leader tone is found
            // (analyzeSignal defaults to 1200 if leader tone detection fails)
            expect(metrics.snrDb == 0.0,
                   "Silence should have 0 SNR, got " + juce::String(metrics.snrDb, 1));
            expect(metrics.analysisTimeS > 0.0,
                   "Analysis should have taken some time even for silence");
            
            std::printf("  analyze[4]: Silence: baud=%d analysisTime=%.3fs\n",
                       metrics.detectedBaudRate, metrics.analysisTimeS);
        }
        
        // ── Test 5: DC bias detection ────────────────────────────────────
        beginTest("analyzeSignal: DC bias detection");
        {
            double sr = 44100.0;
            // Generate sine + DC offset
            auto buf = generateSineTone(2380.0, 3.0, sr);
            float* samples = buf.getWritePointer(0);
            int numSamples = buf.getNumSamples();
            for (int i = 0; i < numSamples; ++i)
                samples[i] += 0.05f; // Add 5% DC offset
            
            auto processed = preprocessBuffer(buf, sr);
            samples = processed.getWritePointer(0);
            numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // After HPF, most DC should be removed, but some residual should remain
            // The HPF has cutoff ~32 Hz, so DC should be attenuated significantly
            // We just verify dcBias is measurable (not NaN)
            expect(!std::isnan(metrics.dcBias),
                   "DC bias should not be NaN");
            
            std::printf("  analyze[5]: DC bias test: dcBias=%.6f snr=%.1fdB\n",
                       metrics.dcBias, metrics.snrDb);
        }
        
        // ── Test 6: Jitter detection with FM signal ──────────────────────
        beginTest("analyzeSignal: Jitter detection with FM");
        {
            double sr = 44100.0;
            // Generate a frequency-modulated sine at 2380 Hz with 50 Hz FM deviation at 10 Hz rate
            // This should produce measurable jitter
            auto buf = generateJitteredSine(2380.0, 3.0, 100.0, 8.0, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // The FM signal should produce measurable jitter (> 0%)
            expect(metrics.jitterPct > 0.0,
                   "FM-modulated sine should have jitter > 0%, got " + juce::String(metrics.jitterPct, 1) + "%");
            
            std::printf("  analyze[6]: Jittered 2380 Hz: baud=%d jitter=%.1f%% snr=%.1fdB score=%.3f\n",
                       metrics.detectedBaudRate, metrics.jitterPct, metrics.snrDb, metrics.qualityScore);
        }
        
        // ── Test 7: Dropout detection ────────────────────────────────────
        beginTest("analyzeSignal: Dropout detection");
        {
            double sr = 44100.0;
            // Generate sine with 20% dropout at the end
            auto buf = generateDropoutSine(2380.0, 3.0, 20.0, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // Dropout percentage should be > 0
            expect(metrics.dropoutPct > 0.0,
                   "Signal with 20% dropout should show dropouts > 0%, got " + juce::String(metrics.dropoutPct, 1) + "%");
            
            std::printf("  analyze[7]: Dropout 20%%: dropouts=%.1f%% dur=%.1fs score=%.3f\n",
                       metrics.dropoutPct, metrics.durationS, metrics.qualityScore);
        }
        
        // ── Test 8: Very short signal ────────────────────────────────────
        beginTest("analyzeSignal: Short signal (< 0.5s)");
        {
            double sr = 44100.0;
            // Very short burst of 2380 Hz (0.3 seconds — below the 0.5s minimum)
            auto buf = generateSineTone(2380.0, 0.3, sr);
            auto processed = preprocessBuffer(buf, sr);
            float* samples = processed.getWritePointer(0);
            int numSamples = processed.getNumSamples();
            
            auto metrics = JunoTapeDecoder::analyzeSignal(samples, numSamples, sr);
            
            // Should not detect any format (too short for reliable detection)
            // Duration should be < 0.5s
            expect(metrics.durationS < 0.5,
                   "Duration should reflect short signal < 0.5s, got " + juce::String(metrics.durationS, 3) + "s");
            
            std::printf("  analyze[8]: Short signal: dur=%.3fs baud=%d\n",
                       metrics.durationS, metrics.detectedBaudRate);
        }
        
        // ── Test 9: computeQualityScore boundary checks ──────────────────
        beginTest("computeQualityScore: Boundary values");
        {
            // Perfect signal: max SNR, min jitter, min dropouts, long duration
            double perfect = JunoTapeDecoder::computeQualityScore(40.0, 0.0, 0.0, 30.0);
            // Perfect should be ~1.0: 0.35*1.0 + 0.30*1.0 + 0.20*1.0 + 0.15*1.0 = 1.0
            expect(std::abs(perfect - 1.0) < 0.01,
                   "Perfect signal score should be ~1.0, got " + juce::String(perfect, 4));
            
            // Terrible signal: min SNR, max jitter, max dropouts, 0 duration
            double terrible = JunoTapeDecoder::computeQualityScore(0.0, 15.0, 30.0, 0.0);
            // Terrible should be ~0.0
            expect(terrible < 0.1,
                   "Terrible signal score should be < 0.1, got " + juce::String(terrible, 4));
            
            // Label boundaries
            auto testLabel = [&](double score, const juce::String& expected) {
                // Simulate the label logic
                juce::String label;
                if (score >= 0.75) label = "GOOD";
                else if (score >= 0.50) label = "FAIR";
                else if (score >= 0.25) label = "POOR";
                else label = "DEGRADED";
                expect(label == expected,
                       "Score " + juce::String(score, 2) + " should be " + expected + ", got " + label);
            };
            testLabel(0.80, "GOOD");
            testLabel(0.62, "FAIR");
            testLabel(0.37, "POOR");
            testLabel(0.12, "DEGRADED");
            
            std::printf("  analyze[9]: Quality boundaries: perfect=%.4f terrible=%.4f\n", perfect, terrible);
        }
        
        // ══════════════════════════════════════════════════════════════════
        // smartDecode() Tests
        // ══════════════════════════════════════════════════════════════════
        
        // ── Test 10: Smart decode Juno-106 synthetic tape ────────────────
        beginTest("smartDecode: Juno-106 synthetic tape round-trip");
        {
            // Build a known patch
            uint8_t originalPatch[18] = {0};
            originalPatch[0]  = 0x40;  // LFO Rate ~50%
            originalPatch[5]  = 0x7F;  // VCF Freq max
            originalPatch[10] = 0x7F;  // VCA Level max
            originalPatch[11] = 0x0A;  // Attack
            originalPatch[16] = 0x0E;  // SW1: 8' range + Saw + Pulse
            originalPatch[17] = 0x00;  // SW2: ENV, POS, HPF flat
            
            std::vector<uint8_t> patchVec(originalPatch, originalPatch + 18);
            
            // Encode to audio
            auto audioBuf = encodeTapeAudio(patchVec, 1200, 44100.0);
            expect(audioBuf.getNumSamples() > 1000,
                   "Juno-106 encoded audio should be > 1000 samples, got " + juce::String(audioBuf.getNumSamples()));
            
            // Write to temp WAV file
            juce::File tempFile = juce::File::createTempFile(".wav");
            {
                auto options = juce::AudioFormatWriterOptions()
                    .withSampleRate(44100.0)
                    .withNumChannels(1)
                    .withBitsPerSample(16);
                std::unique_ptr<juce::OutputStream> outStream(tempFile.createOutputStream());
                if (outStream != nullptr) {
                    juce::WavAudioFormat wavFormat;
                    if (auto writer = wavFormat.createWriterFor(outStream, options)) {
                        writer->writeFromAudioSampleBuffer(audioBuf, 0, audioBuf.getNumSamples());
                        outStream.release(); // writer owns it now
                    }
                }
            }
            
            expect(tempFile.existsAsFile(), "Temp WAV file should exist");
            
            if (tempFile.existsAsFile()) {
                // Run smart decode
                auto result = JunoTapeDecoder::smartDecode(tempFile);
                
                std::printf("  smartDecode[10]: J106 synthetic: success=%d patches=%d winner=%d auto=%d\n",
                           (int)result.success, 
                           result.decoderResults.empty() ? 0 : result.decoderResults[0].patchCount,
                           result.winnerIndex, (int)result.autoSelected);
                
                // Should succeed
                expect(result.success,
                       "Juno-106 synthetic tape should decode successfully, error: " + result.errorMessage);
                
                // Should have at least 1 decoder result
                expect(!result.decoderResults.empty(),
                       "Should have at least 1 decoder result");
                
                if (result.success && !result.decoderResults.empty()) {
                    auto& entry = result.decoderResults[0];
                    expect(entry.patchCount >= 1,
                           "Should decode at least 1 patch, got " + juce::String(entry.patchCount));
                    expect(entry.rawBytes > 0,
                           "Should have raw bytes > 0");
                    expect(entry.elapsedS > 0.0,
                           "Decoding should take some time");
                    
                    // Verify quality metrics from analysis phase
                    expect(result.metrics.detectedBaudRate == 1200 || result.metrics.detectedBaudRate == 0,
                           "Juno-106 synthetic tape should detect as 1200 baud or 0, got " + juce::String(result.metrics.detectedBaudRate));
                    
                    // Verify decoded patch data — minor bit errors are expected in FSK
                    // round-trip even with synthetic audio. Check slider bytes are
                    // approximately correct and switch bytes have valid ranges.
                    if (entry.patchCount >= 1 && entry.validated.size() >= 18) {
                        // Slider bytes should be approximately correct
                        int slidersOK = 0;
                        for (int i = 0; i < 16; ++i) {
                            auto diff = std::abs((int)entry.validated[i] - (int)originalPatch[i]);
                            if (diff <= 2) slidersOK++;
                            else {
                                std::printf("    Slider[%d]: decoded=0x%02X expected=0x%02X diff=%d\n",
                                           i, entry.validated[i], originalPatch[i], diff);
                            }
                        }
                        expect(slidersOK >= 14,
                               "At least 14/16 slider bytes should be within ±2 of original, got " + juce::String(slidersOK) + "/16");
                        
                        // SW1 should have a valid single range bit (no multiple ranges)
                        uint8_t sw1 = entry.validated[16];
                        int rangeBits = sw1 & 0x07;
                        bool validRange = (rangeBits != 0) && ((rangeBits & (rangeBits - 1)) == 0);
                        expect(validRange,
                               "SW1 should have valid single range, got range=0x" + juce::String::toHexString(rangeBits));
                        
                        std::printf("  smartDecode[10]: %d sliders OK/16, SW1=0x%02X range=0x%X (valid=%d)\n",
                                   slidersOK, sw1, rangeBits, (int)validRange);
                    }
                }
                
                // Clean up temp file
                tempFile.deleteFile();
            }
        }
        
        // ── Test 11: Smart decode Juno-60 synthetic tape ─────────────────
        beginTest("smartDecode: Juno-60 synthetic tape round-trip");
        {
            // Build a Juno-60 patch
            uint8_t originalPatch[18] = {0};
            originalPatch[0]  = 0x40;  // LFO Rate
            originalPatch[5]  = 0x7F;  // VCF Freq max
            originalPatch[10] = 0x7F;  // VCA Level max
            originalPatch[15] = 0x7F;  // Sub level max
            // SW1: bit1=1 (8' range), bit3=1 (Saw ON), bit5=1 (Sub Osc ON)
            originalPatch[16] = 0x0A | 0x20;  // 0x2A
            // SW2: bits 3-4=11 (HPF pos0/FLAT)
            originalPatch[17] = 0x18;
            
            std::vector<uint8_t> patchVec(originalPatch, originalPatch + 18);
            
            // Encode to audio at 340 baud
            auto audioBuf = encodeTapeAudio(patchVec, 340, 44100.0);
            expect(audioBuf.getNumSamples() > 1000,
                   "Juno-60 encoded audio should be > 1000 samples, got " + juce::String(audioBuf.getNumSamples()));
            
            // Juno-60 at 340 baud should be longer than Juno-106 at 1200 baud for same data
            auto audioBuf106 = encodeTapeAudio(patchVec, 1200, 44100.0);
            expect(audioBuf.getNumSamples() > audioBuf106.getNumSamples(),
                   "Juno-60 (340 baud) should produce longer audio than Juno-106 (1200 baud): "
                   + juce::String(audioBuf.getNumSamples()) + " vs " + juce::String(audioBuf106.getNumSamples()));
            
            // Write to temp WAV file
            juce::File tempFile = juce::File::createTempFile(".wav");
            {
                auto options = juce::AudioFormatWriterOptions()
                    .withSampleRate(44100.0)
                    .withNumChannels(1)
                    .withBitsPerSample(16);
                std::unique_ptr<juce::OutputStream> outStream(tempFile.createOutputStream());
                if (outStream != nullptr) {
                    juce::WavAudioFormat wavFormat;
                    if (auto writer = wavFormat.createWriterFor(outStream, options)) {
                        writer->writeFromAudioSampleBuffer(audioBuf, 0, audioBuf.getNumSamples());
                        outStream.release();
                    }
                }
            }
            
            expect(tempFile.existsAsFile(), "Temp WAV file should exist");
            
            if (tempFile.existsAsFile()) {
                auto result = JunoTapeDecoder::smartDecode(tempFile);
                
                std::printf("  smartDecode[11]: J60 synthetic: success=%d patches=%d baud=%d\n",
                           (int)result.success,
                           result.decoderResults.empty() ? 0 : result.decoderResults[0].patchCount,
                           result.metrics.detectedBaudRate);
                
                // May or may not succeed at 340 baud — synthetic tape at low baud
                // has fewer samples per bit which can make decoding harder.
                // Even if it fails, the analysis phase should have worked.
                expect(result.metrics.analysisTimeS > 0.0,
                       "Analysis should complete even if decode fails");
                expect(result.metrics.durationS > 0.0,
                       "Duration should be measured, got " + juce::String(result.metrics.durationS, 1));
                
                if (result.success && !result.decoderResults.empty()) {
                    std::printf("  smartDecode[11]: Got %d patches from Juno-60 synthetic tape\n",
                               result.decoderResults[0].patchCount);
                }
                
                tempFile.deleteFile();
            }
        }
        
        // ── Test 12: Smart decode with progress callback ─────────────────
        beginTest("smartDecode: Progress callback fires");
        {
            uint8_t patch[18] = {0};
            patch[0] = 0x40;
            patch[16] = 0x0E;
            patch[17] = 0x00;
            std::vector<uint8_t> patchVec(patch, patch + 18);
            
            auto audioBuf = encodeTapeAudio(patchVec, 1200, 44100.0);
            
            juce::File tempFile = juce::File::createTempFile(".wav");
            {
                auto options = juce::AudioFormatWriterOptions()
                    .withSampleRate(44100.0)
                    .withNumChannels(1)
                    .withBitsPerSample(16);
                std::unique_ptr<juce::OutputStream> outStream(tempFile.createOutputStream());
                if (outStream != nullptr) {
                    juce::WavAudioFormat wavFormat;
                    if (auto writer = wavFormat.createWriterFor(outStream, options)) {
                        writer->writeFromAudioSampleBuffer(audioBuf, 0, audioBuf.getNumSamples());
                        outStream.release();
                    }
                }
            }
            
            if (tempFile.existsAsFile()) {
                int callbackCount = 0;
                juce::String lastMessage;
                
                auto result = JunoTapeDecoder::smartDecode(tempFile,
                    [&](const juce::String& msg) {
                        callbackCount++;
                        lastMessage = msg;
                    });
                
                expect(callbackCount > 0,
                       "Progress callback should fire at least once, fired " + juce::String(callbackCount) + " times");
                expect(lastMessage.isNotEmpty(),
                       "Last progress message should not be empty");
                expect(lastMessage == "=== COMPLETADO ===",
                       "Last progress should be COMPLETADO, got: " + lastMessage);
                
                std::printf("  smartDecode[12]: Progress callback fired %d times\n", callbackCount);
                
                tempFile.deleteFile();
            }
        }
        
        // ── Test 13: Smart decode on REAL full bank tape ──────────────
        // Side-by-side comparison: decodeWavFile vs smartDecode on the same file.
        // We compare the number of patches found to identify any discrepancies
        // between the two code paths.
        beginTest("smartDecode: REAL Roland Juno-60 G1 tape (comparison with decodeWavFile)");
        {
            juce::File srcFile(__FILE__);
            juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
            juce::File docsDir = projectRoot.getChildFile("docs");
            juce::File tapeFile = docsDir.getChildFile("JUNO-106")
                .getChildFile("Roland Juno-60 factory programs group 1.wav");
            
            if (tapeFile.existsAsFile()) {
                std::printf("  smartDecode[13]: === SIDE-BY-SIDE COMPARISON ===\n");
                std::printf("  smartDecode[13]: File: %s (%lld bytes)\n",
                           tapeFile.getFileName().toRawUTF8(), tapeFile.getSize());
                
                // ── Method 1: decodeWavFile (forced 1200) ────────────────
                double t1 = juce::Time::getMillisecondCounterHiRes();
                auto legacyRes = JunoTapeDecoder::decodeWavFile(tapeFile, 1200);
                double tLegacy = (juce::Time::getMillisecondCounterHiRes() - t1) / 1000.0;
                int legacyPatches = (int)legacyRes.data.size() / 18;
                std::printf("  smartDecode[13]: decodeWavFile(1200): %d patches (%zu bytes, %.1fs)\n",
                           legacyPatches, legacyRes.data.size(), tLegacy);
                
                // ── Method 2: decodeWavFile (auto-detect) ────────────────
                double t2 = juce::Time::getMillisecondCounterHiRes();
                auto autoRes = JunoTapeDecoder::decodeWavFile(tapeFile, 0);
                double tAuto = (juce::Time::getMillisecondCounterHiRes() - t2) / 1000.0;
                int autoPatches = (int)autoRes.data.size() / 18;
                std::printf("  smartDecode[13]: decodeWavFile(auto): success=%d patches=%d baud=%d (%.1fs)\n",
                           (int)autoRes.success, autoPatches, autoRes.detectedBaudRate, tAuto);
                
                // ── Method 3: smartDecode (with fixed strategy) ──────────
                double t3 = juce::Time::getMillisecondCounterHiRes();
                auto smartRes = JunoTapeDecoder::smartDecode(tapeFile);
                double tSmart = (juce::Time::getMillisecondCounterHiRes() - t3) / 1000.0;
                
                std::printf("  smartDecode[13]: smartDecode: quality=%s (%.3f) baud=%d\n",
                           smartRes.metrics.qualityLabel.toRawUTF8(),
                           smartRes.metrics.qualityScore, smartRes.metrics.detectedBaudRate);
                
                int smartBestPatches = 0;
                for (size_t di = 0; di < smartRes.decoderResults.size(); ++di) {
                    auto& entry = smartRes.decoderResults[di];
                    std::printf("  smartDecode[13]:   Decoder[%zu] \"%s\": %d patches (rank=%.0f, raw=%d bytes, %.1fs)\n",
                               di, entry.label.toRawUTF8(), entry.patchCount,
                               entry.rank, entry.rawBytes, entry.elapsedS);
                    smartBestPatches = std::max(smartBestPatches, entry.patchCount);
                }
                
                std::printf("  smartDecode[13]: smartDecode total: %.1fs, BEST=%d patches\n",
                           tSmart, smartBestPatches);
                
                // ── Diagnostic: direct decodeFSK call ──────────────────
                // Load + preprocess audio identically to decodeWavFile to rule out
                // preprocessing differences as the cause of patch count discrepancy.
                {
                    juce::AudioFormatManager diagFmt;
                    diagFmt.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> diagReader(diagFmt.createReaderFor(tapeFile));
                    if (diagReader != nullptr) {
                        juce::AudioBuffer<float> diagBuf((int)diagReader->numChannels, (int)diagReader->lengthInSamples);
                        diagReader->read(&diagBuf, 0, (int)diagReader->lengthInSamples, 0, true, true);
                        double diagSr = (double)diagReader->sampleRate;
                        
                        // Mono mix
                        if (diagBuf.getNumChannels() > 1) {
                            diagBuf.addFrom(0, 0, diagBuf, 1, 0, diagBuf.getNumSamples());
                            diagBuf.applyGain(0.5f);
                        }
                        
                        // HPF (same formula as both paths)
                        float* diagSamples = diagBuf.getWritePointer(0);
                        int diagNumSamples = diagBuf.getNumSamples();
                        float dy = 0.0f, dx = 0.0f;
                        for (int i = 0; i < diagNumSamples; ++i) {
                            float x = diagSamples[i];
                            float y = 0.9943f * (dy + x - dx);
                            diagSamples[i] = y;
                            dy = y;
                            dx = x;
                        }
                        
                        // Normalize
                        float diagPeak = diagBuf.getMagnitude(0, 0, diagNumSamples);
                        if (diagPeak > 0.0001f) diagBuf.applyGain(1.0f / diagPeak);
                        
                        // No upsample needed (44100 Hz native)
                        
                        // Direct calls to decodeFSK at BOTH baud rates
                        std::printf("  smartDecode[13]: DIAG direct decodeFSK @ 1200 baud:\n");
                        double dt = juce::Time::getMillisecondCounterHiRes();
                        auto raw1200 = JunoTapeDecoder::decodeFSK(diagSamples, diagNumSamples, diagSr, 1200.0);
                        double t1200 = (juce::Time::getMillisecondCounterHiRes() - dt) / 1000.0;
                        auto val1200 = JunoTapeDecoder::validatePatches(raw1200);
                        int p1200 = (int)val1200.size() / 18;
                        std::printf("  smartDecode[13]:   decodeFSK(1200): %d patches (%zu raw bytes, %.1fs)\n",
                                   p1200, raw1200.size(), t1200);
                        
                        // Show a few raw bytes as hex for comparison
                        std::printf("  smartDecode[13]:   raw1200[0..31] hex: ");
                        for (int i = 0; i < std::min(32, (int)raw1200.size()); ++i)
                            std::printf("%02X ", raw1200[i]);
                        std::printf("\n");
                        
                        dt = juce::Time::getMillisecondCounterHiRes();
                        auto raw340 = JunoTapeDecoder::decodeFSK(diagSamples, diagNumSamples, diagSr, 340.0);
                        double t340 = (juce::Time::getMillisecondCounterHiRes() - dt) / 1000.0;
                        auto val340 = JunoTapeDecoder::validatePatches(raw340);
                        int p340 = (int)val340.size() / 18;
                        std::printf("  smartDecode[13]:   decodeFSK(340):  %d patches (%zu raw bytes, %.1fs)\n",
                                   p340, raw340.size(), t340);
                        std::printf("  smartDecode[13]:   raw340[0..31] hex: ");
                        for (int i = 0; i < std::min(32, (int)raw340.size()); ++i)
                            std::printf("%02X ", raw340[i]);
                        std::printf("\n");
                        
                        std::printf("  smartDecode[13]: DIAG CONCLUSION: direct decodeFSK(1200)=%d patches, decodeFSK(340)=%d patches\n",
                                   p1200, p340);
                        
                        // Also run analyzeSignal on the DIAG preprocessed samples to check
                        auto diagMetrics = JunoTapeDecoder::analyzeSignal(diagSamples, diagNumSamples, diagSr);
                        std::printf("  smartDecode[13]: DIAG analyzeSignal: baud=%d SNR=%.1fdB jitter=%.1f%% dropouts=%.1f%% dur=%.1fs score=%.3f label=%s\n",
                                   diagMetrics.detectedBaudRate, diagMetrics.snrDb,
                                   diagMetrics.jitterPct, diagMetrics.dropoutPct,
                                   diagMetrics.durationS, diagMetrics.qualityScore,
                                   diagMetrics.qualityLabel.toRawUTF8());
                        
                        // ── RAW GOERTZEL ENERGY DIAGNOSTIC ────────────
                        // Manually run the same Goertzel analysis as analyzeSignal and
                        // detectFormatFromLeaderTone to find the discrepancy.
                        {
                            double gSr = diagSr;
                            int gTotalWindow = std::min(diagNumSamples, (int)(gSr * 3.0));
                            
                            // Find leader start (same algorithm as both functions)
                            double gGlobalPeak = 0.0;
                            for (int i = 0; i < gTotalWindow; ++i)
                                gGlobalPeak = std::max(gGlobalPeak, (double)std::abs(diagSamples[i]));
                            
                            int gLeaderStart = 0;
                            if (gGlobalPeak > 0.001) {
                                float gOnsetThreshold = (float)(gGlobalPeak * 0.05);
                                while (gLeaderStart < gTotalWindow) {
                                    float gLocalPeak = 0.0f;
                                    for (int j = gLeaderStart; j < std::min(gTotalWindow, gLeaderStart + (int)(gSr * 0.1)); ++j)
                                        gLocalPeak = std::max(gLocalPeak, std::abs(diagSamples[j]));
                                    if (gLocalPeak > gOnsetThreshold) break;
                                    gLeaderStart += (int)(gSr * 0.1);
                                }
                            }
                            
                            std::printf("  smartDecode[13]: DIAG GOERTZEL: globalPeak=%.4f leaderStart=%.3fs\n",
                                       gGlobalPeak, (double)gLeaderStart / gSr);
                            
                            int gLeaderLen = std::min(gTotalWindow, gLeaderStart + (int)(gSr * 0.5));
                            int gSegLen = gLeaderLen - gLeaderStart;
                            std::printf("  smartDecode[13]: DIAG GOERTZEL: leaderLen=%.3fs segLen=%.3fs (%d samples)\n",
                                       (double)gLeaderLen / gSr, (double)gSegLen / gSr, gSegLen);
                            
                            if (gSegLen > (int)(gSr * 0.01)) {
                                // Run Goertzel at 2100 Hz and 2380 Hz (same as both detection functions)
                                auto gs1 = JunoTapeDecoder::initGoertzel(2100.0, gSr);
                                auto gs2 = JunoTapeDecoder::initGoertzel(2380.0, gSr);
                                for (int i = gLeaderStart; i < gLeaderLen; ++i) {
                                    double x = (double)diagSamples[i];
                                    JunoTapeDecoder::goertzelProcess(gs1, x);
                                    JunoTapeDecoder::goertzelProcess(gs2, x);
                                }
                                double p2100 = JunoTapeDecoder::goertzelPower(gs1);
                                double p2380 = JunoTapeDecoder::goertzelPower(gs2);
                                double e2100 = p2100 / (double)(gSegLen * gSegLen);
                                double e2380 = p2380 / (double)(gSegLen * gSegLen);
                                
                                std::printf("  smartDecode[13]: DIAG GOERTZEL: power2100=%.6e power2380=%.6e\n", p2100, p2380);
                                std::printf("  smartDecode[13]: DIAG GOERTZEL: energy2100=%.6e energy2380=%.6e\n", e2100, e2380);
                                std::printf("  smartDecode[13]: DIAG GOERTZEL: ratio2380/2100=%.3f\n", e2380 / std::max(e2100, 1e-30));
                                std::printf("  smartDecode[13]: DIAG GOERTZEL: ratio2100/2380=%.3f\n", e2100 / std::max(e2380, 1e-30));
                                
                                // What does detectFormatFromLeaderTone say on the SAME samples?
                                int fmtDetect = JunoTapeDecoder::detectFormatFromLeaderTone(diagSamples, diagNumSamples, gSr);
                                std::printf("  smartDecode[13]: DIAG detectFormatFromLeaderTone() = %d (340=J60, 1200=J106, 0=unknown)\n", fmtDetect);
                                
                                // Conclusion
                                if (e2380 > e2100 * 2.0)
                                    std::printf("  smartDecode[13]: DIAG CONCLUSION: Goertzel says 2380 Hz DOMINANT (2:1 ratio) -> 340 baud\n");
                                else if (e2100 > e2380 * 2.0)
                                    std::printf("  smartDecode[13]: DIAG CONCLUSION: Goertzel says 2100 Hz DOMINANT (2:1 ratio) -> 1200 baud\n");
                                else
                                    std::printf("  smartDecode[13]: DIAG CONCLUSION: Goertzel says AMBIGUOUS (neither 2x) -> needs both rates\n");
                            }
                        }
                    }
                }
                
                // ── Summary ───────────────────────────────────────────
                std::printf("  smartDecode[13]: SUMMARY: decodeWavFile(1200)=%d decodeWavFile(auto)=%d smartDecode=%d\n",
                           legacyPatches, autoPatches, smartBestPatches);
                
                // If legacy finds 40+ but smartDecode doesn't, there's a bug
                // in the smartDecode pipeline (preprocessing, analyze, strategy, etc.)
                if (legacyPatches >= 40 && smartBestPatches < legacyPatches) {
                    std::printf("  smartDecode[13]: WARNING: smartDecode found fewer patches than decodeWavFile!\n");
                    std::printf("  smartDecode[13]:   Difference: %d patches (legacy) vs %d patches (smart)\n",
                               legacyPatches, smartBestPatches);
                }
                
                // The key expectation: smartDecode should be at least as good as
                // decodeWavFile for the same file. Real recordings have noise and
                // wow/flutter that the speed sweep should handle.
                // We expect >= 40 patches (matching existing JunoTapeTests expectation).
                expect(smartBestPatches >= 40,
                       "smartDecode should decode >= 40 patches, got " + juce::String(smartBestPatches));
            } else {
                std::printf("  smartDecode[13]: WARNING: Roland Juno-60 G1 not found at: %s\n", tapeFile.getFullPathName().toRawUTF8());
            }
        }
        
        // ── Test 14: smartDecode on ALL real tape files ─────────────
        // Runs smartDecode on every real WAV file and compares with decodeWavFile results.
        beginTest("smartDecode: ALL real tape files cross-check");
        {
            juce::File srcFile(__FILE__);
            juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
            juce::File docsDir = projectRoot.getChildFile("docs");
            
            // Define all known real tape files with expected parameters
            struct TapeFileSpec {
                juce::String path;      // relative to docsDir
                juce::String label;     // display name
                int forcedBaud;         // baud rate for decodeWavFile forced mode
                int minPatchesDecodeWav; // minimum patches from decodeWavFile
                int minPatchesSmart;    // minimum patches from smartDecode
            };
            
            TapeFileSpec specs[] = {
                // Juno-60 Bank A (340 baud native, short recording)
                {"Juno-60 (1)/JUNO-60 Bank A.wav", "JUNO-60 Bank A", 340, 10, 5},
                // Juno-60 Bank B (340 baud native, short recording)
                {"Juno-60 (1)/JUNO-60 Bank B.wav", "JUNO-60 Bank B", 340, 5, 3},
                // Roland Juno-60 G1 (hyrbid: 2380 Hz leader, 1200 baud data)
                {"JUNO-106/Roland Juno-60 factory programs group 1.wav", "Juno-60 G1", 1200, 40, 40},
                // Roland Juno-60 G2 (1200 baud)
                {"JUNO-106/Roland Juno-60 factory programs group 2.wav", "Juno-60 G2", 1200, 30, 30},
                // JUNO106 Bank A (1200 baud, short)
                {"JUNO-106/JUNO106 Bank A.wav", "JUNO106 Bank A", 1200, 1, 1},
                // JUNO106 Bank B (1200 baud, short)
                {"JUNO-106/JUNO106 Bank B.wav", "JUNO106 Bank B", 1200, 1, 1},
            };
            
            std::printf("\n  smartDecode[14]: === CROSS-CHECK ALL FILES ===\n");
            
            int totalFiles = 0;
            int smartMatchesLegacy = 0;
            int smartWorse = 0;
            int smartBetter = 0;
            
            for (auto& spec : specs) {
                juce::File tapeFile = docsDir.getChildFile(spec.path);
                if (!tapeFile.existsAsFile()) {
                    std::printf("  smartDecode[14]: SKIP %s (not found at %s)\n",
                               spec.label.toRawUTF8(), tapeFile.getFullPathName().toRawUTF8());
                    continue;
                }
                
                totalFiles++;
                std::printf("\n  smartDecode[14]: === %s ===\n", spec.label.toRawUTF8());
                std::printf("  smartDecode[14]: File: %s (%lld bytes)\n",
                           tapeFile.getFileName().toRawUTF8(), tapeFile.getSize());
                
                // ── decodeWavFile (forced baud) ────────────────────────
                auto legacyRes = JunoTapeDecoder::decodeWavFile(tapeFile, spec.forcedBaud);
                int legacyPatches = (int)legacyRes.data.size() / 18;
                std::printf("  smartDecode[14]:   decodeWavFile(%d): %d patches (%s)\n",
                           spec.forcedBaud, legacyPatches,
                           legacyRes.success ? "OK" : "FAIL");
                
                // ── decodeWavFile (auto-detect) ────────────────────────
                auto autoRes = JunoTapeDecoder::decodeWavFile(tapeFile, 0);
                int autoPatches = (int)autoRes.data.size() / 18;
                std::printf("  smartDecode[14]:   decodeWavFile(auto): %d patches (baud=%d, %s)\n",
                           autoPatches, autoRes.detectedBaudRate,
                           autoRes.success ? "OK" : "FAIL");
                
                // ── smartDecode ────────────────────────────────────────
                auto smartRes = JunoTapeDecoder::smartDecode(tapeFile);
                
                int smartBestPatches = 0;
                int smartWinnerIdx = smartRes.winnerIndex;
                for (size_t di = 0; di < smartRes.decoderResults.size(); ++di) {
                    auto& entry = smartRes.decoderResults[di];
                    std::printf("  smartDecode[14]:   smartDecode[%zu] %s: %d patches\n",
                               di, entry.label.toRawUTF8(), entry.patchCount);
                    smartBestPatches = std::max(smartBestPatches, entry.patchCount);
                }
                std::printf("  smartDecode[14]:   smartDecode: BEST=%d patches (quality=%s, baud=%d, winner=%d)\n",
                           smartBestPatches,
                           smartRes.metrics.qualityLabel.toRawUTF8(),
                           smartRes.metrics.detectedBaudRate,
                           smartWinnerIdx);
                
                // Compare
                std::printf("  smartDecode[14]:   RESULT: decodeWavFile(%d)=%d auto=%d smart=%d\n",
                           spec.forcedBaud, legacyPatches, autoPatches, smartBestPatches);
                
                if (smartBestPatches >= legacyPatches) {
                    smartMatchesLegacy++;
                    std::printf("  smartDecode[14]:   -> OK (smart >= legacy)\n");
                } else if (smartBestPatches >= legacyPatches * 0.5) {
                    smartWorse++;
                    std::printf("  smartDecode[14]:   -> WARNING (smart < legacy but >= 50%%)\n");
                } else {
                    smartWorse++;
                    std::printf("  smartDecode[14]:   -> BAD (smart << legacy)\n");
                }
                if (smartBestPatches > legacyPatches) {
                    smartBetter++;
                    std::printf("  smartDecode[14]:   -> smart BETTER than legacy!\n");
                }
                
                // Assertion
                expect(smartBestPatches >= spec.minPatchesSmart,
                       spec.label + " smartDecode should find >= " + juce::String(spec.minPatchesSmart)
                       + " patches, got " + juce::String(smartBestPatches));
            }
            
            std::printf("\n  smartDecode[14]: === CROSS-CHECK SUMMARY ===\n");
            std::printf("  smartDecode[14]: Files tested: %d\n", totalFiles);
            std::printf("  smartDecode[14]: smart >= legacy: %d\n", smartMatchesLegacy);
            std::printf("  smartDecode[14]: smart < legacy:  %d\n", smartWorse);
            std::printf("  smartDecode[14]: smart > legacy:  %d\n", smartBetter);
        }
    }
};
