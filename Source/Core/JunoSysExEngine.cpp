#include "JunoSysExEngine.h"
#include "JunoProtocol.h"

using namespace JunoSysEx;

void JunoSysExEngine::handleIncomingSysEx (const juce::MidiMessage& msg,
                                           SynthParams& params)
{
    if (msg.getRawDataSize() < 5) return;
    
    // [Fidelidad] Compatibility Check: Handle both Old and New Roland formats
    // Old: F0 41 <Type> <Channel> ...
    // New: F0 41 <DeviceID> <ModelID> ...
    
    int type = 0, ch = 0, p1 = 0, p2 = 0;
    uint8_t dumpData[18]; 
    
    if (! JunoSysEx::parseMessage (msg, type, ch, p1, p2, dumpData)) {
         return;
    }

    // Verify channel matches or it's Omni
    if (ch != (params.midiChannel - 1) && params.midiChannel != 0) {
        // ... (Optional: add strict channel filtering)
    }

    if (type == kMsgParamChange)
    {
        applyParamChange (p1, p2, params);
    }
    else if (type == kMsgPatchDump)
    {
        applyPatchDump (dumpData, params);
    }
}

juce::MidiMessage JunoSysExEngine::makeParamChange (int channel,
                                                    int paramId,
                                                    int value)
{
    return JunoSysEx::createParamChange (channel, paramId, value & 0x7F);
}

juce::MidiMessage JunoSysExEngine::makePatchDump (int channel,
                                                  const SynthParams& params)
{
    uint8_t body[16] {};

    body[0]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.lfoRate * 127.0f));
    body[1]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.lfoDelay * 127.0f));
    body[2]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.lfoToDCO * 127.0f));
    body[3]  = (uint8_t) juce::jlimit (0, 105, (int) std::round (params.pwmAmount * 105.0f));
    body[4]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.noiseLevel * 127.0f));
    body[5]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.vcfFreq * 127.0f));
    body[6]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.resonance * 127.0f));
    body[7]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.envAmount * 127.0f));
    body[8]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.lfoToVCF * 127.0f));
    body[9]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.kybdTracking * 127.0f));
    body[10] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.vcaLevel * 127.0f));
    body[11] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.attack * 127.0f));
    body[12] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.decay * 127.0f));
    body[13] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.sustain * 127.0f));
    body[14] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.release * 127.0f));
    body[15] = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.subOscLevel * 127.0f));

    uint8_t sw1 = JunoProtocol::encodeSW1(params);
    uint8_t sw2 = JunoProtocol::encodeSW2(params);

    return JunoSysEx::createPatchDump (channel, body, sw1, sw2);
}

void JunoSysExEngine::applyParamChange (int paramId,
                                        int value7bit,
                                        SynthParams& params)
{
    const float norm = juce::jlimit (0.0f, 1.0f, value7bit / 127.0f);

    switch (paramId)
    {
        case LFO_RATE:   params.lfoRate = norm; break;
        case LFO_DELAY:  params.lfoDelay = norm; break;
        case DCO_LFO:    params.lfoToDCO = norm; break;
        case DCO_PWM:    params.pwmAmount = juce::jlimit(0.0f, 1.0f, std::min(value7bit, 105) / 105.0f); break;
        case DCO_NOISE:  params.noiseLevel = norm; break;
        case VCF_FREQ:   params.vcfFreq = norm; break;
        case VCF_RES:    params.resonance = norm; break;
        case VCF_ENV:    params.envAmount = norm; break;
        case VCF_LFO:    params.lfoToVCF = norm; break;
        case VCF_KYBD:   params.kybdTracking = norm; break;
        case VCA_LEVEL:  params.vcaLevel = norm; break;
        case ENV_A:      params.attack = norm; break;
        case ENV_D:      params.decay = norm; break;
        case ENV_S:      params.sustain = norm; break;
        case ENV_R:      params.release = norm; break;
        case DCO_SUB:    params.subOscLevel = norm; break;

        case SWITCHES_1:
             JunoProtocol::decodeSW1((uint8_t)value7bit, params);
             break;

        case SWITCHES_2:
             JunoProtocol::decodeSW2((uint8_t)value7bit, params);
             break;

        default:
            break;
    }
}

void JunoSysExEngine::applyPatchDump (const uint8_t* dumpData,
                                      SynthParams& params)
{
    auto v = [&dumpData] (int idx) -> float
    {
        if (idx == 3) // PWM
            return juce::jlimit (0.0f, 1.0f, std::min((int)dumpData[idx], 105) / 105.0f);
        return juce::jlimit (0.0f, 1.0f, dumpData[idx] / 127.0f);
    };

    params.lfoRate     = v (0);
    params.lfoDelay    = v (1);
    params.lfoToDCO    = v (2);
    params.pwmAmount   = v (3);
    params.noiseLevel  = v (4);
    params.vcfFreq     = v (5);
    params.resonance   = v (6);
    params.envAmount   = v (7);
    params.lfoToVCF    = v (8);
    params.kybdTracking = v (9);
    params.vcaLevel    = v (10);
    params.attack      = v (11);
    params.decay       = v (12);
    params.sustain     = v (13);
    params.release     = v (14);
    params.subOscLevel = v (15);

    // [Sprint 10 Fidelity Alignment] Hardware Juno-106 is not velocity sensitive.
    // We force modern velocity scaling to 0 for incoming 18-byte dumps.
    params.velocitySens = 0.0f;

    JunoProtocol::decodeSW1(dumpData[16], params);
    JunoProtocol::decodeSW2(dumpData[17], params);
}
