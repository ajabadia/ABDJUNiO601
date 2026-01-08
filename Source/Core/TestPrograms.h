#pragma once
#include <JuceHeader.h>

struct TestProgram {
    float lfoRate = 0.5f;     // "5" -> 0.5
    float lfoDelay = 0.0f;
    float lfoToDCO = 0.8f;    // "8" -> 0.8
    int dcoRange = 1;         // 8'
    bool sawOn = false;
    bool pulseOn = false;
    float pwm = 0.0f;
    int pwmMode = 1;          // Manual
    float subOsc = 0.0f;
    float noise = 0.0f;
    int hpfFreq = 1;          // HPF 1
    float vcfFreq = 1.0f;     // "10" -> 1.0
    float resonance = 0.0f;
    float envAmount = 0.0f;
    int vcfPolarity = 0;      // Normal
    float kybdTracking = 1.0f;// "10" -> 1.0
    float lfoToVCF = 0.0f;
    int vcaMode = 0;          // ENV
    float vcaLevel = 0.5f;    // "5" -> 0.5
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 1.0f;     // "10" -> 1.0
    float release = 0.0f;
    bool chorus1 = false;
    bool chorus2 = false;
};

// Helper to obtain test programs without using C++20 designated initializers
static inline TestProgram getTestProgram(int index) {
    TestProgram p; // Defaults loaded

    switch (index) {
        case 0: // 1: VCA OFFSET
            break;
        case 1: // 2: SUB OSC
            p.subOsc = 1.0f; 
            break;
        case 2: // 3: VCA/VCF GAIN
            p.vcfFreq = 0.63f; p.resonance = 1.0f;
            break;
        case 3: // 4: RAMP WAVE
            p.sawOn = true;
            break;
        case 4: // 5: PWM 50%
            p.pulseOn = true; p.pwm = 0.5f;
            break;
        case 5: // 6: NOISE LEVEL
            p.noise = 1.0f;
            break;
        case 6: // 7: VCF HI/LO
            break;
        case 7: // 8: RE-TRIGGER
            p.pulseOn = true; 
            p.decay = 0.13f; p.sustain = 0.0f; p.release = 0.13f;
            break;
    }
    return p;
}
