#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VizASynthAudioProcessorEditor::VizASynthAudioProcessorEditor(VizASynthAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      oscilloscope(p.getProbeManager()),
      spectrumAnalyzer(p.getProbeManager(),
                       [&]() -> vizasynth::PolyBLEPOscillator& {
                           if (auto* voice = p.getVoice(0)) {
                               return voice->getOscillator();
                           }
                           throw std::runtime_error("No voices available to provide an oscillator.");
                       }()),
      harmonicView(p.getProbeManager(),
                   [&]() -> vizasynth::PolyBLEPOscillator& {
                       if (auto* voice = p.getVoice(0)) {
                           return voice->getOscillator();
                       }
                       throw std::runtime_error("No voices available to provide an oscillator.");
                   }()),
      poleZeroPlot(p.getProbeManager(), p.getFilterWrapper()),
      bodePlot(p.getProbeManager(), p.getFilterWrapper()),
      singleCycleView(p.getProbeManager(),
                      [&]() -> vizasynth::PolyBLEPOscillator& {
                          if (auto* voice = p.getVoice(0)) {
                              return voice->getOscillator();
                          }
                          throw std::runtime_error("No voices available to provide an oscillator.");
                      }()),
      envelopeVisualizer(p.getAPVTS())
{
    // Set editor size from configuration
    auto& config = vizasynth::ConfigurationManager::getInstance();
    setSize(config.getWindowWidth(), config.getWindowHeight());

    // Register for configuration changes
    config.addChangeListener(this);

    // Add visualization components
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(harmonicView);
    addAndMakeVisible(poleZeroPlot);
    addAndMakeVisible(bodePlot);
    addAndMakeVisible(singleCycleView);
    addAndMakeVisible(envelopeVisualizer);

    // Setup visualization mode selector buttons
    scopeButton.setClickingTogglesState(false);
    scopeButton.setColour(juce::TextButton::buttonColourId, config.getAccentColour());
    scopeButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Oscilloscope); };
    addAndMakeVisible(scopeButton);

    spectrumButton.setClickingTogglesState(false);
    spectrumButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    spectrumButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Spectrum); };
    addAndMakeVisible(spectrumButton);

    harmonicsButton.setClickingTogglesState(false);
    harmonicsButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    harmonicsButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Harmonics); };
    addAndMakeVisible(harmonicsButton);

    poleZeroButton.setClickingTogglesState(false);
    poleZeroButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    poleZeroButton.onClick = [this]() { setVisualizationMode(VisualizationMode::PoleZero); };
    addAndMakeVisible(poleZeroButton);

    bodeButton.setClickingTogglesState(false);
    bodeButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    bodeButton.onClick = [this]() { setVisualizationMode(VisualizationMode::Bode); };
    addAndMakeVisible(bodeButton);

    // Initial visualization mode
    setVisualizationMode(VisualizationMode::Oscilloscope);

    // Setup probe selector buttons
    auto setupProbeButton = [this, &config](juce::TextButton& button, vizasynth::ProbePoint probe, juce::Colour colour)
    {
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
        button.setColour(juce::TextButton::textColourOffId, colour);
        button.onClick = [this, probe]()
        {
            audioProcessor.getProbeManager().setActiveProbe(probe);
            updateProbeButtons();
        };
        addAndMakeVisible(button);
    };

    setupProbeButton(probeOscButton, vizasynth::ProbePoint::Oscillator, vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::Oscillator));
    setupProbeButton(probeFilterButton, vizasynth::ProbePoint::PostFilter, vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::PostFilter));
    setupProbeButton(probeOutputButton, vizasynth::ProbePoint::Output, vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::Output));

    // Freeze button
    freezeButton.setClickingTogglesState(true);
    freezeButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    freezeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red.darker());
    freezeButton.onClick = [this]()
    {
        bool frozen = freezeButton.getToggleState();
        oscilloscope.setFrozen(frozen);
        spectrumAnalyzer.setFrozen(frozen);
        harmonicView.setFrozen(frozen);
        poleZeroPlot.setFrozen(frozen);
        bodePlot.setFrozen(frozen);
        singleCycleView.setFrozen(frozen);
    };
    addAndMakeVisible(freezeButton);

    // Clear trace button
    clearTraceButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    clearTraceButton.onClick = [this]()
    {
        oscilloscope.clearTrace();
        spectrumAnalyzer.clearTrace();
        harmonicView.clearTrace();
        poleZeroPlot.clearTrace();
        bodePlot.clearTrace();
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

    // Apply theme colors to all components
    applyThemeToComponents();

    // Start timer for UI updates (60 FPS)
    startTimerHz(60);
}

VizASynthAudioProcessorEditor::~VizASynthAudioProcessorEditor()
{
    stopTimer();
    vizasynth::ConfigurationManager::getInstance().removeChangeListener(this);
}

//==============================================================================
void VizASynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto& config = vizasynth::ConfigurationManager::getInstance();

    // Dark background
    g.fillAll(config.getBackgroundColour());

    // --- Draw three rounded panels for Oscillator, Filter, Envelope ---
    int panelX = config.getLayoutInt("components.oscillatorPanel.x", 30);
    int oscPanelW = config.getLayoutInt("components.oscillatorPanel.width", 320);
    int oscPanelH = config.getLayoutInt("components.oscillatorPanel.height", 80);
    int filterPanelW = config.getLayoutInt("components.filterPanel.width", 320);
    int filterPanelH = config.getLayoutInt("components.filterPanel.height", 150);
    int envPanelW = config.getLayoutInt("components.envelopePanel.width", 320);
    int envPanelH = config.getLayoutInt("components.envelopePanel.height", 230);
    int panelSpacing = config.getLayoutInt("components.panels.spacing", 22);
    int oscY = config.getLayoutInt("components.oscillatorPanel.y", 80);
    int filterY = oscY + oscPanelH + panelSpacing;
    int envY = filterY + filterPanelH + panelSpacing;
    int leftPanelTitleYOffset = config.getLayoutInt("components.panels.titleYOffset", 6);
    int leftPanelTitleWidthOffset = config.getLayoutInt("components.panels.titleWidthOffset", 30);
    int leftPanelTitleHeight = config.getLayoutInt("components.panels.titleHeight", 28);
    juce::Colour leftPanelColour = config.getPanelBackgroundColour();
    juce::Colour panelTitleColour = config.getTextHighlightColour();
    float panelCornerRadius = config.getLayoutFloat("components.panels.cornerRadius", 15.0f);
    float panelFontSize = config.getLayoutFloat("components.panels.titleFontSize", 20.0f);

    // Oscillator panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(static_cast<float>(panelX), static_cast<float>(oscY), static_cast<float>(oscPanelW), static_cast<float>(oscPanelH), panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Oscillator", panelX, oscY + leftPanelTitleYOffset, oscPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Filter panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(static_cast<float>(panelX), static_cast<float>(filterY), static_cast<float>(filterPanelW), static_cast<float>(filterPanelH), panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Filter", panelX, filterY + leftPanelTitleYOffset, filterPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Envelope panel
    g.setColour(leftPanelColour);
    g.fillRoundedRectangle(static_cast<float>(panelX), static_cast<float>(envY), static_cast<float>(envPanelW), static_cast<float>(envPanelH), panelCornerRadius);
    g.setColour(panelTitleColour);
    g.setFont(panelFontSize);
    g.drawText("Envelope", panelX, envY + leftPanelTitleYOffset, envPanelW - leftPanelTitleWidthOffset, leftPanelTitleHeight, juce::Justification::centred);

    // Keyboard panel background - calculate position to match the actual keyboard bounds
    int bottomAreaHeight = config.getLayoutInt("components.bottomArea.height", 110);
    int bottomPaddingH = config.getLayoutInt("components.bottomArea.paddingH", 25);
    int bottomPaddingV = config.getLayoutInt("components.bottomArea.paddingV", 5);
    int kbPanelX = bottomPaddingH;
    int kbPanelY = getHeight() - bottomAreaHeight + bottomPaddingV;
    int kbPanelWidth = getWidth() - 2 * bottomPaddingH;
    int kbPanelHeight = bottomAreaHeight - 2 * bottomPaddingV;
    float kbCornerRadius = config.getLayoutFloat("components.keyboardPanel.cornerRadius", 10.0f);
    g.setColour(config.getThemeColour("colors.keyboard.panelBackground", juce::Colour(0xff2a2a2a)));
    g.fillRoundedRectangle(static_cast<float>(kbPanelX), static_cast<float>(kbPanelY), static_cast<float>(kbPanelWidth), static_cast<float>(kbPanelHeight), kbCornerRadius);
}

void VizASynthAudioProcessorEditor::resized()
{
    auto& config = vizasynth::ConfigurationManager::getInstance();

    // Layout values loaded from configuration with fallback defaults
    struct Layout {
        int margin;
        int panelSpacing;
        int panelInnerMargin;

        // Left panel
        int leftPanelWidth;
        int oscPanelH;
        int filterPanelH;
        int envPanelH;
        int oscPanelW;
        int filterPanelW;
        int envPanelW;
        int labelHeight;
        int comboHeight;
        int filterKnobSize;
        int filterKnobSpacing;
        int filterKnobYOffset;
        int envKnobSize;
        int envKnobSpacing;
        int envelopeVisHeight;
        int bottomAreaHeight;
        int volumeSectionWidth;
        int meterWidth;
        int masterVolumeLabelHeight;
        int oscTitleOffsetX;
        int oscComboOffsetX;
        int oscComboX;
        int oscComboWidth;
        int filterKnob1X;
        int filterKnob2X;
        int envKnobStartX;
        int envKnobYOffset;
        int filterLabelYOffset;
        int envVisYOffset;
        int envVisXOffset;
        // Visualization controls
        int vizControlAreaHPad;
        int vizControlAreaVPad;
        int vizControlAreaBottomPad;
        int vizControlAreaHeight;
        int vizControlScopeWidth;
        int vizControlScopeSpacing;
        int vizControlSpectrumWidth;
        int vizControlSectionSpacing;
        int vizControlProbeWidth;
        int vizControlProbeSpacing;
        int vizControlFreezeWidth;
        int vizControlClearWidth;
        int vizControlLabelWidth;
        int bottomPaddingH;
        int bottomPaddingV;

        Layout(vizasynth::ConfigurationManager& c) :
            margin(c.getLayoutInt("components.panels.margin", 5)),
            panelSpacing(c.getLayoutInt("components.panels.spacing", 22)),
            panelInnerMargin(c.getLayoutInt("components.panels.innerMargin", 18)),
            leftPanelWidth(c.getLayoutInt("components.leftPanel.width", 370)),
            oscPanelH(c.getLayoutInt("components.oscillatorPanel.height", 80)),
            filterPanelH(c.getLayoutInt("components.filterPanel.height", 150)),
            envPanelH(c.getLayoutInt("components.envelopePanel.height", 230)),
            oscPanelW(c.getLayoutInt("components.oscillatorPanel.width", 320)),
            filterPanelW(c.getLayoutInt("components.filterPanel.width", 320)),
            envPanelW(c.getLayoutInt("components.envelopePanel.width", 320)),
            labelHeight(c.getLayoutInt("components.oscillatorPanel.labelHeight", 22)),
            comboHeight(c.getLayoutInt("components.oscillatorPanel.comboHeight", 32)),
            filterKnobSize(c.getLayoutInt("components.filterPanel.knobSize", 80)),
            filterKnobSpacing(c.getLayoutInt("components.filterPanel.knobSpacing", 80)),
            filterKnobYOffset(c.getLayoutInt("components.filterPanel.knobYOffset", 2)),
            envKnobSize(c.getLayoutInt("components.envelopePanel.knobSize", 80)),
            envKnobSpacing(c.getLayoutInt("components.envelopePanel.knobSpacing", 2)),
            envelopeVisHeight(c.getLayoutInt("components.envelopePanel.visHeight", 100)),
            bottomAreaHeight(c.getLayoutInt("components.bottomArea.height", 110)),
            volumeSectionWidth(c.getLayoutInt("components.volumeSection.width", 100)),
            meterWidth(c.getLayoutInt("components.volumeSection.meterWidth", 20)),
            masterVolumeLabelHeight(c.getLayoutInt("components.volumeSection.labelHeight", 18)),
            oscTitleOffsetX(c.getLayoutInt("components.oscillatorPanel.comboOffsetX", 30)),
            oscComboOffsetX(c.getLayoutInt("components.oscillatorPanel.comboOffsetX", 30)),
            oscComboX(c.getLayoutInt("components.oscillatorPanel.comboX", 120)),
            oscComboWidth(c.getLayoutInt("components.oscillatorPanel.comboWidth", 170)),
            filterKnob1X(c.getLayoutInt("components.filterPanel.knob1X", 80)),
            filterKnob2X(c.getLayoutInt("components.filterPanel.knob2X", 200)),
            envKnobStartX(c.getLayoutInt("components.envelopePanel.knobStartX", 28)),
            envKnobYOffset(c.getLayoutInt("components.envelopePanel.knobYOffset", 2)),
            filterLabelYOffset(c.getLayoutInt("components.filterPanel.labelYOffset", 38)),
            envVisYOffset(c.getLayoutInt("components.envelopePanel.visYOffset", 38)),
            envVisXOffset(c.getLayoutInt("components.envelopePanel.visXOffset", 22)),
            vizControlAreaHPad(c.getLayoutInt("components.vizControls.hPad", 5)),
            vizControlAreaVPad(c.getLayoutInt("components.vizControls.vPad", 10)),
            vizControlAreaBottomPad(c.getLayoutInt("components.vizControls.bottomPad", 60)),
            vizControlAreaHeight(c.getLayoutInt("components.vizControls.areaHeight", 50)),
            vizControlScopeWidth(c.getLayoutInt("components.buttons.scope.width", 60)),
            vizControlScopeSpacing(c.getLayoutInt("components.vizControls.buttonSpacing", 2)),
            vizControlSpectrumWidth(c.getLayoutInt("components.buttons.spectrum.width", 70)),
            vizControlSectionSpacing(c.getLayoutInt("components.vizControls.sectionSpacing", 12)),
            vizControlProbeWidth(c.getLayoutInt("components.buttons.probe.width", 45)),
            vizControlProbeSpacing(c.getLayoutInt("components.vizControls.probeSpacing", 4)),
            vizControlFreezeWidth(c.getLayoutInt("components.buttons.freeze.width", 55)),
            vizControlClearWidth(c.getLayoutInt("components.buttons.clear.width", 50)),
            vizControlLabelWidth(c.getLayoutInt("components.buttons.timeLabel.width", 35)),
            bottomPaddingH(c.getLayoutInt("components.bottomArea.paddingH", 25)),
            bottomPaddingV(c.getLayoutInt("components.bottomArea.paddingV", 5))
        {}
    } layout(config);
    
    auto bounds = getLocalBounds();

    // Bottom area for keyboard and volume controls
    auto bottomArea = bounds.removeFromBottom(layout.bottomAreaHeight).reduced(layout.bottomPaddingH, layout.bottomPaddingV);
    auto volumeSection = bottomArea.removeFromRight(layout.volumeSectionWidth);
    volumeSection = volumeSection.reduced(config.getLayoutInt("components.volumeSection.paddingH", 5), 0);
    masterVolumeLabel.setBounds(volumeSection.removeFromTop(layout.masterVolumeLabelHeight));
    auto sliderAndMeter = volumeSection;
    auto meterArea = sliderAndMeter.removeFromRight(layout.meterWidth);
    levelMeter.setBounds(meterArea.reduced(0, 2));
    masterVolumeSlider.setBounds(sliderAndMeter);
    virtualKeyboard.setBounds(bottomArea.reduced(0, config.getLayoutInt("components.keyboardPanel.paddingV", 5)));

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
    int oscComboY = oscY + config.getLayoutInt("components.oscillatorPanel.comboY", 40);
    oscTypeLabel.setBounds(panelX + layout.panelInnerMargin + layout.oscComboOffsetX, oscComboY, 100, layout.labelHeight);
    oscTypeCombo.setBounds(panelX + layout.oscComboX + layout.oscComboOffsetX, oscComboY + config.getLayoutInt("components.oscillatorPanel.comboYOffset", -2), layout.oscComboWidth, layout.comboHeight);

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
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::Spectrum)
    {
        spectrumAnalyzer.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::Harmonics)
    {
        harmonicView.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::PoleZero)
    {
        poleZeroPlot.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
    }
    else // Bode mode
    {
        bodePlot.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
    }

    // Controls below visualization
    int harmonicsButtonWidth = config.getLayoutInt("components.buttons.harmonics.width", 70);
    int poleZeroButtonWidth = config.getLayoutInt("components.buttons.poleZero.width", 35);
    int bodeButtonWidth = config.getLayoutInt("components.buttons.bode.width", 45);
    auto vizControlArea = vizArea.reduced(layout.vizControlAreaHPad, layout.vizControlAreaVPad);
    scopeButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlScopeWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    spectrumButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlSpectrumWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    harmonicsButton.setBounds(vizControlArea.removeFromLeft(harmonicsButtonWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    poleZeroButton.setBounds(vizControlArea.removeFromLeft(poleZeroButtonWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    bodeButton.setBounds(vizControlArea.removeFromLeft(bodeButtonWidth));
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
        case 0: singleCycleView.setWaveformType(vizasynth::OscillatorSource::Waveform::Sine); break;
        case 1: singleCycleView.setWaveformType(vizasynth::OscillatorSource::Waveform::Saw); break;
        case 2: singleCycleView.setWaveformType(vizasynth::OscillatorSource::Waveform::Square); break;
        default: singleCycleView.setWaveformType(vizasynth::OscillatorSource::Waveform::Sine); break;
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

void VizASynthAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // Configuration has changed - apply theme updates and re-layout
    applyThemeToComponents();
    resized();
    repaint();
}

void VizASynthAudioProcessorEditor::applyThemeToComponents()
{
    auto& config = vizasynth::ConfigurationManager::getInstance();

    // Apply slider colors
    auto thumbColor = config.getThemeColour("colors.sliders.rotaryFill", juce::Colour(0xff5c9fd4));
    auto trackColor = config.getThemeColour("colors.sliders.rotaryOutline", juce::Colour(0xff2d2d44));
    auto textBoxBg = config.getThemeColour("colors.sliders.textBox", juce::Colour(0xff1a1a2e));
    auto textBoxText = config.getThemeColour("colors.sliders.textBoxText", juce::Colour(0xffe0e0e0));
    auto textBoxOutline = config.getThemeColour("colors.sliders.textBoxOutline", juce::Colour(0xff3d3d54));

    for (auto* slider : {&cutoffSlider, &resonanceSlider, &attackSlider,
                         &decaySlider, &sustainSlider, &releaseSlider}) {
        slider->setColour(juce::Slider::rotarySliderFillColourId, thumbColor);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, trackColor);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, textBoxBg);
        slider->setColour(juce::Slider::textBoxTextColourId, textBoxText);
        slider->setColour(juce::Slider::textBoxOutlineColourId, textBoxOutline);
    }

    // Linear sliders (masterVolume, timeWindow)
    masterVolumeSlider.setColour(juce::Slider::thumbColourId, thumbColor);
    masterVolumeSlider.setColour(juce::Slider::trackColourId, trackColor);
    masterVolumeSlider.setColour(juce::Slider::textBoxBackgroundColourId, textBoxBg);
    masterVolumeSlider.setColour(juce::Slider::textBoxTextColourId, textBoxText);
    masterVolumeSlider.setColour(juce::Slider::textBoxOutlineColourId, textBoxOutline);

    timeWindowSlider.setColour(juce::Slider::thumbColourId, thumbColor);
    timeWindowSlider.setColour(juce::Slider::trackColourId, trackColor);
    timeWindowSlider.setColour(juce::Slider::textBoxBackgroundColourId, textBoxBg);
    timeWindowSlider.setColour(juce::Slider::textBoxTextColourId, textBoxText);
    timeWindowSlider.setColour(juce::Slider::textBoxOutlineColourId, textBoxOutline);

    // Apply button colors
    auto buttonDefault = config.getThemeColour("colors.buttons.default", config.getPanelBackgroundColour());
    auto buttonText = config.getThemeColour("colors.buttons.text", juce::Colour(0xffe0e0e0));
    auto toggleOnColor = config.getThemeColour("colors.buttons.toggleOn", juce::Colours::red.darker());

    for (auto* btn : {&scopeButton, &spectrumButton, &harmonicsButton, &poleZeroButton, &bodeButton, &probeOscButton, &probeFilterButton,
                      &probeOutputButton, &clearTraceButton}) {
        btn->setColour(juce::TextButton::buttonColourId, buttonDefault);
        btn->setColour(juce::TextButton::textColourOffId, buttonText);
    }

    freezeButton.setColour(juce::TextButton::buttonColourId, buttonDefault);
    freezeButton.setColour(juce::TextButton::buttonOnColourId, toggleOnColor);
    freezeButton.setColour(juce::TextButton::textColourOffId, buttonText);

    // Apply label colors
    auto labelColor = config.getThemeColour("colors.labels.text", juce::Colour(0xffe0e0e0));
    for (auto* label : {&oscTypeLabel, &cutoffLabel, &resonanceLabel, &attackLabel,
                        &decayLabel, &sustainLabel, &releaseLabel, &masterVolumeLabel, &timeWindowLabel}) {
        label->setColour(juce::Label::textColourId, labelColor);
    }

    // Update visualization mode button colors (keep current active state)
    updateVisualizationMode();
    updateProbeButtons();
}

void VizASynthAudioProcessorEditor::updateProbeButtons()
{
    auto& config = vizasynth::ConfigurationManager::getInstance();
    auto activeProbe = audioProcessor.getProbeManager().getActiveProbe();

    auto highlightButton = [&config](juce::TextButton& button, bool active, juce::Colour colour)
    {
        if (active)
        {
            button.setColour(juce::TextButton::buttonColourId, colour.darker(0.3f));
            button.setColour(juce::TextButton::textColourOffId, config.getTextHighlightColour());
        }
        else
        {
            button.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
            button.setColour(juce::TextButton::textColourOffId, colour);
        }
    };

    highlightButton(probeOscButton, activeProbe == vizasynth::ProbePoint::Oscillator,
                    vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::Oscillator));
    highlightButton(probeFilterButton, activeProbe == vizasynth::ProbePoint::PostFilter,
                    vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::PostFilter));
    highlightButton(probeOutputButton, activeProbe == vizasynth::ProbePoint::Output,
                    vizasynth::Oscilloscope::getProbeColour(vizasynth::ProbePoint::Output));
}

void VizASynthAudioProcessorEditor::setVisualizationMode(VisualizationMode mode)
{
    currentVizMode = mode;
    updateVisualizationMode();
}

void VizASynthAudioProcessorEditor::updateVisualizationMode()
{
    auto& config = vizasynth::ConfigurationManager::getInstance();
    bool isScope = (currentVizMode == VisualizationMode::Oscilloscope);
    bool isSpectrum = (currentVizMode == VisualizationMode::Spectrum);
    bool isHarmonics = (currentVizMode == VisualizationMode::Harmonics);
    bool isPoleZero = (currentVizMode == VisualizationMode::PoleZero);
    bool isBode = (currentVizMode == VisualizationMode::Bode);

    // Show/hide appropriate visualization
    oscilloscope.setVisible(isScope);
    singleCycleView.setVisible(isScope);
    spectrumAnalyzer.setVisible(isSpectrum);
    harmonicView.setVisible(isHarmonics);
    poleZeroPlot.setVisible(isPoleZero);
    bodePlot.setVisible(isBode);

    // Update button highlighting
    scopeButton.setColour(juce::TextButton::buttonColourId,
                          isScope ? config.getAccentColour() : config.getPanelBackgroundColour());
    spectrumButton.setColour(juce::TextButton::buttonColourId,
                             isSpectrum ? config.getAccentColour() : config.getPanelBackgroundColour());
    harmonicsButton.setColour(juce::TextButton::buttonColourId,
                              isHarmonics ? config.getAccentColour() : config.getPanelBackgroundColour());
    poleZeroButton.setColour(juce::TextButton::buttonColourId,
                              isPoleZero ? config.getAccentColour() : config.getPanelBackgroundColour());
    bodeButton.setColour(juce::TextButton::buttonColourId,
                         isBode ? config.getAccentColour() : config.getPanelBackgroundColour());

    // Show/hide time window control (only relevant for oscilloscope)
    timeWindowSlider.setEnabled(isScope);
    timeWindowLabel.setAlpha(isScope ? 1.0f : 0.4f);
    timeWindowSlider.setAlpha(isScope ? 1.0f : 0.4f);

    // Re-layout the visualization area
    resized();
}
