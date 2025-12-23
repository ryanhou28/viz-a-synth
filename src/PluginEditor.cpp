#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VizASynthAudioProcessorEditor::VizASynthAudioProcessorEditor(VizASynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set editor size
    setSize(800, 600);

    // Setup oscillator type combo box
    oscTypeCombo.addItem("Sine", 1);
    oscTypeCombo.addItem("Saw", 2);
    oscTypeCombo.addItem("Square", 3);
    addAndMakeVisible(oscTypeCombo);

    oscTypeLabel.setText("Oscillator", juce::dontSendNotification);
    oscTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(oscTypeLabel);

    // Setup filter cutoff slider
    cutoffSlider.setSliderStyle(juce::Slider::Rotary);
    cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(cutoffSlider);

    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(cutoffLabel);

    // Setup filter resonance slider
    resonanceSlider.setSliderStyle(juce::Slider::Rotary);
    resonanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(resonanceSlider);

    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    resonanceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(resonanceLabel);

    // Setup ADSR sliders
    attackSlider.setSliderStyle(juce::Slider::LinearVertical);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(attackSlider);

    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(attackLabel);

    decaySlider.setSliderStyle(juce::Slider::LinearVertical);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(decaySlider);

    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(decayLabel);

    sustainSlider.setSliderStyle(juce::Slider::LinearVertical);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(sustainSlider);

    sustainLabel.setText("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sustainLabel);

    releaseSlider.setSliderStyle(juce::Slider::LinearVertical);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible(releaseSlider);

    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(releaseLabel);

    // Create parameter attachments
    auto& apvts = audioProcessor.getAPVTS();

    oscTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "oscType", oscTypeCombo);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "cutoff", cutoffSlider);

    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "resonance", resonanceSlider);

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "attack", attackSlider);

    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "decay", decaySlider);

    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "sustain", sustainSlider);

    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "release", releaseSlider);

    // Start timer for UI updates (60 FPS)
    startTimerHz(60);
}

VizASynthAudioProcessorEditor::~VizASynthAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void VizASynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff1a1a1a));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("Viz-A-Synth", 10, 10, getWidth() - 20, 40, juce::Justification::centred);

    // Control panel background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(20, 60, 350, 520, 10);

    // Visualization area background
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(390, 60, 390, 520, 10);

    // Placeholder text for visualization
    g.setColour(juce::Colours::grey);
    g.setFont(16.0f);
    g.drawText("Oscilloscope / Spectrum", 390, 250, 390, 100, juce::Justification::centred);
    g.setFont(12.0f);
    g.drawText("Coming soon...", 390, 320, 390, 30, juce::Justification::centred);
}

void VizASynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Control panel area (left side)
    auto controlArea = bounds.removeFromLeft(370).reduced(30, 70);

    // Oscillator section
    auto oscArea = controlArea.removeFromTop(80);
    oscTypeLabel.setBounds(oscArea.removeFromLeft(100));
    oscTypeCombo.setBounds(oscArea.reduced(10, 20));

    controlArea.removeFromTop(20); // Spacer

    // Filter section
    auto filterArea = controlArea.removeFromTop(120);
    auto cutoffArea = filterArea.removeFromLeft(filterArea.getWidth() / 2);
    cutoffLabel.setBounds(cutoffArea.removeFromTop(20));
    cutoffSlider.setBounds(cutoffArea);

    resonanceLabel.setBounds(filterArea.removeFromTop(20));
    resonanceSlider.setBounds(filterArea);

    controlArea.removeFromTop(20); // Spacer

    // ADSR section
    auto adsrArea = controlArea.removeFromTop(200);

    auto attackArea = adsrArea.removeFromLeft(adsrArea.getWidth() / 4);
    attackLabel.setBounds(attackArea.removeFromBottom(20));
    attackSlider.setBounds(attackArea);

    auto decayArea = adsrArea.removeFromLeft(adsrArea.getWidth() / 3);
    decayLabel.setBounds(decayArea.removeFromBottom(20));
    decaySlider.setBounds(decayArea);

    auto sustainArea = adsrArea.removeFromLeft(adsrArea.getWidth() / 2);
    sustainLabel.setBounds(sustainArea.removeFromBottom(20));
    sustainSlider.setBounds(sustainArea);

    releaseLabel.setBounds(adsrArea.removeFromBottom(20));
    releaseSlider.setBounds(adsrArea);
}

void VizASynthAudioProcessorEditor::timerCallback()
{
    // Future: Update oscilloscope/spectrum display here
}
