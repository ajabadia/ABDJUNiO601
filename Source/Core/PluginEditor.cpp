
#include "PluginEditor.h"
#include "PresetManager.h"
#include "BuildVersion.h"

SimpleJuno106AudioProcessorEditor::SimpleJuno106AudioProcessorEditor (SimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      lfoSection(p.getAPVTS(), p.getMidiLearnHandler()),
      dcoSection(p.getAPVTS(), p.getMidiLearnHandler()),
      hpfSection(p.getAPVTS(), p.getMidiLearnHandler()),
      vcfSection(p.getAPVTS(), p.getMidiLearnHandler()),
      vcaSection(p.getAPVTS(), p.getMidiLearnHandler()),
      envSection(p.getAPVTS(), p.getMidiLearnHandler()),
      chorusSection(p.getAPVTS(), p.getMidiLearnHandler()),
      controlSection(p, p.getAPVTS(), *p.getPresetManager(), p.getMidiLearnHandler()),
      performanceSection(p.getAPVTS(), p.getMidiLearnHandler()),
      midiKeyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1500, 700); 

    addAndMakeVisible(lfoSection);
    addAndMakeVisible(dcoSection);
    addAndMakeVisible(hpfSection);
    addAndMakeVisible(vcfSection);
    addAndMakeVisible(vcaSection);
    addAndMakeVisible(envSection);
    addAndMakeVisible(chorusSection);
    addAndMakeVisible(controlSection);
    addAndMakeVisible(performanceSection);
    addAndMakeVisible(midiKeyboard);
    
    // === CALLBACKS ===
    controlSection.onPresetLoad = [this](int index) {
        audioProcessor.loadPreset(index);
    };
    
    controlSection.onDump = [this] {
        audioProcessor.sendPatchDump();
    };

    controlSection.dumpButton.onClick = [this] {
        audioProcessor.sendPatchDump();
    };
    
    // Range: C2 (36) to C7 (96) = 61 keys (Authentic Juno-106)
    midiKeyboard.setAvailableRange(36, 96); 
    
    setLookAndFeel(&lookAndFeel);
}

SimpleJuno106AudioProcessorEditor::~SimpleJuno106AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

namespace {
    constexpr int kHeaderHeight = 40;
    constexpr int kSynthHeight = 240;
    constexpr int kCtrlHeight = 150;
    constexpr int kPerfWidth = 220;
    
    // Synth Strip Widths (Total ~1500)
    constexpr int kWidthLFO = 130;
    constexpr int kWidthDCO = 440; 
    constexpr int kWidthHPF = 100;
    constexpr int kWidthVCF = 360; 
    constexpr int kWidthVCA = 140;
    constexpr int kWidthENV = 240;
}

void SimpleJuno106AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (JunoUI::kPanelGrey);
    
    // Identity and Brand
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("JUNiO 601", 10, 0, 200, kHeaderHeight, juce::Justification::centredLeft);
    
    g.setFont(juce::Font(12.0f));
    g.drawText("Build: " JUNO_BUILD_VERSION " (" JUNO_BUILD_TIMESTAMP ")", 
               getWidth() - 310, 0, 300, kHeaderHeight, juce::Justification::centredRight);
}

void SimpleJuno106AudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    
    // 1. Header
    r.removeFromTop(kHeaderHeight);
    
    // 2. Synth Strip
    auto synthArea = r.removeFromTop(kSynthHeight);
    
    lfoSection.setBounds(synthArea.removeFromLeft(kWidthLFO).reduced(1));
    dcoSection.setBounds(synthArea.removeFromLeft(kWidthDCO).reduced(1));
    hpfSection.setBounds(synthArea.removeFromLeft(kWidthHPF).reduced(1));
    vcfSection.setBounds(synthArea.removeFromLeft(kWidthVCF).reduced(1));
    vcaSection.setBounds(synthArea.removeFromLeft(kWidthVCA).reduced(1));
    envSection.setBounds(synthArea.removeFromLeft(kWidthENV).reduced(1));
    chorusSection.setBounds(synthArea.reduced(1)); 
    
    // 3. Control Strip
    controlSection.setBounds(r.removeFromTop(kCtrlHeight).reduced(1));
    
    // 4. Performance & Keyboard
    auto bottomArea = r; 
    performanceSection.setBounds(bottomArea.removeFromLeft(kPerfWidth).reduced(1));
    
    float keyWidth = (float)bottomArea.getWidth() / 36.0f;
    midiKeyboard.setKeyWidth(keyWidth);
    midiKeyboard.setBounds(bottomArea.reduced(1));
}
