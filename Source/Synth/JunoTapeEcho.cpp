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

    // 1) Prepare Waveguide Spring Reverb
    springReverb_.prepare(sampleRate_);

    // 2) Prepare Schroeder Reverb: 4 parallel comb filters + 2 allpass
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

    // Reset Waveguide Reverb
    springReverb_.reset();

    // Reset Schroeder Reverb
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
    // Delay time: sync mode uses BPM+division, free mode uses repeatRate knob (50ms-500ms)
    float baseDelayMs;
    if (syncEnabled_ && hostBPM_ > 0.0) {
        // Sync to DAW BPM with musical division
        static constexpr double kDivBeats[9] = {
            4.0,         // 0: 1/1  (whole note)
            2.0,         // 1: 1/2  (half note)
            1.0,         // 2: 1/4  (quarter note)  ← default
            2.0 / 3.0,   // 3: 1/4T (quarter triplet)
            0.5,         // 4: 1/8  (eighth note)
            1.0 / 3.0,   // 5: 1/8T (eighth triplet)
            0.25,        // 6: 1/16 (sixteenth note)
            1.0 / 6.0,   // 7: 1/16T
            0.125        // 8: 1/32
        };
        int divIdx = juce::jlimit(0, 8, syncDivision_);
        double divBeats = kDivBeats[divIdx];
        baseDelayMs = (float)(60.0 / hostBPM_ * divBeats * 1000.0);
        // Clamp to reasonable range (10ms - 2000ms)
        baseDelayMs = juce::jlimit(10.0f, 2000.0f, baseDelayMs);
    } else {
        // Free mode: repeat rate maps to 50ms - 500ms
        baseDelayMs = 50.0f + repeatRate_ * 450.0f;
    }
    float baseDelaySamples = (float)(baseDelayMs * 0.001 * sampleRate_);
    
    // Intensity (0-1) -> feedback gain (0 - 0.95)
    float feedback = intensity_ * 0.95f;
    
    // Echo volume (0-1) -> wet gain
    // [RE-201 Authentic] Echo Cancel: zero echo gain, keep reverb.
    // When echoCancel_ is ON, echoes are silenced but reverb is preserved.
    // Linear gain (not square) so echo is clearly audible at moderate settings;
    // the reverb has internal feedback that naturally boosts its level.
    float echoGain = echoCancel_ ? 0.0f : echoVol_;
    
    // Reverb volume — linear gain
    float reverbGain = reverbVol_;
    
    // Bass/Treble (0-1) -> gain (-12dB to +12dB)
    float bassGainDb = (bass_ - 0.5f) * 24.0f;
    float trebleGainDb = (treble_ - 0.5f) * 24.0f;

    // Feedback LPF: cutoff varies with repeat rate and echoIsolator_ (simulates tape speed EQ + damping)
    float baseLpfCutoff = feedbackLpfBase_ + (1.0f - repeatRate_) * feedbackLpfRange_;
    float lpfCutoff = baseLpfCutoff * std::pow(1.0f - echoIsolator_, 1.5f);
    lpfCutoff = juce::jlimit(200.0f, 15000.0f, lpfCutoff);
    for (int ch = 0; ch < numCh; ++ch)
    {
        bassFilters_[ch].setParams(bassFreq_, 0.707f, bassGainDb, (float)sampleRate_);
        trebleFilters_[ch].setParams(trebleFreq_, 0.707f, trebleGainDb, (float)sampleRate_);
        feedbackLPFs_[ch].setCutoff(lpfCutoff, (float)sampleRate_);
    }

    // Setup Schroeder reverb LPFs
    for (int i = 0; i < 4; ++i)
        reverbLPFs_[i].setCutoff(schroederLpf_, (float)sampleRate_);

    // Active heads + reverb for current preset
    auto preset = getPresetConfig(delaySetting_);
    auto& heads = preset.heads;
    bool reverbOn = preset.reverbOn;

    // Process sample by sample
    for (int s = 0; s < numSamples; ++s)
    {
        // --- Wow/Flutter modulation ---
        wowPhase_ += wowRate_ / (float)(sampleRate_);
        flutterPhase_ += flutterRate_ / (float)(sampleRate_);
        dirtPhase_ += tapeScrapeRate_ / (float)(sampleRate_);

        if (wowPhase_ >= 1.0f) wowPhase_ -= 1.0f;
        if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;
        if (dirtPhase_ >= 1.0f) dirtPhase_ -= 1.0f;

        // LFO modulation values (depth scaled by wow & flutter knob)
        float modScale = wowFlutter_ * wowFlutterScale_;
        float wowMod = std::sin(wowPhase_ * 2.0f * juce::MathConstants<float>::pi) * wowAmp_ * modScale;
        float flutterMod = std::sin(flutterPhase_ * 2.0f * juce::MathConstants<float>::pi) * flutterAmp_ * modScale;
        float dirtMod = std::sin(dirtPhase_ * 2.0f * juce::MathConstants<float>::pi) * tapeScrapeAmp_ * modScale;
        
        float totalMod = 1.0f + wowMod + flutterMod + dirtMod;

        // --- Sum all channels to mono for delay input ---
        float monoInput = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            monoInput += buffer.getSample(ch, s);
        monoInput /= (float)numCh;
        monoInput *= inputLevel_; // Calibration input gain

        // --- Read playheads first (so they feed into saturation & delay write correctly) ---
        int activeCount = 0;
        for (int h = 0; h < 3; ++h)
            if (heads[h]) ++activeCount;

        float gainPerHead = (activeCount > 0) ? (1.0f / (float)activeCount) : 0.33f;

        float delayOutput = 0.0f;
        if (activeCount > 0)
        {
            for (int h = 0; h < 3; ++h)
            {
                if (!heads[h]) continue;
                float headRatio = (h == 0) ? 1.0f : (h == 1 ? head2Ratio_ : head3Ratio_);
                float headDelay = baseDelaySamples * headRatio * totalMod;
                delayOutput += delayLine_.read(headDelay) * gainPerHead;
            }
        }

        // --- Tape saturation (pure tanh feedback input) ---
        // Feed delayOutput * feedback back into the tape saturation write loop (filtered by Isolator LPF)
        float feedbackSignal = delayOutput * feedback;
        feedbackSignal = feedbackLPFs_[0].process(feedbackSignal);
        
        float satInput = monoInput + feedbackSignal;
        float satOutput = std::tanh(satInput * saturationInputGain_);
        satState_ = satOutput;

        // Write saturated signal to delay line
        delayLine_.write(satOutput);

        // --- Reverb processing ---
        float reverbOutL = 0.0f;
        float reverbOutR = 0.0f;

        if (reverbOn)
        {
            // Reverb input: delay output if echo is active, otherwise dry monoInput (Reverb Only mode)
            float revInput = (activeCount > 0) ? delayOutput : monoInput;

            if (reverbType_ == 0)
            {
                // TYPE 0: High-quality Waveguide Spring Reverb (Stereo)
                // Map reverbDecay_ (0 to 1) linearly: at 0.5, feedback is 0.15f; at 1.0, feedback is 0.25f; at 0.0, feedback is 0.05f.
                float springFeedback = 0.05f + reverbDecay_ * 0.20f;
                springReverb_.process(revInput, revInput, reverbOutL, reverbOutR, springFeedback, springReflectionScale_);
                reverbOutL *= springGain_;
                reverbOutR *= springGain_;
            }
            else
            {
                // Schroeder-Moorer implementations (Mono)
                float schroederOut = 0.0f;
                if (reverbType_ == 1)
                {
                    // TYPE 1: Schroeder Short (Dark Spring)
                    const float shortTimesMs[4] = { 30.0f, 37.0f, 43.0f, 50.0f };
                    const float shortGains[4]  = { 0.6f, 0.5f, 0.4f, 0.3f };
                    float schroederFeedback = 0.5f + reverbDecay_ * 0.45f; // 0.725f at decay=0.5
                    for (int i = 0; i < 4; ++i)
                    {
                        float revDelay = shortTimesMs[i] * 0.001f * (float)sampleRate_;
                        float revRead = reverbDelays_[i].read(revDelay);
                        float revWrite = revInput * shortGains[i] + revRead * schroederFeedback;
                        reverbDelays_[i].write(revWrite);
                        schroederOut += revRead * 0.25f;
                    }
                }
                else
                {
                    // TYPE 2: Schroeder Hybrid
                    const float hybridTimesMs[4] = { 30.0f, 37.0f, 73.0f, 88.0f };
                    const float hybridGains[4]  = { 0.6f, 0.5f, 0.5f, 0.4f };
                    float schroederFeedback = 0.5f + reverbDecay_ * 0.46f; // 0.73f at decay=0.5
                    for (int i = 0; i < 4; ++i)
                    {
                        float revDelay = hybridTimesMs[i] * 0.001f * (float)sampleRate_;
                        float revRead = reverbDelays_[i].read(revDelay);
                        float revWrite = revInput * hybridGains[i] + revRead * schroederFeedback;
                        reverbDelays_[i].write(revWrite);
                        schroederOut += revRead * 0.25f;
                    }
                }

                schroederOut = std::tanh(schroederOut * schroederSatDrive_) * schroederGain_; // Soft clip and scale up
                reverbOutL = schroederOut;
                reverbOutR = schroederOut;
            }
        }

        // --- Mix per channel ---
        for (int ch = 0; ch < numCh; ++ch)
        {
            // Wet signal: delay output + reverb, filtered by the tone stack
            float reverbOut = (ch == 0) ? reverbOutL : reverbOutR;
            float wet = delayOutput * echoGain + reverbOut * reverbGain;
            wet = bassFilters_[ch].process(wet);
            wet = trebleFilters_[ch].process(wet);

            // Dry signal (original)
            float dry = buffer.getSample(ch, s);

            // [RE-201 Authentic] Parallel mix: dry signal passes through unattenuated,
            // echo and reverb are ADDED on top (controlled by wetDry_ as a send level).
            // The real RE-201 has no balance knob — ECHO VOLUME and REVERB VOLUME
            // are independent sends that add to the dry signal.
            // Hard-clip at ±1.0 to prevent digital overshoot from parallel sum.
            float out = dry + wet * wetDry_;
            buffer.setSample(ch, s, juce::jlimit(-1.0f, 1.0f, out));
        }
    }
}
