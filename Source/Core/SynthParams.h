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
 * [Audit] Authentic Juno-106 Chorus Constants (Service Manual Aligned)
 */
struct JunoChorusConstants {
    static constexpr float kRateI = 0.4f;       // Fixed ~0.4 Hz
    static constexpr float kDepthI = 0.12f;
    static constexpr float kDelayI = 6.0f;      

    static constexpr float kRateII = 0.6f;      // Fixed ~0.6 Hz
    static constexpr float kDepthII = 0.25f;    
    static constexpr float kDelayII = 8.0f;     

    static constexpr float kRateIII = 1.0f;     // Mode I+II
    static constexpr float kDepthIII = 0.15f;   
    static constexpr float kDelayIII = 7.0f;    
    
    static constexpr float kNoiseLevel = 0.0005f; 
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
