#include "JunoLFO.h"
#include "../Core/JunoConstants.h"
#include "../Core/CalibrationSettings.h"
#include <cmath>
#include <algorithm>

// Replicates the attack increment calculation from the ADSR (copied from JunoADSR.cpp)
static uint16_t LfoAttackIncFromSlider(float slider)
{
    float s = std::clamp(slider, 0.f, 1.f);
    if (s < 0.003937f) return 0x3FFF;
    if (s <= 0.500000f)
        return static_cast<uint16_t>(8192.f / (s * 127.f) + 0.5f);
    if (s <= 0.681102f)
        return static_cast<uint16_t>(305.03f - 352.26f * s + 0.5f);
    if (s <= 0.846457f)
        return static_cast<uint16_t>(194.74f - 190.50f * s + 0.5f);
    if (s <= 0.956693f)
        return static_cast<uint16_t>(86.37f - 62.52f * s + 0.5f);
    return static_cast<uint16_t>(std::max(
        static_cast<int>(148.f - 127.f * s + 0.5f), 1));
}

JunoLFO::JunoLFO() {
    reset();
}

void JunoLFO::prepare(double sr, int maxBlockSize) {
    juce::ignoreUnused(maxBlockSize);
    sampleRate = sr;
    reset();
}

void JunoLFO::reset() {
    mIntAccum = 0;
    mIntRising = true;
    mIntPolarity = false;
    mIntTri = 0.0f;
    mLastTri = 0.0f;
    mHoldoffAccum = 0;
    mHoldoffInc = 0;
    mRampAccum = 0;
    mRampInc = 0;
    mInHoldoff = false;
    mArmed = true;
    mAmpInt = 0.0f;
    currentValue = 0.0f;
    mWasGated = false;
}

void JunoLFO::setDepth(float amount) {
    depth = juce::jlimit(0.0f, 1.0f, amount);
}

void JunoLFO::setDelay(float seconds) {
    delay = juce::jlimit(0.0f, 3.0f, seconds);
}

void JunoLFO::noteOn() {
    // Legacy noteOn wrapper - does not do anything since updateGateState manages reset
}

void JunoLFO::noteOff() {
    // Legacy noteOff wrapper
}

void JunoLFO::updateGateState(bool gated, bool trigger) {
    bool isGatedNow = gated || trigger;

    // We reset on the rising edge of gated state if we are armed
    if (!isGatedNow) {
        mArmed = true;
    }
    if (isGatedNow && !mWasGated && mArmed) {
        mHoldoffAccum = 0;
        mRampAccum = 0;
        mAmpInt = 0.0f;
        mArmed = false;
        
        // Setup Holdoff increment using ADSR attack rate for this delay value
        mHoldoffInc = LfoAttackIncFromSlider(delay / 3.0f); // delay mapped back to 0-1 range
        mInHoldoff = (mHoldoffInc < 0x3FFF);
        
        int pot = static_cast<int>((delay / 3.0f) * 127.f + 0.5f);
        int idx = std::clamp(pot >> 4, 0, 7);
        if (cal != nullptr) {
            mRampInc = cal->getLfoRampIncrement(idx);
        } else {
            static constexpr uint16_t kDefaultLfoRampTbl[8] = {
                0xFFFF, 0x0419, 0x020C, 0x015E, 0x0100, 0x0100, 0x0100, 0x0100
            };
            mRampInc = kDefaultLfoRampTbl[idx];
        }
    }
    mWasGated = isGatedNow;
}

void JunoLFO::tick106() {
    // 1. Advance LFO Speed Accumulator
    if (mIntRising) {
        uint32_t sum = static_cast<uint32_t>(mIntAccum) + mIntCoeff;
        uint32_t lfoAccumMax = (cal != nullptr) ? (uint32_t)cal->getValue("lfoAccumMax") : 8191;
        if (sum >= (lfoAccumMax + 1)) {
            mIntAccum = (uint16_t)lfoAccumMax;
            mIntRising = false;
        } else {
            mIntAccum = static_cast<uint16_t>(sum);
        }
    } else {
        if (mIntCoeff > mIntAccum) {
            mIntAccum = 0;
            mIntRising = true;
            mIntPolarity = !mIntPolarity; // Polarity flips only at bottom clamp
        } else {
            mIntAccum -= mIntCoeff;
        }
    }

    uint32_t lfoAccumMax = (cal != nullptr) ? (uint32_t)cal->getValue("lfoAccumMax") : 8191;
    float mag = static_cast<float>(mIntAccum) / (float)lfoAccumMax;
    mIntTri = mIntPolarity ? -mag : mag;

    // 2. Onset delay envelope: holdoff then ramp
    uint32_t lfoHoldoffThresh = (cal != nullptr) ? (uint32_t)cal->getValue("lfoHoldoffThresh") : 16384;
    if (mInHoldoff) {
        uint32_t sum = static_cast<uint32_t>(mHoldoffAccum) + mHoldoffInc;
        if (sum >= lfoHoldoffThresh) {
            mInHoldoff = false;
            mHoldoffAccum = (uint16_t)lfoHoldoffThresh;
        } else {
            mHoldoffAccum = static_cast<uint16_t>(sum);
        }
        mAmpInt = 0.0f;
    } else {
        uint32_t sum = static_cast<uint32_t>(mRampAccum) + mRampInc;
        if (sum >= 0x10000) {
            mRampAccum = 0xFFFF;
            mAmpInt = 1.0f;
        } else {
            mRampAccum = static_cast<uint16_t>(sum);
            mAmpInt = static_cast<float>(mRampAccum >> 8) / 255.f;
        }
    }
}

float JunoLFO::process(float lfoRateSlider) {
    if (cal != nullptr) {
        int byte = static_cast<int>(lfoRateSlider * 127.f + 0.5f);
        byte = std::clamp(byte, 0, 127);
        mIntCoeff = cal->getLfoSpeedCoeff(byte);
    } else {
        // Fallback default table index
        static constexpr uint16_t kDefaultLfoSpeedTbl[128] = {
            0x0005, 0x000f, 0x0019, 0x0028, 0x0037, 0x0046, 0x0050, 0x005a,
            0x0064, 0x006e, 0x0078, 0x0082, 0x008c, 0x0096, 0x00a0, 0x00aa,
            0x00b4, 0x00be, 0x00c8, 0x00d2, 0x00dc, 0x00e6, 0x00f0, 0x00fa,
            0x0104, 0x010e, 0x0118, 0x0122, 0x012c, 0x0136, 0x0140, 0x014a,
            0x0154, 0x015e, 0x0168, 0x0172, 0x017c, 0x0186, 0x0190, 0x019a,
            0x01a4, 0x01ae, 0x01b8, 0x01c2, 0x01cc, 0x01d6, 0x01e0, 0x01ea,
            0x01f4, 0x01fe, 0x0208, 0x0212, 0x021c, 0x0226, 0x0230, 0x023a,
            0x0244, 0x024e, 0x0258, 0x0262, 0x026c, 0x0276, 0x0280, 0x028a,
            0x029a, 0x02aa, 0x02ba, 0x02ca, 0x02da, 0x02ea, 0x02fa, 0x030a,
            0x031a, 0x032a, 0x033a, 0x034a, 0x035a, 0x036a, 0x037a, 0x038a,
            0x039a, 0x03aa, 0x03ba, 0x03ca, 0x03da, 0x03ea, 0x03fa, 0x040a,
            0x041a, 0x042a, 0x043a, 0x044a, 0x045a, 0x046a, 0x047a, 0x048a,
            0x04be, 0x04f2, 0x0526, 0x055a, 0x058e, 0x05c2, 0x05f6, 0x062c,
            0x0672, 0x06b8, 0x0708, 0x0758, 0x07a8, 0x07f8, 0x085c, 0x08c0,
            0x0924, 0x0988, 0x09ec, 0x0a50, 0x0ab4, 0x0b18, 0x0b7c, 0x0be0,
            0x0c58, 0x0cd0, 0x0d48, 0x0dde, 0x0e74, 0x0f0a, 0x0fa0, 0x1000
        };
        int byte = static_cast<int>(lfoRateSlider * 127.f + 0.5f);
        byte = std::clamp(byte, 0, 127);
        mIntCoeff = kDefaultLfoSpeedTbl[byte];
    }

    // Process tick at ~234.2 Hz rate (TickPeriod = 4.2335ms)
    // We compute how many ticks fit into 1 sample, and accumulate
    // For a single sample, we tick the LFO if we cross the period boundary.
    // In order to be simple and accurate, we can tick based on a time accumulator
    // similar to ADSR tick interpolation.
    
    // We'll calculate current value from mIntTri and mAmpInt
    // Let's use the rounded triangle clipping (cubic soft clipping) matches capacitor rounding
    float tri = mIntTri;
    tri = tri * (1.5f - 0.5f * tri * tri);
    
    mLastTri = tri;
    currentValue = tri * depth * mAmpInt;
    return currentValue;
}
