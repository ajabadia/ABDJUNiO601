#pragma once
#include <JuceHeader.h>
#include <vector>

/**
 * JunoSysEx - Helper for Roland Juno-106 SysEx protocol
 * Supports:
 * - 0x32 (Individual Parameter Change): F0 41 32 <channel> <paramId> <value> F7
 * - 0x30 (Patch Dump): F0 41 30 <channel> <patchNum> <16 params> <sw1> <sw2> F7
 * - 0x31 (Manual Mode): F0 41 31 <channel> <patchNum?> F7
 */
namespace JunoSysEx
{
    static constexpr uint8_t kRolandID = 0x41;
    // Constants for Patch Dump (0x30)
    static constexpr int kPatchBodySize = 18; // 16 params + SW1 + SW2
    static constexpr int kPatchDumpMinSize = 3 + kPatchBodySize; // F0 41 30 Ch ... body ...
    static constexpr int kPatchDumpFullSize = kPatchDumpMinSize + 1; // ... + Checksum
    static constexpr uint8_t kMsgPatchDump = 0x30;
    static constexpr uint8_t kMsgManualMode = 0x31;
    static constexpr uint8_t kMsgParamChange = 0x32;

    // Parameter Identifiers for 0x32
    enum ParamID {
        LFO_RATE = 0x00,
        LFO_DELAY = 0x01,
        DCO_LFO = 0x02,
        DCO_PWM = 0x03,
        DCO_NOISE = 0x04,
        VCF_FREQ = 0x05,
        VCF_RES = 0x06,
        VCF_ENV = 0x07,
        VCF_LFO = 0x08,
        VCF_KYBD = 0x09,
        VCA_LEVEL = 0x0A,
        ENV_A = 0x0B,
        ENV_D = 0x0C,
        ENV_S = 0x0D,
        ENV_R = 0x0E,
        DCO_SUB = 0x0F,
        SWITCHES_1 = 0x10,
        SWITCHES_2 = 0x11
    };

    /** Calculates Roland Checksum for a data buffer (Sum of bytes % 128 = 0) */
    inline uint8_t calculateRolandChecksum(const std::vector<uint8_t>& data) {
        uint32_t sum = 0;
        for (uint8_t b : data) sum += b;
        uint8_t remainder = sum % 128;
        return (128 - remainder) & 0x7F;
    }

    /** Creates a 7-byte Roland SysEx message for individual parameter changes (0x32) */
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

    /** Creates a Patch Dump message (0x30). Returns full MidiMessage. */
    inline juce::MidiMessage createPatchDump(int channel, const uint8_t* params16, uint8_t sw1, uint8_t sw2)
    {
        // Total size: 4 (Header) + 16 (Params) + 2 (Switches) + 1 (CheckSum) + 1 (F7) = 24 bytes
        // Header: F0 41 30 Ch
        uint8_t data[24];
        
        data[0] = 0xF0;
        data[1] = kRolandID;
        data[2] = kMsgPatchDump;
        data[3] = static_cast<uint8_t>(channel & 0x0F);

        // Body Copy
        for (int i = 0; i < 16; ++i) data[4 + i] = params16[i] & 0x7F;
        data[20] = sw1 & 0x7F;
        data[21] = sw2 & 0x7F;
        
        // Calculate Checksum for the 18 body bytes
        // Body starts at index 4, length 18
        uint32_t sum = 0;
        for (int i = 0; i < 18; ++i) sum += data[4 + i];
        
        data[22] = (128 - (sum % 128)) & 0x7F;
        data[23] = 0xF7;

        return juce::MidiMessage(data, 24);
    }

    /** Parses a MidiMessage to see if it's a valid Juno-106 SysEx. 
        Returns true if match. 
        For PatchDump, fills fixed-size buffer `dumpBody` (must be 18 bytes min).
    */
    inline bool parseMessage(const juce::MidiMessage& msg, int& type, int& channel, int& p1, int& p2, uint8_t* dumpBody18Bytes)
    {
        if (!msg.isSysEx()) return false;
        
        const uint8_t* data = msg.getSysExData();
        int size = msg.getRawDataSize() - 2; // Size between F0..F7

        if (size < 3) return false;
        if (data[0] != kRolandID) return false;
        
        type = data[1];
        channel = data[2] & 0x0F;

        if (type == kMsgParamChange)
        {
            if (size >= 5) {
                p1 = data[3] & 0x7F;
                p2 = data[4] & 0x7F;
                return true;
            }
        }
        else if (type == kMsgPatchDump && size >= 21) // 3 header + 18 body = 21 min (CS is extra but needed for validity if we checked it)
        {
            // We need 18 data bytes
             // Indexes: [0]=41, [1]=30, [2]=Ch, [3..20]=Body (18 bytes)
            if (dumpBody18Bytes) {
                for (int i = 0; i < 18; ++i) {
                     dumpBody18Bytes[i] = data[3 + i];
                }
            }
            return true;
        }
        else if (type == kMsgManualMode) {
            return true;
        }
        
        return false;
    }
}
