#pragma once

#include <JuceHeader.h>

/**
 * SynthParams - Parameter definitions for JUNiO 601
 */
struct SynthParams {
    int dcoRange = 1;           
    bool sawOn = true;          
    bool pulseOn = true;        
    float pwmAmount = 0.5f;     
    int pwmMode = 0;            
    float subOscLevel = 0.0f;   
    float noiseLevel = 0.0f;    
    float lfoToDCO = 0.0f;      
    
    float vcfFreq = 0.8f;       
    float resonance = 0.0f;     
    float envAmount = 0.5f;     
    
    float attack = 0.01f;       
    float decay = 0.3f;         
    float sustain = 0.7f;       
    float release = 0.5f;       
    
    float lfoRate = 0.5f;       
    float lfoDelay = 0.0f;      
    
    bool chorus1 = false;       
    bool chorus2 = false;       
    
    int vcaMode = 0;           
    int chorusMode = 0;         
    int polyMode = 1;           
    
    float vcaLevel = 0.8f;      
    
    float benderValue = 0.0f;   
    float benderToDCO = 1.0f;   
    float benderToVCF = 0.0f;   
    float benderToLFO = 0.0f;   
    
    float drift = 0.0f;            
    float tune = 0.0f;             
    
    float vcfLFOAmount = 0.0f;     
    float lfoToVCF = 0.0f;         
    float kybdTracking = 0.0f;     
    int vcfPolarity = 0;           
    
    int hpfFreq = 0;               
    
    bool portamentoOn = false;     
    bool portamentoLegato = false; 
    float portamentoTime = 0.0f;   
};

/**
 * [VCA/Chorus Audit] Authentic Juno-106 Chorus Constants (Service Manual Aligned)
 */
struct JunoChorusConstants {
    // Juno-106 Service Manual: LFO is ~0.5Hz for I, ~0.83Hz for II
    static constexpr float kRateI = 0.5f;
    static constexpr float kDepthI = 0.15f; // Approximated depth
    static constexpr float kDelayI = 14.5f;   // ~14.5ms delay

    static constexpr float kRateII = 0.83f;
    static constexpr float kDepthII = 0.35f;  // Approximated depth
    static constexpr float kDelayII = 14.5f;  // ~14.5ms delay

    // Mode I+II is not a standard feature. We use values that create a pleasant, richer effect.
    static constexpr float kRateIII = 0.9f;
    static constexpr float kDepthIII = 0.25f;
    static constexpr float kDelayIII = 12.0f;
    
    // Noise level, slightly reduced for musicality
    static constexpr float kNoiseLevel = 0.0004f;
};

struct JunoTimeCurves {
    static constexpr float kAttackMin  = 0.001f; 
    static constexpr float kAttackMax  = 3.0f;   
    static constexpr float kDecayMin   = 0.001f; 
    static constexpr float kDecayMax   = 12.0f;  
    static constexpr float kReleaseMin = 0.001f; 
    static constexpr float kReleaseMax = 12.0f;  

    static constexpr float kLfoMinHz = 0.1f;
    static constexpr float kLfoMaxHz = 30.0f;
};

