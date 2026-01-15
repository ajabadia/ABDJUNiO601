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
    
    // [Fidelidad] Authentic Spring-Mass Physics
    benderLever.onDragStart = [this] { 
        isDragging = true; 
        stopTimer(); 
        benderVelocity = 0.0;
    };
    
    benderLever.onDragEnd = [this] { 
        isDragging = false; 
        startTimerHz(60); 
    };
    
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
    // g.drawRect(r, 1.0f); // Removed per user request 

    g.setColour(JunoUI::kTextWhite);
    g.setFont(juce::FontOptions("Arial", 11.0f, juce::Font::bold));
    g.drawText("BENDER", getLocalBounds().removeFromBottom(15), juce::Justification::centred);
}

void JunoBender::resized() {
    auto r = getLocalBounds().reduced(5);

    // Título "BENDER" ocupa espacio abajo
    // Título "BENDER" ocupa espacio abajo
    int footerH = 20;
    int benderLeverH = 40;
    
    // 1. Sliders Superiores (Made taller)
    int sliderH = 100; // Fixed taller height for sliders
    auto topArea = r.removeFromTop(sliderH);
    
    // Explicit Gap between Sliders and Bender Lever Area
    r.removeFromTop(10); 
    
    int colW = topArea.getWidth() / 3;

    auto place = [&](juce::Slider& s, juce::Label& l, int idx) {
        l.setBounds(idx * colW, topArea.getY(), colW, 15);
        s.setBounds(idx * colW + (colW - 30)/2, topArea.getY() + 15, 30, topArea.getHeight() - 15);
    };

    place(dcoSlider, dcoLabel, 0);
    place(vcfSlider, vcfLabel, 1);
    place(lfoSlider, lfoLabel, 2);

    // 2. Palanca Bender (In remaining space)
    auto benderArea = r; 
    benderArea.removeFromBottom(footerH);
    
    // Ensure bender lever has its own fixed height and doesn't push others
    benderLever.setBounds(benderArea.getX(), benderArea.getBottom() - benderLeverH, benderArea.getWidth(), benderLeverH);
}

void JunoBender::timerCallback() {
    if (isDragging) return;
    
    double x = benderLever.getValue();
    
    // [Fidelidad] Spring-Mass Constants (Calibrated for Roland Joystick feel)
    const double k = 0.25; // Spring stiffness (Return speed)
    const double d = 0.18; // Damping (Oscillation decay)
    
    double force = -k * x;
    double damping = -d * benderVelocity;
    double acceleration = force + damping;
    
    benderVelocity += acceleration;
    double nextX = x + benderVelocity;
    
    // Snap to zero if settled
    if (std::abs(nextX) < 0.005 && std::abs(benderVelocity) < 0.005) {
        benderLever.setValue(0.0, juce::sendNotificationSync);
        stopTimer();
    } else {
        benderLever.setValue(nextX, juce::sendNotificationSync);
    }
}
