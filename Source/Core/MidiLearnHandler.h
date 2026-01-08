#pragma once
#include <JuceHeader.h>
#include <map>

/**
 * MidiLearnHandler - Manages MIDI CC to Parameter mappings and Learn mode.
 */
class MidiLearnHandler
{
public:
    MidiLearnHandler() = default;

    static bool isProtectedCC(int cc) {
        return cc == 1 || cc == 64; // Mod Wheel & Sustain
    }

    /** Processes an incoming CC message */
    void handleIncomingCC(int ccNumber, int value, juce::AudioProcessorValueTreeState& apvts)
    {
        if (isLearning && learningParamID.isNotEmpty())
        {
            if (isProtectedCC(ccNumber)) return; // Ignore protected CCs
            
            bind(ccNumber, learningParamID);
            isLearning = false;
            learningParamID = "";
            
            if (onMappingChanged) onMappingChanged();
            return;
        }

        auto it = ccToParam.find(ccNumber);
        if (it != ccToParam.end())
        {
            if (auto* param = apvts.getParameter(it->second))
            {
                float normalizedValue = value / 127.0f;
                param->setValueNotifyingHost(normalizedValue);
            }
        }
    }

    /** Binds a CC number to a parameter ID */
    void bind(int ccNumber, const juce::String& paramID)
    {
        unbindParam(paramID); // Ensure one-to-one
        ccToParam[ccNumber] = paramID;
    }
    
    /** Unbinds a specific CC */
    void unbindCC(int ccNumber) {
        ccToParam.erase(ccNumber);
    }
    
    /** Unbinds a specific Parameter */
    void unbindParam(const juce::String& paramID) {
        for (auto it = ccToParam.begin(); it != ccToParam.end(); ) {
            if (it->second == paramID) it = ccToParam.erase(it);
            else ++it;
        }
    }

    /** Enables learn mode for a specific parameter */
    void startLearning(const juce::String& paramID)
    {
        isLearning = true;
        learningParamID = paramID;
    }

    /** Returns the CC mapped to a parameter, or -1 if none */
    int getCCForParam(const juce::String& paramID) const
    {
        for (auto const& [cc, id] : ccToParam)
        {
            if (id == paramID) return cc;
        }
        return -1;
    }

    /** Reset all mappings */
    void clearMappings() { ccToParam.clear(); }

    /** Serializes mappings to a ValueTree */
    juce::ValueTree saveState() const
    {
        juce::ValueTree vt("MIDI_MAPPINGS");
        for (auto const& [cc, id] : ccToParam)
        {
            juce::ValueTree entry("MAP");
            entry.setProperty("cc", cc, nullptr);
            entry.setProperty("param", id, nullptr);
            vt.appendChild(entry, nullptr);
        }
        return vt;
    }

    /** Deserializes mappings from a ValueTree */
    void loadState(const juce::ValueTree& vt)
    {
        ccToParam.clear(); // Ensure clean slate even if VT is invalid/empty

        if (vt.getType() != juce::Identifier("MIDI_MAPPINGS")) return;
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto child = vt.getChild(i);
            if (child.getType() == juce::Identifier("MAP"))
            {
                int cc = child.getProperty("cc");
                juce::String id = child.getProperty("param");
                if (cc >= 0 && cc <= 127 && id.isNotEmpty())
                {
                    ccToParam[cc] = id;
                }
            }
        }
    }

    bool getIsLearning() const { return isLearning; }
    juce::String getLearningParamID() const { return learningParamID; }

    std::function<void()> onMappingChanged;

private:
    std::map<int, juce::String> ccToParam;
    bool isLearning = false;
    juce::String learningParamID;
};
