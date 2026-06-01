#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

// ============================================================
// BBDFilter — 2nd-order Butterworth biquad (TPT SVF) + TPT
// 1-pole lowpass, matched to the Juno-6/60 chorus prefilter.
// ============================================================
struct BBDFilter
{
  static constexpr float kBiquadFc = 9661.f;
  static constexpr float kBiquadQ  = 0.7071f;   // Butterworth
  static constexpr float kPoleFc   = 20000.f;    // HF tilt pole

  // --- TPT SVF biquad (2nd-order lowpass) ---
  struct SVFBiquad
  {
    float mIC1eq = 0.f;  // integrator 1 state
    float mIC2eq = 0.f;  // integrator 2 state

    void Reset() { mIC1eq = mIC2eq = 0.f; }

    void SetState(float value)
    {
      mIC1eq = 0.f;
      mIC2eq = value;
    }

    float Process(float input, float g, float a1)
    {
      float v3 = input - mIC2eq;
      float v1 = a1 * mIC1eq + a1 * g * v3;
      float v2 = mIC2eq + g * v1;
      mIC1eq = 2.f * v1 - mIC1eq;
      mIC2eq = 2.f * v2 - mIC2eq;
      return v2;  // lowpass output
    }
  };

  // --- TPT 1-pole lowpass ---
  struct TPT1
  {
    float mS = 0.f;

    void Reset() { mS = 0.f; }

    void SetState(float value) { mS = value; }

    float Process(float input, float g)
    {
      float v = (input - mS) * g / (1.f + g);
      float lp = mS + v;
      mS = lp + v;
      return lp;
    }
  };

  SVFBiquad mBiquad;
  TPT1 mPole;
  float mG_bq  = 0.f;   // biquad integrator gain
  float mA1_bq = 0.f;   // biquad feedback coefficient
  float mG_p   = 0.f;   // 1-pole integrator gain

  void Init(float sampleRate, float biquadFc = kBiquadFc)
  {
    float piVal = juce::MathConstants<float>::pi;
    // Biquad coefficients
    float fc = std::min(biquadFc, sampleRate * 0.45f);
    mG_bq = std::tan(piVal * fc / sampleRate);
    mA1_bq = 1.f / (1.f + mG_bq / kBiquadQ + mG_bq * mG_bq);

    // 1-pole coefficient
    float fc_p = std::min(kPoleFc, sampleRate * 0.45f);
    mG_p = std::tan(piVal * fc_p / sampleRate);

    Reset();
  }

  void Reset()
  {
    mBiquad.Reset();
    mPole.Reset();
  }

  void SetState(float value)
  {
    mBiquad.SetState(value);
    mPole.SetState(value);
  }

  float Process(float input)
  {
    float x = mBiquad.Process(input, mG_bq, mA1_bq);
    x = mPole.Process(x, mG_p);
    return x;
  }
};
