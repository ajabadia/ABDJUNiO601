/*
 * ABD JUNiO 601 - MidiLearnHandler Unit Tests
 *
 * Tests MIDI CC to parameter binding, specifically:
 *   - CC80 -> delayEchoCancel (RE-201 Echo Cancel footswitch)
 *   - bind/getCCForParam mappings
 *   - handleIncomingCC sets parameter values
 *   - Protected CCs (1, 7, 10, 64, 120, 121, 123) cannot be bound during learn
 *   - One-to-one mapping (unbind previous when binding new)
 *   - Serialization/deserialization of mappings
 */

#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>
#include "MidiLearnHandler.h"

// Minimal test processor that provides required virtual methods
class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor() : AudioProcessor(BusesProperties()) {}
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Test"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return ""; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

// Helper: create a minimal APVTS with the delayEchoCancel parameter
static juce::AudioProcessorValueTreeState createTestAPVTS()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterBool>("delayEchoCancel", "Echo Cancel", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("delayEnabled", "Delay Enabled", false));

    static TestProcessor processor;
    return juce::AudioProcessorValueTreeState(processor, nullptr, "test", std::move(layout));
}

class MidiLearnHandlerTests : public juce::UnitTest
{
public:
    MidiLearnHandlerTests() : juce::UnitTest("MidiLearnHandler Tests", "MidiLearn") {}

    void runTest() override
    {
        // ====================================================================
        // 1. CC80 can be bound to delayEchoCancel
        // ====================================================================
        beginTest("CC80 binds to delayEchoCancel");
        {
            MidiLearnHandler handler;
            handler.bind(80, "delayEchoCancel");

            int cc = handler.getCCForParam("delayEchoCancel");
            expect(cc == 80, "CC80 should be mapped to delayEchoCancel, got CC" + juce::String(cc));
        }

        // ====================================================================
        // 2. handleIncomingCC sets parameter value
        // ====================================================================
        beginTest("handleIncomingCC sets parameter via APVTS");
        {
            MidiLearnHandler handler;
            auto apvts = createTestAPVTS();

            handler.bind(80, "delayEchoCancel");

            // CC80 value=127 (ON) should set delayEchoCancel to 1.0
            handler.handleIncomingCC(80, 127, apvts);

            auto* param = apvts.getParameter("delayEchoCancel");
            expect(param != nullptr, "delayEchoCancel param should exist");
            expect(param->getValue() > 0.5f, "CC80 value 127 should set delayEchoCancel ON, got "
                   + juce::String(param->getValue()));

            // CC80 value=0 (OFF) should set delayEchoCancel to 0.0
            handler.handleIncomingCC(80, 0, apvts);
            expect(param->getValue() <= 0.5f, "CC80 value 0 should set delayEchoCancel OFF, got "
                   + juce::String(param->getValue()));
        }

        // ====================================================================
        // 3. Protected CCs are rejected during learn mode
        // ====================================================================
        beginTest("Protected CCs rejected during learn mode");
        {
            MidiLearnHandler handler;
            auto apvts = createTestAPVTS();

            // Test all protected CCs: 1(Mod), 7(Vol), 10(Pan), 64(Sustain), 120, 121, 123
            int protectedCCs[] = {1, 7, 10, 64, 120, 121, 123};
            for (int cc : protectedCCs)
            {
                handler.startLearning("delayEchoCancel");
                handler.handleIncomingCC(cc, 64, apvts);

                // In learn mode, protected CCs are rejected (isLearning stays true, no binding)
                // So delayEchoCancel should NOT be bound
                expect(handler.getCCForParam("delayEchoCancel") == -1,
                       "Protected CC " + juce::String(cc) + " should be rejected in learn mode");
            }
        }

        // ====================================================================
        // 4. One-to-one mapping: binding new CC unbinds previous
        // ====================================================================
        beginTest("Binding new CC unbinds previous for same parameter");
        {
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            expect(handler.getCCForParam("delayEchoCancel") == 80);

            handler.bind(81, "delayEchoCancel");
            expect(handler.getCCForParam("delayEchoCancel") == 81,
                   "New CC81 should now be mapped");
            // CC80 should be unbound (no parameter mapped to it now)
            expect(handler.getCCForParam("delayEchoCancel") != 80);
        }

        // ====================================================================
        // 5. One-to-one mapping: binding same CC to different param unbinds old param
        // ====================================================================
        beginTest("Binding same CC to different parameter unbinds old parameter");
        {
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            handler.bind(80, "delayEnabled");

            expect(handler.getCCForParam("delayEchoCancel") == -1,
                   "delayEchoCancel should be unbound after CC80 reassigned");
            expect(handler.getCCForParam("delayEnabled") == 80,
                   "delayEnabled should now be mapped to CC80");
        }

        // ====================================================================
        // 6. unbindParam removes mapping
        // ====================================================================
        beginTest("unbindParam removes CC mapping");
        {
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            expect(handler.getCCForParam("delayEchoCancel") == 80);

            handler.unbindParam("delayEchoCancel");
            expect(handler.getCCForParam("delayEchoCancel") == -1,
                   "delayEchoCancel should be unbound");
        }

        // ====================================================================
        // 7. unbindCC removes specific CC mapping
        // ====================================================================
        beginTest("unbindCC removes specific CC mapping");
        {
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            handler.bind(81, "delayEnabled");
            expect(handler.getCCForParam("delayEchoCancel") == 80);

            handler.unbindCC(80);
            expect(handler.getCCForParam("delayEchoCancel") == -1,
                   "delayEchoCancel should be unbound after CC80 unbind");
            expect(handler.getCCForParam("delayEnabled") == 81,
                   "delayEnabled should still be mapped to CC81");
        }

        // ====================================================================
        // 8. clearMappings removes all mappings
        // ====================================================================
        beginTest("clearMappings removes all CC mappings");
        {
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            handler.bind(81, "delayEnabled");
            handler.clearMappings();

            expect(handler.getCCForParam("delayEchoCancel") == -1);
            expect(handler.getCCForParam("delayEnabled") == -1);
        }

        // ====================================================================
        // 9. saveState/loadState serializes mappings correctly
        // ====================================================================
        beginTest("saveState and loadState preserve mappings");
        {
            MidiLearnHandler handler;
            handler.bind(80, "delayEchoCancel");
            handler.bind(81, "delayEnabled");

            auto state = handler.saveState();

            MidiLearnHandler handler2;
            handler2.loadState(state);

            expect(handler2.getCCForParam("delayEchoCancel") == 80,
                   "loadState should restore delayEchoCancel mapping");
            expect(handler2.getCCForParam("delayEnabled") == 81,
                   "loadState should restore delayEnabled mapping");
        }

        // ====================================================================
        // 10. Learn mode is disabled after binding CC (no value is set during learn)
        // ====================================================================
        beginTest("Learn mode disabled after binding CC");
        {
            MidiLearnHandler handler;

            handler.startLearning("delayEchoCancel");
            expect(handler.getIsLearning(), "Should be in learning mode");

            // When learn mode is active, receiving CC80 should bind it
            auto apvts = createTestAPVTS();
            handler.handleIncomingCC(80, 64, apvts); // value=64 is just a test value

            expect(!handler.getIsLearning(), "Learn mode should be disabled after binding");
            expect(handler.getCCForParam("delayEchoCancel") == 80,
                   "CC80 should be bound after learning");

            // NOTE: During learn mode, handleIncomingCC only binds the CC
            // and does NOT set the parameter value. The parameter value
            // is only set when a mapped CC is received in normal mode.
        }

        // ====================================================================
        // 11. CC80 range test: values 0-63 = OFF, 64-127 = ON
        // ====================================================================
        beginTest("CC80 value thresholds: 0-63 OFF, 64-127 ON");
        {
            MidiLearnHandler handler;
            auto apvts = createTestAPVTS();
            handler.bind(80, "delayEchoCancel");

            // Test boundary: value 63 should be OFF
            handler.handleIncomingCC(80, 63, apvts);
            auto* param = apvts.getParameter("delayEchoCancel");
            expect(param->getValue() <= 0.5f, "CC80 value 63 should be OFF (delayEchoCancel <= 0.5)");

            // Test boundary: value 64 should be ON
            handler.handleIncomingCC(80, 64, apvts);
            expect(param->getValue() > 0.5f, "CC80 value 64 should be ON (delayEchoCancel > 0.5)");

            // Test max: value 127
            handler.handleIncomingCC(80, 127, apvts);
            expect(param->getValue() > 0.9f, "CC80 value 127 should be fully ON");
        }
    }
};

static MidiLearnHandlerTests midiLearnTests;

// ============================================================================
// MidiLearnIntegrationTests - Full CC->parameter flow integration tests
// ============================================================================

// Mock AudioProcessor that tracks parameter changes and supports callbacks
class MockAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    MockAudioProcessor() : AudioProcessor(BusesProperties()) {}
    ~MockAudioProcessor() override { if (apvts) apvts->removeParameterListener("delayEchoCancel", this); }

    // juce::AudioProcessorValueTreeState::Listener interface
    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        auto* param = apvts ? apvts->getParameter("delayEchoCancel") : nullptr;
        float oldValue = param ? param->getValue() : 0.0f;
        changes.push_back({"delayEchoCancel", newValue, oldValue});
    }

    // Track parameter value changes
    struct ParameterChange {
        juce::String paramID;
        float newValue;
        float oldValue;
    };
    std::vector<ParameterChange> changes;

    void clearChanges() { changes.clear(); }

    // Create APVTS with delay-related parameters
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add(std::make_unique<juce::AudioParameterBool>("delayEchoCancel", "Echo Cancel", false));
        layout.add(std::make_unique<juce::AudioParameterBool>("delayEnabled", "Delay Enable", false));
        layout.add(std::make_unique<juce::AudioParameterFloat>("delayRepeatRate", "Repeat Rate",
            juce::NormalisableRange(0.0f, 1.0f), 0.5f));
        layout.add(std::make_unique<juce::AudioParameterInt>("delaySyncDivision", "Sync Division",
            0, 8, 2));
        return layout;
    }

    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    void createAPVTS()
    {
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*this, nullptr, "test", createLayout());
        apvts->addParameterListener("delayEchoCancel", this);
    }

    // AudioProcessor required methods
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "MockProcessor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return ""; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

class MidiLearnIntegrationTests : public juce::UnitTest
{
public:
    MidiLearnIntegrationTests() : juce::UnitTest("MidiLearnHandler Integration", "MidiLearn") {}

    void runTest() override
    {
        // ====================================================================
        // 1. End-to-end: CC80 ON/OFF toggles delayEchoCancel parameter
        // ====================================================================
        beginTest("CC80 footswitch toggles delayEchoCancel ON/OFF");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            auto* param = processor.apvts->getParameter("delayEchoCancel");
            expect(param != nullptr);
            expect(param->getValue() <= 0.5f, "Initial state should be OFF");

            // CC80 value=127 (footswitch pressed ON)
            processor.clearChanges();
            handler.handleIncomingCC(80, 127, *processor.apvts);
            expect(param->getValue() > 0.5f, "After CC80=127, delayEchoCancel should be ON");
            expect(processor.changes.size() == 1, "Should have recorded 1 parameter change");
            expect(processor.changes[0].paramID == "delayEchoCancel", "Change should be for delayEchoCancel");

            // CC80 value=0 (footswitch released OFF)
            handler.handleIncomingCC(80, 0, *processor.apvts);
            expect(param->getValue() <= 0.5f, "After CC80=0, delayEchoCancel should be OFF");
        }

        // ====================================================================
        // 2. Rapid CC messages are all processed correctly
        // ====================================================================
        beginTest("Rapid CC80 messages are all processed");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;
            handler.bind(80, "delayEchoCancel");

            // Send rapid sequence: ON, OFF, ON, OFF
            int values[] = {127, 0, 127, 0};
            for (int i = 0; i < 4; ++i)
            {
                processor.clearChanges();
                handler.handleIncomingCC(80, values[i], *processor.apvts);
                expect(processor.changes.size() == 1,
                       "Message " + juce::String(i) + " should produce exactly 1 change");
            }
        }

        // ====================================================================
        // 3. Unmapped CC does not affect any parameter
        // ====================================================================
        beginTest("Unmapped CC does not modify any parameter");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            // CC81 is not bound to anything
            processor.clearChanges();
            handler.handleIncomingCC(81, 127, *processor.apvts);

            auto* param = processor.apvts->getParameter("delayEchoCancel");
            expect(param->getValue() <= 0.5f, "Unmapped CC81 should not affect delayEchoCancel");
            expect(processor.changes.empty(), "No parameter changes should occur");
        }

        // ====================================================================
        // 4. Multiple parameters can be bound and updated
        // ====================================================================
        beginTest("Multiple CCs control different parameters independently");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            handler.bind(80, "delayEchoCancel");
            handler.bind(81, "delayRepeatRate");

            // Set delayEchoCancel ON
            processor.clearChanges();
            handler.handleIncomingCC(80, 127, *processor.apvts);
            expect(processor.apvts->getParameter("delayEchoCancel")->getValue() > 0.5f);

            // Set delayRepeatRate to 0.75
            processor.clearChanges();
            handler.handleIncomingCC(81, 96, *processor.apvts); // 96/127 ≈ 0.756
            float repeatRate = processor.apvts->getParameter("delayRepeatRate")->getValue();
            expect(repeatRate > 0.7f && repeatRate < 0.8f,
                   "delayRepeatRate should be ~0.756, got " + juce::String(repeatRate));

            // Verify delayEchoCancel is still ON (not affected by CC81)
            expect(processor.apvts->getParameter("delayEchoCancel")->getValue() > 0.5f);
        }

        // ====================================================================
        // 5. Learn mode flow: user assigns CC to parameter interactively
        // ====================================================================
        beginTest("Learn mode assigns CC to parameter on next incoming CC");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            // User starts learn mode for delaySyncDivision
            handler.startLearning("delaySyncDivision");
            expect(handler.getIsLearning(), "Should be in learn mode");
            expect(handler.getLearningParamID() == "delaySyncDivision",
                   "Should be learning delaySyncDivision");

            // User presses CC20 on MIDI controller
            handler.handleIncomingCC(20, 64, *processor.apvts);

            expect(!handler.getIsLearning(), "Learn mode should end after binding");
            expect(handler.getCCForParam("delaySyncDivision") == 20,
                   "CC20 should now be bound to delaySyncDivision");

            // Verify CC20 controls the parameter
            handler.handleIncomingCC(20, 127, *processor.apvts);
            expect(processor.apvts->getParameter("delaySyncDivision")->getValue() > 0.9f,
                   "CC20 should set delaySyncDivision to max");
        }

        // ====================================================================
        // 6. State serialization survives full round-trip
        // ====================================================================
        beginTest("MIDI mappings survive save/load round-trip");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            // Setup: bind several CCs
            handler.bind(80, "delayEchoCancel");
            handler.bind(81, "delayEnabled");
            handler.bind(82, "delayRepeatRate");

            // Save state
            auto state = handler.saveState();

            // Simulate plugin reload (new handler instance)
            MidiLearnHandler handler2;
            handler2.loadState(state);

            // Verify all mappings restored
            expect(handler2.getCCForParam("delayEchoCancel") == 80);
            expect(handler2.getCCForParam("delayEnabled") == 81);
            expect(handler2.getCCForParam("delayRepeatRate") == 82);

            // Verify all mappings work
            handler2.handleIncomingCC(80, 127, *processor.apvts);
            expect(processor.apvts->getParameter("delayEchoCancel")->getValue() > 0.5f);

            handler2.handleIncomingCC(81, 127, *processor.apvts);
            expect(processor.apvts->getParameter("delayEnabled")->getValue() > 0.5f);

            handler2.handleIncomingCC(82, 64, *processor.apvts);
            expect(processor.apvts->getParameter("delayRepeatRate")->getValue() > 0.4f);
        }

        // ====================================================================
        // 7. CC80 with different value ranges controls float parameter smoothly
        // ====================================================================
        beginTest("CC80 controls float parameter (delayRepeatRate) smoothly");
        {
            MockAudioProcessor processor;
            processor.createAPVTS();
            MidiLearnHandler handler;

            handler.bind(81, "delayRepeatRate");

            // Test linear mapping: 0, 63, 64, 127
            struct TestPoint { int cc; float expectedMin; float expectedMax; };
            TestPoint points[] = {
                {0, 0.0f, 0.05f},
                {63, 0.45f, 0.55f},
                {64, 0.48f, 0.56f},
                {127, 0.95f, 1.0f}
            };

            for (auto& p : points)
            {
                handler.handleIncomingCC(81, p.cc, *processor.apvts);
                float v = processor.apvts->getParameter("delayRepeatRate")->getValue();
                expect(v >= p.expectedMin && v <= p.expectedMax,
                       "CC=" + juce::String(p.cc) + " should give value ~"
                       + juce::String(p.cc / 127.0f, 2) + ", got " + juce::String(v, 3));
            }
        }

        // ====================================================================
        // 8. onMappingChanged callback is triggered
        // ====================================================================
        beginTest("onMappingChanged callback is triggered on bind");
        {
            MidiLearnHandler handler;
            int callbackCount = 0;
            handler.onMappingChanged = [&callbackCount]() { ++callbackCount; };

            handler.bind(80, "delayEchoCancel");
            expect(callbackCount == 1, "Callback should trigger after first bind");

            handler.bind(81, "delayEnabled");
            expect(callbackCount == 2, "Callback should trigger after second bind");

            handler.bind(80, "delayEchoCancel"); // Same binding should still trigger callback
            expect(callbackCount == 3, "Re-binding same CC should trigger callback");
        }
    }
};

static MidiLearnIntegrationTests midiLearnIntegrationTests;