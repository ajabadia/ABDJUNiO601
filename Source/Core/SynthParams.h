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
    
    float vcfFreq = 1.0f;       
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
    
    float thermalDrift = 0.0f; // [Senior Audit] Global drift
    float tune = 0.5f;          
    int midiChannel = 1;        // [Added for SysEx]
    
    float vcfLFOAmount = 0.0f;     
    float lfoToVCF = 0.0f;         
    float kybdTracking = 0.0f;     
    int vcfPolarity = 0;           
    
    int hpfFreq = 0;               
    
    bool portamentoOn = false;     
    bool portamentoLegato = false; 
    float portamentoTime = 0.0f;   
    bool midiOut = false;          // [Added] Sync for SysEx
};

/**
 * [VCA/Chorus Audit] Authentic Juno-106 Chorus Constants (Service Manual Aligned)
 */
struct JunoChorusConstants {
    // [Fidelidad] Medidas de servicio (BDD capacitor 2.2 uF): 0.47Hz y 0.78Hz
    static constexpr float kRateI = 0.47f;
    static constexpr float kDepthI = 0.15f; 
    static constexpr float kDelayI = 14.5f;   

    static constexpr float kRateII = 0.78f;
    static constexpr float kDepthII = 0.35f;  
    static constexpr float kDelayII = 14.5f;  

    static constexpr float kRateIII = 0.9f;
    static constexpr float kDepthIII = 0.25f;
    static constexpr float kDelayIII = 12.0f;
    
    // [Fidelidad] El nivel de ruido de los BBD es constante e independiente del modo
    static constexpr float kNoiseLevel = 0.0006f; 
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

