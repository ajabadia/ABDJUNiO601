#pragma once
#include <JuceHeader.h>
#include <vector>

/**
 * TapeSignalRestorer provides advanced DSP tools to clean and restore
 * degraded FSK tape recordings (e.g., 8-bit, low level, high noise).
 */
class TapeSignalRestorer {
public:
    struct RestorationParams {
        bool enableHPF = true;
        bool enableExpansion = false;
        double expansionGamma = 0.5; // 1.0 = linear, < 1.0 = expanding
        bool enableBandpass = false;
        double centerFreqSpace = 1360.0;
        double centerFreqMark = 2380.0;
        double bandwidth = 150.0;
    };

    /**
     * Processes a buffer of audio samples to improve FSK decoding reliability.
     * @param buffer The audio buffer to process (in-place).
     * @param params Parameters for the restoration process.
     */
    static void process(juce::AudioBuffer<float>& buffer, const RestorationParams& params) {
        int numSamples = buffer.getNumSamples();
        float* samples = buffer.getWritePointer(0);

        // 1. High-Pass Filter (DC Bias Removal)
        if (params.enableHPF) {
            float y_prev = 0.0f, x_prev = 0.0f;
            const float alpha = 0.9943f;
            for (int i = 0; i < numSamples; ++i) {
                float x = samples[i];
                float y = alpha * (y_prev + x - x_prev);
                samples[i] = y;
                y_prev = y;
                x_prev = x;
            }
        }

        // 2. Normalization
        float peak = buffer.getMagnitude(0, 0, numSamples);
        if (peak > 0.0001f) {
            buffer.applyGain(1.0f / peak);
        }

        // 3. Non-linear Expansion
        if (params.enableExpansion) {
            double gamma = params.expansionGamma;
            for (int i = 0; i < numSamples; ++i) {
                float s = samples[i];
                samples[i] = (s >= 0 ? 1.0f : -1.0f) * std::pow(std::abs(s), (float)gamma);
            }
        }

        // 4. Dual Band-pass Filtering
        if (params.enableBandpass) {
            applyDualBandpass(buffer, params);
        }
    }

private:
    static void applyDualBandpass(juce::AudioBuffer<float>& buffer, const RestorationParams& params) {
        int numSamples = buffer.getNumSamples();
        float* samples = buffer.getWritePointer(0);
        
        // Temporary buffer for filtering
        std::vector<float> filtered(numSamples, 0.0f);
        
        // We apply two BPFs and sum them.
        // Since we are in a prototype phase, we'll use a simple IIR BPF.
        // For a production VST, we would use juce::dsp::IIR::Filter.
        
        auto applyBPF = [&](double centerFreq, double bw) {
            double sr = 44100.0; // Assumed after upsampling
            double omega = 2.0 * juce::MathConstants<double>::pi * centerFreq / sr;
            double alpha = std::sin(omega) * std::sinh(std::log(2.0) / 2.0 * bw * omega / (2.0 * juce::MathConstants<double>::pi));
            
            // Simple 2nd order BPF coefficients
            double a0 = 1.0 + alpha;
            double a1 = -2.0 * std::cos(omega);
            double a2 = 1.0 - alpha;
            double b0 = alpha;
            double b1 = 0.0;
            double b2 = -alpha;
            
            double invA0 = 1.0 / a0;
            double b0_a0 = b0 * invA0;
            double b1_a0 = b1 * invA0;
            double b2_a0 = b2 * invA0;
            double a1_a0 = a1 * invA0;
            double a2_a0 = a2 * invA0;
            
            float x1 = 0.0f, x2 = 0.0f;
            float y1 = 0.0f, y2 = 0.0f;
            for (int i = 0; i < numSamples; ++i) {
                float x = samples[i];
                float y = (float)(b0_a0 * x + b1_a0 * x1 + b2_a0 * x2 - a1_a0 * y1 - a2_a0 * y2);
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
                filtered[i] += y;
            }
        };

        applyBPF(params.centerFreqSpace, params.bandwidth);
        applyBPF(params.centerFreqMark, params.bandwidth);
        
        // Mix back to buffer
        for (int i = 0; i < numSamples; ++i) {
            samples[i] = filtered[i];
        }
        
        // Re-normalize after filtering
        float newPeak = buffer.getMagnitude(0, 0, numSamples);
        if (newPeak > 0.0001f) buffer.applyGain(1.0f / newPeak);
    }
};
