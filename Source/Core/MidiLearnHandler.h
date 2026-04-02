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
        // Protected: Mod(1), Vol(7), Pan(10), Sustain(64), 
        // AllSoundOff(120), Reset(121), AllNotesOff(123)
        return cc == 1 || cc == 7 || cc == 10 || cc == 64 || 
               cc == 120 || cc == 121 || cc == 123;
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

    void setLearning(bool learn) { isLearning = learn; if (!learn) learningParamID = ""; }

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
        juce::ValueTree vt("MidiLearn");
        for (auto const& [cc, id] : ccToParam)
        {
            juce::ValueTree entry("Mapping");
            entry.setProperty("cc", cc, nullptr);
            entry.setProperty("paramID", id, nullptr);
            vt.addChild(entry, -1, nullptr);
        }
        return vt;
    }

    /** Deserializes mappings from a ValueTree */
    void loadState(const juce::ValueTree& vt)
    {
        ccToParam.clear();

        if (vt.getType().toString() != "MidiLearn") return;
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto child = vt.getChild(i);
            if (child.hasType("Mapping"))
            {
                int cc = child.getProperty("cc", -1);
                juce::String id = child.getProperty("paramID", "");
                if (cc >= 0 && cc <= 127 && id.isNotEmpty())
                    ccToParam[cc] = id;
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
