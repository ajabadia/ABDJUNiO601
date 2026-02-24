#include "JunoSysExEngine.h"

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
    body[3]  = (uint8_t) juce::jlimit (0, 127, (int) std::round (params.pwmAmount * 127.0f));
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

    // [Hardware Authenticity] SW1: VCA Mode, Pulse, Saw, Range
    uint8_t sw1 = 0;
    if (params.vcaMode == 1) sw1 |= (1 << 0);
    if (params.pulseOn)      sw1 |= (1 << 1);
    if (params.sawOn)        sw1 |= (1 << 2);
    sw1 |= (uint8_t)((params.dcoRange & 0x03) << 4);
    
    // [Hardware Authenticity] SW2: HPF, Polarity, PWM Mode, Chorus
    uint8_t sw2 = 0;
    sw2 |= (uint8_t)(params.hpfFreq & 0x03);
    if (params.vcfPolarity == 1) sw2 |= (1 << 2);
    if (params.pwmMode == 1)     sw2 |= (1 << 3);

    const bool chorusOn = params.chorus1 || params.chorus2;
    if (!chorusOn)  sw2 |= (1 << 4); // 1 = OFF
    if (params.chorus1) sw2 |= (1 << 5);
    if (params.chorus2) sw2 |= (1 << 6);

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
        case DCO_PWM:    params.pwmAmount = norm; break;
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
             params.sawOn    = (value7bit & (1 << 0)) != 0;
             params.pulseOn  = (value7bit & (1 << 1)) != 0;
             params.pwmMode  = (value7bit & (1 << 2)) ? 1 : 0;
             params.vcaMode  = (value7bit & (1 << 3)) ? 1 : 0;
             params.dcoRange = (value7bit >> 4) & 0x03;
             params.vcfPolarity = (value7bit & (1 << 6)) ? 1 : 0;
            break;

        case SWITCHES_2:
             params.hpfFreq     = (value7bit & 0x03);
             {
                 bool cOff = (value7bit & (1 << 4)) != 0;
                 bool cI   = (value7bit & (1 << 5)) != 0;
                 bool cII  = (value7bit & (1 << 6)) != 0;
                 params.chorus1 = !cOff && cI;
                 params.chorus2 = !cOff && cII;
             }
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

    const uint8_t sw1 = dumpData[16];
    const uint8_t sw2 = dumpData[17];

    params.sawOn       = (sw1 & (1 << 0)) != 0;
    params.pulseOn     = (sw1 & (1 << 1)) != 0;
    params.pwmMode     = (sw1 & (1 << 2)) ? 1 : 0;
    params.vcaMode     = (sw1 & (1 << 3)) ? 1 : 0;
    params.dcoRange    = (sw1 >> 4) & 0x03;
    params.vcfPolarity = (sw1 & (1 << 6)) ? 1 : 0;

    params.hpfFreq     = (sw2 & 0x03);
    
    const bool chorusOff = (sw2 & (1 << 4)) != 0;
    const bool chorusI   = (sw2 & (1 << 5)) != 0;
    const bool chorusII  = (sw2 & (1 << 6)) != 0;
    params.chorus1 = !chorusOff && chorusI;
    params.chorus2 = !chorusOff && chorusII;
}
