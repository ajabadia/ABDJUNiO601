#pragma once
#include <cmath>
#include <JuceHeader.h>

// ============================================================
// Juno-106 HPF — hardware-accurate implementation
//
// Derived from temp_ultramaster/Source/DSP/KR106_HPF.h
// (ngspice AC simulation + schematic analysis).
//
// Position 0: Bass Boost (+10.5 dB low shelf, ~59–170 Hz)
// Position 1: FLAT (bypass)
// Position 2: HPF 236 Hz  (C=.015µF, R_eff=44.9K)
// Position 3: HPF 754 Hz  (C=.0047µF, R_eff=44.9K)
// ============================================================

// ============================================================
// BassBoostFilter — Juno-106 HPF position 0
//
// Two-stage circuit + inverting summer (M5218L op-amps):
//   Stage 1: RC network R1(47K)||C1(.047µF), CA(.01µF) shunt
//   Stage 2: non-inverting amp, Rg(10K), Rf(100K)||Cf(.022µF)
//   Summer:  R43(47K) direct + R44(220K) boost + R45(47K) feedback
//
// Transfer function:
//   DC gain  = +10.50 dB  (20·log10(1 + 11·R45/R44))
//   HF gain  = +1.41  dB
//   Poles: ~59 Hz, ~72 Hz   Zeros: ~72 Hz, ~170 Hz
//
// Verified against hardware noise sweep, RMS error 0.55 dB.
// ============================================================
struct BassBoostFilter
{
    // Component values from schematic
    static constexpr float kR1  = 47e3f;
    static constexpr float kC1  = 0.047e-6f;
    static constexpr float kCA  = 0.01e-6f;
    static constexpr float kRg  = 10e3f;
    static constexpr float kRf  = 100e3f;
    static constexpr float kCf  = 0.022e-6f;
    static constexpr float kR43 = 47e3f;
    static constexpr float kR44 = 220e3f;
    static constexpr float kR45 = 47e3f;

    float b0 = 1.f, b1 = 0.f, b2 = 0.f;
    float a1 = 0.f, a2 = 0.f;
    double z1 = 0.0, z2 = 0.0;

    void init(float sampleRate)
    {
        const float tau_1z = kR1 * kC1;
        const float tau_1p = kR1 * (kC1 + kCA);
        const float tau_2z = (kRg * kRf / (kRg + kRf)) * kCf;
        const float tau_2p = kRf * kCf;

        const float G2_dc  = 1.f + kRf / kRg;   // 11.0
        const float alpha  = kR45 / kR44;        // 0.2136
        const float direct = kR45 / kR43;        // 1.0

        const float D0 = 1.f;
        const float D1 = tau_1p + tau_2p;
        const float D2 = tau_1p * tau_2p;

        const float Nb0 = 1.f;
        const float Nb1 = tau_1z + tau_2z;
        const float Nb2 = tau_1z * tau_2z;

        const float ag = alpha * G2_dc;
        const float N0 = direct * D0 + ag * Nb0;
        const float N1 = direct * D1 + ag * Nb1;
        const float N2 = direct * D2 + ag * Nb2;

        // Bilinear transform (no pre-warp needed: poles at 60–200 Hz << Nyquist)
        const float K  = 2.f * sampleRate;
        const float K2 = K * K;
        const float a0 = D0 + D1 * K + D2 * K2;
        b0 = (N0 + N1 * K + N2 * K2) / a0;
        b1 = 2.f * (N0 - N2 * K2) / a0;
        b2 = (N0 - N1 * K + N2 * K2) / a0;
        a1 = 2.f * (D0 - D2 * K2) / a0;
        a2 = (D0 - D1 * K + D2 * K2) / a0;

        reset();
    }

    void reset() { z1 = z2 = 0.0; }

    float process(float x)
    {
        float y = b0 * x + static_cast<float>(z1);
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// ============================================================
// JunoHPF — combines BassBoostFilter (pos 0) +
//            1-pole TPT HPF (pos 2/3) + bypass (pos 1)
//
// The hardware uses a 1-pole RC high-pass (−6 dB/oct) for
// positions 2 and 3. We model it as a 1-pole TPT bilinear.
// ============================================================
struct JunoHPF
{
    BassBoostFilter bassBoost;

    // 1-pole TPT state
    float hpState = 0.f;
    float hpG     = 0.f;    // TPT coefficient

    int   currentPos       = 1;     // 0=BassBoost, 1=Flat, 2=HPF mid, 3=HPF high
    float sampleRate       = 44100.f;
    float bassBoostGain    = 1.0f;  // Calibration scale (1.0 = hardware accurate)

    void prepare(float sr)
    {
        sampleRate = sr;
        bassBoost.init(sr);
        reset();
    }

    // Re-init BassBoostFilter when sample rate changes mid-session
    void reinit(float sr)
    {
        sampleRate = sr;
        bassBoost.init(sr);
    }

    void reset()
    {
        hpState = 0.f;
        bassBoost.reset();
    }

    // Call when position or calibration values change
    void setPosition(int pos, float freq2Hz, float freq3Hz, float bbGain = 1.0f)
    {
        currentPos     = pos;
        bassBoostGain  = bbGain;
        float freqHz   = 0.f;
        if (pos == 2) freqHz = freq2Hz;
        else if (pos == 3) freqHz = freq3Hz;

        if (freqHz > 0.f)
        {
            // TPT 1-pole coefficient: g = tan(pi*fc/fs)
            float fc = std::min(freqHz / sampleRate, 0.49f);
            hpG = std::tan(juce::MathConstants<float>::pi * fc);
        }
        else
        {
            hpG = 0.f;
        }
    }

    // Process one sample
    float process(float x)
    {
        switch (currentPos)
        {
            case 0:  // Bass Boost (hardware biquad circuit)
                return bassBoost.process(x) * bassBoostGain;

            case 1:  // Flat — bypass
                return x;

            case 2:  // HPF 236 Hz (1-pole TPT, -6 dB/oct like hardware RC)
            case 3:  // HPF 754 Hz (1-pole TPT, -6 dB/oct like hardware RC)
            {
                if (hpG <= 0.f) return x;
                float v   = (x - hpState) * hpG / (1.f + hpG);
                float lp  = hpState + v;
                hpState   = lp + v;
                return x - lp;
            }

            default:
                return x;
        }
    }
};
