#pragma once

#include <JuceHeader.h>

/**
 * SynthParams - Parameter definitions for SimpleJuno106
 * 
 * Simple, flat structure - no complex modular architecture
 */
struct SynthParams {
    // DCO (Complete Juno-106 authentic)
    int dcoRange = 1;           // 0=16', 1=8', 2=4' (octave selector)
    bool sawOn = true;          // On/Off (Authentic: Switched, not mixed)
    bool pulseOn = true;        // On/Off (Authentic: Switched)
    float pwmAmount = 0.5f;     // 0-1 (pulse width or PWM depth)
    int pwmMode = 0;            // 0=MAN, 1=LFO
    float subOscLevel = 0.0f;   // 0-1
    float noiseLevel = 0.0f;    // 0-1 (NEW!)
    float lfoToDCO = 0.0f;      // 0-1 (LFO modulation to pitch) (NEW!)
    
    // VCF
    float vcfFreq = 0.8f;       // 0-1 (mapped to Hz in Voice)
    float resonance = 0.0f;     // 0-1
    float envAmount = 0.5f;     // 0-1 (envelope modulation depth)
    
    // ADSR
    // ADSR (Mapped logarithmically in Voice.cpp)
    // Attack: 1.5ms to 3s
    // Decay: 1.5ms to 12s
    // Release: 1.5ms to 12s
    float attack = 0.01f;       // 0-1 (normalized)
    float decay = 0.3f;         // 0-1 (normalized)
    float sustain = 0.7f;       // 0-1 (linear level)
    float release = 0.5f;       // 0-1 (normalized)
    
    // LFO (Juno-106 authentic)
    // Rate: 0.1Hz to 30Hz (Exponential)
    float lfoRate = 2.0f;       // 0-1 (normalized)
    // lfoDepth removed - Authenticity fix: Global depth doesn't exist.
    // Depths are controlled per-section (DCO, VCF, PWM).
    float lfoDelay = 0.0f;      // 0-5 seconds (fade-in time)
    // lfoDestination removed - authentic Juno-106 has dedicated sliders per section
    
    // Chorus (can combine both)
    bool chorus1 = false;       // Chorus I
    bool chorus2 = false;       // Chorus II
    
    // VCA
    int vcaMode = 0;           // 0=ENV, 1=GATE
    
    // Chorus/Ensemble (Authentic Juno-106)
    int chorusMode = 0;         // 0=Off, 1=I, 2=II, 3=I+II
    
    // POLY Modes (Authentic Juno-106)
    // 1 = POLY1 (round-robin, natural release, smoother)
    // 2 = POLY2 (lowest-free, statically assigned, cleaner/rigid)
    // 3 = UNISON (6 voices per note)
    int polyMode = 1;           // 1=POLY1, 2=POLY2, 3=UNISON
    
    // Master
    float vcaLevel = 0.8f;      // 0-1
    
    // Bender
    float benderValue = 0.0f;   // -1 to +1
    float benderToDCO = 1.0f;   // 0-1 (amount to pitch)
    float benderToVCF = 0.0f;   // 0-1 (amount to filter)
    float benderToLFO = 0.0f;   // 0-1 (amount to LFO rate)
    
    // Analog Character
    float drift = 0.0f;            // 0 to 1, analog drift amount
    float tune = 0.0f;             // ±50 cents (Master Tune)
    
    // VCF Modulation (Juno-106 authentic controls)
    // vcfEnvAmount removed (consolidated with envAmount)
    
    float vcfLFOAmount = 0.0f;     // LFO: 0 to 1 (LFO modulation depth)
    float lfoToVCF = 0.0f;         // LFO: 0 to 1 (authentic LFO to VCF slider)
    float kybdTracking = 0.0f;     // KYBD: 0 to 1 (keyboard tracking amount)
    int vcfPolarity = 0;           // 0=Normal, 1=Inverted
    
    // HPF (High-Pass Filter - Juno-106 authentic)
    int hpfFreq = 0;               // 0=Off, 1=80Hz, 2=180Hz, 3=330Hz
    
    // Portamento
    bool portamentoOn = false;     // ON/OFF
    bool portamentoLegato = false; // ON: Glide only if voices active (Legato). OFF: Always glide.
    float portamentoTime = 0.0f;   // 0-1 (0-5 seconds glide time)
};

/**
 * Authentic Juno-106 Chorus Constants
 * Rates are fixed on the hardware.
 * Noise is BBD clock noise / bucket loss.
 */
struct JunoChorusConstants {
    // Mode I
    static constexpr float kRateI = 0.5f;       // ~0.5 Hz
    static constexpr float kDepthI = 0.12f;
    static constexpr float kDelayI = 6.0f;      // ms

    // Mode II
    static constexpr float kRateII = 0.8f;      // ~0.8 Hz
    static constexpr float kDepthII = 0.25f;    // Deeper
    static constexpr float kDelayII = 8.0f;     // ms

    // Mode I+II
    static constexpr float kRateIII = 1.0f;     // ~1.0 Hz (Fastest)
    static constexpr float kDepthIII = 0.15f;   // Shallow but wobbles
    static constexpr float kDelayIII = 7.0f;    // ms
    
    // BBD Noise Floor
    static constexpr float kNoiseLevel = 0.0005f; // ~ -66dB
};

/**
 * Authentic Juno-106 Timing Curves
 * Mapped from sliders (0-1) to seconds/Hz
 */
struct JunoTimeCurves {
    // Envelope Times (Seconds)
    static constexpr float kAttackMin  = 0.001f; // 1 ms
    static constexpr float kAttackMax  = 3.0f;   // 3 s
    static constexpr float kDecayMin   = 0.001f; // 1 ms
    static constexpr float kDecayMax   = 12.0f;  // 12 s
    static constexpr float kReleaseMin = 0.001f; // 1 ms
    static constexpr float kReleaseMax = 12.0f;  // 12 s

    // LFO Rates (Hz)
    static constexpr float kLfoMinHz = 0.1f;
    static constexpr float kLfoMaxHz = 30.0f;
};

