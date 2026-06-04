#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>

class JunoSmartImportHtmlTests : public juce::UnitTest {
public:
    JunoSmartImportHtmlTests() : juce::UnitTest("JunoSmartImport HTML", "JunoUI") {}

    void runTest() override
    {
        beginTest("Smart Tape Import dialog HTML structure");
        {
            // Resolve the index.html path relative to this source file
            // Source: Source/Core/Tests/SmartImportHtmlTests.cpp
            // Need 4 levels: Tests/ -> Core/ -> Source/ -> project root
            juce::File srcFile(__FILE__);
            juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
            juce::File htmlFile = projectRoot.getChildFile("Source/UI/WebUI/index.html");

            if (!htmlFile.existsAsFile()) {
                std::printf("  smartImportHTML: WARNING: index.html not found at %s\n",
                           htmlFile.getFullPathName().toRawUTF8());
                return;
            }

            juce::String html = htmlFile.loadFileAsString();
            expect(html.isNotEmpty(), "index.html should not be empty");

            std::printf("  smartImportHTML: Loaded index.html (%d bytes)\n", html.length());

            // Helper lambda for counting substring occurrences
            auto countOccurrences = [](const juce::String& str, const juce::String& sub) -> int {
                int count = 0;
                int pos = 0;
                while ((pos = str.indexOf(pos, sub)) >= 0) {
                    count++;
                    pos += sub.length();
                }
                return count;
            };

            // -- 1. Main overlay container --
            {
                int found = html.indexOf(juce::String("id=\"modal-smartImport\""));
                expect(found >= 0, "Should find #modal-smartImport div");

                found = html.indexOf(found, juce::String("class=\"modal-overlay\""));
                expect(found >= 0, "modal-smartImport should be a modal-overlay");

                found = html.indexOf(juce::String("z-index: 15000"));
                expect(found >= 0, "modal-smartImport should have z-index: 15000");
            }

            // -- 2. Container structure: header / body / footer --
            {
                int containerIdx = html.indexOf(juce::String("class=\"smart-import-container\""));
                expect(containerIdx >= 0, "Should find .smart-import-container");

                int headerIdx = html.indexOf(juce::String("class=\"smart-import-header\""));
                expect(headerIdx >= 0, "Should find .smart-import-header");

                int bodyIdx = html.indexOf(juce::String("class=\"smart-import-body\""));
                expect(bodyIdx >= 0, "Should find .smart-import-body");

                int footerIdx = html.indexOf(juce::String("class=\"smart-import-footer\""));
                expect(footerIdx >= 0, "Should find .smart-import-footer");

                expect(headerIdx < bodyIdx && bodyIdx < footerIdx,
                       "Elements should be ordered: header < body < footer");

                std::printf("  smartImportHTML: container @%d, header @%d, body @%d, footer @%d\n",
                           containerIdx, headerIdx, bodyIdx, footerIdx);
            }

            // -- 3. Header elements --
            {
                int titleIdx = html.indexOf(juce::String("SMART TAPE IMPORT"));
                expect(titleIdx >= 0, "Header should contain 'SMART TAPE IMPORT' title");

                int fnIdx = html.indexOf(juce::String("id=\"si-fileName\""));
                expect(fnIdx >= 0, "Header should have #si-fileName span");

                int closeIdx = html.indexOf(juce::String("closeSmartImport()"));
                expect(closeIdx >= 0, "Header should have close button calling closeSmartImport()");
            }

            // -- 4. Progress section --
            {
                int progressIdx = html.indexOf(juce::String("id=\"si-progress-section\""));
                expect(progressIdx >= 0, "Should have #si-progress-section");

                int logIdx = html.indexOf(juce::String("id=\"si-progress-log\""));
                expect(logIdx >= 0, "Should have #si-progress-log div");

                int statusIdx = html.indexOf(juce::String("id=\"si-progress-status\""));
                expect(statusIdx >= 0, "Should have #si-progress-status span");

                int defaultTextIdx = html.indexOf(juce::String("Waiting for analysis..."));
                expect(defaultTextIdx >= 0, "Progress log should have default 'Waiting for analysis...' text");

                // Initial status text
                int analyzingIdx = html.indexOf(juce::String("ANALYZING..."));
                expect(analyzingIdx >= 0, "Progress status should show 'ANALYZING...' initially");
            }

            // -- 5. Results section (hidden by default) --
            {
                int resultsIdx = html.indexOf(juce::String("id=\"si-results-section\""));
                expect(resultsIdx >= 0, "Should have #si-results-section");

                int hiddenIdx = html.indexOf(resultsIdx, juce::String("display: none"));
                expect(hiddenIdx >= 0, "Results section should be hidden by default (display: none)");
            }

            // -- 6. Quality metrics grid --
            {
                int gridIdx = html.indexOf(juce::String("id=\"si-metrics-grid\""));
                expect(gridIdx >= 0, "Should have #si-metrics-grid");

                int snrIdx = html.indexOf(juce::String("id=\"si-snr\""));
                expect(snrIdx >= 0, "Should have #si-snr metric");

                int jitterIdx = html.indexOf(juce::String("id=\"si-jitter\""));
                expect(jitterIdx >= 0, "Should have #si-jitter metric");

                int dropoutsIdx = html.indexOf(juce::String("id=\"si-dropouts\""));
                expect(dropoutsIdx >= 0, "Should have #si-dropouts metric");

                int durationIdx = html.indexOf(juce::String("id=\"si-duration\""));
                expect(durationIdx >= 0, "Should have #si-duration metric");

                int badgeIdx = html.indexOf(juce::String("id=\"si-quality-badge\""));
                expect(badgeIdx >= 0, "Should have #si-quality-badge");
            }

            // -- 7. Decoder results --
            {
                int decoderIdx = html.indexOf(juce::String("id=\"si-decoder-list\""));
                expect(decoderIdx >= 0, "Should have #si-decoder-list div");
            }

            // -- 8. Waveform canvas --
            {
                int canvasIdx = html.indexOf(juce::String("id=\"si-waveform-canvas\""));
                expect(canvasIdx >= 0, "Should have #si-waveform-canvas canvas");

                int widthIdx = html.indexOf(canvasIdx, juce::String("width=\"740\""));
                expect(widthIdx >= 0, "Waveform canvas should be 740px wide");

                int heightIdx = html.indexOf(canvasIdx, juce::String("height=\"80\""));
                expect(heightIdx >= 0, "Waveform canvas should be 80px high");
            }

            // -- 9. Footer buttons --
            {
                // Find the SECOND closeSmartImport() (footer has it too)
                int firstClose = html.indexOf(juce::String("closeSmartImport()"));
                int secondClose = html.indexOf(firstClose + 1, juce::String("closeSmartImport()"));
                int cancelBtnIdx = html.indexOf(secondClose, juce::String("CANCEL"));
                expect(cancelBtnIdx > secondClose, "Footer should have CANCEL button near closeSmartImport()");

                int importIdx = html.indexOf(juce::String("id=\"btn-si-import\""));
                expect(importIdx >= 0, "Should have #btn-si-import button");

                int importTextIdx = html.indexOf(importIdx, juce::String("IMPORT SELECTED"));
                expect(importTextIdx >= 0, "Import button should display 'IMPORT SELECTED'");

                int confirmIdx = html.indexOf(juce::String("confirmSmartImport()"));
                expect(confirmIdx >= 0, "Import button should call confirmSmartImport()");

                int disabledIdx = html.indexOf(importIdx, juce::String("disabled"));
                expect(disabledIdx >= 0, "Import button should be disabled by default");
            }

            // -- 10. Structural sanity: unique sections --
            {
                int headerCount = countOccurrences(html, juce::String("class=\"smart-import-header\""));
                int bodyCount   = countOccurrences(html, juce::String("class=\"smart-import-body\""));
                int footerCount = countOccurrences(html, juce::String("class=\"smart-import-footer\""));

                expect(headerCount == 1, "There should be exactly 1 .smart-import-header, got " + juce::String(headerCount));
                expect(bodyCount == 1, "There should be exactly 1 .smart-import-body, got " + juce::String(bodyCount));
                expect(footerCount == 1, "There should be exactly 1 .smart-import-footer, got " + juce::String(footerCount));
            }

            std::printf("  smartImportHTML: === ALL CHECKS PASSED ===\n");
        }
    }
};
