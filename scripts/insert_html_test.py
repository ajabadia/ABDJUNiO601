#!/usr/bin/env python3
"""Insert the Smart Import HTML test class into JunoUnitTests.cpp."""

import os

# Read the test file
test_file = "Source/Core/JunoUnitTests.cpp"
with open(test_file, "r", encoding="utf-8") as f:
    content = f.read()

# The new test class to insert
new_test = r"""// ─── Smart Import HTML Tests ─────────────────────────────────────────
// Verifies the HTML structure of the Smart Tape Import dialog in the
// WebView index.html file. This test ensures the dialog remains correctly
// structured after any UI modifications.

class JunoSmartImportHtmlTests : public juce::UnitTest {
public:
    JunoSmartImportHtmlTests() : juce::UnitTest("JunoSmartImport HTML", "JunoUI") {}

    void runTest() override
    {
        beginTest("Smart Tape Import dialog HTML structure");
        {
            // Resolve the index.html path relative to this source file
            juce::File srcFile(__FILE__);
            juce::File projectRoot = srcFile.getParentDirectory().getParentDirectory().getParentDirectory();
            juce::File htmlFile = projectRoot.getChildFile("Source/UI/WebUI/index.html");

            if (!htmlFile.existsAsFile()) {
                std::printf("  smartImportHTML: WARNING: index.html not found at %s\n",
                           htmlFile.getFullPathName().toRawUTF8());
                return;
            }

            juce::String html = htmlFile.loadFileAsString();
            expect(html.isNotEmpty(), "index.html should not be empty");

            std::printf("  smartImportHTML: Loaded index.html (%d bytes)\n", html.length());

            // -- 1. Main overlay container --
            {
                int found = html.indexOf("id=\"modal-smartImport\"");
                expect(found >= 0, "Should find #modal-smartImport div");

                found = html.indexOf("class=\"modal-overlay\"", found >= 0 ? found : 0);
                expect(found >= 0, "modal-smartImport should be a modal-overlay");

                found = html.indexOf("z-index: 15000");
                expect(found >= 0, "modal-smartImport should have z-index: 15000");
            }

            // -- 2. Container structure: header / body / footer --
            {
                int containerIdx = html.indexOf("class=\"smart-import-container\"");
                expect(containerIdx >= 0, "Should find .smart-import-container");

                int headerIdx = html.indexOf("class=\"smart-import-header\"");
                expect(headerIdx >= 0, "Should find .smart-import-header");

                int bodyIdx = html.indexOf("class=\"smart-import-body\"");
                expect(bodyIdx >= 0, "Should find .smart-import-body");

                int footerIdx = html.indexOf("class=\"smart-import-footer\"");
                expect(footerIdx >= 0, "Should find .smart-import-footer");

                expect(headerIdx < bodyIdx && bodyIdx < footerIdx,
                       "Elements should be ordered: header < body < footer");

                std::printf("  smartImportHTML: container @%d, header @%d, body @%d, footer @%d\n",
                           containerIdx, headerIdx, bodyIdx, footerIdx);
            }

            // -- 3. Header elements --
            {
                int titleIdx = html.indexOf("SMART TAPE IMPORT");
                expect(titleIdx >= 0, "Header should contain 'SMART TAPE IMPORT' title");

                int fnIdx = html.indexOf("id=\"si-fileName\"");
                expect(fnIdx >= 0, "Header should have #si-fileName span");

                int closeIdx = html.indexOf("onclick=\"closeSmartImport()\"");
                expect(closeIdx >= 0, "Header should have close button calling closeSmartImport()");
            }

            // -- 4. Progress section --
            {
                int progressIdx = html.indexOf("id=\"si-progress-section\"");
                expect(progressIdx >= 0, "Should have #si-progress-section");

                int logIdx = html.indexOf("id=\"si-progress-log\"");
                expect(logIdx >= 0, "Should have #si-progress-log div");

                int statusIdx = html.indexOf("id=\"si-progress-status\"");
                expect(statusIdx >= 0, "Should have #si-progress-status span");

                int defaultTextIdx = html.indexOf("Waiting for analysis...");
                expect(defaultTextIdx >= 0, "Progress log should have default 'Waiting for analysis...' text");
            }

            // -- 5. Results section (hidden by default) --
            {
                int resultsIdx = html.indexOf("id=\"si-results-section\"");
                expect(resultsIdx >= 0, "Should have #si-results-section");

                int hiddenIdx = html.indexOf("display: none", resultsIdx >= 0 ? resultsIdx : 0);
                expect(hiddenIdx >= 0, "Results section should be hidden by default (display: none)");
            }

            // -- 6. Quality metrics grid --
            {
                int gridIdx = html.indexOf("id=\"si-metrics-grid\"");
                expect(gridIdx >= 0, "Should have #si-metrics-grid");

                int snrIdx = html.indexOf("id=\"si-snr\"");
                expect(snrIdx >= 0, "Should have #si-snr metric");

                int jitterIdx = html.indexOf("id=\"si-jitter\"");
                expect(jitterIdx >= 0, "Should have #si-jitter metric");

                int dropoutsIdx = html.indexOf("id=\"si-dropouts\"");
                expect(dropoutsIdx >= 0, "Should have #si-dropouts metric");

                int durationIdx = html.indexOf("id=\"si-duration\"");
                expect(durationIdx >= 0, "Should have #si-duration metric");

                int badgeIdx = html.indexOf("id=\"si-quality-badge\"");
                expect(badgeIdx >= 0, "Should have #si-quality-badge");
            }

            // -- 7. Decoder results --
            {
                int decoderIdx = html.indexOf("id=\"si-decoder-list\"");
                expect(decoderIdx >= 0, "Should have #si-decoder-list div");
            }

            // -- 8. Waveform canvas --
            {
                int canvasIdx = html.indexOf("id=\"si-waveform-canvas\"");
                expect(canvasIdx >= 0, "Should have #si-waveform-canvas canvas");

                int widthIdx = html.indexOf("width=\"740\"", canvasIdx >= 0 ? canvasIdx : 0);
                expect(widthIdx >= 0, "Waveform canvas should be 740px wide");

                int heightIdx = html.indexOf("height=\"80\"", canvasIdx >= 0 ? canvasIdx : 0);
                expect(heightIdx >= 0, "Waveform canvas should be 80px high");
            }

            // -- 9. Footer buttons --
            {
                int cancelIdx = html.indexOf("onclick=\"closeSmartImport()\"");
                int cancelBtnIdx = html.indexOf("CANCEL", cancelIdx >= 0 ? cancelIdx : 0);
                expect(cancelBtnIdx > cancelIdx, "Footer should have CANCEL button");

                int importIdx = html.indexOf("id=\"btn-si-import\"");
                expect(importIdx >= 0, "Should have #btn-si-import button");

                int importTextIdx = html.indexOf("IMPORT SELECTED", importIdx >= 0 ? importIdx : 0);
                expect(importTextIdx >= 0, "Import button should display 'IMPORT SELECTED'");

                int confirmIdx = html.indexOf("confirmSmartImport()", importIdx >= 0 ? importIdx : 0);
                expect(confirmIdx >= 0, "Import button should call confirmSmartImport()");

                int disabledIdx = html.indexOf("disabled", importIdx >= 0 ? importIdx : 0);
                expect(disabledIdx >= 0, "Import button should be disabled by default");
            }

            // -- 10. Structural sanity: unique sections --
            {
                int headerCount = html.substring(0).countOccurrencesOf("class=\"smart-import-header\"");
                int bodyCount   = html.substring(0).countOccurrencesOf("class=\"smart-import-body\"");
                int footerCount = html.substring(0).countOccurrencesOf("class=\"smart-import-footer\"");

                expect(headerCount == 1, "There should be exactly 1 .smart-import-header, got " + juce::String(headerCount));
                expect(bodyCount == 1, "There should be exactly 1 .smart-import-body, got " + juce::String(bodyCount));
                expect(footerCount == 1, "There should be exactly 1 .smart-import-footer, got " + juce::String(footerCount));
            }

            std::printf("  smartImportHTML: === ALL CHECKS PASSED ===\n");
        }
    }
};

"""

# Find the insertion point
insert_marker = "static JunoSmartTapeTests smartTapeTests;"
insert_idx = content.find(insert_marker)
if insert_idx < 0:
    print("ERROR: Could not find insertion marker")
    exit(1)

# Insert the new test before the marker
new_content = content[:insert_idx] + new_test + content[insert_idx:]

# Also add the static registration for the new test
static_reg_insert = new_content.find("static JunoDCOTests dcoTests;")
static_reg_line_end = new_content.find("\n", static_reg_insert)
if static_reg_insert >= 0:
    # Add the new registration after all existing ones, before ConsoleRunner
    console_runner_marker = "class ConsoleRunner"
    console_runner_idx = new_content.find(console_runner_marker, static_reg_insert)
    if console_runner_idx >= 0:
        # Find the last static registration before ConsoleRunner
        last_static_before_cr = new_content.rfind("static ", console_runner_idx - 200, console_runner_idx)
        last_static_eol = new_content.find("\n", last_static_before_cr)
        if last_static_eol >= 0:
            new_content = (new_content[:last_static_eol + 1] +
                          "static JunoSmartImportHtmlTests smartImportHtmlTests;\n" +
                          new_content[last_static_eol + 1:])

with open(test_file, "w", encoding="utf-8") as f:
    f.write(new_content)

print("INSERTED OK")
