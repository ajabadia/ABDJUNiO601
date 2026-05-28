#pragma once

#include <cstdint>
#include "SynthParams.h"

/**
 * JunoProtocol - Centralized Bit-Mapping for Roland Juno-106
 * Aligned with official SysEx and Tape specifications.
 */
class JunoProtocol {
public:
    /**
     * SW1 (Byte 16): Range, Waves, Chorus [VERIFIED HARDWARE SPEC]
     * 
     * Bit 0: 16' Range (1=ON)
     * Bit 1: 8'  Range (1=ON)
     * bit 2: 4'  Range (1=ON)
     * Bit 3: Pulse Wave (1=ON)
     * Bit 4: Saw Wave (1=ON)
     * Bit 5: Chorus Enable (0=ON, 1=OFF) [Active Low]
     * Bit 6: Chorus Mode (0=II, 1=I)
     */
    static uint8_t encodeSW1(const SynthParams& p) {
        uint8_t b = 0;
        
        // Range (Bits 0-2) - Mutually exclusive flag
        if (p.dcoRange == 0) b |= (1 << 0); // 16'
        else if (p.dcoRange == 1) b |= (1 << 1); // 8'
        else if (p.dcoRange == 2) b |= (1 << 2); // 4'

        // Waves (Bits 3-4)
        if (p.pulseOn) b |= (1 << 3);
        if (p.sawOn)   b |= (1 << 4);

        // Chorus (Bits 5-6)
        bool cOn = (p.chorus1 || p.chorus2);
        if (!cOn) b |= (1 << 5); // 1 = OFF
        // If ON, bit 5 is 0. 
        
        // Chorus Mode (Bit 6): 0 = II, 1 = I
        if (p.chorus1) b |= (1 << 6); 
        // if p.chorus2, bit 6 stays 0 (Mode II).

        return b;
    }

    static void decodeSW1(uint8_t b, SynthParams& p) {
        // Bits 0-2: Range
        if (b & (1 << 0))      p.dcoRange = 0;
        else if (b & (1 << 1)) p.dcoRange = 1;
        else if (b & (1 << 2)) p.dcoRange = 2;

        // Bits 3-4: Waves
        p.pulseOn = (b & (1 << 3)) != 0;
        p.sawOn   = (b & (1 << 4)) != 0;

        // Bit 5: Chorus Enable (0 = ON)
        bool cEnabled = (b & (1 << 5)) == 0;
        // Bit 6: Chorus Mode (0 = II, 1 = I)
        bool cModeI = (b & (1 << 6)) != 0;

        if (cEnabled) {
            p.chorus1 = cModeI;
            p.chorus2 = !cModeI;
        } else {
            p.chorus1 = false;
            p.chorus2 = false;
        }
    }

    /**
     * SW2 (Byte 17): PWM, VCA, VCF, HPF [VERIFIED HARDWARE SPEC]
     * 
     * Bit 0: PWM Mode (0=LFO, 1=Manual)
     * Bit 1: VCF Polarity (0=POS, 1=NEG)
     * Bit 2: VCA Mode (0=ENV, 1=GATE)
     * Bits 4+3: HPF (00=3, 01=2, 10=1, 11=0)
     */
    static uint8_t encodeSW2(const SynthParams& p) {
        uint8_t b = 0;
        
        // Bit 0: PWM Mode
        if (p.pwmMode == 1) b |= (1 << 0); 

        // Bit 1: VCF Polarity
        if (p.vcfPolarity == 1) b |= (1 << 1);

        // Bit 2: VCA Mode
        if (p.vcaMode == 1) b |= (1 << 2);

        // Bits 3-4: HPF (Hardware: 3 - hpfFreq)
        int hwHpf = 3 - juce::jlimit(0, 3, p.hpfFreq); 
        b |= (uint8_t)((hwHpf & 0x03) << 3);

        return b;
    }

    static void decodeSW2(uint8_t b, SynthParams& p) {
        p.pwmMode = (b & (1 << 0)) ? 1 : 0;
        p.vcfPolarity = (b & (1 << 1)) ? 1 : 0;
        p.vcaMode = (b & (1 << 2)) ? 1 : 0;
        
        // HPF: 3 - hwHpf
        int hwHpf = (b >> 3) & 0x03;
        p.hpfFreq = 3 - hwHpf;
    }
};
