#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

/**
 * TuningManager
 * 
 * Handles microtuning frequency tables and Scala (.scl) file parsing.
 */
class TuningManager {
public:
    TuningManager() {
        resetToStandard();
    }

    void resetToStandard() {
        for (int i = 0; i < 128; ++i) {
            frequencies[i] = 440.0f * std::pow(2.0f, (i - 69) / 12.0f);
        }
        isUsingCustomTuning = false;
    }

    const float* getTuningTable() const {
        return frequencies.data();
    }

    bool parseSCL(const juce::File& file) {
        juce::StringArray lines;
        file.readLines(lines);

        int state = 0; // 0: looking for name/comment, 1: looking for count, 2: parsing intervals
        int intervalCount = 0;
        std::vector<double> scale;
        scale.push_back(1.0); // Tonic

        for (auto& line : lines) {
            auto trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWith("!")) continue;

            if (state == 0) {
                // Skip description
                state = 1;
                continue;
            } else if (state == 1) {
                intervalCount = trimmed.getIntValue();
                state = 2;
                continue;
            } else if (state == 2) {
                if (trimmed.contains(".")) {
                    // Cents
                    double cents = trimmed.getDoubleValue();
                    scale.push_back(std::pow(2.0, cents / 1200.0));
                } else if (trimmed.contains("/")) {
                    // Ratio
                    auto parts = juce::StringArray::fromTokens(trimmed, "/", "");
                    if (parts.size() == 2) {
                        double num = parts[0].getDoubleValue();
                        double den = parts[1].getDoubleValue();
                        if (den != 0) scale.push_back(num / den);
                    }
                } else {
                    // Raw ratio or cents without dot?
                    double val = trimmed.getDoubleValue();
                    if (val > 0) scale.push_back(val);
                }

                if (scale.size() > (size_t)intervalCount) break;
            }
        }

        if (scale.size() <= 1) return false;

        // Populate table (MIDI 60 = Middle C = Tonic for now, or use KBM logic)
        // Simple implementation: Middle C (60) is the tonic.
        int tonicNote = 60;
        double tonicFreq = 261.625565; // Standard Middle C

        for (int i = 0; i < 128; ++i) {
            int diff = i - tonicNote;
            int octaves = static_cast<int>(std::floor(diff / (double)intervalCount));
            int degree = diff % intervalCount;
            if (degree < 0) {
                degree += intervalCount;
            }

            frequencies[i] = static_cast<float>(tonicFreq * std::pow(scale.back(), octaves) * scale[degree]);
        }

        isUsingCustomTuning = true;
        return true;
    }

    bool isActive() const { return isUsingCustomTuning; }

private:
    std::array<float, 128> frequencies;
    bool isUsingCustomTuning = false;
};
