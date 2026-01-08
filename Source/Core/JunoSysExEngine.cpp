#include "JunoSysExEngine.h"

using namespace JunoSysEx;

void JunoSysExEngine::handleIncomingSysEx (const juce::MidiMessage& msg,
                                           SynthParams& params)
{
    int type = 0, ch = 0, p1 = 0, p2 = 0;
    uint8_t dumpData[18]; 
    
    if (! JunoSysEx::parseMessage (msg, type, ch, p1, p2, dumpData))
        return;

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

    uint8_t sw1 = 0;
    if (params.dcoRange == 0) sw1 |= (uint8_t)(1 << 0);
    if (params.dcoRange == 1) sw1 |= (uint8_t)(1 << 1);
    if (params.dcoRange == 2) sw1 |= (uint8_t)(1 << 2);
    if (params.pulseOn)       sw1 |= (uint8_t)(1 << 3);
    if (params.sawOn)         sw1 |= (uint8_t)(1 << 4);
    
    const bool chorusOn = params.chorus1 || params.chorus2;
    const bool chorusMode1 = params.chorus1 && !params.chorus2;
    if (chorusOn)    sw1 |= (uint8_t)(1 << 5);
    if (chorusMode1) sw1 |= (uint8_t)(1 << 6);

    uint8_t sw2 = 0;
    if (params.pwmMode == 1)     sw2 |= (uint8_t)(1 << 0);
    if (params.vcaMode == 1)     sw2 |= (uint8_t)(1 << 1);
    if (params.vcfPolarity == 1) sw2 |= (uint8_t)(1 << 2);
    const int hpfVal = juce::jlimit (0, 3, params.hpfFreq);
    sw2 |= (uint8_t) ((hpfVal & 0x03) << 3);

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
             if      (value7bit & (1 << 0)) params.dcoRange = 0;
             else if (value7bit & (1 << 1)) params.dcoRange = 1;
             else if (value7bit & (1 << 2)) params.dcoRange = 2;
             
             params.pulseOn = (value7bit & (1 << 3)) != 0;
             params.sawOn   = (value7bit & (1 << 4)) != 0;
             
             {
                 bool cOn = (value7bit & (1 << 5)) != 0;
                 bool cMode1 = (value7bit & (1 << 6)) != 0;
                 params.chorus1 = cOn && cMode1;
                 params.chorus2 = cOn && !cMode1;
             }
            break;
        case SWITCHES_2:
             params.pwmMode     = (value7bit & (1 << 0)) ? 1 : 0;
             params.vcaMode     = (value7bit & (1 << 1)) ? 1 : 0;
             params.vcfPolarity = (value7bit & (1 << 2)) ? 1 : 0;
             params.hpfFreq     = (value7bit >> 3) & 0x03;
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

    if      (sw1 & (1 << 0)) params.dcoRange = 0;
    else if (sw1 & (1 << 1)) params.dcoRange = 1;
    else if (sw1 & (1 << 2)) params.dcoRange = 2;

    params.pulseOn = (sw1 & (1 << 3)) != 0;
    params.sawOn   = (sw1 & (1 << 4)) != 0;

    const bool chorusOn   = (sw1 & (1 << 5)) != 0;
    const bool chorusMode1 = (sw1 & (1 << 6)) != 0;
    params.chorus1 = chorusOn && chorusMode1;
    params.chorus2 = chorusOn && !chorusMode1;

    params.pwmMode     = (sw2 & (1 << 0)) ? 1 : 0;
    params.vcaMode     = (sw2 & (1 << 1)) ? 1 : 0;
    params.vcfPolarity = (sw2 & (1 << 2)) ? 1 : 0;
    params.hpfFreq     = (sw2 >> 3) & 0x03;
}
