#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/Oscillators/OscillatorSource.h"
#include "DSP/Oscillators/PolyBLEPOscillator.h"

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
      transferFunctionDisplay(p.getProbeManager(), p.getFilterWrapper()),
      impulseResponse(p.getProbeManager(), p.getFilterWrapper()),
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
    addAndMakeVisible(transferFunctionDisplay);
    addAndMakeVisible(impulseResponse);
    addAndMakeVisible(singleCycleView);
    addAndMakeVisible(envelopeVisualizer);

    // Register as ProbeRegistry listener for dynamic probe button updates
    audioProcessor.getProbeRegistry().addListener(this);

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

    transferFunctionButton.setClickingTogglesState(false);
    transferFunctionButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    transferFunctionButton.onClick = [this]() { setVisualizationMode(VisualizationMode::TransferFunction); };
    addAndMakeVisible(transferFunctionButton);

    impulseResponseButton.setClickingTogglesState(false);
    impulseResponseButton.setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
    impulseResponseButton.onClick = [this]() { setVisualizationMode(VisualizationMode::ImpulseResponse); };
    addAndMakeVisible(impulseResponseButton);

    // Initial visualization mode
    setVisualizationMode(VisualizationMode::Oscilloscope);

    // Setup dynamic probe selector buttons from ProbeRegistry
    updateFromProbeRegistry();

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
        transferFunctionDisplay.setFrozen(frozen);
        impulseResponse.setFrozen(frozen);
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
        transferFunctionDisplay.clearTrace();
        impulseResponse.clearTrace();
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

    // Setup oscillator node selector (Phase 2)
    oscNodeSelector.addItem("Osc 1", 1);  // Default oscillator
    oscNodeSelector.setSelectedId(1, juce::dontSendNotification);
    oscNodeSelector.onChange = [this]() { onOscillatorSelected(); };
    addAndMakeVisible(oscNodeSelector);

    // Setup oscillator type combo box
    oscTypeCombo.addItem("Sine", 1);
    oscTypeCombo.addItem("Saw", 2);
    oscTypeCombo.addItem("Square", 3);
    addAndMakeVisible(oscTypeCombo);

    oscTypeLabel.setText("Oscillator", juce::dontSendNotification);
    oscTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(oscTypeLabel);

    // Setup band-limiting toggle (for aliasing demonstration)
    bandLimitedToggle.setButtonText("PolyBLEP");
    bandLimitedToggle.setTooltip("Enable band-limited oscillator (PolyBLEP)\nDisable to hear aliasing artifacts");
    addAndMakeVisible(bandLimitedToggle);

    // Setup detune slider (Phase 2.4)
    detuneSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    detuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    detuneSlider.setRange(-100.0, 100.0, 1.0);
    detuneSlider.setValue(0.0);
    detuneSlider.setTextValueSuffix(" ct");
    detuneSlider.onValueChange = [this]() {
        // Update the selected oscillator's detune
        if (auto* graph = audioProcessor.getVoiceGraph()) {
            if (auto* node = graph->getNode(selectedOscillatorId)) {
                if (auto* osc = dynamic_cast<vizasynth::OscillatorSource*>(node)) {
                    osc->setDetuneCents(static_cast<float>(detuneSlider.getValue()));
                }
            }
        }
    };
    addAndMakeVisible(detuneSlider);

    detuneLabel.setText("Detune", juce::dontSendNotification);
    detuneLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(detuneLabel);

    // Setup octave slider (Phase 2.4)
    octaveSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    octaveSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 35, 20);
    octaveSlider.setRange(-2.0, 2.0, 1.0);
    octaveSlider.setValue(0.0);
    octaveSlider.onValueChange = [this]() {
        // Update the selected oscillator's octave
        if (auto* graph = audioProcessor.getVoiceGraph()) {
            if (auto* node = graph->getNode(selectedOscillatorId)) {
                if (auto* osc = dynamic_cast<vizasynth::OscillatorSource*>(node)) {
                    osc->setOctaveOffset(static_cast<int>(octaveSlider.getValue()));
                }
            }
        }
    };
    addAndMakeVisible(octaveSlider);

    octaveLabel.setText("Octave", juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(octaveLabel);

    // Setup filter node selector (Phase 2)
    filterNodeSelector.addItem("Filter 1", 1);  // Default filter
    filterNodeSelector.setSelectedId(1, juce::dontSendNotification);
    filterNodeSelector.onChange = [this]() { onFilterSelected(); };
    addAndMakeVisible(filterNodeSelector);

    // Setup filter type combo box
    filterTypeCombo.addItem("LP", 1);   // Lowpass
    filterTypeCombo.addItem("HP", 2);   // Highpass
    filterTypeCombo.addItem("BP", 3);   // Bandpass
    filterTypeCombo.addItem("N", 4);    // Notch
    addAndMakeVisible(filterTypeCombo);

    filterTypeLabel.setText("Type", juce::dontSendNotification);
    filterTypeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterTypeLabel);

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

    // Setup chain editor button
    chainEditorButton.setClickingTogglesState(true);
    chainEditorButton.setColour(juce::TextButton::buttonColourId, config.getAccentColour());
    chainEditorButton.onClick = [this]() {
        showChainEditor = chainEditorButton.getToggleState();
        chainEditor.setVisible(showChainEditor);
        resized();
    };
    addAndMakeVisible(chainEditorButton);

    // Setup chain editor (initially hidden)
    // Phase 3.5 Option B: Connect to voice graph for full integration
    if (auto* voiceGraph = audioProcessor.getVoiceGraph()) {
        chainEditor.setGraph(voiceGraph);
        chainEditor.setGraphModifiedCallback([this](vizasynth::SignalGraph* graph) {
            #if JUCE_DEBUG
            juce::Logger::writeToLog("[Phase 3.5 Full Integration] Voice graph modified via ChainEditor");
            #endif
            // Graph modifications will affect audio output!
            // Modifications are thread-safe via ChainModificationManager
        });
    } else {
        // Fallback to demo graph if voice not available
        chainEditor.setGraph(&audioProcessor.getDemoGraph());
        #if JUCE_DEBUG
        juce::Logger::writeToLog("[Phase 3.5] Fallback: Using demo graph (voice not ready)");
        #endif
    }

    // Set close callback to hide the chain editor
    chainEditor.setCloseCallback([this]() {
        showChainEditor = false;
        chainEditor.setVisible(false);
        chainEditorButton.setToggleState(false, juce::dontSendNotification);
        resized();
    });

    // Connect ChainEditor to ProbeRegistry for automatic probe registration
    chainEditor.setProbeRegistry(&audioProcessor.getProbeRegistry());

    chainEditor.setVisible(false);
    addAndMakeVisible(chainEditor);

    // Wire up SignalGraph to all visualizations that support per-visualization node targeting
    // This enables filter/oscillator selection dropdowns when multiple nodes exist
    if (auto* voiceGraph = audioProcessor.getVoiceGraph()) {
        bodePlot.setSignalGraph(voiceGraph);
        poleZeroPlot.setSignalGraph(voiceGraph);
        impulseResponse.setSignalGraph(voiceGraph);
        transferFunctionDisplay.setSignalGraph(voiceGraph);
        harmonicView.setSignalGraph(voiceGraph);
        singleCycleView.setSignalGraph(voiceGraph);
    }

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

    bandLimitedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "bandLimited", bandLimitedToggle);

    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "filterType", filterTypeCombo);

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

    // Register as SignalGraph listener for node selector updates (Phase 2)
    if (auto* voiceGraph = audioProcessor.getVoiceGraph()) {
        voiceGraph->addListener(this);
    }

    // Initialize node selectors from current graph state
    updateNodeSelectors();

    // Start timer for UI updates (60 FPS)
    startTimerHz(60);
}

VizASynthAudioProcessorEditor::~VizASynthAudioProcessorEditor()
{
    stopTimer();
    vizasynth::ConfigurationManager::getInstance().removeChangeListener(this);
    audioProcessor.getProbeRegistry().removeListener(this);

    // Unregister from SignalGraph
    if (auto* voiceGraph = audioProcessor.getVoiceGraph()) {
        voiceGraph->removeListener(this);
    }
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
    int oscY = 10;  // Top margin for oscillator panel
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

    // Oscillator panel Y position (no signal flow view anymore)
    int oscY = leftPanelArea.getY() + 5;
    int filterY = oscY + layout.oscPanelH + layout.panelSpacing;
    int envY = filterY + layout.filterPanelH + layout.panelSpacing;

    // Oscillator section (centered vertically in osc panel)
    int oscComboY = oscY + config.getLayoutInt("components.oscillatorPanel.comboY", 35);
    int oscComboYOffset = config.getLayoutInt("components.oscillatorPanel.comboYOffset", -2);
    int nodeSelectorWidth = config.getLayoutInt("components.oscillatorPanel.nodeSelectorWidth", 65);

    // Node selector at the far left (Phase 2)
    int oscSelectorX = panelX + layout.panelInnerMargin;
    oscNodeSelector.setBounds(oscSelectorX, oscComboY + oscComboYOffset, nodeSelectorWidth, layout.comboHeight);

    // Label and type combo to the right of selector
    int oscLabelX = oscSelectorX + nodeSelectorWidth + 5;
    oscTypeLabel.setBounds(oscLabelX, oscComboY, 65, layout.labelHeight);
    int oscComboX = oscLabelX + 65;
    oscTypeCombo.setBounds(oscComboX, oscComboY + oscComboYOffset, 70, layout.comboHeight);

    // Band-limiting toggle (PolyBLEP) - positioned to the right of oscillator type
    int bandLimitedX = oscComboX + 75;
    bandLimitedToggle.setBounds(bandLimitedX, oscComboY + oscComboYOffset, 80, layout.comboHeight);

    // Detune and Octave controls (Phase 2.4) - below waveform row
    int secondRowYOffset = config.getLayoutInt("components.oscillatorPanel.secondRowYOffset", 35);
    int oscSecondRowY = oscComboY + secondRowYOffset;
    int detuneWidth = config.getLayoutInt("components.oscillatorPanel.detuneSliderWidth", 100);
    int octaveWidth = config.getLayoutInt("components.oscillatorPanel.octaveSliderWidth", 70);
    int labelWidth = 45;

    detuneLabel.setBounds(oscSelectorX, oscSecondRowY, labelWidth, 20);
    detuneSlider.setBounds(oscSelectorX + labelWidth, oscSecondRowY, detuneWidth, 20);

    int octaveLabelX = oscSelectorX + labelWidth + detuneWidth + 8;
    octaveLabel.setBounds(octaveLabelX, oscSecondRowY, labelWidth, 20);
    octaveSlider.setBounds(octaveLabelX + labelWidth, oscSecondRowY, octaveWidth, 20);

    // Filter section (centered vertically in filter panel)
    int filterLabelY = filterY + layout.filterLabelYOffset;
    int filterKnobY = filterLabelY + layout.labelHeight + layout.filterKnobYOffset;

    // Filter node selector (Phase 2) - at top of filter section
    int filterSelectorY = filterY + config.getLayoutInt("components.filterPanel.nodeSelectorY", 10);
    int filterNodeSelectorWidth = config.getLayoutInt("components.filterPanel.nodeSelectorWidth", 75);
    filterNodeSelector.setBounds(panelX + layout.panelInnerMargin, filterSelectorY, filterNodeSelectorWidth, layout.comboHeight);

    // Filter type combo box (positioned to the left of cutoff knob)
    int filterTypeX = panelX + config.getLayoutInt("components.filterPanel.typeX", 18);
    int filterTypeWidth = config.getLayoutInt("components.filterPanel.typeWidth", 55);
    int filterTypeComboY = filterKnobY + config.getLayoutInt("components.filterPanel.typeYOffset", 15);
    int filterTypeComboHeight = config.getLayoutInt("components.filterPanel.typeHeight", 30);
    filterTypeLabel.setBounds(filterTypeX, filterLabelY, filterTypeWidth, layout.labelHeight);
    filterTypeCombo.setBounds(filterTypeX, filterTypeComboY, filterTypeWidth, filterTypeComboHeight);

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
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::Spectrum)
    {
        spectrumAnalyzer.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::Harmonics)
    {
        harmonicView.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::PoleZero)
    {
        poleZeroPlot.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::Bode)
    {
        bodePlot.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else if (currentVizMode == VisualizationMode::TransferFunction)
    {
        transferFunctionDisplay.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
        impulseResponse.setBounds(scopeArea);   // Hidden but positioned
    }
    else // ImpulseResponse mode
    {
        impulseResponse.setBounds(scopeArea);
        oscilloscope.setBounds(scopeArea);      // Hidden but positioned
        singleCycleView.setBounds(scopeArea);   // Hidden but positioned
        spectrumAnalyzer.setBounds(scopeArea);  // Hidden but positioned
        harmonicView.setBounds(scopeArea);      // Hidden but positioned
        poleZeroPlot.setBounds(scopeArea);      // Hidden but positioned
        bodePlot.setBounds(scopeArea);          // Hidden but positioned
        transferFunctionDisplay.setBounds(scopeArea);  // Hidden but positioned
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
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    int transferFunctionButtonWidth = config.getLayoutInt("components.buttons.transferFunction.width", 40);
    transferFunctionButton.setBounds(vizControlArea.removeFromLeft(transferFunctionButtonWidth));
    vizControlArea.removeFromLeft(layout.vizControlScopeSpacing);
    int impulseResponseButtonWidth = config.getLayoutInt("components.buttons.impulseResponse.width", 40);
    impulseResponseButton.setBounds(vizControlArea.removeFromLeft(impulseResponseButtonWidth));
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);

    // Layout dynamic probe buttons
    for (size_t i = 0; i < probeButtons.size(); ++i) {
        probeButtons[i]->setBounds(vizControlArea.removeFromLeft(layout.vizControlProbeWidth));
        if (i < probeButtons.size() - 1) {
            vizControlArea.removeFromLeft(layout.vizControlProbeSpacing);
        }
    }
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);
    freezeButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlFreezeWidth));
    vizControlArea.removeFromLeft(layout.vizControlProbeSpacing);
    clearTraceButton.setBounds(vizControlArea.removeFromLeft(layout.vizControlClearWidth));
    vizControlArea.removeFromLeft(layout.vizControlSectionSpacing);
    timeWindowLabel.setBounds(vizControlArea.removeFromLeft(layout.vizControlLabelWidth));
    timeWindowSlider.setBounds(vizControlArea);

    // Chain Editor button - place it in the top-right corner
    int chainEditorButtonWidth = 100;
    int chainEditorButtonHeight = 30;
    chainEditorButton.setBounds(getWidth() - chainEditorButtonWidth - 10, 10,
                                 chainEditorButtonWidth, chainEditorButtonHeight);

    // Chain Editor - overlay the entire window when visible
    if (showChainEditor) {
        chainEditor.setBounds(getLocalBounds());
    }
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

    for (auto* btn : {&scopeButton, &spectrumButton, &harmonicsButton, &poleZeroButton, &bodeButton, &transferFunctionButton,
                      &impulseResponseButton, &clearTraceButton}) {
        btn->setColour(juce::TextButton::buttonColourId, buttonDefault);
        btn->setColour(juce::TextButton::textColourOffId, buttonText);
    }

    // Apply theme to dynamic probe buttons
    for (auto& button : probeButtons) {
        button->setColour(juce::TextButton::buttonColourId, buttonDefault);
        button->setColour(juce::TextButton::textColourOffId, buttonText);
    }

    freezeButton.setColour(juce::TextButton::buttonColourId, buttonDefault);
    freezeButton.setColour(juce::TextButton::buttonOnColourId, toggleOnColor);
    freezeButton.setColour(juce::TextButton::textColourOffId, buttonText);

    // Apply label colors
    auto labelColor = config.getThemeColour("colors.labels.text", juce::Colour(0xffe0e0e0));
    for (auto* label : {&oscTypeLabel, &filterTypeLabel, &cutoffLabel, &resonanceLabel, &attackLabel,
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
    auto& registry = audioProcessor.getProbeRegistry();
    auto activeProbeId = registry.getActiveProbe();
    auto availableProbes = registry.getAvailableProbes();

    // Ensure we have the right number of buttons
    if (probeButtons.size() != availableProbes.size()) {
        // Button count mismatch - should call updateFromProbeRegistry instead
        return;
    }

    // Update highlighting for each button
    for (size_t i = 0; i < probeButtons.size(); ++i) {
        auto& button = probeButtons[i];
        const auto& probe = availableProbes[i];
        bool isActive = (probe.id == activeProbeId);

        if (isActive) {
            button->setColour(juce::TextButton::buttonColourId, probe.color.darker(0.3f));
            button->setColour(juce::TextButton::textColourOffId, config.getTextHighlightColour());
        } else {
            button->setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
            button->setColour(juce::TextButton::textColourOffId, probe.color);
        }
    }
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
    bool isTransferFunction = (currentVizMode == VisualizationMode::TransferFunction);
    bool isImpulseResponse = (currentVizMode == VisualizationMode::ImpulseResponse);

    // Show/hide appropriate visualization
    oscilloscope.setVisible(isScope);
    singleCycleView.setVisible(isScope);
    spectrumAnalyzer.setVisible(isSpectrum);
    harmonicView.setVisible(isHarmonics);
    poleZeroPlot.setVisible(isPoleZero);
    bodePlot.setVisible(isBode);
    transferFunctionDisplay.setVisible(isTransferFunction);
    impulseResponse.setVisible(isImpulseResponse);

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
    transferFunctionButton.setColour(juce::TextButton::buttonColourId,
                                     isTransferFunction ? config.getAccentColour() : config.getPanelBackgroundColour());
    impulseResponseButton.setColour(juce::TextButton::buttonColourId,
                                    isImpulseResponse ? config.getAccentColour() : config.getPanelBackgroundColour());

    // Show/hide time window control (only relevant for oscilloscope)
    timeWindowSlider.setEnabled(isScope);
    timeWindowLabel.setAlpha(isScope ? 1.0f : 0.4f);
    timeWindowSlider.setAlpha(isScope ? 1.0f : 0.4f);

    // Re-layout the visualization area
    resized();
}

//==============================================================================
// ProbeRegistryListener implementation

void VizASynthAudioProcessorEditor::onProbeRegistered(const std::string& probeId)
{
    // Rebuild all probe buttons when a new probe is registered
    juce::MessageManager::callAsync([this]() {
        updateFromProbeRegistry();
    });
}

void VizASynthAudioProcessorEditor::onProbeUnregistered(const std::string& probeId)
{
    // Rebuild all probe buttons when a probe is unregistered
    juce::MessageManager::callAsync([this]() {
        updateFromProbeRegistry();
    });
}

void VizASynthAudioProcessorEditor::onActiveProbeChanged(const std::string& probeId)
{
    // Debounce: prevent rapid updates that cause flashing
    juce::int64 currentTime = juce::Time::currentTimeMillis();
    if (currentTime - lastProbeUpdateTime < kProbeUpdateDebounceMs) {
        return;  // Skip this update - too soon after the last one
    }
    lastProbeUpdateTime = currentTime;

    // Update button highlighting when active probe changes
    juce::MessageManager::callAsync([this]() {
        if (!isUpdatingProbeSelection) {
            updateProbeButtons();
        }
    });
}

void VizASynthAudioProcessorEditor::updateFromProbeRegistry()
{
    auto& config = vizasynth::ConfigurationManager::getInstance();
    auto& registry = audioProcessor.getProbeRegistry();
    auto availableProbes = registry.getAvailableProbes();

    // Remove existing probe buttons
    for (auto& button : probeButtons) {
        removeChildComponent(button.get());
    }
    probeButtons.clear();

    // Create new buttons for each registered probe
    for (const auto& probe : availableProbes) {
        auto button = std::make_unique<juce::TextButton>(probe.displayName);
        button->setClickingTogglesState(false);
        button->setColour(juce::TextButton::buttonColourId, config.getPanelBackgroundColour());
        button->setColour(juce::TextButton::textColourOffId, probe.color);

        // Capture probe ID for the click handler
        auto probeId = probe.id;
        button->onClick = [this, probeId]() {
            // Set flag to prevent cascading updates during selection
            isUpdatingProbeSelection = true;
            audioProcessor.getProbeRegistry().setActiveProbe(probeId);
            updateProbeButtons();
            isUpdatingProbeSelection = false;
        };

        addAndMakeVisible(*button);
        probeButtons.push_back(std::move(button));
    }

    // Update button highlighting based on active probe
    updateProbeButtons();

    // Re-layout to accommodate new buttons
    resized();
}

//==============================================================================
// SignalGraphListener implementation

void VizASynthAudioProcessorEditor::onNodeAdded(const std::string& nodeId, const std::string& nodeType)
{
    // Update node selectors when a new node is added
    juce::MessageManager::callAsync([this]() {
        updateNodeSelectors();

        // Update visualization node lists
        if (auto* graph = audioProcessor.getVoiceGraph()) {
            bodePlot.setSignalGraph(graph);
            poleZeroPlot.setSignalGraph(graph);
            impulseResponse.setSignalGraph(graph);
            transferFunctionDisplay.setSignalGraph(graph);
            harmonicView.setSignalGraph(graph);
            singleCycleView.setSignalGraph(graph);
        }
    });
}

void VizASynthAudioProcessorEditor::onNodeRemoved(const std::string& nodeId)
{
    // Update node selectors and check if selected node was removed
    juce::MessageManager::callAsync([this, nodeId]() {
        // If removed node was selected, select another one
        if (selectedOscillatorId == nodeId) {
            selectedOscillatorId = "osc1";  // Reset to default
        }
        if (selectedFilterId == nodeId) {
            selectedFilterId = "filter1";  // Reset to default
        }
        updateNodeSelectors();

        // Update visualization node lists
        if (auto* graph = audioProcessor.getVoiceGraph()) {
            bodePlot.setSignalGraph(graph);
            poleZeroPlot.setSignalGraph(graph);
            impulseResponse.setSignalGraph(graph);
            transferFunctionDisplay.setSignalGraph(graph);
            harmonicView.setSignalGraph(graph);
            singleCycleView.setSignalGraph(graph);
        }
    });
}

void VizASynthAudioProcessorEditor::onConnectionAdded(const std::string& /*sourceId*/, const std::string& /*destId*/)
{
    // Connection changes don't require UI updates for visualizations
}

void VizASynthAudioProcessorEditor::onConnectionRemoved(const std::string& /*sourceId*/, const std::string& /*destId*/)
{
    // Connection changes don't require UI updates for visualizations
}

void VizASynthAudioProcessorEditor::onGraphStructureChanged()
{
    // Full graph reset - rebuild selectors and update visualizations
    juce::MessageManager::callAsync([this]() {
        updateNodeSelectors();

        // Update all visualizations with the new graph
        if (auto* graph = audioProcessor.getVoiceGraph()) {
            bodePlot.setSignalGraph(graph);
            poleZeroPlot.setSignalGraph(graph);
            impulseResponse.setSignalGraph(graph);
            transferFunctionDisplay.setSignalGraph(graph);
            harmonicView.setSignalGraph(graph);
            singleCycleView.setSignalGraph(graph);
        }
    });
}

//==============================================================================
// Node selection helpers

void VizASynthAudioProcessorEditor::updateNodeSelectors()
{
    auto* graph = audioProcessor.getVoiceGraph();
    if (!graph) return;

    // Update oscillator selector
    oscNodeSelector.clear(juce::dontSendNotification);
    int oscItemId = 1;
    int selectedOscItemId = 1;

    graph->forEachNode([this, &oscItemId, &selectedOscItemId](const std::string& nodeId, const vizasynth::SignalNode* node) {
        if (dynamic_cast<const vizasynth::OscillatorSource*>(node) != nullptr) {
            // Create display name (e.g., "Osc 1" from "osc1")
            std::string displayName = node->getName();
            if (displayName.length() > 10) {
                displayName = displayName.substr(0, 10);
            }
            oscNodeSelector.addItem(displayName, oscItemId);

            if (nodeId == selectedOscillatorId) {
                selectedOscItemId = oscItemId;
            }
            oscItemId++;
        }
    });

    if (oscNodeSelector.getNumItems() > 0) {
        oscNodeSelector.setSelectedId(selectedOscItemId, juce::dontSendNotification);
    }

    // Update filter selector
    filterNodeSelector.clear(juce::dontSendNotification);
    int filterItemId = 1;
    int selectedFilterItemId = 1;

    graph->forEachNode([this, &filterItemId, &selectedFilterItemId](const std::string& nodeId, const vizasynth::SignalNode* node) {
        // Check if it's a filter (has LTI analysis support)
        if (node->supportsAnalysis() && node->isLTI()) {
            std::string displayName = node->getName();
            if (displayName.length() > 10) {
                displayName = displayName.substr(0, 10);
            }
            filterNodeSelector.addItem(displayName, filterItemId);

            if (nodeId == selectedFilterId) {
                selectedFilterItemId = filterItemId;
            }
            filterItemId++;
        }
    });

    if (filterNodeSelector.getNumItems() > 0) {
        filterNodeSelector.setSelectedId(selectedFilterItemId, juce::dontSendNotification);
    }

    // Update panel controls to reflect selected nodes
    updateOscillatorPanel();
    updateFilterPanel();
}

void VizASynthAudioProcessorEditor::updateOscillatorPanel()
{
    auto* graph = audioProcessor.getVoiceGraph();
    if (!graph) return;

    auto* node = graph->getNode(selectedOscillatorId);
    if (auto* osc = dynamic_cast<vizasynth::OscillatorSource*>(node)) {
        // Update detune/octave sliders to match selected oscillator
        detuneSlider.setValue(osc->getDetuneCents(), juce::dontSendNotification);
        octaveSlider.setValue(osc->getOctaveOffset(), juce::dontSendNotification);

        // Update waveform combo if it's a PolyBLEP oscillator
        if (auto* polyblep = dynamic_cast<vizasynth::PolyBLEPOscillator*>(osc)) {
            int waveformIndex = static_cast<int>(polyblep->getWaveform());
            oscTypeCombo.setSelectedId(waveformIndex + 1, juce::dontSendNotification);
        }
    }
}

void VizASynthAudioProcessorEditor::updateFilterPanel()
{
    // Filter panel updates would be similar to oscillator
    // For now, the filter parameters are connected via APVTS attachments
}

void VizASynthAudioProcessorEditor::onOscillatorSelected()
{
    // Find the node ID corresponding to the selected combo box item
    auto* graph = audioProcessor.getVoiceGraph();
    if (!graph) return;

    int selectedItem = oscNodeSelector.getSelectedItemIndex();
    int currentIndex = 0;

    graph->forEachNode([this, &currentIndex, selectedItem](const std::string& nodeId, const vizasynth::SignalNode* node) {
        if (dynamic_cast<const vizasynth::OscillatorSource*>(node) != nullptr) {
            if (currentIndex == selectedItem) {
                selectedOscillatorId = nodeId;
            }
            currentIndex++;
        }
    });

    updateOscillatorPanel();
}

void VizASynthAudioProcessorEditor::onFilterSelected()
{
    // Find the node ID corresponding to the selected combo box item
    auto* graph = audioProcessor.getVoiceGraph();
    if (!graph) return;

    int selectedItem = filterNodeSelector.getSelectedItemIndex();
    int currentIndex = 0;

    graph->forEachNode([this, &currentIndex, selectedItem](const std::string& nodeId, const vizasynth::SignalNode* node) {
        if (node->supportsAnalysis() && node->isLTI()) {
            if (currentIndex == selectedItem) {
                selectedFilterId = nodeId;
            }
            currentIndex++;
        }
    });

    updateFilterPanel();
}
