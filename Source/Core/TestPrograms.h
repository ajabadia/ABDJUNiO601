#pragma once
#include <JuceHeader.h>

struct TestProgram {
    float lfoRate = 0.5f;     
    float lfoDelay = 0.0f;
    float lfoToDCO = 0.0f;    
    int dcoRange = 1;         // 8'
    bool sawOn = false;
    bool pulseOn = false;
    float pwm = 0.0f;
    int pwmMode = 0;          // Manual
    float subOsc = 0.0f;
    float noise = 0.0f;
    int hpfFreq = 1;          // Off/Flat
    float vcfFreq = 1.0f;     // Open
    float resonance = 0.0f;
    float envAmount = 0.0f;
    int vcfPolarity = 0;      
    float kybdTracking = 0.0f;
    float lfoToVCF = 0.0f;
    int vcaMode = 1;          // GATE (Default for Calibration)
    float vcaLevel = 0.8f;    
    float attack = 0.0f;
    float decay = 0.5f;
    float sustain = 1.0f;     
    float release = 0.0f;
    bool chorus1 = false;
    bool chorus2 = false;
};

/**
 * [Audit] Official Roland Juno-106 Service Mode Programs
 */
static inline TestProgram getTestProgram(int index) {
    TestProgram p; 

    switch (index) {
        case 0: // 1: DCO SAW (Tuning)
            p.sawOn = true;
            break;
        case 1: // 2: DCO SQUARE (PWM Adj)
            p.pulseOn = true; p.pwm = 0.5f;
            break;
        case 2: // 3: PWM LFO (LFO depth)
            p.pulseOn = true; p.pwm = 0.5f; p.pwmMode = 1; p.lfoRate = 0.5f;
            break;
        case 3: // 4: SUB OSC (Sub Level)
            p.subOsc = 1.0f; 
            break;
        case 4: // 5: NOISE (Noise Level)
            p.noise = 1.0f;
            break;
        case 5: // 6: ADSR CHECK (Env Adj)
            p.sawOn = true; p.vcaMode = 0; p.vcfFreq = 0.5f; p.envAmount = 0.8f;
            p.attack = 0.1f; p.decay = 0.3f; p.sustain = 0.5f; p.release = 0.3f;
            break;
        case 6: // 7: CHORUS I (Bias Adj)
            p.sawOn = true; p.chorus1 = true;
            break;
        case 7: // 8: CHORUS II (Balance Adj)
            p.sawOn = true; p.chorus2 = true;
            break;
    }
    return p;
}
