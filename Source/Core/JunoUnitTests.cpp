#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

/**
 * JunoUnitTests — ConsoleRunner
 *
 * This file is the test runner entry point. All individual test classes
 * have been extracted to Source/Core/Tests/*.cpp.
 *
 * See:
 *   Source/Core/Tests/DCOTests.cpp              — JunoDCOTests, JunoSubOscTests, JunoNoiseTests
 *   Source/Core/Tests/ADSRTests.cpp             — JunoADSRTests, JunoADSRTimingTest
 *   Source/Core/Tests/VCFTests.cpp              — JunoVCFTests
 *   Source/Core/Tests/ChorusTests.cpp           — ChorusBBDTests
 *   Source/Core/Tests/SysExTests.cpp            — JunoSysExTests
 *   Source/Core/Tests/TapeTests.cpp             — JunoTapeTests
 *   Source/Core/Tests/FormatConverterTests.cpp  — JunoFormatConverterTests
 *   Source/Core/Tests/DCBCorrectorTests.cpp     — JunoDcbCorrectorTests
 *   Source/Core/Tests/SmartTapeTests.cpp        — JunoSmartTapeTests
 *   Source/Core/Tests/SmartImportHtmlTests.cpp  — JunoSmartImportHtmlTests
 *   Source/Core/Tests/MemoryTests.cpp           — JunoMemoryTests
 *   Source/Core/Tests/UnisonTests.cpp           — JunoUnisonTests
 *   Source/Core/Tests/PresetBrowserTests.cpp    — PresetBrowser layout tests
 */

class ConsoleRunner : public juce::UnitTestRunner
{
    void logMessage(const juce::String& message) override
    {
        std::printf("%s\n", message.toRawUTF8());
    }
};

int main()
{
    std::printf("--- JUNO UNIT TESTS STARTING ---\n");
    ConsoleRunner runner;
    runner.setPassesAreLogged(true);
    runner.runAllTests();

    int totalFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i) {
        if (auto* r = runner.getResult(i)) {
            totalFailures += r->failures;
        }
    }

    std::printf("--- JUNO UNIT TESTS FINISHED ---\n");
    return (totalFailures > 0) ? 1 : 0;
}
