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

