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
                      }()),
      envelopeVisualizer(p.getAPVTS())
{
    // Set editor size (expanded for keyboard)
    setSize(1000, 700);

    // Add visualization components
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(singleCycleView);
    addAndMakeVisible(envelopeVisualizer);

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

    int filterSliderTextWidth = 60;
    int filterSliderTextHeight = 20;

    // Setup filter cutoff slider
    cutoffSlider.setSliderStyle(juce::Slider::Rotary);
    cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, filterSliderTextWidth, filterSliderTextHeight);
    addAndMakeVisible(cutoffSlider);

    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(cutoffLabel);

    // Setup filter resonance slider
    resonanceSlider.setSliderStyle(juce::Slider::Rotary);
    resonanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, filterSliderTextWidth, filterSliderTextHeight);
    addAndMakeVisible(resonanceSlider);

    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    resonanceLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(resonanceLabel);

    // Setup ADSR sliders as rotary knobs (match cutoff/resonance value box size)
    int adsrTextBoxWidth = 40;
    int adsrTextBoxHeight = filterSliderTextHeight;

    attackSlider.setSliderStyle(juce::Slider::Rotary);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, adsrTextBoxWidth, adsrTextBoxHeight);
    addAndMakeVisible(attackSlider);
    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(attackLabel);

    decaySlider.setSliderStyle(juce::Slider::Rotary);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, adsrTextBoxWidth, adsrTextBoxHeight);
    addAndMakeVisible(decaySlider);
    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(decayLabel);

    sustainSlider.setSliderStyle(juce::Slider::Rotary);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, adsrTextBoxWidth, adsrTextBoxHeight);
    addAndMakeVisible(sustainSlider);
    sustainLabel.setText("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sustainLabel);

    releaseSlider.setSliderStyle(juce::Slider::Rotary);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, adsrTextBoxWidth, adsrTextBoxHeight);
    addAndMakeVisible(releaseSlider);
    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(releaseLabel);

    // Setup master volume slider
    int masterVolumeTextWidth = 60;
    int masterVolumeTextHeight = 20;
    masterVolumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, masterVolumeTextWidth, masterVolumeTextHeight);
    addAndMakeVisible(masterVolumeSlider);

    masterVolumeLabel.setText("Volume", juce::dontSendNotification);
    masterVolumeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(masterVolumeLabel);

    // Setup level meter
    levelMeter.setLevelCallback([this]() { return audioProcessor.getOutputLevel(); });
    levelMeter.setClippingCallback(
        [this]() { return audioProcessor.isClipping(); },
        [this]() { audioProcessor.resetClipping(); });
    addAndMakeVisible(levelMeter);

    // Setup virtual keyboard
    virtualKeyboard.setNoteCallback([this](const juce::MidiMessage& msg) {
        audioProcessor.addMidiMessage(msg);
        // Trigger envelope visualization
        if (msg.isNoteOn())
            envelopeVisualizer.triggerEnvelope();
        else if (msg.isNoteOff())
            envelopeVisualizer.releaseEnvelope();
    });
    virtualKeyboard.setActiveNotesCallback([this]() {
        std::vector<std::pair<int, float>> notes;
        for (auto& info : audioProcessor.getActiveNotes())
            notes.push_back({info.note, info.velocity});
        return notes;
    });
    addAndMakeVisible(virtualKeyboard);

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

    masterVolumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "masterVolume", masterVolumeSlider);

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

    // --- Draw three rounded panels for Oscillator, Filter, Envelope ---
    g.setFont(18.0f);
    int panelX = 30;
    int oscPanelW = 320, oscPanelH = 80;
    int filterPanelW = 320, filterPanelH = 150;
    int envPanelW = 320, envPanelH = 230;
    int panelSpacing = 22;
    int oscY = 80;
    int filterY = oscY + oscPanelH + panelSpacing;
    int envY = filterY + filterPanelH + panelSpacing;
    int leftPanelTitleYOffset = 6;
    int leftPanelTitleWidthOffset = 30;
    int leftPanelTitleHeight = 28;
    juce::Colour leftPanelColour = juce::Colour(0xff232b33);
    juce::Colour panelTitleColour = juce::Colours::white.withAlpha(0.9f);
    float panelCornerRadius = 15.0f;
    float panelFontSize = 20.0f;

    // Oscillator panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(panelX, oscY, oscPanelW, oscPanelH, panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Oscillator", panelX, oscY + leftPanelTitleYOffset, oscPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Filter panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(panelX, filterY, filterPanelW, filterPanelH, panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Filter", panelX, filterY + leftPanelTitleYOffset, filterPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Envelope panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(panelX, envY, envPanelW, envPanelH, panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Envelope", panelX, envY + leftPanelTitleYOffset, envPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Keyboard panel background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(20, 590, getWidth() - 40, 100, 10);
}

void VizASynthAudioProcessorEditor::resized()
{
    struct Layout {
        int margin = 5;
        int panelSpacing = 22;
        int panelInnerMargin = 18;
        
        // Left panel
        int leftPanelWidth = 370;
        int oscPanelH = 80;
        int filterPanelH = 150;
        int envPanelH = 230;
        int oscPanelW = 320;
        int filterPanelW = 320;
        int envPanelW = 320;
        int labelHeight = 22;
        int comboHeight = 32;
        int filterKnobSize = 80;
        int filterKnobSpacing = 80;
        int filterKnobYOffset = 2;
        int envKnobSize = 80;
        int envKnobSpacing = 2;
        int envelopeVisHeight = 100;
        int bottomAreaHeight = 110;
        int volumeSectionWidth = 100;
        int meterWidth = 20;
        int masterVolumeLabelHeight = 18;
        int oscTitleOffsetX = 30;
        int oscComboOffsetX = 30;
        int filterKnob1X = 80;
        int filterKnob2X = 200;
        int envKnobStartX = 28;
        int envKnobYOffset = 2;
        int filterLabelYOffset = 38;
        int envVisYOffset = 38;
        int envVisXOffset = 22;
        // Visualization controls
        int vizControlAreaHPad = 5;
        int vizControlAreaVPad = 10;
        int vizControlAreaBottomPad = 60;
        int vizControlAreaHeight = 50;
        int vizControlScopeWidth = 60;
        int vizControlScopeSpacing = 2;
        int vizControlSpectrumWidth = 70;
        int vizControlSectionSpacing = 12;
        int vizControlProbeWidth = 45;
        int vizControlProbeSpacing = 4;
        int vizControlFreezeWidth = 55;
        int vizControlClearWidth = 50;
        int vizControlLabelWidth = 35;
    } layout;
    
    auto bounds = getLocalBounds();

    // Bottom area for keyboard and volume controls
    auto bottomArea = bounds.removeFromBottom(layout.bottomAreaHeight).reduced(25, 5);
    auto volumeSection = bottomArea.removeFromRight(layout.volumeSectionWidth);
    volumeSection = volumeSection.reduced(5, 0);
    masterVolumeLabel.setBounds(volumeSection.removeFromTop(layout.masterVolumeLabelHeight));
    auto sliderAndMeter = volumeSection;
    auto meterArea = sliderAndMeter.removeFromRight(layout.meterWidth);
    levelMeter.setBounds(meterArea.reduced(0, 2));
    masterVolumeSlider.setBounds(sliderAndMeter);
    virtualKeyboard.setBounds(bottomArea.reduced(0, 5));

    // --- Split main area into left (panels) and right (visualization) ---
    auto mainArea = bounds.reduced(layout.margin, layout.margin);
    auto leftPanelArea = mainArea.removeFromLeft(layout.leftPanelWidth);
    auto rightVizArea = mainArea;

    // --- Control panel area (left side) ---
    int panelX = leftPanelArea.getX();
    int oscY = leftPanelArea.getY() + 70;
    int filterY = oscY + layout.oscPanelH + layout.panelSpacing;
    int envY = filterY + layout.filterPanelH + layout.panelSpacing;

    // Oscillator section (centered vertically in osc panel)
    int oscComboY = oscY + 40;
    oscTypeLabel.setBounds(panelX + layout.panelInnerMargin + layout.oscComboOffsetX, oscComboY, 100, layout.labelHeight);
    oscTypeCombo.setBounds(panelX + 120 + layout.oscComboOffsetX, oscComboY - 2, 170, layout.comboHeight);

    // Filter section (centered vertically in filter panel)
    int filterLabelY = filterY + layout.filterLabelYOffset;
    int filterKnobY = filterLabelY + layout.labelHeight + layout.filterKnobYOffset;
    cutoffLabel.setBounds(panelX + layout.filterKnob1X, filterLabelY, layout.filterKnobSize, layout.labelHeight);
    cutoffSlider.setBounds(panelX + layout.filterKnob1X, filterKnobY, layout.filterKnobSize, layout.filterKnobSize);
    resonanceLabel.setBounds(panelX + layout.filterKnob2X, filterLabelY, layout.filterKnobSize, layout.labelHeight);
    resonanceSlider.setBounds(panelX + layout.filterKnob2X, filterKnobY, layout.filterKnobSize, layout.filterKnobSize);

    // Envelope section
    int envVisY = envY + layout.envVisYOffset;
    envelopeVisualizer.setBounds(panelX + layout.panelInnerMargin + layout.envVisXOffset, envVisY, layout.envPanelW - 2 * layout.panelInnerMargin, layout.envelopeVisHeight);
    int envKnobY = envVisY + layout.envelopeVisHeight + layout.envKnobYOffset;
    for (int i = 0; i < 4; ++i) {
        int knobX = panelX + layout.envKnobStartX + i * (layout.envKnobSize + layout.envKnobSpacing);
        switch (i) {
            case 0:
                attackSlider.setBounds(knobX, envKnobY, layout.envKnobSize, layout.envKnobSize);
                attackLabel.setBounds(knobX, envKnobY + layout.envKnobSize, layout.envKnobSize, 20);
                break;
            case 1:
                decaySlider.setBounds(knobX, envKnobY, layout.envKnobSize, layout.envKnobSize);
                decayLabel.setBounds(knobX, envKnobY + layout.envKnobSize, layout.envKnobSize, 20);
                break;
            case 2:
                sustainSlider.setBounds(knobX, envKnobY, layout.envKnobSize, layout.envKnobSize);
                sustainLabel.setBounds(knobX, envKnobY + layout.envKnobSize, layout.envKnobSize, 20);
                break;
            case 3:
                releaseSlider.setBounds(knobX, envKnobY, layout.envKnobSize, layout.envKnobSize);
                releaseLabel.setBounds(knobX, envKnobY + layout.envKnobSize, layout.envKnobSize, 20);
                break;
        }
    }

    // --- Visualization area (right side) ---
    auto vizArea = rightVizArea.reduced(layout.margin, layout.vizControlAreaBottomPad);
    auto scopeArea = vizArea.removeFromTop(vizArea.getHeight() - layout.vizControlAreaHeight);
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
    auto vizControlArea = vizArea.reduced(layout.vizControlAreaHPad, layout.vizControlAreaVPad);
    scopeButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlScopeWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    spectrumButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlSpectrumWidth));
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);
    probeOscButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlProbeWidth));
    vizControlArea.removeFromLeft(layout.vizControlProbeSpacing);
    probeFilterButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlProbeWidth));
    vizControlArea.removeFromLeft(layout.vizControlProbeSpacing);
    probeOutputButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlProbeWidth));
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);
    freezeButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlFreezeWidth));
    vizControlArea.removeFromLeft(layout.vizControlProbeSpacing);
    clearTraceButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlClearWidth));
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);
    timeWindowLabel.setBounds(vizControlArea.removeFromLeft(layout.vizControlLabelWidth));
    timeWindowSlider.setBounds(vizControlArea);
}

void VizASynthAudioProcessorEditor::timerCallback()
{
    // Update probe button highlighting
    updateProbeButtons();

    // Sync SingleCycleView waveform type with oscillator setting
    int oscType = static_cast<int>(audioProcessor.getAPVTS().getRawParameterValue("oscType")->load());
    switch (oscType)
    {
        case 0: singleCycleView.setWaveformType(OscillatorWaveform::Sine); break;
        case 1: singleCycleView.setWaveformType(OscillatorWaveform::Saw); break;
        case 2: singleCycleView.setWaveformType(OscillatorWaveform::Square); break;
        default: singleCycleView.setWaveformType(OscillatorWaveform::Sine); break;
    }

    // Track note changes for envelope visualization (handles external MIDI)
    int currentNoteCount = static_cast<int>(audioProcessor.getActiveNotes().size());
    if (currentNoteCount > lastActiveNoteCount)
    {
        // New note triggered
        envelopeVisualizer.triggerEnvelope();
    }
    else if (currentNoteCount < lastActiveNoteCount && currentNoteCount == 0)
    {
        // All notes released
        envelopeVisualizer.releaseEnvelope();
    }
    lastActiveNoteCount = currentNoteCount;
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
