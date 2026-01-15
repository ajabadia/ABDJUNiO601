#pragma once
#include <JuceHeader.h>
#include <vector>

/**
 * JunoSysEx - Helper for Roland Juno-106 SysEx protocol
 * Aligned with Official Roland Specs & junpatch.bas (1994)
 */
namespace JunoSysEx
{
    static constexpr uint8_t kRolandID = 0x41;
    static constexpr uint8_t kMsgPatchDump = 0x30;
    static constexpr uint8_t kMsgManualMode = 0x31;
    static constexpr uint8_t kMsgParamChange = 0x32;

    enum ParamID {
        LFO_RATE = 0x00, LFO_DELAY = 0x01, DCO_LFO = 0x02, DCO_PWM = 0x03,
        DCO_NOISE = 0x04, VCF_FREQ = 0x05, VCF_RES = 0x06, VCF_ENV = 0x07,
        VCF_LFO = 0x08, VCF_KYBD = 0x09, VCA_LEVEL = 0x0A, ENV_A = 0x0B,
        ENV_D = 0x0C, ENV_S = 0x0D, ENV_R = 0x0E, DCO_SUB = 0x0F,
        SWITCHES_1 = 0x10, SWITCHES_2 = 0x11
    };

    /** Individual Parameter Change (0x32) - 7 bytes */
    inline juce::MidiMessage createParamChange(int channel, int paramId, int value)
    {
        uint8_t data[7];
        data[0] = 0xF0;
        data[1] = kRolandID;
        data[2] = kMsgParamChange;
        data[3] = static_cast<uint8_t>(channel & 0x0F);
        data[4] = static_cast<uint8_t>(paramId & 0x7F);
        data[5] = static_cast<uint8_t>(value & 0x7F);
        data[6] = 0xF7;
        return juce::MidiMessage(data, 7);
    }

    /** Manual Mode (0x31) - 6 bytes */
    inline juce::MidiMessage createManualMode(int channel)
    {
        uint8_t data[6];
        data[0] = 0xF0;
        data[1] = kRolandID;
        data[2] = kMsgManualMode;
        data[3] = static_cast<uint8_t>(channel & 0x0F);
        data[4] = 0x00; 
        data[5] = 0xF7;
        return juce::MidiMessage(data, 6);
    }

    static constexpr int kPatchDumpSize = 23;

    /** Patch Dump (0x30) - 23 bytes (F0, 41, 30, Channel, 18-byte Body, F7) */
    inline juce::MidiMessage createPatchDump(int channel, const uint8_t* params16, uint8_t sw1, uint8_t sw2)
    {
        uint8_t data[kPatchDumpSize]; 
        data[0] = 0xF0;
        data[1] = kRolandID;
        data[2] = kMsgPatchDump;
        data[3] = static_cast<uint8_t>(channel & 0x0F);

        memcpy(data + 4, params16, 16);
        data[20] = sw1 & 0x7F;
        data[21] = sw2 & 0x7F;
        data[22] = 0xF7;
        
        return juce::MidiMessage(data, kPatchDumpSize); 
    }

    inline bool parseMessage(const juce::MidiMessage& msg, int& type, int& channel, int& p1, int& p2, uint8_t* dumpBody18Bytes)
    {
        if (!msg.isSysEx()) return false;
        
        const uint8_t* data = msg.getSysExData();
        const int size = msg.getSysExDataSize();
        
        if (size < 2 || data[0] != kRolandID) return false;
        
        type = data[1];
        channel = data[2] & 0x0F;

        if (type == kMsgParamChange && size >= 7) { 
             // F0 41 32 ch p v F7 (7 bytes)
            p1 = data[4] & 0x7F;
            p2 = data[5] & 0x7F;
            return true;
        }
        else if (type == kMsgPatchDump && size >= 23) { 
            // F0 41 30 ch [16 params] [sw1] [sw2] F7 = 23 bytes
            if (dumpBody18Bytes) {
                memcpy(dumpBody18Bytes, data + 3, 18); // offset 4 if 23 bytes? No, data starts at data[3]? 
                // Wait, MidiMessage::getSysExData() returns the body excluding F0/F7? 
                // JUCE: getSysExData returns the bytes AFTER F0. 
                // Standard: F0 41 30 ch ... F7. 
                // data[0]=41, data[1]=30, data[2]=ch. Data starts at data[3].
                memcpy(dumpBody18Bytes, data + 3, 18);
            }
            return true;
        }
        else if (type == kMsgManualMode && size >= 6) {
             return true;
        }
        return false;
    }

    /**
     * Checks all parameters between 'oldP' and 'newP' and generates SysEx messages
     * for any differences. Handles packed "Switch" bytes efficiently.
     */
    inline void checkSend(const SynthParams& oldP, const SynthParams& newP, int channel, juce::MidiBuffer& buffer, int samplePos)
    {
        // Macros for simple comparisons (7-bit sliders)
        #define CHECK_VAR(param, id, maxVal) \
            if (std::abs(oldP.param - newP.param) > 0.005f) { \
                int v = (int)(newP.param * maxVal); \
                if (v > 127) v = 127; \
                buffer.addEvent(createParamChange(channel, id, v), samplePos); \
            }

        // LFO
        CHECK_VAR(lfoRate, LFO_RATE, 127.0f);
        CHECK_VAR(lfoDelay, LFO_DELAY, 127.0f);
        
        // DCO
        CHECK_VAR(lfoToDCO, DCO_LFO, 127.0f);
        CHECK_VAR(pwmAmount, DCO_PWM, 127.0f);
        CHECK_VAR(noiseLevel, DCO_NOISE, 127.0f);
        CHECK_VAR(subOscLevel, DCO_SUB, 127.0f);
        
        // VCF
        CHECK_VAR(vcfFreq, VCF_FREQ, 127.0f);
        CHECK_VAR(resonance, VCF_RES, 127.0f);
        CHECK_VAR(envAmount, VCF_ENV, 127.0f);
        CHECK_VAR(lfoToVCF, VCF_LFO, 127.0f);
        CHECK_VAR(kybdTracking, VCF_KYBD, 127.0f);
        
        // VCA
        CHECK_VAR(vcaLevel, VCA_LEVEL, 127.0f);
        
        // ENV
        CHECK_VAR(attack, ENV_A, 127.0f);
        CHECK_VAR(decay, ENV_D, 127.0f);
        CHECK_VAR(sustain, ENV_S, 127.0f);
        CHECK_VAR(release, ENV_R, 127.0f);
        
        #undef CHECK_VAR
        
        // PACKED SWITCHES
        // We reconstruct the full bytes and see if they differ.
        
        // Switch 1: Chorus, HPF
        // HPF: 0-3 (Bits 0-1? Or 0-2? Juno HPF is 0(off), 1, 2, 3) 
        // Real Juno-106: HPF 0-3 maps to low bits.
        // Chorus: Off(0), I(1), II(2). Bits 4-6? 
        // 106 Service Manual: 
        // Bit 0,1: HPF (0-3)
        // Bit 4: Chorus 1
        // Bit 5: Chorus 2
        // Bit 6: Chorus? (Usually Ch1 | Ch2<<1)
        
        auto makeSw1 = [](const SynthParams& p) -> uint8_t {
            uint8_t b = (uint8_t)(p.hpfFreq & 0x03); 
            if (p.chorus1) b |= (1 << 4);
            if (p.chorus2) b |= (1 << 5); // Usually distinct bits
            return b;
        };

        // Switch 2: Pulse, Saw, Range, VCA Mode, Polarity
        // 106 Service Manual:
        // Bit 0: VCA Mode (0=Env, 1=Gate)
        // Bit 1: Pulse 
        // Bit 2: Saw
        // Bit 3: Unused?
        // Bit 4-5: Range (16'=1, 8'=2, 4'=3 ?) Or 0,1,2?
        // Bit 6: VCF Polarity (0=Norm, 1=Inv)
        
        auto makeSw2 = [](const SynthParams& p) -> uint8_t {
             uint8_t b = 0;
             if (p.vcaMode == 1) b |= 1; // 1=Gate
             if (p.pulseOn) b |= (1 << 1);
             if (p.sawOn) b |= (1 << 2);
             
             // Range: 0=16, 1=8, 2=4.
             // Manual: Range is Bit 4,5. 
             // 16' -> 1? 8' -> 2? 4' -> 3?
             // Standard: 0=16, 1=8, 2=4.
             b |= ((p.dcoRange & 3) << 4); 
             
             if (p.vcfPolarity == 1) b |= (1 << 6);
             return b;
        };

        uint8_t oldSw1 = makeSw1(oldP);
        uint8_t newSw1 = makeSw1(newP);
        if (oldSw1 != newSw1) {
            buffer.addEvent(createParamChange(channel, SWITCHES_1, newSw1), samplePos);
        }

        uint8_t oldSw2 = makeSw2(oldP);
        uint8_t newSw2 = makeSw2(newP);
        if (oldSw2 != newSw2) {
             buffer.addEvent(createParamChange(channel, SWITCHES_2, newSw2), samplePos);
        }
    }
}
