#pragma once
#include <JuceHeader.h>
#include <vector>
#include <set>
#include <cmath>

class JunoTapeDecoder {
public:
    struct DecodeResult {
        bool success = false;
        std::vector<uint8_t> data;
        int detectedBaudRate = 0; // 0=unknown, 340=Juno-60, 1200=Juno-106
        juce::String errorMessage;
    };

    static inline std::vector<uint8_t> validatePatches(const std::vector<uint8_t>& decodedBytes)
    {
        std::vector<uint8_t> validatedPatches;
        for (size_t i = 0; i < decodedBytes.size(); ++i) {
            // Juno patches are 18 bytes + 1 checksum byte.
            if (i + 19 <= decodedBytes.size()) {
                uint8_t checksum = 0;
                for (int j = 0; j < 18; ++j) {
                    checksum += decodedBytes[i + j];
                }
                checksum &= 0x7F;

                if (checksum == decodedBytes[i + 18]) {
                    validatedPatches.insert(validatedPatches.end(), decodedBytes.begin() + i, decodedBytes.begin() + i + 18);
                    i += 18; // Skip the data and checksum we just processed
                }
            }
        }
        return validatedPatches;
    }

    // --- DCB Format Corrector ---
    // Applies format-specific constraints to switch bytes (SW1, SW2) to fix
    // bit errors introduced by FSK decoding of noisy 8-bit tape recordings.
    //
    // Key insight: From analysis of decoded tape patches, ~98% have invalid bits
    // in SW2 reserved positions (set to 1 when hardware spec requires 0).
    // The corrector uses these hardware-verified constraints to fix known errors.
    //
    // Juno-60 SW2: bits 2,5,6,7 reserved → must be 0
    // Juno-106 SW2: bits 5,6,7 reserved → must be 0
    // Juno-106 SW1: bit 7 reserved → must be 0
    // Juno-60 SW1 bits 0-2 (range): mutually exclusive (only one range active)
    //
    // Returns corrected patches (same count as input).
    // Patches with already-valid bits pass through unchanged.
    static inline std::vector<uint8_t> correctDcbFormat(const std::vector<uint8_t>& patches, int formatBaudRate)
    {
        if (patches.empty() || patches.size() % 18 != 0)
            return patches;
        
        bool isJuno60 = (formatBaudRate == 340);
        std::vector<uint8_t> corrected;
        corrected.reserve(patches.size());
        
        // SW2 reserved bit masks:
        // Juno-60: reserved bits are 2,5,6,7 → keep only bits 0,1,3,4
        //   mask = (1<<0)|(1<<1)|(1<<3)|(1<<4) = 1+2+8+16 = 0x1B
        // Juno-106: reserved bits are 5,6,7 → keep only bits 0-4
        //   mask = (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4) = 1+2+4+8+16 = 0x1F
        const uint8_t sw2Mask = isJuno60 ? 0x1B : 0x1F;
        
        for (size_t i = 0; i < patches.size(); i += 18) {
            std::vector<uint8_t> patch(patches.begin() + i, patches.begin() + i + 18);
            uint8_t& sw1 = patch[16];
            uint8_t& sw2 = patch[17];
            
            // Step 1: Clear SW2 reserved bits (unconditionally safe per hardware spec)
            sw2 &= sw2Mask;
            
            // Step 2: For Juno-106, clear SW1 bit 7 (reserved per hardware spec)
            if (!isJuno60)
                sw1 &= 0x7F;
            
            // Step 3: Fix SW1 range bits (bits 0-2) for BOTH formats
            // Both Juno-60 and Juno-106 have a single mechanical range switch —
            // only ONE range can be active at a time (16', 8', or 4').
            // If zero or multiple range bits are set, default to 8' (most common).
            {
                int rangeBits = sw1 & 0x07;
                if (rangeBits == 0 || (rangeBits & (rangeBits - 1)) != 0) {
                    // No range or multiple ranges → reset to 8' (bit 1)
                    sw1 = (sw1 & 0xF8) | (1 << 1);
                }
            }
            
            corrected.insert(corrected.end(), patch.begin(), patch.end());
        }
        
        return corrected;
    }

    // --- Goertzel-based FSK Bit Detection ---
    // Goertzel is a single-frequency DFT that works reliably even with very short
    // sample windows (~10 samples). Much more robust than zero-crossing counting
    // for high baud rates like Juno-106's 1200 baud (~37 samples/bit).
    
    struct GoertzelState {
        double s1, s2, coeff;
    };
    
    static inline GoertzelState initGoertzel(double targetFreq, double sr) {
        double omega = 2.0 * juce::MathConstants<double>::pi * targetFreq / sr;
        return { 0.0, 0.0, 2.0 * std::cos(omega) };
    }
    
    static inline void goertzelProcess(GoertzelState& st, double x) {
        double s0 = x + st.coeff * st.s1 - st.s2;
        st.s2 = st.s1;
        st.s1 = s0;
    }
    
    static inline double goertzelPower(const GoertzelState& st) {
        return st.s1 * st.s1 + st.s2 * st.s2 - st.coeff * st.s1 * st.s2;
    }
    
    // Detect bit using dual Goertzel: compare energy at space freq vs mark freq.
    // Returns 0 for space, 1 for mark.
    static inline int goertzelDetectBit(const float* samples, int start, int length,
                                        const GoertzelState& gsInit, const GoertzelState& gmInit)
    {
        if (length < 4) return 0;
        
        GoertzelState gs = gsInit;
        GoertzelState gm = gmInit;
        
        for (int i = start; i < start + length; ++i) {
            double x = (double)samples[i];
            goertzelProcess(gs, x);
            goertzelProcess(gm, x);
        }
        
        double powerSpace = goertzelPower(gs);
        double powerMark  = goertzelPower(gm);
        
        // Normalize by window length for fair comparison across sub-windows
        double norm = (double)length * (double)length;
        return (powerMark / norm > powerSpace / norm) ? 1 : 0;
    }
    
    // Detect tape format by analyzing the leader tone (preamble) frequency.
    // Juno-60 leader tone: ~2380 Hz marks
    // Juno-106 leader tone: ~2100 Hz marks
    // Returns: 0 = unknown/no signal, 340 = Juno-60, 1200 = Juno-106
    static inline int detectFormatFromLeaderTone(const float* samples, int numSamples, double sr)
    {
        // Analyze a window of up to 3 seconds, but we'll restrict the Goertzel
        // analysis to the first ~1.0s after onset to avoid counting data bytes
        // that follow the leader tone.
        int totalWindow = std::min(numSamples, (int)(sr * 3.0));
        if (totalWindow < (int)(sr * 0.5)) return 0;
        
        // Find where the leader tone actually starts (skip leading silence) by scanning
        // for the first region where envelope exceeds 5% of the global peak
        float globalPeak = 0.0f;
        for (int i = 0; i < totalWindow; ++i)
            globalPeak = std::max(globalPeak, std::abs(samples[i]));
        if (globalPeak < 0.001f) return 0;
        
        float onsetThreshold = globalPeak * 0.05f;
        int leaderStart = 0;
        while (leaderStart < totalWindow) {
            float localPeak = 0.0f;
            for (int j = leaderStart; j < std::min(totalWindow, leaderStart + (int)(sr * 0.1)); ++j)
                localPeak = std::max(localPeak, std::abs(samples[j]));
            if (localPeak > onsetThreshold) break;
            leaderStart += (int)(sr * 0.1);
        }
        
        if (leaderStart >= totalWindow) return 0;
        
        // IMPORTANT: Restrict analysis to ~0.5 second after onset to capture
        // only the pure leader tone and avoid including data bytes that follow.
        // The leader tone is a continuous stream of marks (constant frequency)
        // that typically lasts 0.5-1.0 seconds. After the leader, data bytes
        // begin with alternating space (1300/1360 Hz) and mark (2100/2380 Hz)
        // frequencies. Including data in the analysis window can bias detection,
        // especially on short recordings where the leader-to-data ratio is small.
        int leaderLen = std::min(totalWindow, leaderStart + (int)(sr * 0.5));
        int leaderSegmentLen = leaderLen - leaderStart;
        if (leaderSegmentLen < (int)(sr * 0.2)) return 0;  // Need at least 200ms of leader
        
        // Use dual Goertzel to compare energy at Juno-106 mark (2100 Hz) vs Juno-60 mark (2380 Hz).
        // Goertzel acts as a narrow bandpass filter, rejecting noise outside each target frequency.
        // Much more robust than zero-crossing counting for noisy recordings.
        GoertzelState gs106 = initGoertzel(2100.0, sr);  // Juno-106 mark frequency
        GoertzelState gs60  = initGoertzel(2380.0, sr);  // Juno-60 mark frequency
        
        for (int i = leaderStart; i < leaderLen; ++i) {
            double x = (double)samples[i];
            goertzelProcess(gs106, x);
            goertzelProcess(gs60,  x);
        }
        
        double power106 = goertzelPower(gs106);
        double power60  = goertzelPower(gs60);
        
        // Normalize by segment length squared
        double norm = (double)leaderSegmentLen * (double)leaderSegmentLen;
        double energy106 = power106 / norm;
        double energy60  = power60  / norm;
        
        // Require minimum energy to avoid false detection on silence/noise
        constexpr double kMinEnergy = 1e-8;
        if (energy106 < kMinEnergy && energy60 < kMinEnergy)
            return 0;
        
        // Ratio check: require the winner to have at least 2x the energy of the loser
        // to avoid ambiguous detections
        if (energy60 > energy106 * 2.0)
            return 340;  // Juno-60 (2380 Hz dominant)
        else if (energy106 > energy60 * 2.0)
            return 1200; // Juno-106 (2100 Hz dominant)
        else
            return 0;    // Too close to call
    }
    
    // FSK demodulator using Goertzel-based bit detection with majority voting.
    // Handles both Juno-60 (340 baud) and Juno-106 (1200 baud).
    // narrowSweep: if true, uses a tighter speed range (0.93-1.07 step 0.02 = 8 factors x 5 phases = 40 combos)
    // for faster decoding on good-quality tapes. Default (false) sweeps 0.86-1.14 step 0.01 (29 x 5 = 145 combos)
    // to handle heavy wow/flutter on degraded tapes.
    static inline std::vector<uint8_t> decodeFSK(const float* samples, int numSamples, double sr, double bitsPerSecond, bool narrowSweep = false)
    {
        const double nominalSamplesPerBit = sr / bitsPerSecond;
        
        // Frequencies for the specific format
        const double freqSpace = (bitsPerSecond < 600.0) ? 1360.0 : 1300.0;  // space (0)
        const double freqMark  = (bitsPerSecond < 600.0) ? 2380.0 : 2100.0;  // mark (1)
        
        // Pre-initialize Goertzel states for both frequencies (avoids re-init per bit)
        GoertzelState gsTemplate = initGoertzel(freqSpace, sr);
        GoertzelState gmTemplate = initGoertzel(freqMark, sr);


        // Speed factor sweep — wider for real cassette wow/flutter, narrower for fast/clean
        std::vector<double> speedFactors;
        if (narrowSweep) {
            for (double s = 0.93; s <= 1.07; s += 0.02)
                speedFactors.push_back(s);
        } else {
            for (double s = 0.86; s <= 1.14; s += 0.01)
                speedFactors.push_back(s);
        }
        
        // Phase offset sweep
        std::vector<double> phaseOffsets;
        for (double p = 0.0; p < 1.0; p += 0.2) {
            phaseOffsets.push_back(p);
        }
        
        std::vector<uint8_t> bestDecodedBytes;
        size_t maxValidPatches = 0;
        size_t bestRawByteCount = 0;
        
        for (double speedFactor : speedFactors) {
            double spb = nominalSamplesPerBit * speedFactor;
            
            for (double phaseOffset : phaseOffsets) {
                // Generate bit stream using fractional sample accumulation
                std::vector<uint8_t> bits;
                double samplePos = phaseOffset * spb;
                
                while ((int)(samplePos + spb) < numSamples) {
                    int bitStart = (int)samplePos;
                    int bitEnd = (int)(samplePos + spb);
                    int bitLen = bitEnd - bitStart;
                    if (bitLen < 4) { samplePos += spb; continue; }
                    
                    // Goertzel majority voting: sub-windows compare space vs mark energy
                    // At 1200 baud:  ~37 samples/bit → 1 sub-window of ~37 samples
                    //   Goertzel bandwidth: 44100/37 ≈ 1192 Hz (enough for 1060 Hz separation)
                    // At 340 baud:  ~130 samples/bit → 2 sub-windows of ~65 samples each
                    //   Goertzel bandwidth: 44100/65 ≈ 678 Hz (excellent for 1020 Hz separation)
                    // Smaller windows (e.g. 32 samples, bandwidth 1378 Hz) allow the space
                    // frequency (1360 Hz) to leak into the mark filter (2380 Hz) and vice versa,
                    // causing bit errors on clean synthetic signals. Larger windows keep the
                    // filters narrow enough to cleanly separate the two FSK frequencies.
                    constexpr int kMinSubWindowSamples = 65;
                    int numSubWindows = std::max(1, std::min(4, bitLen / kMinSubWindowSamples));
                    int subLen = bitLen / numSubWindows;
                    int voteMark = 0;
                    for (int sub = 0; sub < numSubWindows; ++sub) {
                        int subStart = bitStart + sub * subLen;
                        int subEnd = (sub < numSubWindows - 1) ? (subStart + subLen) : bitEnd;
                        int subLength = subEnd - subStart;
                        if (subLength < 4) continue;
                        
                        if (goertzelDetectBit(samples, subStart, subLength, gsTemplate, gmTemplate))
                            voteMark++;
                    }
                    
                    // Bit = mark if majority of sub-windows voted mark
                    bits.push_back((voteMark > numSubWindows / 2) ? (uint8_t)1 : (uint8_t)0);
                    samplePos += spb;
                }
                
                // Extract bytes from bit stream
                // Try multiple bit stream offsets
                for (int offset = 0; offset < 4; ++offset) {
                    std::vector<uint8_t> bytes;
                    int i = offset;
                    
                    while (i + 10 < (int)bits.size()) {
                        // Look for start bit transition: 1 (idle/mark) followed by 0 (space)
                        // i points to the last mark before the start bit (or the stop bit of the previous byte)
                        if (bits[i] == 1 && bits[i + 1] == 0) {
                            uint8_t byte = 0;
                            bool validFrame = true;
                            
                            // Read 8 data bits (LSB first).
                            // Data bits start at i+2 (skip the start bit at i+1).
                            for (int b = 0; b < 8; ++b) {
                                int idx = i + 2 + b;
                                if (idx < (int)bits.size()) {
                                    if (bits[idx] == 1)
                                        byte |= (uint8_t)(1 << b);
                                } else {
                                    validFrame = false;
                                    break;
                                }
                            }
                            
                            // Check stop bit (should be mark/1). Stop bit is at i+10 (skip mark+start+8data).
                            int stopIdx = i + 2 + 8;
                            if (validFrame && stopIdx < (int)bits.size() && bits[stopIdx] == 1) {
                                bytes.push_back(byte & 0x7F);
                                // Set i to stopIdx-1 so the loop's i++ makes i == stopIdx.
                                // Next iteration finds the transition from this stop bit (1)
                                // to the next byte's start bit (0).
                                i = stopIdx - 1;
                            }
                        }
                        i++;
                    }
                    
                    std::vector<uint8_t> validated = validatePatches(bytes);
                    size_t numPatches = validated.size() / 18;
                    
                    // Capture: (a) the first attempt with any raw bytes, then (b) only overwrite if
                    // this attempt produces more validated patches than the current best.
                    // This ensures we don't lose a good result to a later (worse) one.
                    bool needsUpdate = false;
                    if (bestRawByteCount == 0 && !bytes.empty()) {
                        needsUpdate = true; // First attempt with any data
                    } else if (numPatches > maxValidPatches) {
                        needsUpdate = true; // More patches than current best
                    }
                    
                    if (needsUpdate) {
                        if (bestRawByteCount == 0 && !bytes.empty())
                            bestRawByteCount = bytes.size();
                        maxValidPatches = numPatches;
                        bestDecodedBytes = std::move(bytes);
                        if (numPatches >= 56) break;
                    }
                }
                
                if (maxValidPatches >= 56) break;
            }
            if (maxValidPatches >= 56) break;
        }
        
        return bestDecodedBytes;
    }

    // Quick format detection from a WAV file using leader tone analysis.
    // Only reads the first 3 seconds — much faster than full decode.
    // Returns: 0 = unknown, 340 = Juno-60, 1200 = Juno-106
    static inline int quickDetectFormat(const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr) return 0;
        
        // Only need first 3 seconds for leader tone analysis
        int numSamples = (int)std::min(reader->lengthInSamples, (int64)(reader->sampleRate * 3.0));
        if (numSamples < (int)(reader->sampleRate * 0.5)) return 0;
        
        juce::AudioBuffer<float> buffer((int)reader->numChannels, numSamples);
        reader->read(&buffer, 0, numSamples, 0, true, true);
        
        // Mix to mono
        if (buffer.getNumChannels() > 1) {
            buffer.addFrom(0, 0, buffer, 1, 0, numSamples);
            buffer.applyGain(0.5f);
        }
        
        // DC removal
        float* samples = buffer.getWritePointer(0);
        float y_prev = 0.0f, x_prev = 0.0f;
        const float alpha = 0.9943f;
        for (int i = 0; i < numSamples; ++i) {
            float x = samples[i];
            float y = alpha * (y_prev + x - x_prev);
            samples[i] = y;
            y_prev = y;
            x_prev = x;
        }
        
        // Normalize
        float maxPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            maxPeak = std::max(maxPeak, std::abs(samples[i]));
        if (maxPeak > 0.0001f) {
            float invPeak = 1.0f / maxPeak;
            for (int i = 0; i < numSamples; ++i)
                samples[i] *= invPeak;
        }
        
        return detectFormatFromLeaderTone(samples, numSamples, reader->sampleRate);
    }

    // ─── Signal Analysis (SmartTapeReader Phase 1) ──────────────────────
    //
    // Extracts quality metrics from preprocessed tape audio to determine
    // the optimal decoding strategy.
    //
    // Metrics:
    //   1. SNR (dB): Goertzel energy at mark freq vs noise between mark and space
    //   2. Jitter (%): Std dev of leader tone frequency across sliding windows
    //   3. Duration (s): Effective signal length after leader tone
    //   4. Dropouts (%): % of samples below 5% peak in data zone
    //   5. Bandwidth (Hz): Frequency at 95th percentile spectrogram energy
    //   6. DC Bias: Residual DC offset after HPF
    //
    // Quality score (0.0-1.0): weighted composite for strategy selection
    
    struct SignalMetrics {
        double snrDb = 0.0;
        double jitterPct = 0.0;
        double durationS = 0.0;
        double dcBias = 0.0;
        double dropoutPct = 0.0;
        double bandwidthHz = 0.0;
        double qualityScore = 0.0;
        juce::String qualityLabel; // "GOOD", "FAIR", "POOR", "DEGRADED"
        int detectedBaudRate = 0;
        double analysisTimeS = 0.0;
    };
    
    // Estimate SNR in FSK band using Goertzel at mark frequency vs noise.
    static inline double estimateSnr(const float* samples, int leaderStart, int segLen, double sr, double markFreq)
    {
        if (segLen < (int)(sr * 0.1)) return 0.0;
        
        auto gsMark = initGoertzel(markFreq, sr);
        auto gsNoise = initGoertzel(markFreq * 0.82, sr); // ~midway to space
        auto gsOob = initGoertzel(4000.0, sr);
        
        for (int i = leaderStart; i < leaderStart + segLen; ++i) {
            double x = (double)samples[i];
            goertzelProcess(gsMark, x);
            goertzelProcess(gsNoise, x);
            goertzelProcess(gsOob, x);
        }
        
        double signalPower = goertzelPower(gsMark);
        double noiseFloor = std::max(goertzelPower(gsNoise), goertzelPower(gsOob));
        
        if (noiseFloor < 1e-12 || signalPower < noiseFloor) return 0.0;
        return std::min(60.0, std::max(0.0, 10.0 * std::log10(signalPower / noiseFloor)));
    }
    
    // Measure leader tone frequency stability (jitter).
    static inline double measureJitter(const float* samples, int leaderStart, int leaderLen, double sr, double nominalFreq)
    {
        juce::ignoreUnused(nominalFreq);
        int windowSize = (int)(sr * 0.05); // 50ms windows
        int hopSize = windowSize / 2;
        
        int analysisStart = leaderStart + (int)(sr * 0.1); // skip first 100ms
        int analysisEnd = std::min(leaderLen, leaderStart + (int)(sr * 0.5));
        
        if (analysisEnd - analysisStart < windowSize) return 0.0;
        
        std::vector<double> frequencies;
        for (int ws = analysisStart; ws + windowSize <= analysisEnd; ws += hopSize) {
            double peak = 0.0;
            for (int i = ws; i < ws + windowSize; ++i)
                peak = std::max(peak, (double)std::abs(samples[i]));
            if (peak < 0.01) continue;
            
            // Zero-crossing counting
            double gate = peak * 0.1;
            int zc = 0;
            bool lastAbove = samples[ws] > gate;
            for (int i = ws + 1; i < ws + windowSize; ++i) {
                bool above = samples[i] > gate;
                bool below = samples[i] < -gate;
                if (above && !lastAbove) { zc++; lastAbove = true; }
                else if (below && lastAbove) { zc++; lastAbove = false; }
            }
            if (zc > 0)
                frequencies.push_back(zc * sr / (2.0 * windowSize));
        }
        
        if (frequencies.size() < 3) return 0.0;
        
        double mean = 0.0;
        for (auto f : frequencies) mean += f;
        mean /= frequencies.size();
        if (mean < 1.0) return 0.0;
        
        double variance = 0.0;
        for (auto f : frequencies) variance += (f - mean) * (f - mean);
        variance /= frequencies.size();
        
        return std::min(15.0, std::sqrt(variance) / mean * 100.0);
    }
    
    // Measure effective signal duration (leader end to last data).
    static inline double measureDuration(const float* samples, int numSamples, double sr)
    {
        int windowSize = (int)(sr * 0.05);
        int hopSize = windowSize / 4;
        int numWindows = std::max(1, (numSamples - windowSize) / hopSize);
        
        if (numWindows < 10) return (double)numSamples / sr;
        
        double maxEnv = 0.0;
        std::vector<double> envelope(numWindows, 0.0);
        std::vector<double> winTimes(numWindows, 0.0);
        
        for (int wi = 0; wi < numWindows; ++wi) {
            int ws = wi * hopSize;
            int we = std::min(numSamples, ws + windowSize);
            double sumSq = 0.0;
            for (int i = ws; i < we; ++i) sumSq += (double)samples[i] * (double)samples[i];
            double rms = std::sqrt(sumSq / (we - ws));
            envelope[wi] = rms;
            winTimes[wi] = (double)ws / sr;
            maxEnv = std::max(maxEnv, rms);
        }
        
        if (maxEnv < 0.001) return 0.0;
        double threshold = maxEnv * 0.10;
        
        int firstAbove = -1, lastAbove = -1;
        for (int wi = 0; wi < numWindows; ++wi) {
            if (envelope[wi] > threshold) {
                if (firstAbove < 0) firstAbove = wi;
                lastAbove = wi;
            }
        }
        
        if (firstAbove < 0 || lastAbove <= firstAbove) return 0.0;
        return winTimes[lastAbove] - winTimes[firstAbove];
    }
    
    // Measure dropout percentage in data zone.
    static inline double measureDropouts(const float* samples, int numSamples, int leaderStart, double sr)
    {
        int dataStart = std::min(numSamples, leaderStart + (int)(sr * 0.6));
        int dataLen = numSamples - dataStart;
        if (dataLen < (int)(sr * 0.5)) return 0.0;
        
        double peak = 0.0;
        for (int i = dataStart; i < numSamples; ++i)
            peak = std::max(peak, (double)std::abs(samples[i]));
        if (peak < 0.001) return 100.0;
        
        double threshold = peak * 0.05;
        int dropoutCount = 0;
        for (int i = dataStart; i < numSamples; ++i) {
            if (std::abs(samples[i]) < threshold)
                dropoutCount++;
        }
        return (double)dropoutCount * 100.0 / dataLen;
    }
    
    // Compute quality score from metrics (same formula as Python TapeAnalyzer).
    static inline double computeQualityScore(double snrDb, double jitterPct, double dropoutPct, double durationS)
    {
        double snrNorm = std::max(0.0, std::min(1.0, snrDb / 40.0));
        double jitterNorm = std::max(0.0, std::min(1.0, jitterPct / 15.0));
        double dropoutNorm = std::max(0.0, std::min(1.0, dropoutPct / 30.0));
        double durationNorm = std::max(0.0, std::min(1.0, durationS / 30.0));
        
        return 0.35 * snrNorm + 0.30 * (1.0 - jitterNorm) 
             + 0.20 * (1.0 - dropoutNorm) + 0.15 * durationNorm;
    }
    
    // Full signal analysis — SmartTapeReader Phase 1.
    static inline SignalMetrics analyzeSignal(const float* samples, int numSamples, double sr, int forcedBaud = 0)
    {
        double t0 = juce::Time::getMillisecondCounterHiRes();
        SignalMetrics m;
        
        // Detect leader start
        double globalPeak = 0.0;
        int totalWindow = std::min(numSamples, (int)(sr * 3.0));
        for (int i = 0; i < totalWindow; ++i) globalPeak = std::max(globalPeak, (double)std::abs(samples[i]));
        
        int leaderStart = 0;
        if (globalPeak > 0.001) {
            float onsetThreshold = (float)(globalPeak * 0.05);
            while (leaderStart < totalWindow) {
                float localPeak = 0.0f;
                for (int j = leaderStart; j < std::min(totalWindow, leaderStart + (int)(sr * 0.1)); ++j)
                    localPeak = std::max(localPeak, std::abs(samples[j]));
                if (localPeak > onsetThreshold) break;
                leaderStart += (int)(sr * 0.1);
            }
        }
        
        if (leaderStart >= totalWindow) {
            m.analysisTimeS = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            return m;
        }
        
        int leaderLen = std::min(totalWindow, leaderStart + (int)(sr * 0.5));
        int segLen = leaderLen - leaderStart;
        
        // Detect format
        int baud = forcedBaud;
        if (baud == 0) {
            auto gs106 = initGoertzel(2100.0, sr);
            auto gs60 = initGoertzel(2380.0, sr);
            for (int i = leaderStart; i < leaderLen; ++i) {
                double x = (double)samples[i];
                goertzelProcess(gs106, x);
                goertzelProcess(gs60, x);
            }
            double e106 = goertzelPower(gs106) / (double)(segLen * segLen);
            double e60 = goertzelPower(gs60) / (double)(segLen * segLen);
            if (e60 > e106 * 2.0) baud = 340;
            else if (e106 > e60 * 2.0) baud = 1200;
            else baud = 1200; // default
        }
        m.detectedBaudRate = baud;
        
        // 1. SNR
        double markFreq = (baud == 340) ? 2380.0 : 2100.0;
        m.snrDb = estimateSnr(samples, leaderStart, segLen, sr, markFreq);
        
        // 2. Jitter
        m.jitterPct = measureJitter(samples, leaderStart, leaderLen, sr, markFreq);
        
        // 3. Duration
        m.durationS = measureDuration(samples, numSamples, sr);
        
        // 4. DC Bias
        double sum = 0.0;
        for (int i = 0; i < numSamples; ++i) sum += samples[i];
        m.dcBias = std::abs(sum / numSamples);
        
        // 5. Dropouts
        m.dropoutPct = measureDropouts(samples, numSamples, leaderStart, sr);
        
        // 6. Compute quality score
        m.qualityScore = computeQualityScore(m.snrDb, m.jitterPct, m.dropoutPct, m.durationS);
        if (m.qualityScore >= 0.75) m.qualityLabel = "GOOD";
        else if (m.qualityScore >= 0.50) m.qualityLabel = "FAIR";
        else if (m.qualityScore >= 0.25) m.qualityLabel = "POOR";
        else m.qualityLabel = "DEGRADED";
        
        m.analysisTimeS = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        return m;
    }
    
    // ─── Smart Decode (SmartTapeReader Phases 2-5) ─────────────────────
    //
    // Full pipeline: analyze -> select strategy -> decode (fast & BF) -> rank -> decide.
    // Provides progress callbacks for UI log.
    
    struct SmartDecodeResult {
        bool success = false;
        SignalMetrics metrics;
        
        struct DecoderEntry {
            juce::String label;
            std::vector<uint8_t> validated;
            int patchCount = 0;
            double elapsedS = 0.0;
            int rawBytes = 0;
            double rank = 0.0;
            int duplicates = 0;
        };
        
        std::vector<DecoderEntry> decoderResults;
        int winnerIndex = -1;
        bool autoSelected = false;
        juce::String errorMessage;
    };
    
    // Smart decode: runs analysis + strategy selection + decoding + ranking.
    // Progress callback receives messages for the UI log.
    static inline SmartDecodeResult smartDecode(
        const juce::File& file,
        std::function<void(const juce::String&)> progressCallback = nullptr)
    {
        auto log = [&](const juce::String& msg) {
            if (progressCallback) progressCallback(msg);
        };
        
        SmartDecodeResult result;
        
        log("Cargando archivo WAV...");
        
        // Load audio
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(file));
        if (reader == nullptr) {
            result.errorMessage = "Could not read WAV file.";
            return result;
        }
        
        juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);
        
        // Preprocess: mono mix, HPF, normalize, upsample
        log("Preprocesando senal: mono, HPF, normalizar...");
        
        if (buffer.getNumChannels() > 1) {
            buffer.addFrom(0, 0, buffer, 1, 0, buffer.getNumSamples());
            buffer.applyGain(0.5f);
        }
        
        float* samples = buffer.getWritePointer(0);
        int numSamples = buffer.getNumSamples();
        
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
        float peak = buffer.getMagnitude(0, 0, numSamples);
        if (peak > 0.0001f) buffer.applyGain(1.0f / peak);
        else { result.errorMessage = "Signal is silence."; return result; }
        
        // Upsample
        double sr = (double)reader->sampleRate;
        if (sr < 43900.0 && sr > 0.0) {
            constexpr double kTargetSr = 44100.0;
            int upLen = (int)((double)numSamples * kTargetSr / sr + 0.5);
            juce::AudioBuffer<float> upBuf(1, upLen);
            juce::LagrangeInterpolator interp;
            interp.reset();
            interp.process(sr / kTargetSr, samples, upBuf.getWritePointer(0), upLen);
            std::swap(buffer, upBuf);
            samples = buffer.getWritePointer(0);
            numSamples = buffer.getNumSamples();
            sr = kTargetSr;
        }
        
        // Phase 1: Analyze signal
        log("=== Fase 1: Analizando calidad de la senal... ===");
        auto metrics = analyzeSignal(samples, numSamples, sr);
        result.metrics = metrics;
        
            log(juce::String("  Formato: ") + (metrics.detectedBaudRate == 340 ? "Juno-60 (340 baud)" : "Juno-106 (1200 baud)"));
        log("  SNR: " + juce::String(metrics.snrDb, 1) + " dB");
        log("  Jitter: " + juce::String(metrics.jitterPct, 1) + "%");
        log("  Dropouts: " + juce::String(metrics.dropoutPct, 1) + "%");
        log("  Duracion: " + juce::String(metrics.durationS, 1) + "s");
        log("  Calidad: " + metrics.qualityLabel + " (score: " + juce::String(metrics.qualityScore, 3) + ")");
        
        // Phase 2: Select strategy
        int baud = metrics.detectedBaudRate;
        bool useFast = metrics.qualityScore >= 0.75;
        bool useBf  = metrics.qualityScore >= 0.50 && metrics.qualityScore < 0.75;
        
        // Always try both baud rates (like decodeWavFile auto mode) because
        // analyzeSignal() can misdetect the format on real-world recordings.
        std::vector<int> baudRates;
        if (baud == 340 || baud == 1200) {
            baudRates.push_back(baud);
            baudRates.push_back((baud == 340) ? 1200 : 340);
        } else {
            baudRates.push_back(340);
            baudRates.push_back(1200);
        }
        
        juce::String strategyDesc;
        std::vector<std::pair<juce::String, bool>> strategies;
        
        if (useFast && metrics.durationS >= 10.0) {
            strategyDesc = "GOOD: Goertzel Fast (limited sweep, 2 baud rates)";
            strategies.push_back({"fast", true});
        } else if (useBf) {
            strategyDesc = "FAIR: Goertzel Brute-force (2 baud rates)";
            strategies.push_back({"bf", false});
        } else {
            strategyDesc = "POOR/DEGRADED: Multiples decodificadores (2 baud rates)";
            strategies.push_back({"fast", true});
            strategies.push_back({"bf", false});
        }
        
        log("=== Fase 2: Estrategia seleccionada ===");
        log("  Estrategia: " + strategyDesc);
        
        // Phase 3: Decode (try each strategy at both baud rates)
        log("=== Fase 3: Decodificando... ===");
        
        for (auto& strat : strategies) {
            bool fast = strat.second;
            juce::String strategyName = strat.first == "fast" ? "Fast" : "BF";
            
            for (int br : baudRates) {
                juce::String label = "Goertzel " + strategyName + " @ " + juce::String(br) + " baud";
                
                log("  [" + label + "] Decodificando...");
                
                double t0 = juce::Time::getMillisecondCounterHiRes();
                
                std::vector<uint8_t> decodedBytes;
                if (fast) {
                    // Fast strategy: narrow sweep (40 combos, 0.93-1.07 step 0.02)
                    decodedBytes = decodeFSK(samples, numSamples, sr, (double)br, true);
                } else {
                    // Full brute-force: wide sweep (145 combos, 0.86-1.14 step 0.01)
                    decodedBytes = decodeFSK(samples, numSamples, sr, (double)br, false);
                }
                
                // Validate and correct using the correct format for this baud rate
                auto validated = validatePatches(decodedBytes);
                validated = correctDcbFormat(validated, br);
                
                double elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
                int patchCount = (int)validated.size() / 18;
                
                log("    -> " + label + ": " + juce::String(patchCount) + " patches " 
                    + "(" + juce::String(decodedBytes.size()) + " bytes raw, " 
                    + juce::String(elapsed, 1) + "s)");
                
                SmartDecodeResult::DecoderEntry entry;
                entry.label = label;
                entry.validated = std::move(validated);
                entry.patchCount = patchCount;
                entry.elapsedS = elapsed;
                entry.rawBytes = (int)decodedBytes.size();
                result.decoderResults.push_back(std::move(entry));
            }
        }
        
        // Phase 4: Rank results
        log("=== Fase 4: Ranking resultados... ===");
        
        for (auto& entry : result.decoderResults) {
            // Count duplicates
            int duplicates = 0;
            std::set<std::vector<uint8_t>> seen;
            for (size_t i = 0; i + 18 <= entry.validated.size(); i += 18) {
                std::vector<uint8_t> patch(entry.validated.begin() + i, entry.validated.begin() + i + 18);
                if (seen.count(patch)) duplicates++;
                else seen.insert(patch);
            }
            entry.duplicates = duplicates;
            
            // Compute rank
            int rawBytes = entry.rawBytes;
            int maxPatchesFromRaw = (rawBytes >= 19) ? rawBytes / 19 : 0;
            double checksumRate = (maxPatchesFromRaw > 0) ? (double)entry.patchCount / maxPatchesFromRaw : 0.0;
            
            entry.rank = entry.patchCount * 1000.0 + checksumRate * 100.0 - duplicates * 500.0;
        }
        
        // Sort by rank
        std::sort(result.decoderResults.begin(), result.decoderResults.end(),
            [](const auto& a, const auto& b) { return a.rank > b.rank; });
        
        {
            juce::String header = juce::String("  ") + juce::String("Decoder");
            header = header + juce::String().paddedRight((juce_wchar)' ', 30);
            header = header + "Patches  Rank";
            log(header);
        }
        for (auto& entry : result.decoderResults) {
            juce::String line = juce::String("  ");
            line += entry.label.paddedRight((juce_wchar)' ', 30);
            line += juce::String(entry.patchCount).paddedRight((juce_wchar)' ', 8);
            line += juce::String(entry.rank, 1);
            log(line);
        }
        
        // Phase 5: Decide
        log("=== Fase 5: Decision final ===");
        
        if (!result.decoderResults.empty()) {
            result.winnerIndex = 0;
            bool autoSelect = true;
            
            if (result.decoderResults.size() >= 2) {
                double ratio = result.decoderResults[0].rank / std::max(result.decoderResults[1].rank, 1.0);
                autoSelect = ratio >= 1.20;
            }
            
            result.autoSelected = autoSelect;
            result.success = result.decoderResults[0].patchCount > 0;
            
            if (autoSelect) {
                log("  [AUTO] Seleccionado: " + result.decoderResults[0].label 
                    + " (" + juce::String(result.decoderResults[0].patchCount) + " patches)");
            } else {
                log("  [OPCIONES] Multiples resultados disponibles:");
                for (size_t i = 0; i < result.decoderResults.size(); ++i) {
                    auto& e = result.decoderResults[i];
                    log("    " + juce::String((int)(i+1)) + ". " + e.label 
                        + ": " + juce::String(e.patchCount) + " patches (rank=" + juce::String(e.rank, 0) + ")");
                }
            }
        } else {
            result.errorMessage = "No se encontraron patches en ninguna estrategia.";
        }
        
        log("=== COMPLETADO ===");
        return result;
    }

    // ─── Original decodeWavFile ────────────────────────────────────────
    
    static inline DecodeResult decodeWavFile(const juce::File& file, int forcedBaudRate = 0)
    {
        DecodeResult result;
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr) {
            result.errorMessage = "Could not read WAV file: " + file.getFileName();
            return result;
        }
        
        juce::AudioBuffer<float> buffer(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);

        // --- Signal Pre-processing for Robustness ---
        if (buffer.getNumChannels() > 1) {
            buffer.addFrom(0, 0, buffer, 1, 0, buffer.getNumSamples());
            buffer.applyGain(0.5f);
        }

        float* samples = buffer.getWritePointer(0);
        const int numSamples = buffer.getNumSamples();

        // 1-pole HPF to remove DC offset/drift
        float y_prev = 0.0f;
        float x_prev = 0.0f;
        const float alpha = 0.9943f;
        for (int i = 0; i < numSamples; ++i) {
            float x = samples[i];
            float y = alpha * (y_prev + x - x_prev);
            samples[i] = y;
            y_prev = y;
            x_prev = x;
        }
        
        // Normalize
        float maxPeak = buffer.getMagnitude(0, 0, numSamples);
        if (maxPeak > 0.0001f) {
            buffer.applyGain(1.0f / maxPeak);
        } else {
            result.errorMessage = "Signal is silence or too quiet after DC removal.";
            return result;
        }

        // --- Upsample to 44100 Hz (if needed) ---
        double newSampleRate = (double)reader->sampleRate;
        if (reader->sampleRate < 43900.0 && reader->sampleRate > 0.0) {
            constexpr double kTargetSr = 44100.0;
            int upsampledLen = (int)((double)numSamples * kTargetSr / (double)reader->sampleRate + 0.5);
            
            juce::AudioBuffer<float> upsampledBuffer(1, upsampledLen);
            float* upsampledData = upsampledBuffer.getWritePointer(0);
            
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            double ratio = (double)reader->sampleRate / kTargetSr;
            interpolator.process(ratio, samples, upsampledData, upsampledLen);
            
            std::swap(buffer, upsampledBuffer);
            samples = buffer.getWritePointer(0);
            newSampleRate = kTargetSr;
        }
        const int finalNumSamples = buffer.getNumSamples();

        // --- FSK Decoding ---
        std::vector<uint8_t> bestDecoded;
        size_t bestCount = 0;
        int bestBaudRate = 0;
        
        auto tryFormat = [&](int baud) {
            if (baud != 340 && baud != 1200) return;
            auto bytes = decodeFSK(samples, finalNumSamples, newSampleRate, (double)baud);
            auto validated = validatePatches(bytes);
            validated = correctDcbFormat(validated, baud);
            size_t count = validated.size() / 18;
            if (count > bestCount) {
                bestCount = count;
                bestDecoded = std::move(validated);
                bestBaudRate = baud;
            }
        };
        
        if (forcedBaudRate != 0) {
            if (forcedBaudRate == 340 || forcedBaudRate == 1200)
                tryFormat(forcedBaudRate);
        } else {
            int hintBaudRate = detectFormatFromLeaderTone(samples, finalNumSamples, newSampleRate);
            if (hintBaudRate == 340 || hintBaudRate == 1200) {
                tryFormat(hintBaudRate);
                tryFormat((hintBaudRate == 340) ? 1200 : 340);
            } else {
                tryFormat(340);
                tryFormat(1200);
            }
        }
        
        if (bestCount > 0) {
            result.data = std::move(bestDecoded);
            result.detectedBaudRate = bestBaudRate;
            result.success = true;
        } else {
            result.errorMessage = "Decoded audio, but no valid Juno tape data blocks were found.";
        }
        
        return result;
    }
};
