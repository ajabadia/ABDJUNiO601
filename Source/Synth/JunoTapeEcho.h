/*
 * ABD JUNiO 601 - JunoTapeEcho
 * Self-contained tape echo DSP engine (RE-201 inspired)
 * 
 * DESIGN: This entire module can be deleted to remove all delay
 * functionality from the project. Only connected via:
 *   - PluginProcessor (adds params, calls process())
 *   - CMakeLists.txt (adds source files)
 * 
 * To remove: delete this .h/.cpp pair and revert those two files.
 */

#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

// ============================================================================
// JunoTapeEcho - Main delay processor
// ============================================================================
class JunoTapeEcho
{
public:
    JunoTapeEcho() = default;
    ~JunoTapeEcho() = default;

    // --- Lifecycle ---
    void prepare(double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    // --- Processing ---
    void process(juce::AudioBuffer<float>& buffer);

    // --- Parameters (normalized 0..1) ---
    void setEnabled(bool enabled) noexcept          { enabled_ = enabled; }
    void setDelaySetting(int setting) noexcept      { delaySetting_ = juce::jlimit(0, 11, setting); }
    void setRepeatRate(float rate) noexcept         { repeatRate_ = rate; }
    void setIntensity(float intensity) noexcept     { intensity_ = intensity; }
    void setBass(float bass) noexcept               { bass_ = bass; }
    void setTreble(float treble) noexcept           { treble_ = treble; }
    void setReverbVol(float vol) noexcept           { reverbVol_ = vol; }
    void setEchoVol(float vol) noexcept             { echoVol_ = vol; }
    void setEchoCancel(bool cancel) noexcept        { echoCancel_ = cancel; }
    void setSyncEnabled(bool sync) noexcept          { syncEnabled_ = sync; }
    void setSyncDivision(int division) noexcept      { syncDivision_ = juce::jlimit(0, 8, division); }
    void setHostBPM(double bpm) noexcept             { hostBPM_ = juce::jlimit(20.0, 300.0, bpm); }

    // --- Calibration-linked parameters (from CalibrationSettings) ---
    void setInputLevel(float level) noexcept        { inputLevel_ = juce::jlimit(0.0f, 1.0f, level); }
    void setWetDry(float wetDry) noexcept           { wetDry_ = juce::jlimit(0.0f, 1.0f, wetDry); }
    void setReverbType(int type) noexcept           { reverbType_ = juce::jlimit(0, 2, type); }
    void setWowFlutter(float wowFlutter) noexcept   { wowFlutter_ = juce::jlimit(0.0f, 1.0f, wowFlutter); }
    void setReverbDecay(float decay) noexcept       { reverbDecay_ = juce::jlimit(0.0f, 1.0f, decay); }
    void setEchoIsolator(float isolator) noexcept   { echoIsolator_ = juce::jlimit(0.0f, 1.0f, isolator); }

    void setWowRate(float val) noexcept                 { wowRate_ = val; }
    void setFlutterRate(float val) noexcept             { flutterRate_ = val; }
    void setTapeScrapeRate(float val) noexcept          { tapeScrapeRate_ = val; }
    void setWowAmp(float val) noexcept                  { wowAmp_ = val; }
    void setFlutterAmp(float val) noexcept              { flutterAmp_ = val; }
    void setTapeScrapeAmp(float val) noexcept           { tapeScrapeAmp_ = val; }
    void setWowFlutterScale(float val) noexcept         { wowFlutterScale_ = val; }
    void setSaturationInputGain(float val) noexcept     { saturationInputGain_ = val; }
    void setHead2Ratio(float val) noexcept              { head2Ratio_ = val; }
    void setHead3Ratio(float val) noexcept              { head3Ratio_ = val; }
    void setBassFreq(float val) noexcept                { bassFreq_ = val; }
    void setTrebleFreq(float val) noexcept              { trebleFreq_ = val; }
    void setFeedbackLpfBase(float val) noexcept         { feedbackLpfBase_ = val; }
    void setFeedbackLpfRange(float val) noexcept        { feedbackLpfRange_ = val; }
    void setSpringGain(float val) noexcept              { springGain_ = val; }
    void setSpringReflectionScale(float val) noexcept   { springReflectionScale_ = val; }
    void setSchroederLpf(float val) noexcept            { schroederLpf_ = val; }
    void setSchroederGain(float val) noexcept           { schroederGain_ = val; }
    void setSchroederSatDrive(float val) noexcept       { schroederSatDrive_ = val; }

    bool isEnabled() const noexcept                 { return enabled_; }
    int getDelaySetting() const noexcept            { return delaySetting_; }

    // Authentic RE-201 active heads mapping for unit tests & internal use
    static std::array<bool, 3> getActiveHeads(int setting) noexcept
    {
        switch (setting)
        {
            case 0:  return {true,  false, false}; // 1: Echo Head 1
            case 1:  return {false, true,  false}; // 2: Echo Head 2
            case 2:  return {false, false, true};  // 3: Echo Head 3
            case 3:  return {true,  true,  false}; // 4: Echo H1+H2
            case 4:  return {true,  false, false}; // 5: Echo H1 + Rev
            case 5:  return {false, true,  false}; // 6: Echo H2 + Rev
            case 6:  return {false, false, true};  // 7: Echo H3 + Rev
            case 7:  return {true,  true,  false}; // 8: Echo H1+H2 + Rev
            case 8:  return {true,  false, true};  // 9: Echo H1+H3 + Rev
            case 9:  return {false, true,  true};  // 10: Echo H2+H3 + Rev
            case 10: return {true,  true,  true};  // 11: Echo H1+H2+H3 + Rev
            case 11: return {false, false, false}; // 12: Reverb Only
            default: return {true,  false, false};
        }
    }

private:
    // --- Internal types ---

    // Circular delay line with interpolation
    class DelayLine
    {
    public:
        DelayLine() = default;

        void prepare(double sampleRate, int maxDelaySamples)
        {
            sr_ = sampleRate;
            size_ = juce::nextPowerOfTwo(maxDelaySamples + 1024);
            mask_ = size_ - 1;
            buffer_.resize(size_, 0.0f);
            writePos_ = 0;
        }

        void reset()
        {
            std::fill(buffer_.begin(), buffer_.end(), 0.0f);
            writePos_ = 0;
        }

        void write(float sample)
        {
            buffer_[writePos_ & mask_] = sample;
            ++writePos_;
        }

        float read(float delaySamples) const
        {
            if (delaySamples <= 0.0f) return buffer_[(writePos_ - 1) & mask_];
            if (delaySamples >= (float)size_) delaySamples = (float)(size_ - 1);

            float readPos = (float)writePos_ - delaySamples - 1.0f;
            while (readPos < 0.0f) {
                readPos += (float)size_;
            }
            int idx = (int)readPos;
            float frac = readPos - (float)idx;

            // Cubic interpolation
            const float x0 = buffer_[(idx - 1) & mask_];
            const float x1 = buffer_[(idx)     & mask_];
            const float x2 = buffer_[(idx + 1) & mask_];
            const float x3 = buffer_[(idx + 2) & mask_];

            const float a0 = x3 - x2 + x1 - x0;
            const float a1 = x0 - x1 - a0;
            const float a2 = x2 - x0;
            const float a3 = x1;

            return a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
        }

    private:
        std::vector<float> buffer_;
        double sr_ = 44100.0;
        int size_ = 1;
        int mask_ = 0;
        int writePos_ = 0;
    };

    // One-pole lowpass filter
    class OnePoleLPF
    {
    public:
        void setCutoff(float freqHz, float sr)
        {
            float g = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * freqHz / sr);
            g_ = juce::jlimit(0.0f, 1.0f, g);
        }

        float process(float input)
        {
            z_ = input * g_ + z_ * (1.0f - g_);
            return z_;
        }

        void reset() { z_ = 0.0f; }

    private:
        float g_ = 0.0f;
        float z_ = 0.0f;
    };

    // Biquad filter (shelving + LP/HP)
    class Biquad
    {
    public:
        enum Type { LowShelf, HighShelf, LowPass, HighPass };

        void setType(Type t) { type_ = t; }
        
        void setParams(float freq, float q, float gainDb, float sr)
        {
            float w0 = 2.0f * juce::MathConstants<float>::pi * freq / sr;
            float cosW0 = std::cos(w0);

            if (type_ == LowPass)
            {
                float alpha = std::sin(w0) / (2.0f * q);
                a0_ = 1.0f + alpha;
                b0_ = (1.0f - cosW0) * 0.5f;
                b1_ = 1.0f - cosW0;
                b2_ = (1.0f - cosW0) * 0.5f;
                a1_ = -2.0f * cosW0;
                a2_ = 1.0f - alpha;
            }
            else if (type_ == HighPass)
            {
                float alpha = std::sin(w0) / (2.0f * q);
                a0_ = 1.0f + alpha;
                b0_ = (1.0f + cosW0) * 0.5f;
                b1_ = -(1.0f + cosW0);
                b2_ = (1.0f + cosW0) * 0.5f;
                a1_ = -2.0f * cosW0;
                a2_ = 1.0f - alpha;
            }
            else
            {
                float A = std::pow(10.0f, gainDb / 40.0f);
                float alpha = std::sin(w0) / (2.0f * q);

                if (type_ == LowShelf)
                {
                    float beta = 2.0f * std::sqrt(A) * alpha;
                    b0_ = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + beta);
                    b1_ = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
                    b2_ = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - beta);
                    a0_ = (A + 1.0f) + (A - 1.0f) * cosW0 + beta;
                    a1_ = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
                    a2_ = (A + 1.0f) + (A - 1.0f) * cosW0 - beta;
                }
                else // HighShelf
                {
                    float beta = 2.0f * std::sqrt(A) * alpha;
                    b0_ = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + beta);
                    b1_ = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
                    b2_ = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - beta);
                    a0_ = (A + 1.0f) - (A - 1.0f) * cosW0 + beta;
                    a1_ = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
                    a2_ = (A + 1.0f) - (A - 1.0f) * cosW0 - beta;
                }
            }

            // Normalize
            float invA0 = 1.0f / a0_;
            b0_ *= invA0; b1_ *= invA0; b2_ *= invA0;
            a1_ *= invA0; a2_ *= invA0;
        }

        float process(float input)
        {
            float out = b0_ * input + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
            x2_ = x1_; x1_ = input;
            y2_ = y1_; y1_ = out;
            
            // Sanitize
            if (std::isnan(y1_) || std::isinf(y1_)) y1_ = 0.0f;
            return y1_;
        }

        void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }

    private:
        Type type_ = LowShelf;
        float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
        float a0_ = 1.0f, a1_ = 0.0f, a2_ = 0.0f;
        float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
    };

    // Schroeder Allpass Delay
    class AllPassDelay
    {
    public:
        AllPassDelay() = default;

        void prepare(int delaySamples, float gain)
        {
            size_ = delaySamples + 1;
            if (size_ < 2) size_ = 2;
            buffer_.assign(size_, 0.0f);
            writePos_ = 0;
            g_ = gain;
        }

        float process(float x)
        {
            int readPos = writePos_ - (size_ - 1);
            if (readPos < 0) readPos += size_;
            float delayed = buffer_[readPos];
            float y = -g_ * x + delayed;
            buffer_[writePos_] = x + g_ * delayed;
            
            // Sanitize
            if (std::isnan(buffer_[writePos_]) || std::isinf(buffer_[writePos_]))
                buffer_[writePos_] = 0.0f;

            if (++writePos_ >= size_) writePos_ = 0;
            return y;
        }

        void reset()
        {
            std::fill(buffer_.begin(), buffer_.end(), 0.0f);
            writePos_ = 0;
        }

    private:
        std::vector<float> buffer_;
        int size_ = 2;
        int writePos_ = 0;
        float g_ = 0.5f;
    };

    // Waveguide Spring Reverb (8 springs, 4 guides per spring)
    class SpringReverb
    {
    public:
        static constexpr int NUM_SPRINGS = 8;
        static constexpr int GUIDES_PER_SPRING = 4;

        SpringReverb() = default;

        void prepare(double sr)
        {
            sampleRate_ = sr;
            // Xorshift32 for deterministic pseudo-random parameters
            uint32_t state = 777u;
            auto nextRand = [&]() {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return (float)(int32_t)state / 2147483648.0f;
            };

            for (int s = 0; s < NUM_SPRINGS; ++s)
            {
                for (int g = 0; g < GUIDES_PER_SPRING; ++g)
                {
                    int idx = s * GUIDES_PER_SPRING + g;
                    float ms = 30.0f + std::abs(nextRand()) * 20.0f;
                    int delaySamp = (int)(ms * sr / 1000.0f);
                    if (delaySamp < 4) delaySamp = 4;
                    delayTimes_[idx] = (float)delaySamp * 0.9f;

                    uint32_t seed = 1000u + (uint32_t)(s * 100 + g * 10);
                    guides_[idx].prepare(delaySamp + 4, (float)sr, seed);
                }
            }
        }

        void process(float inL, float inR, float& outL, float& outR, float feedbackGain = 0.15f, float reflectionScale = 0.25f)
        {
            float sumL = 0.0f, sumR = 0.0f;

            for (int s = 0; s < NUM_SPRINGS; ++s)
            {
                float input = (s < NUM_SPRINGS / 2) ? inL : inR;
                float springOut = 0.0f;

                for (int g = 0; g < GUIDES_PER_SPRING; ++g)
                {
                    int idx = s * GUIDES_PER_SPRING + g;
                    springOut += guides_[idx].process(input, delayTimes_[idx], feedbackGain) * reflectionScale;
                }

                if (s < NUM_SPRINGS / 2)
                    sumL += springOut;
                else
                    sumR += springOut;
            }

            outL = sumL;
            outR = sumR;
        }

        void reset()
        {
            for (int i = 0; i < NUM_SPRINGS * GUIDES_PER_SPRING; ++i)
                guides_[i].reset();
        }

    private:
        struct WaveguideUnit
        {
            DelayLine delay_;
            AllPassDelay ap_[5];
            Biquad lpf_;
            Biquad hpf_;

            void prepare(int delaySamples, float sr, uint32_t seed)
            {
                delay_.prepare(sr, delaySamples);

                uint32_t state = seed;
                auto nextRand = [&]() {
                    state ^= state << 13;
                    state ^= state >> 17;
                    state ^= state << 5;
                    return (float)(int32_t)state / 2147483648.0f;
                };

                for (int i = 0; i < 5; ++i)
                {
                    int apDelay = 3 + (int)(std::abs(nextRand()) * 30.0f);
                    float apGain = 0.2f + std::abs(nextRand()) * 0.25f; // 0.2-0.45
                    ap_[i].prepare(apDelay, apGain);
                }

                lpf_.setType(Biquad::LowPass);
                lpf_.setParams(6000.0f, 0.707f, 0.0f, sr);

                hpf_.setType(Biquad::HighPass);
                hpf_.setParams(120.0f, 0.707f, 0.0f, sr);
            }

            float process(float x, float delaySamp, float feedbackGain = 0.15f)
            {
                float d = delay_.read(delaySamp);
                float y = d;
                for (int i = 0; i < 5; ++i)
                    y = ap_[i].process(y);

                y = lpf_.process(y);
                y = hpf_.process(y);
                
                if (std::isnan(y) || std::isinf(y)) y = 0.0f;
                y = juce::jlimit(-1.0f, 1.0f, y);

                delay_.write(x + y * feedbackGain);
                return y;
            }

            void reset()
            {
                delay_.reset();
                for (int i = 0; i < 5; ++i) ap_[i].reset();
                lpf_.reset();
                hpf_.reset();
            }
        };

        WaveguideUnit guides_[NUM_SPRINGS * GUIDES_PER_SPRING];
        float delayTimes_[NUM_SPRINGS * GUIDES_PER_SPRING];
        double sampleRate_ = 44100.0;
    };

    // Simple noise generator for wow/flutter
    class NoiseGen
    {
    public:
        NoiseGen() : rng_(12345) {}
        
        float next()
        {
            // White noise via linear congruential
            rng_ = (rng_ * 1664525 + 1013904223) & 0xFFFFFFFF;
            return (float)(int)rng_ * 4.6566129e-10f; // convert to [-1, 1]
        }

        void seed(uint32_t s) { rng_ = s; }

    private:
        uint32_t rng_;
    };

    // --- Member variables ---
    bool enabled_ = false;
    int delaySetting_ = 11;    // 0-11, maps to RE-201 preset positions (default=11 REV ONLY)
    float repeatRate_ = 0.5f;
    float intensity_ = 0.5f;
    float bass_ = 0.5f;
    float treble_ = 0.5f;
    float reverbVol_ = 0.5f;
    float echoVol_ = 0.5f;
    bool echoCancel_ = false;
    bool syncEnabled_ = false;
    int syncDivision_ = 3;      // 0-8 (kDivBeats index, default 3 = 1/4 note)
    double hostBPM_ = 120.0;

    // Calibration-linked params
    float inputLevel_ = 0.8f;
    float wetDry_ = 0.5f;
    int reverbType_ = 0;
    float wowFlutter_ = 0.5f;
    float reverbDecay_ = 0.5f;
    float echoIsolator_ = 0.5f;

    float wowRate_ = 0.5f;
    float flutterRate_ = 8.0f;
    float tapeScrapeRate_ = 12.0f;
    float wowAmp_ = 0.003f;
    float flutterAmp_ = 0.001f;
    float tapeScrapeAmp_ = 0.0005f;
    float wowFlutterScale_ = 2.0f;
    float saturationInputGain_ = 1.5f;
    float head2Ratio_ = 2.0f;
    float head3Ratio_ = 3.0f;
    float bassFreq_ = 300.0f;
    float trebleFreq_ = 3000.0f;
    float feedbackLpfBase_ = 5000.0f;
    float feedbackLpfRange_ = 10000.0f;
    float springGain_ = 3.0f;
    float springReflectionScale_ = 0.25f;
    float schroederLpf_ = 8000.0f;
    float schroederGain_ = 1.5f;
    float schroederSatDrive_ = 1.5f;

    double sampleRate_ = 44100.0;
    int numChannels_ = 2;

    // Delay line - mono sum for feedback, stereo output
    DelayLine delayLine_;
    
    // Per-channel filters
    std::vector<OnePoleLPF> feedbackLPFs_;
    std::vector<Biquad> bassFilters_;
    std::vector<Biquad> trebleFilters_;
    
    // Reverb implementations:
    // 1) Waveguide Spring Reverb
    SpringReverb springReverb_;
    // 2) Original Schroeder-Moorer spring model
    std::vector<DelayLine> reverbDelays_;
    std::vector<OnePoleLPF> reverbLPFs_;

    // Wow/flutter
    NoiseGen noiseGen_;
    float wowPhase_ = 0.0f;
    float flutterPhase_ = 0.0f;
    float dirtPhase_ = 0.0f;

    // Tape saturation state
    float satState_ = 0.0f;

    // Head delay multipliers (RE-201 ratios)
    static constexpr float kHeadRatios[3] = { 1.0f, 2.0f, 3.0f };

    // Map delay setting (0-11) to active heads + reverb enable
    struct PresetConfig {
        std::array<bool, 3> heads;   // H1, H2, H3 active
        bool reverbOn;               // spring reverb enabled
    };
    PresetConfig getPresetConfig(int setting) const;
};

// ============================================================================
// Inline implementation of helper
// ============================================================================
inline JunoTapeEcho::PresetConfig JunoTapeEcho::getPresetConfig(int setting) const
{
    // Authentic RE-201 presets configuration:
    //  0-3: Echo only (1: H1, 2: H2, 3: H3, 4: H1+H2)
    //  4-10: Echo + Reverb (5: H1+R, 6: H2+R, 7: H3+R, 8: H1+H2+R, 9: H1+H3+R, 10: H2+H3+R, 11: H1+H2+H3+R)
    //  11: Reverb Only
    switch (setting)
    {
        case 0:  return {{true,  false, false}, false}; // 1: Echo Head 1
        case 1:  return {{false, true,  false}, false}; // 2: Echo Head 2
        case 2:  return {{false, false, true},  false}; // 3: Echo Head 3
        case 3:  return {{true,  true,  false}, false}; // 4: Echo H1+H2
        case 4:  return {{true,  false, false}, true};  // 5: Echo H1 + Rev
        case 5:  return {{false, true,  false}, true};  // 6: Echo H2 + Rev
        case 6:  return {{false, false, true},  true};  // 7: Echo H3 + Rev
        case 7:  return {{true,  true,  false}, true};  // 8: Echo H1+H2 + Rev
        case 8:  return {{true,  false, true},  true};  // 9: Echo H1+H3 + Rev
        case 9:  return {{false, true,  true},  true};  // 10: Echo H2+H3 + Rev
        case 10: return {{true,  true,  true},  true};  // 11: Echo H1+H2+H3 + Rev
        case 11: return {{false, false, false}, true};  // 12: Reverb Only
        default: return {{true,  false, false}, false};
    }
}
