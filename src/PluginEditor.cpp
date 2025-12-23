#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VizASynthAudioProcessorEditor::VizASynthAudioProcessorEditor(VizASynthAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      oscilloscope(p.getProbeManager()),
      spectrumAnalyzer(p.getProbeManager()),
      singleCycleView(p.getProbeManager(),
                      [&]() -> PolyBLEPOscillator& {
                          if (auto* voice = p.getVoice(0)) {
                              return voice->getOscillator();
                          }
                          throw std::runtime_error("No voices available to provide an oscillator.");
                      }())
{
    // Set editor size
    setSize(1000, 600);

    // Add visualization components
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(singleCycleView);

    // Setup visualization mode selector buttons
    scopeButton.setClickingTogglesState(false);
    scopeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a4a4a));
    scopeButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Oscilloscope); };
    addAndMakeVisible(scopeButton);

    spectrumButton.setClickingTogglesState(false);
    spectrumButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    spectrumButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Spectrum); };
    addAndMakeVisible(spectrumButton);

    // Initial visualization mode
    setVisualizationMode(VisualizationMode::Oscilloscope);

    // Setup probe selector buttons
    auto setupProbeButton = [this](juce::TextButton& button, ProbePoint probe, juce::Colour colour)
    {
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
        button.setColour(juce::TextButton::textColourOffId, colour);
        button.onClick = [this, probe]()
        {
            audioProcessor.getProbeManager().setActiveProbe(probe);
            updateProbeButtons();
        };
        addAndMakeVisible(button);
    };

    setupProbeButton(probeOscButton, ProbePoint::Oscillator, Oscilloscope::getProbeColour(ProbePoint::Oscillator));
    setupProbeButton(probeFilterButton, ProbePoint::PostFilter, Oscilloscope::getProbeColour(ProbePoint::PostFilter));
    setupProbeButton(probeOutputButton, ProbePoint::Output, Oscilloscope::getProbeColour(ProbePoint::Output));

    // Freeze button
    freezeButton.setClickingTogglesState(true);
    freezeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    freezeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red.darker());
    freezeButton.onClick = [this]()
    {
        bool frozen = freezeButton.getToggleState();
        oscilloscope.setFrozen(frozen);
        spectrumAnalyzer.setFrozen(frozen);
        singleCycleView.setFrozen(frozen);
    };
    addAndMakeVisible(freezeButton);

    // Clear trace button
    clearTraceButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    clearTraceButton.onClick = [this]()
    {
        oscilloscope.clearFrozenTrace();
        spectrumAnalyzer.clearFrozenTrace();
        singleCycleView.clearFrozenTrace();
    };
    addAndMakeVisible(clearTraceButton);

    // Time window slider
    timeWindowSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timeWindowSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    timeWindowSlider.setRange(1.0, 50.0, 0.5);
    timeWindowSlider.setValue(10.0);
    timeWindowSlider.setTextValueSuffix(" ms");
    timeWindowSlider.onValueChange = [this]()
    {
        oscilloscope.setTimeWindow(static_cast<float>(timeWindowSlider.getValue()));
    };
    addAndMakeVisible(timeWindowSlider);

    timeWindowLabel.setText("Time", juce::dontSendNotification);
    timeWindowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timeWindowLabel);

    // Update probe button states
    updateProbeButtons();

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
}

void VizASynthAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Control panel area (left side)
    auto controlArea = bounds.removeFromLeft(370).reduced(30, 70);

    // Visualization area (right side)
    auto vizArea = bounds.reduced(10, 60);

    // Visualization area (minus control bar at bottom)
    auto scopeArea = vizArea.removeFromTop(vizArea.getHeight() - 50);

    // In Oscilloscope mode: stack oscilloscope and single-cycle view
    // In Spectrum mode: show only spectrum analyzer
    if (currentVizMode == VisualizationMode::Oscilloscope)
    {
        auto topHalf = scopeArea.removeFromTop(scopeArea.getHeight() / 2);
        oscilloscope.setBounds(topHalf.reduced(0, 2));
        singleCycleView.setBounds(scopeArea.reduced(0, 2));
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
    }
    else
    {
        spectrumAnalyzer.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
    }

    // Controls below visualization
    auto vizControlArea = vizArea.reduced(5, 10);

    // Visualization mode selector (Scope / Spectrum)
    scopeButton.setBounds(vizControlArea.removeFromLeft(60));
    vizControlArea.removeFromLeft(2);
    spectrumButton.setBounds(vizControlArea.removeFromLeft(70));

    vizControlArea.removeFromLeft(12);

    // Probe buttons
    probeOscButton.setBounds(vizControlArea.removeFromLeft(45));
    vizControlArea.removeFromLeft(4);
    probeFilterButton.setBounds(vizControlArea.removeFromLeft(45));
    vizControlArea.removeFromLeft(4);
    probeOutputButton.setBounds(vizControlArea.removeFromLeft(45));

    vizControlArea.removeFromLeft(12);

    // Freeze button
    freezeButton.setBounds(vizControlArea.removeFromLeft(55));
    vizControlArea.removeFromLeft(4);

    // Clear trace button
    clearTraceButton.setBounds(vizControlArea.removeFromLeft(50));

    vizControlArea.removeFromLeft(12);

    // Time window control (only relevant for oscilloscope, but always visible)
    timeWindowLabel.setBounds(vizControlArea.removeFromLeft(35));
    timeWindowSlider.setBounds(vizControlArea);

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
    // Update probe button highlighting
    updateProbeButtons();
}

void VizASynthAudioProcessorEditor::updateProbeButtons()
{
    auto activeProbe = audioProcessor.getProbeManager().getActiveProbe();

    auto highlightButton = [](juce::TextButton& button, bool active, juce::Colour colour)
    {
        if (active)
        {
            button.setColour(juce::TextButton::buttonColourId, colour.darker(0.3f));
            button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
            button.setColour(juce::TextButton::textColourOffId, colour);
        }
    };

    highlightButton(probeOscButton, activeProbe == ProbePoint::Oscillator,
                    Oscilloscope::getProbeColour(ProbePoint::Oscillator));
    highlightButton(probeFilterButton, activeProbe == ProbePoint::PostFilter,
                    Oscilloscope::getProbeColour(ProbePoint::PostFilter));
    highlightButton(probeOutputButton, activeProbe == ProbePoint::Output,
                    Oscilloscope::getProbeColour(ProbePoint::Output));
}

void VizASynthAudioProcessorEditor::setVisualizationMode(VisualizationMode mode)
{
    currentVizMode = mode;
    updateVisualizationMode();
}

void VizASynthAudioProcessorEditor::updateVisualizationMode()
{
    bool isScope = (currentVizMode == VisualizationMode::Oscilloscope);

    // Show/hide appropriate visualization
    oscilloscope.setVisible(isScope);
    singleCycleView.setVisible(isScope);
    spectrumAnalyzer.setVisible(!isScope);

    // Update button highlighting
    scopeButton.setColour(juce::TextButton::buttonColourId,
                          isScope ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff3a3a3a));
    spectrumButton.setColour(juce::TextButton::buttonColourId,
                             !isScope ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff3a3a3a));

    // Show/hide time window control (only relevant for oscilloscope)
    timeWindowSlider.setEnabled(isScope);
    timeWindowLabel.setAlpha(isScope ? 1.0f : 0.4f);
    timeWindowSlider.setAlpha(isScope ? 1.0f : 0.4f);

    // Re-layout the visualization area
    resized();
}
