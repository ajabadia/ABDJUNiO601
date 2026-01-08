#include "JunoBender.h"

JunoBender::JunoBender()
{
    // Sliders verticales
    JunoUI::setupVerticalSlider(dcoSlider); addAndMakeVisible(dcoSlider); JunoUI::setupLabel(dcoLabel, "DCO", *this);
    JunoUI::setupVerticalSlider(vcfSlider); addAndMakeVisible(vcfSlider); JunoUI::setupLabel(vcfLabel, "VCF", *this);
    JunoUI::setupVerticalSlider(lfoSlider); addAndMakeVisible(lfoSlider); JunoUI::setupLabel(lfoLabel, "LFO", *this);

    // Bender Horizontal
    benderLever.setSliderStyle(juce::Slider::LinearHorizontal);
    benderLever.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    // Establecer rango simétrico para el pitch bend
    benderLever.setRange(-1.0, 1.0, 0.0); 
    benderLever.setValue(0.0);
    // Hacer que vuelva al centro al soltar
    benderLever.setDoubleClickReturnValue(true, 0.0);
    benderLever.onDragEnd = [this] { benderLever.setValue(0.0, juce::sendNotificationSync); };
    
    addAndMakeVisible(benderLever);
}

void JunoBender::attachToParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // IDs deben coincidir exactamente con SynthParams.h
    dcoAttachment = std::make_unique<SliderAttachment>(apvts, "benderToDCO", dcoSlider);
    vcfAttachment = std::make_unique<SliderAttachment>(apvts, "benderToVCF", vcfSlider);
    lfoAttachment = std::make_unique<SliderAttachment>(apvts, "benderToLFO", lfoSlider);
    
    // Bender suele ser MIDI directo, pero si tienes parámetro:
    benderAttachment = std::make_unique<SliderAttachment>(apvts, "bender", benderLever);
}

void JunoBender::paint(juce::Graphics& g) {
    g.fillAll(JunoUI::kBenderBox);

    // Caja blanca alrededor del fader del bender
    auto r = benderLever.getBounds().toFloat().expanded(2.0f);
    g.setColour(juce::Colours::white);
    g.drawRect(r, 1.0f); 

    g.setColour(JunoUI::kTextWhite);
    g.setFont(juce::FontOptions("Arial", 11.0f, juce::Font::bold));
    g.drawText("BENDER", getLocalBounds().removeFromBottom(15), juce::Justification::centred);
}

void JunoBender::resized() {
    auto r = getLocalBounds().reduced(5);

    // Título "BENDER" ocupa espacio abajo
    int footerH = 20;
    
    // 1. Sliders Superiores (Control de cantidad)
    int slidersAreaH = 100; // Altura fija suficiente para ver los sliders
    auto topArea = r.removeFromTop(slidersAreaH);
    
    int colW = topArea.getWidth() / 3;
    int sliderW = 30;

    auto place = [&](juce::Slider& s, juce::Label& l, int idx) {
        l.setBounds(idx * colW, topArea.getY(), colW, 15);
        s.setBounds(idx * colW + (colW - 30)/2, topArea.getY() + 15, 30, topArea.getHeight() - 15);
    };

    place(dcoSlider, dcoLabel, 0);
    place(vcfSlider, vcfLabel, 1);
    place(lfoSlider, lfoLabel, 2);

    // 2. Palanca Bender (Ocupa el resto menos footer)
    auto benderArea = r; 
    benderArea.removeFromBottom(footerH); // Espacio para texto
    
    // Centrar la palanca horizontalmente y verticalmente en su zona
    int benderHeight = 40;
    int benderY = benderArea.getY() + (benderArea.getHeight() - benderHeight) / 2;
    benderLever.setBounds(benderArea.getX(), benderY, benderArea.getWidth(), benderHeight);
}
