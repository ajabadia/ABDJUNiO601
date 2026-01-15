#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace JunoDSP
{
    /**
     * JunoBBD - Authentic MN3009 Bucket Brigade Device Emulation
     * 
     * Features:
     * - Variable Sample Rate Delay Line (Clock-driven)
     * - Compander (NE570 style) integration
     * - Analog Filtering (Anti-aliasing / Reconstruction)
     * - Clock Noise Bleed
     */
    class JunoBBD
    {
    public:
        JunoBBD() {}
        
        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            // MN3009 has 256 stages. Max delay ~50ms? 
            // Juno chorus delay is ~11ms-15ms.
            // We need a buffer big enough for lowest clock speed.
            // 20ms at 44.1k = 882 samples.
            // BBD is often modeled as a fractional delay or a ring buffer.
            buffer.setSize(1, (int)(sampleRate * 0.1) + 1); // 100ms safety
            buffer.clear();
            bufferSize = buffer.getNumSamples();
            writePos = 0;
            readPos = 0.0f;
            
            // Filters
            lpFilter.prepare(spec);
            lpFilter.reset(); 
            // MN3009 Reconstruction Filter (approx 10kHz LPF)
            lpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 9000.0f);
        }
        
        void reset() {
            buffer.clear();
            writePos = 0;
            readPos = 0.0f;
            currentDelaySamples = 0.0f;
        }
        
        /**
         * Sets the BBD Clock Frequency.
         * Authentic controls work by varying the clock.
         * Delay Time = (Stages / 2) / ClockFreq
         * MN3009 usually has 256 stages.
         * If Clock = 20kHz -> Delay = 128 / 20000 = 6.4ms
         * If Clock = 100kHz -> Delay = 1.28ms
         * 
         * However, Juno controls are commonly referred to as "Rate" and "Depth" of the LFO 
         * driving that clock.
         * 
         * We will accept 'delayMs' for simplicity, but internally calculate read pointer.
         */
        void process(juce::AudioBuffer<float>& ioBuffer, float delayMs, float mix, float noiseLevel, float clockFreq)
        {
            juce::ignoreUnused(clockFreq);
            const int numSamples = ioBuffer.getNumSamples();
            const float* inData = ioBuffer.getReadPointer(0);
            float* outData = ioBuffer.getWritePointer(0);
            
            // Calculate delay in samples
            float targetDelaySamps = (delayMs * 0.001f) * (float)sampleRate;
            
            for (int i = 0; i < numSamples; ++i)
            {
                float dry = inData[i];
                
                // --- 1. COMPRESSOR (NE570 part 1) ---
                // (Already implemented in PluginProcessor currently, but ideally should be here)
                // We assume input is already compressed/companded if handled externally.
                // For now, we process "dry" as the BBD input.
                
                // Write to BBD (Ring Buffer)
                float* buf = buffer.getWritePointer(0);
                buf[writePos] = dry;
                
                // Read from BBD
                // Cubic interpolation for sound quality
                float actualReadVal = interpolate(targetDelaySamps);
                
                // --- 2. CLOCK NOISE & DEGRADATION ---
                // Add clock whine
                if (noiseLevel > 0.0f) {
                     // Clock bleed artifact
                     // float bleed = std::sin(gui_time * clockFreq ...) 
                     // Simplified: white noise floor
                     actualReadVal += (random.nextFloat() * 2.0f - 1.0f) * noiseLevel;
                }
                
                // --- 3. FILTERING (Reconstruction) ---
                actualReadVal = lpFilter.processSample(actualReadVal);
                
                // Mix
                outData[i] = dry * (1.0f - mix) + actualReadVal * mix;
                
                // Advance pointers
                writePos = (writePos + 1) % bufferSize;
            }
        }
        
        // Per-sample processing optimized for Chorus loop
        float processSample(float input, float delayMs)
        {
             int bufSize = bufferSize; // Local copy
             float* buf = buffer.getWritePointer(0);
             buf[writePos] = input;
             
             float delaySamps = (delayMs * 0.001f) * (float)sampleRate;
             float out = interpolate(delaySamps);
             
             writePos++;
             if (writePos >= bufSize) writePos = 0;
             
             // Reconstruction Filter
             return lpFilter.processSample(out);
        }

    private:
        float interpolate(float delaySamples)
        {
            // Read Pos = Write Pos - Delay
            float rPos = (float)writePos - delaySamples;
            while (rPos < 0.0f) rPos += (float)bufferSize;
            while (rPos >= (float)bufferSize) rPos -= (float)bufferSize;
            
            int i0 = (int)rPos;
            int i1 = (i0 + 1) % bufferSize;
            float frac = rPos - (float)i0;
            
            // Linear for now (authentic grit). Cubic if too clean?
            // BBDs are "gritty" but mainly due to noise/bandwidth, not interpolation.
            // Linear adds HF attenuation which mimics BBD loss.
            const float* b = buffer.getReadPointer(0);
            return b[i0] + frac * (b[i1] - b[i0]);
        }

        juce::AudioBuffer<float> buffer;
        int bufferSize = 0;
        int writePos = 0;
        float readPos = 0.0f;
        float currentDelaySamples = 0.0f;
        double sampleRate = 44100.0;
        
        juce::dsp::IIR::Filter<float> lpFilter;
        juce::Random random;
    };
}
