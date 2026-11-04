// Source/Core/Tests/LfoTablesTests.cpp
//
// Guards the ROM LFO tables (Sprint 17 FW.1 / FW.8): the values must stay the
// byte-for-byte ROM transcription, and there must be exactly one copy of them.

#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>

#include "../LfoTables.h"

class LfoTablesTests : public juce::UnitTest
{
public:
    LfoTablesTests()
        : juce::UnitTest("LFO ROM Tables", "JunoDSP") {}

    void runTest() override
    {
        beginTest("Speed table is the 128-entry ROM transcription");
        {
            expectEquals(LfoTables::kLfoSpeedTableSize, 128);
            expectEquals((int)LfoTables::kDefaultLfoSpeedTbl[0], 0x0005);
            expectEquals((int)LfoTables::kDefaultLfoSpeedTbl[1], 0x000f);
            expectEquals((int)LfoTables::kDefaultLfoSpeedTbl[95], 0x048a);
            expectEquals((int)LfoTables::kDefaultLfoSpeedTbl[96], 0x04be);
            expectEquals((int)LfoTables::kDefaultLfoSpeedTbl[127], 0x1000);
        }

        beginTest("Speed table never decreases");
        {
            // The lower 96 entries are an arithmetic ramp (+0x0a every 15 steps);
            // from index 96 the ROM ramps faster, but the table is still monotonic.
            for (int i = 1; i < LfoTables::kLfoSpeedTableSize; ++i)
                expect(LfoTables::kDefaultLfoSpeedTbl[i] >= LfoTables::kDefaultLfoSpeedTbl[i - 1]);
        }

        beginTest("Ramp table is the 8-entry ROM transcription");
        {
            expectEquals(LfoTables::kLfoRampTableSize, 8);
            expectEquals((int)LfoTables::kDefaultLfoRampTbl[0], 0xFFFF); // delay off
            expectEquals((int)LfoTables::kDefaultLfoRampTbl[1], 0x0419);
            expectEquals((int)LfoTables::kDefaultLfoRampTbl[3], 0x015E);
            for (int i = 4; i < LfoTables::kLfoRampTableSize; ++i)
                expectEquals((int)LfoTables::kDefaultLfoRampTbl[i], 0x0100);
        }

        beginTest("Accessors clamp instead of wrapping");
        {
            expectEquals((int)LfoTables::getLfoSpeedCoeff(-5), 0x0005);
            expectEquals((int)LfoTables::getLfoSpeedCoeff(999), 0x1000);
            expectEquals((int)LfoTables::getLfoSpeedCoeff(64), 0x029a);
            expectEquals((int)LfoTables::getLfoRampIncrement(-1), 0xFFFF);
            expectEquals((int)LfoTables::getLfoRampIncrement(99), 0x0100);
        }
    }
};

static LfoTablesTests lfoTablesTests;
