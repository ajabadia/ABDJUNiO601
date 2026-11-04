#include "JunoLFO.h"
#include "../Core/CalibrationSettings.h"
#include "../Core/JunoConstants.h"
#include "../Core/LfoTables.h"
#include <algorithm>
#include <cmath>

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

JunoLFO::JunoLFO()
{
    reset();
}

void JunoLFO::prepare(double sr, int maxBlockSize)
{
    juce::ignoreUnused(maxBlockSize);
    sampleRate = sr;
    reset();
}

void JunoLFO::reset()
{
    mIntAccum     = 0;
    mIntRising    = true;
    mIntPolarity  = false;
    mIntTri       = 0.0f;
    mLastTri      = 0.0f;
    mHoldoffAccum = 0;
    mHoldoffInc   = 0;
    mRampAccum    = 0;
    mRampInc      = 0;
    mInHoldoff    = false;
    mArmed        = true;
    mAmpInt       = 0.0f;
    currentValue  = 0.0f;
    mWasGated     = false;
}

void JunoLFO::setDepth(float amount)
{
    depth = juce::jlimit(0.0f, 1.0f, amount);
}

void JunoLFO::setDelay(float seconds)
{
    delay = juce::jlimit(0.0f, 3.0f, seconds);
}

void JunoLFO::noteOn()
{
    // Legacy noteOn wrapper - does not do anything since updateGateState manages reset
}

void JunoLFO::noteOff()
{
    // Legacy noteOff wrapper
}

void JunoLFO::updateGateState(bool gated, bool trigger)
{
    bool isGatedNow = gated || trigger;

    // We reset on the rising edge of gated state if we are armed
    if (!isGatedNow)
    {
        mArmed = true;
    }
    if (isGatedNow && !mWasGated && mArmed)
    {
        mHoldoffAccum = 0;
        mRampAccum    = 0;
        mAmpInt       = 0.0f;
        mArmed        = false;

        // Setup Holdoff increment using ADSR attack rate for this delay value
        mHoldoffInc = LfoAttackIncFromSlider(delay / 3.0f); // delay mapped back to 0-1 range
        // FW.3: the firmware holdoff accumulator ($FF23) runs modulo 0x4000, so the
        // delay ends when that 14-bit accumulator wraps. 0x4000 is the modulus, never
        // an in-run value. delay == 0 yields the fastest ADSR increment (0x3FFF),
        // which is the firmware's "no delay" case.
        const uint32_t holdoffModulus = (cal != nullptr)
                                            ? static_cast<uint32_t>(cal->getValue("lfoHoldoffThresh"))
                                            : 0x4000u;
        mInHoldoff                    = (mHoldoffInc != 0 && static_cast<uint32_t>(mHoldoffInc) < holdoffModulus);

        int pot = static_cast<int>((delay / 3.0f) * 127.f + 0.5f);
        int idx = std::clamp(pot >> 4, 0, 7);
        if (cal != nullptr)
        {
            mRampInc = cal->getLfoRampIncrement(idx);
        }
        else
        {
            mRampInc = LfoTables::getLfoRampIncrement(idx);
        }
    }
    mWasGated = isGatedNow;
}

void JunoLFO::tick106()
{
    // 1. Advance LFO Speed Accumulator
    if (mIntRising)
    {
        uint32_t sum         = static_cast<uint32_t>(mIntAccum) + mIntCoeff;
        uint32_t lfoAccumMax = (cal != nullptr) ? (uint32_t)cal->getValue("lfoAccumMax") : 8191;
        if (sum >= (lfoAccumMax + 1))
        {
            mIntAccum  = (uint16_t)lfoAccumMax;
            mIntRising = false;
        }
        else
        {
            mIntAccum = static_cast<uint16_t>(sum);
        }
    }
    else
    {
        if (mIntCoeff > mIntAccum)
        {
            mIntAccum    = 0;
            mIntRising   = true;
            mIntPolarity = !mIntPolarity; // Polarity flips only at bottom clamp
        }
        else
        {
            mIntAccum -= mIntCoeff;
        }
    }

    uint32_t lfoAccumMax = (cal != nullptr) ? (uint32_t)cal->getValue("lfoAccumMax") : 8191;
    float mag            = static_cast<float>(mIntAccum) / (float)lfoAccumMax;
    mIntTri              = mIntPolarity ? -mag : mag;

    // 2. Onset delay envelope: holdoff then ramp
    uint32_t lfoHoldoffThresh = (cal != nullptr) ? (uint32_t)cal->getValue("lfoHoldoffThresh") : 16384;
    if (mInHoldoff)
    {
        uint32_t sum = static_cast<uint32_t>(mHoldoffAccum) + mHoldoffInc;
        if (sum >= lfoHoldoffThresh)
        {
            // FW.3: lfoHoldoffThresh is the 0x4000 modulus; the delay ends on the wrap.
            mInHoldoff    = false;
            mHoldoffAccum = static_cast<uint16_t>(lfoHoldoffThresh - 1);
        }
        else
        {
            mHoldoffAccum = static_cast<uint16_t>(sum);
        }
        mAmpInt = 0.0f;
    }
    else
    {
        uint32_t sum = static_cast<uint32_t>(mRampAccum) + mRampInc;
        if (sum >= 0x10000)
        {
            mRampAccum = 0xFFFF;
            mAmpInt    = 1.0f;
        }
        else
        {
            mRampAccum = static_cast<uint16_t>(sum);
            mAmpInt    = static_cast<float>(mRampAccum >> 8) / 255.f;
        }
    }
}

float JunoLFO::process(float lfoRateSlider)
{
    if (cal != nullptr)
    {
        int byte  = static_cast<int>(lfoRateSlider * 127.f + 0.5f);
        byte      = std::clamp(byte, 0, 127);
        mIntCoeff = cal->getLfoSpeedCoeff(byte);
    }
    else
    {
        // Fallback to the shared ROM table (LfoTables, FW.8)
        int byte  = static_cast<int>(lfoRateSlider * 127.f + 0.5f);
        byte      = std::clamp(byte, 0, 127);
        mIntCoeff = LfoTables::getLfoSpeedCoeff(byte);
    }

    // FW.4: the master loop steps the LFO every 4.2335 ms (236.2 Hz), the same
    // period used by the ADSR. The older 234.2 Hz figure was inconsistent and is
    // not used anywhere in the model.
    // We compute how many ticks fit into 1 sample, and accumulate
    // For a single sample, we tick the LFO if we cross the period boundary.
    // In order to be simple and accurate, we can tick based on a time accumulator
    // similar to ADSR tick interpolation.

    // We'll calculate current value from mIntTri and mAmpInt
    // Let's use the rounded triangle clipping (cubic soft clipping) matches capacitor rounding
    float tri = mIntTri;
    tri       = tri * (1.5f - 0.5f * tri * tri);

    mLastTri     = tri;
    currentValue = tri * depth * mAmpInt;
    return currentValue;
}
