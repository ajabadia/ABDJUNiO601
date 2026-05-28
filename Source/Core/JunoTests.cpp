#include "JunoTests.h"
#include "PresetManager.h"
#include "BaseClass/PresetManagerBase.h"
#include "SynthParams.h"
#include "JunoSysExEngine.h"
#include "JunoProtocol.h"
#include <JuceHeader.h>
#include <cmath>
#include <cstring>

namespace JunoTests
{
    static bool compareBool(const juce::ValueTree& a, const juce::ValueTree& b, const juce::Identifier& id) {
        bool vA = (bool)a.getProperty(id, false);
        bool vB = (bool)b.getProperty(id, false);
        if (vA != vB) {
            juce::Logger::writeToLog("[TEST] MISMATCH [" + id.toString() + "]: expected " + (vA ? "true" : "false") + ", got " + (vB ? "true" : "false"));
            return false;
        }
        return true;
    }

    static bool compareInt(const juce::ValueTree& a, const juce::ValueTree& b, const juce::Identifier& id) {
        int vA = (int)std::round((float)a.getProperty(id, 0));
        int vB = (int)std::round((float)b.getProperty(id, 0));
        if (vA != vB) {
            juce::Logger::writeToLog("[TEST] MISMATCH [" + id.toString() + "]: expected " + juce::String(vA) + ", got " + juce::String(vB));
            return false;
        }
        return true;
    }

    static bool compareFloat(const juce::ValueTree& a, const juce::ValueTree& b, const juce::Identifier& id, float eps = 1.0f / 127.0f) {
        float vA = (float)a.getProperty(id, 0.0f);
        float vB = (float)b.getProperty(id, 0.0f);
        if (std::abs(vA - vB) > eps) {
            juce::Logger::writeToLog("[TEST] MISMATCH [" + id.toString() + "]: expected " + juce::String(vA) + ", got " + juce::String(vB) + " (diff: " + juce::String(std::abs(vA - vB)) + ")");
            return false;
        }
        return true;
    }

    void runJunoPatchRoundtripTest (PresetManager& pm) {
        int f = 0;
        juce::StringArray dummy;
        runJunoPatchRoundtripTest(pm, f, dummy);
    }

    void runJunoPatchRoundtripTest (PresetManager& pm, int& failuresOut)
    {
        juce::StringArray dummy;
        runJunoPatchRoundtripTest(pm, failuresOut, dummy);
    }

    void runJunoPatchRoundtripTest (PresetManager& pm, int& failuresOut, juce::StringArray& failedNamesOut)
    {
        juce::Logger::writeToLog ("[TEST] Juno patch roundtrip START (Full 128 library)");
        pm.loadFactoryPresets();
        
        if (pm.libraries_.empty()) {
            juce::Logger::writeToLog ("[TEST] No libraries to test");
            return;
        }

        const int libIdx = 0; 
        pm.selectLibrary(libIdx);
        const int numPresets = (int)pm.libraries_[libIdx].patches.size();
        
        int failures = 0;
        for (int i = 0; i < numPresets; ++i) {
            auto* original = pm.getPreset (i);
            if (original == nullptr) continue;

            const juce::ValueTree& stateA = original->state;
            auto bytes = pm.stateToBytes(stateA); 
            jassert (bytes.size() == 18);

            uint8_t raw[18];
            std::memcpy (raw, bytes.data(), 18);

            auto rebuiltPreset = pm.createPresetFromJunoBytes ("TEST", raw);
            const juce::ValueTree& stateB = rebuiltPreset.state;

            bool ok = true;
            ok &= compareInt   (stateA, stateB, "dcoRange");
            ok &= compareBool  (stateA, stateB, "sawOn");
            ok &= compareBool  (stateA, stateB, "pulseOn");
            ok &= compareBool  (stateA, stateB, "chorus1");
            ok &= compareBool  (stateA, stateB, "chorus2");
            ok &= compareBool  (stateA, stateB, "pwmMode");
            ok &= compareBool  (stateA, stateB, "vcaMode");
            ok &= compareBool  (stateA, stateB, "vcfPolarity");
            ok &= compareInt   (stateA, stateB, "hpfFreq");

            ok &= compareFloat (stateA, stateB, "lfoRate");
            ok &= compareFloat (stateA, stateB, "lfoDelay");
            ok &= compareFloat (stateA, stateB, "lfoToDCO");
            ok &= compareFloat (stateA, stateB, "pwm");
            ok &= compareFloat (stateA, stateB, "noise");
            ok &= compareFloat (stateA, stateB, "vcfFreq");
            ok &= compareFloat (stateA, stateB, "resonance");
            ok &= compareFloat (stateA, stateB, "envAmount");
            ok &= compareFloat (stateA, stateB, "lfoToVCF");
            ok &= compareFloat (stateA, stateB, "kybdTracking");
            ok &= compareFloat (stateA, stateB, "vcaLevel");
            ok &= compareFloat (stateA, stateB, "attack");
            ok &= compareFloat (stateA, stateB, "decay");
            ok &= compareFloat (stateA, stateB, "sustain");
            ok &= compareFloat (stateA, stateB, "release");
            ok &= compareFloat (stateA, stateB, "subOsc");

            if (!ok) {
                juce::Logger::writeToLog ("[TEST] FAILED: " + original->name + " (index " + juce::String(i) + ")");
                failures++;
                failedNamesOut.add("[" + juce::String(i) + "] " + original->name);
            }
        }
        
        failuresOut = failures;
        if (failures == 0) {
            juce::Logger::writeToLog ("[TEST] Juno patch roundtrip OK for all " + juce::String(numPresets) + " presets");
        } else {
            juce::Logger::writeToLog ("[TEST] Juno patch roundtrip FAILED with " + juce::String(failures) + " errors");
        }
        juce::Logger::writeToLog ("[TEST] Juno patch roundtrip END");
    }

    void runSysExPatchDumpRoundtripTest() {
        bool ok = false;
        runSysExPatchDumpRoundtripTest(ok);
    }

    void runSysExPatchDumpRoundtripTest(bool& okOut)
    {
        juce::Logger::writeToLog ("[TEST] SysEx patch dump roundtrip START");
        okOut = false;

        SynthParams pA;
        pA.dcoRange        = 2;
        pA.sawOn           = true;
        pA.pulseOn         = true;
        pA.pwmAmount       = 0.73f;
        pA.pwmMode         = 1;
        pA.subOscLevel     = 0.41f;
        pA.noiseLevel      = 0.17f;
        pA.lfoToDCO        = 0.33f;
        pA.vcfFreq         = 0.62f;
        pA.resonance       = 0.55f;
        pA.envAmount       = 0.81f;
        pA.lfoToVCF        = 0.27f;
        pA.kybdTracking    = 0.49f;
        pA.vcaLevel        = 0.88f;
        pA.attack          = 0.12f;
        pA.decay           = 0.44f;
        pA.sustain         = 0.38f;
        pA.release         = 0.66f;
        pA.lfoRate         = 0.59f;
        pA.lfoDelay        = 0.21f;
        pA.chorus1         = true;
        pA.chorus2         = false;
        pA.vcaMode         = 1;
        pA.vcfPolarity     = 1;
        pA.hpfFreq         = 2;   

        JunoSysExEngine engine;
        auto msg = engine.makePatchDump (0, pA);
        jassert (msg.isSysEx());

        // Use official parser for real validation
        int type, chan, p1, p2;
        uint8_t dumpData[18];
        if (JunoSysEx::parseMessage(msg, type, chan, p1, p2, dumpData)) {
            if (type == JunoSysEx::kMsgPatchDump) {
                SynthParams pB;
                JunoSysExEngine::applyPatchDump (dumpData, pB);

                bool ok = true;
                auto eqF = [](float a, float b) { return std::abs(a - b) <= 0.02f; }; 

                if (pA.dcoRange != pB.dcoRange) ok = false;
                if (pA.sawOn != pB.sawOn) ok = false;
                if (pA.pulseOn != pB.pulseOn) ok = false;
                if (pA.chorus1 != pB.chorus1) ok = false;
                if (pA.chorus2 != pB.chorus2) ok = false;
                if (pA.pwmMode != pB.pwmMode) ok = false;
                if (pA.vcaMode != pB.vcaMode) ok = false;
                if (pA.vcfPolarity != pB.vcfPolarity) ok = false;
                if (pA.hpfFreq != pB.hpfFreq) ok = false;

                ok = ok && eqF(pA.lfoRate, pB.lfoRate);
                ok = ok && eqF(pA.lfoDelay, pB.lfoDelay);
                ok = ok && eqF(pA.lfoToDCO, pB.lfoToDCO);
                ok = ok && eqF(pA.pwmAmount, pB.pwmAmount);
                ok = ok && eqF(pA.noiseLevel, pB.noiseLevel);
                ok = ok && eqF(pA.vcfFreq, pB.vcfFreq);
                ok = ok && eqF(pA.resonance, pB.resonance);
                ok = ok && eqF(pA.envAmount, pB.envAmount);
                ok = ok && eqF(pA.lfoToVCF, pB.lfoToVCF);
                ok = ok && eqF(pA.kybdTracking, pB.kybdTracking);
                ok = ok && eqF(pA.vcaLevel, pB.vcaLevel);
                ok = ok && eqF(pA.attack, pB.attack);
                ok = ok && eqF(pA.decay, pB.decay);
                ok = ok && eqF(pA.sustain, pB.sustain);
                ok = ok && eqF(pA.release, pB.release);
                ok = ok && eqF(pA.subOscLevel, pB.subOscLevel);

                okOut = ok;
                if (!ok) { juce::Logger::writeToLog ("[TEST] SysEx patch dump roundtrip FAILED"); }
                else     { juce::Logger::writeToLog ("[TEST] SysEx patch dump roundtrip OK"); }
            }
        } else {
            juce::Logger::writeToLog ("[TEST] SysEx parse FAILED");
        }
        juce::Logger::writeToLog ("[TEST] SysEx patch dump roundtrip END");
    }

    void runPresetJsonRoundtripTest (PresetManager& pm) {
        bool ok = false;
        runPresetJsonRoundtripTest(pm, ok);
    }

    void runPresetJsonRoundtripTest (PresetManager& pm, bool& okOut)
    {
        juce::Logger::writeToLog ("[TEST] ValueTree XML/JSON Transport roundtrip START");
        okOut = false;
        pm.loadFactoryPresets();
        if (pm.libraries_.empty()) {
            juce::Logger::writeToLog ("[TEST] No libraries to test");
            return;
        }

        const int libIndex = 0; 
        pm.selectLibrary (libIndex);

        std::vector<juce::ValueTree> originalStates;
        const int numPresets = (int)pm.libraries_[libIndex].patches.size();
        for (int i = 0; i < numPresets; ++i) {
            if (auto* p = pm.getPreset (i)) {
                originalStates.push_back (p->state.createCopy());
            }
        }

        juce::DynamicObject::Ptr root (new juce::DynamicObject);
        juce::Array<juce::var> jsonPresets;
        for (int i = 0; i < numPresets; ++i) {
            if (auto* p = pm.getPreset (i)) {
                juce::DynamicObject::Ptr o (new juce::DynamicObject);
                o->setProperty ("name",  p->name);
                o->setProperty ("state", p->state.toXmlString());
                jsonPresets.add (juce::var (o.get()));
            }
        }
        root->setProperty ("libraryName", pm.libraries_[libIndex].name);
        root->setProperty ("presets", jsonPresets);

        const juce::String jsonText = juce::JSON::toString (juce::var (root.get()));

        juce::var parsed = juce::JSON::parse (jsonText);
        auto* parsedRoot = parsed.getDynamicObject();
        if (parsedRoot == nullptr) return;
        
        juce::Array<juce::var> parsedPresets = *parsedRoot->getProperty ("presets").getArray();

        bool okAll = true;
        for (int i = 0; i < (int)originalStates.size(); ++i) {
            if (auto* o = parsedPresets[i].getDynamicObject()) {
                juce::String stateXml = o->getProperty ("state").toString();
                auto xml = juce::parseXML (stateXml);
                if (xml == nullptr) { okAll = false; continue; }
                juce::ValueTree stateB = juce::ValueTree::fromXml (*xml);
                
                if (originalStates[i].toXmlString() != stateB.toXmlString()) {
                    juce::Logger::writeToLog ("[TEST] JSON roundtrip mismatch at preset " + juce::String (i));
                    okAll = false;
                }
            }
        }

        okOut = okAll;
        if (!okAll) {
            juce::Logger::writeToLog ("[TEST] ValueTree XML/JSON Transport roundtrip FAILED");
        } else {
            juce::Logger::writeToLog ("[TEST] ValueTree XML/JSON Transport roundtrip OK");
        }
        juce::Logger::writeToLog ("[TEST] ValueTree XML/JSON Transport roundtrip END");
    }
} // namespace JunoTests
