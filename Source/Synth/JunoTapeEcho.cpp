/*
 * ABD JUNiO 601 - JunoTapeEcho implementation
 * RE-201 inspired tape echo DSP
 */

#include "JunoTapeEcho.h"

// ============================================================================
// Prepare & Reset
// ============================================================================
void JunoTapeEcho::prepare(double sampleRate, int numChannels, int maxBlockSize)
{
    juce::ignoreUnused(maxBlockSize);
    sampleRate_ = sampleRate;
    numChannels_ = juce::jlimit(1, 2, numChannels);

    // Delay line: max ~1.5 seconds at 3x head ratio
    int maxDelay = (int)(sampleRate * 1.5f);
    delayLine_.prepare(sampleRate, maxDelay);

    // Per-channel filters
    feedbackLPFs_.resize(numChannels_);
    bassFilters_.resize(numChannels_);
    trebleFilters_.resize(numChannels_);

    for (int ch = 0; ch < numChannels_; ++ch)
    {
        bassFilters_[ch].setType(Biquad::LowShelf);
        trebleFilters_[ch].setType(Biquad::HighShelf);
    }

    // Reverb: 4 parallel comb filters + 2 allpass
    reverbDelays_.resize(4);
    reverbLPFs_.resize(4);
    for (auto& d : reverbDelays_)
        d.prepare(sampleRate, (int)(sampleRate * 0.1f));

    // Seed noise generator
    noiseGen_.seed(54321);

    reset();
}

void JunoTapeEcho::reset()
{
    delayLine_.reset();
    
    for (auto& f : feedbackLPFs_) f.reset();
    for (auto& f : bassFilters_) f.reset();
    for (auto& f : trebleFilters_) f.reset();
    for (auto& d : reverbDelays_) d.reset();
    for (auto& f : reverbLPFs_) f.reset();

    wowPhase_ = 0.0f;
    flutterPhase_ = 0.0f;
    dirtPhase_ = 0.0f;
    satState_ = 0.0f;
}

// ============================================================================
// Process
// ============================================================================
void JunoTapeEcho::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled_)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(numChannels_, buffer.getNumChannels());

    if (numCh == 0 || numSamples == 0)
        return;

    // Map parameters to internal values
    // Repeat rate (0-1) -> delay time (50ms - 500ms)
    float baseDelayMs = 50.0f + repeatRate_ * 450.0f;
    float baseDelaySamples = (float)(baseDelayMs * 0.001 * sampleRate_);
    
    // Intensity (0-1) -> feedback gain (0 - 0.95)
    float feedback = intensity_ * 0.95f;
    
    // Echo volume (0-1) -> wet gain
    float echoGain = echoVol_ * echoVol_; // square law for better feel
    
    // Reverb volume
    float reverbGain = reverbVol_ * reverbVol_;
    
    // Bass/Treble (0-1) -> gain (-12dB to +12dB)
    float bassGainDb = (bass_ - 0.5f) * 24.0f;
    float trebleGainDb = (treble_ - 0.5f) * 24.0f;

    // Setup filters for tone stack
    for (int ch = 0; ch < numCh; ++ch)
    {
        bassFilters_[ch].setParams(300.0f, 0.707f, bassGainDb, (float)sampleRate_);
        trebleFilters_[ch].setParams(3000.0f, 0.707f, trebleGainDb, (float)sampleRate_);
        
        // Feedback LPF: cutoff varies with repeat rate (simulates tape speed EQ)
        float lpfCutoff = 5000.0f + (1.0f - repeatRate_) * 10000.0f;
        feedbackLPFs_[ch].setCutoff(juce::jlimit(800.0f, 15000.0f, lpfCutoff), (float)sampleRate_);
    }

    // Setup reverb (LPF cutoff for all types)
    for (int i = 0; i < 4; ++i)
        reverbLPFs_[i].setCutoff(8000.0f, (float)sampleRate_);

    // Active heads for current setting
    auto heads = getActiveHeads(delaySetting_);

    // Process sample by sample
    for (int s = 0; s < numSamples; ++s)
    {
        // --- Wow/Flutter modulation ---
        wowPhase_ += 1.0f / (float)(sampleRate_ * 2.0f);  // 0.5 Hz wow
        flutterPhase_ += 8.0f / (float)(sampleRate_);      // 8 Hz flutter
        dirtPhase_ += 12.0f / (float)(sampleRate_);        // 12 Hz "dirt"

        if (wowPhase_ >= 1.0f) wowPhase_ -= 1.0f;
        if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;
        if (dirtPhase_ >= 1.0f) dirtPhase_ -= 1.0f;

        // LFO modulation values
        float wowMod = std::sin(wowPhase_ * 2.0f * juce::MathConstants<float>::pi) * 0.003f;  // 0.3% wow
        float flutterMod = std::sin(flutterPhase_ * 2.0f * juce::MathConstants<float>::pi) * 0.001f;  // 0.1% flutter
        float dirtMod = noiseGen_.next() * 0.0005f;  // noise modulation
        
        float totalMod = 1.0f + wowMod + flutterMod + dirtMod;

        // --- Sum all channels to mono for delay input ---
        float monoInput = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            monoInput += buffer.getSample(ch, s);
        monoInput /= (float)numCh;
        monoInput *= inputLevel_; // Calibration input gain

        // --- Tape saturation (tanh approximation) ---
        float satInput = monoInput + satState_ * feedback * 0.3f;
        float satOutput = std::tanh(satInput * 1.5f) * 0.7f + satInput * 0.3f;
        satState_ = satOutput;

        // Write to delay line
        delayLine_.write(satOutput);

        // --- Read from active heads ---
        float delayOutput = 0.0f;
        int activeCount = 0;
        for (int h = 0; h < 3; ++h)
        {
            if (!heads[h]) continue;
            ++activeCount;
            
            float headDelay = baseDelaySamples * kHeadRatios[h] * totalMod;
            float headGain = 0.33f; // Even mix of active heads
            delayOutput += delayLine_.read(headDelay) * headGain;
        }

        // Apply feedback with LPF
        delayOutput *= feedback;
        for (int ch = 0; ch < numCh; ++ch)
        {
            // Write feedback back to delay line via the next iteration's satInput
            // (feedback is applied in the satInput calculation above)
        }

        // --- Tone stack (bass/treble per channel) ---
        // --- Reverb (3 algorithms based on reverbType_) ---
        float reverbOut = 0.0f;
        
        if (reverbType_ == 0)
        {
            // Type 0: Short Schroeder-Moorer (original, dark spring)
            const float shortTimesMs[4] = { 30.0f, 37.0f, 43.0f, 50.0f };
            const float shortGains[4]  = { 0.6f, 0.5f, 0.4f, 0.3f };
            for (int i = 0; i < 4; ++i)
            {
                float revDelay = shortTimesMs[i] * 0.001f * (float)sampleRate_;
                float revRead = reverbDelays_[i].read(revDelay);
                float revWrite = monoInput * shortGains[i] + revRead * 0.3f;
                reverbDelays_[i].write(revWrite);
                reverbOut += revRead * 0.25f;
            }
        }
        else if (reverbType_ == 1)
        {
            // Type 1: Longer decay (brighter, more spacious)
            const float longTimesMs[4] = { 50.0f, 62.0f, 73.0f, 88.0f };
            const float longGains[4]  = { 0.7f, 0.6f, 0.5f, 0.4f };
            for (int i = 0; i < 4; ++i)
            {
                float revDelay = longTimesMs[i] * 0.001f * (float)sampleRate_;
                float revRead = reverbDelays_[i].read(revDelay);
                float revWrite = monoInput * longGains[i] + revRead * 0.45f;
                reverbDelays_[i].write(revWrite);
                reverbOut += revRead * 0.25f;
            }
        }
        else
        {
            // Type 2: Hybrid (2 short + 2 long)
            const float hybridTimesMs[4] = { 30.0f, 37.0f, 73.0f, 88.0f };
            const float hybridGains[4]  = { 0.6f, 0.5f, 0.5f, 0.4f };
            for (int i = 0; i < 4; ++i)
            {
                float revDelay = hybridTimesMs[i] * 0.001f * (float)sampleRate_;
                float revRead = reverbDelays_[i].read(revDelay);
                float revWrite = monoInput * hybridGains[i] + revRead * 0.38f;
                reverbDelays_[i].write(revWrite);
                reverbOut += revRead * 0.25f;
            }
        }
        
        reverbOut = std::tanh(reverbOut * 0.5f); // Soft clip reverb
        
        // --- Mix per channel ---
        for (int ch = 0; ch < numCh; ++ch)
        {
            // Wet signal: delay output + tone stack + reverb
            float wet = delayOutput * echoGain;
            wet = bassFilters_[ch].process(wet);
            wet = trebleFilters_[ch].process(wet);
            wet += reverbOut * reverbGain;

            // Dry signal (original)
            float dry = buffer.getSample(ch, s);

            // Mix: dry/wet crossfade (wetDry_ = 0..1)
            buffer.setSample(ch, s, dry * (1.0f - wetDry_) + wet * wetDry_);
        }
    }
}

// ============================================================================
// Explicit instantiation of getActiveHeads is inline in header
// ============================================================================
