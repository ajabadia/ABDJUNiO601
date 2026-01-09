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

    /** Patch Dump (0x30) - 24 bytes (F0, Header, 18-byte Body, F7) */
    inline juce::MidiMessage createPatchDump(int channel, const uint8_t* params16, uint8_t sw1, uint8_t sw2)
    {
        uint8_t data[24];
        data[0] = 0xF0;
        data[1] = kRolandID;
        data[2] = kMsgPatchDump;
        data[3] = static_cast<uint8_t>(channel & 0x0F);
        data[4] = 0x00; // Patch Number placeholder

        memcpy(data + 5, params16, 16);
        data[21] = sw1 & 0x7F;
        data[22] = sw2 & 0x7F;
        data[23] = 0xF7;

        return juce::MidiMessage(data, 24);
    }

    inline bool parseMessage(const juce::MidiMessage& msg, int& type, int& channel, int& p1, int& p2, uint8_t* dumpBody18Bytes)
    {
        if (!msg.isSysEx()) return false;
        
        const uint8_t* data = msg.getSysExData();
        const int size = msg.getSysExDataSize();
        
        if (size < 2 || data[0] != kRolandID) return false;
        
        type = data[1];
        channel = data[2] & 0x0F;

        if (type == kMsgParamChange && size == 5) {
            p1 = data[3] & 0x7F;
            p2 = data[4] & 0x7F;
            return true;
        }
        else if (type == kMsgPatchDump && size == 22) { // RolandID(1) + Type(1) + Chan(1) + PatchNum(1) + Body(18) = 22
            if (dumpBody18Bytes) {
                memcpy(dumpBody18Bytes, data + 4, 18);
            }
            return true;
        }
        else if (type == kMsgManualMode && size == 4) {
             return true;
        }
        return false;
    }
}
