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
    void setDelaySetting(int setting) noexcept      { delaySetting_ = juce::jlimit(0, 10, setting); }
    void setRepeatRate(float rate) noexcept         { repeatRate_ = rate; }
    void setIntensity(float intensity) noexcept     { intensity_ = intensity; }
    void setBass(float bass) noexcept               { bass_ = bass; }
    void setTreble(float treble) noexcept           { treble_ = treble; }
    void setReverbVol(float vol) noexcept           { reverbVol_ = vol; }
    void setEchoVol(float vol) noexcept             { echoVol_ = vol; }

    bool isEnabled() const noexcept                 { return enabled_; }
    int getDelaySetting() const noexcept            { return delaySetting_; }

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

    // Biquad filter (shelving)
    class Biquad
    {
    public:
        enum Type { LowShelf, HighShelf };

        void setType(Type t) { type_ = t; }
        
        void setParams(float freq, float q, float gainDb, float sr)
        {
            float A = std::pow(10.0f, gainDb / 40.0f);
            float w0 = 2.0f * juce::MathConstants<float>::pi * freq / sr;
            float alpha = std::sin(w0) / (2.0f * q);

            float cosW0 = std::cos(w0);

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
            return out;
        }

        void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }

    private:
        Type type_ = LowShelf;
        float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
        float a0_ = 1.0f, a1_ = 0.0f, a2_ = 0.0f;
        float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
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
    int delaySetting_ = 0;     // 0-10, maps to head combos
    float repeatRate_ = 0.5f;
    float intensity_ = 0.5f;
    float bass_ = 0.5f;
    float treble_ = 0.5f;
    float reverbVol_ = 0.5f;
    float echoVol_ = 0.5f;

    double sampleRate_ = 44100.0;
    int numChannels_ = 2;

    // Delay line - mono sum for feedback, stereo output
    DelayLine delayLine_;
    
    // Per-channel filters
    std::vector<OnePoleLPF> feedbackLPFs_;
    std::vector<Biquad> bassFilters_;
    std::vector<Biquad> trebleFilters_;
    
    // Reverb (simple Schroeder-Moorer)
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

    // Map delay setting (0-10) to active heads
    // 0-10: head combinations matching RE-201
    // 0=single head 1, 1=head 2, 2=head 3, 3=head 1+2, etc.
    std::array<bool, 3> getActiveHeads(int setting) const;
};

// ============================================================================
// Inline implementation of helper
// ============================================================================
inline std::array<bool, 3> JunoTapeEcho::getActiveHeads(int setting) const
{
    switch (setting)
    {
        case 0:  return {true,  false, false}; // Head 1 only
        case 1:  return {false, true,  false}; // Head 2 only
        case 2:  return {false, false, true};  // Head 3 only
        case 3:  return {true,  true,  false}; // 1+2
        case 4:  return {true,  false, true};  // 1+3
        case 5:  return {false, true,  true};  // 2+3
        case 6:  return {true,  true,  true};  // 1+2+3
        case 7:  return {true,  true,  false}; // 1+2 (alt mode)
        case 8:  return {true,  false, true};  // 1+3 (alt mode)
        case 9:  return {false, true,  true};  // 2+3 (alt mode)
        case 10: return {true,  true,  true};  // 1+2+3 (alt mode)
        default: return {true,  false, false};
    }
}
