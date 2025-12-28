#pragma once

#include "PluginProcessor.h"
#include "Visualization/TimeDomain/Oscilloscope.h"
#include "Visualization/FrequencyDomain/SpectrumAnalyzer.h"
#include "Visualization/FrequencyDomain/HarmonicView.h"
#include "Visualization/FrequencyDomain/BodePlot.h"
#include "Visualization/ZDomain/PoleZeroPlot.h"
#include "Visualization/ZDomain/TransferFunctionDisplay.h"
#include "Visualization/TimeDomain/ImpulseResponse.h"
#include "Visualization/SingleCycleView.h"
#include "Visualization/EnvelopeVisualizer.h"
#include "UI/LevelMeter.h"
#include "UI/VirtualKeyboard.h"
#include "Core/Configuration.h"
#include "Visualization/ProbeRegistry.h"
#include "UI/ChainEditor.h"
#include "DSP/SignalGraph.h"

//==============================================================================
/**
 * Visualization mode enumeration
 */
enum class VisualizationMode
{
    Oscilloscope,
    Spectrum,
    Harmonics,
    PoleZero,
    Bode,
    TransferFunction,
    ImpulseResponse
};

//==============================================================================
/**
 * Main editor UI for Viz-A-Synth
 */
class VizASynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer,
                                       public juce::ChangeListener,
                                       public vizasynth::ProbeRegistryListener,
                                       public vizasynth::SignalGraphListener
{
public:
    VizASynthAudioProcessorEditor(VizASynthAudioProcessor&);
    ~VizASynthAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // ProbeRegistryListener interface
    void onProbeRegistered(const std::string& probeId) override;
    void onProbeUnregistered(const std::string& probeId) override;
    void onActiveProbeChanged(const std::string& probeId) override;

    // SignalGraphListener interface
    void onNodeAdded(const std::string& nodeId, const std::string& nodeType) override;
    void onNodeRemoved(const std::string& nodeId) override;
    void onConnectionAdded(const std::string& sourceId, const std::string& destId) override;
    void onConnectionRemoved(const std::string& sourceId, const std::string& destId) override;
    void onGraphStructureChanged() override;

    void updateFromProbeRegistry();
    void updateProbeButtons();
    void updateVisualizationMode();
    void setVisualizationMode(VisualizationMode mode);
    void applyThemeToComponents();

    // Node selection helpers
    void updateNodeSelectors();
    void updateOscillatorPanel();
    void updateFilterPanel();
    void onOscillatorSelected();
    void onFilterSelected();

    VizASynthAudioProcessor& audioProcessor;

    // Track active note count for envelope visualization
    int lastActiveNoteCount = 0;

    // Visualization
    vizasynth::Oscilloscope oscilloscope;
    vizasynth::SpectrumAnalyzer spectrumAnalyzer;
    vizasynth::HarmonicView harmonicView;
    vizasynth::PoleZeroPlot poleZeroPlot;
    vizasynth::BodePlot bodePlot;
    vizasynth::TransferFunctionDisplay transferFunctionDisplay;
    vizasynth::ImpulseResponse impulseResponse;
    vizasynth::SingleCycleView singleCycleView;
    vizasynth::EnvelopeVisualizer envelopeVisualizer;
    VisualizationMode currentVizMode = VisualizationMode::Oscilloscope;

    // Visualization mode selector
    juce::TextButton scopeButton{"Scope"};
    juce::TextButton spectrumButton{"Spectrum"};
    juce::TextButton harmonicsButton{"Harmonics"};
    juce::TextButton poleZeroButton{"P/Z"};
    juce::TextButton bodeButton{"Bode"};
    juce::TextButton transferFunctionButton{"H(z)"};
    juce::TextButton impulseResponseButton{"h[n]"};

    // Dynamic probe selector buttons (generated from ProbeRegistry)
    std::vector<std::unique_ptr<juce::TextButton>> probeButtons;
    juce::TextButton freezeButton{"Freeze"};
    juce::TextButton clearTraceButton{"Clear"};

    // Time window slider
    juce::Slider timeWindowSlider;
    juce::Label timeWindowLabel;

    // Node selectors (Phase 2: select which node to control)
    juce::ComboBox oscNodeSelector;
    juce::ComboBox filterNodeSelector;
    std::string selectedOscillatorId = "osc1";  // Currently selected oscillator
    std::string selectedFilterId = "filter1";   // Currently selected filter

    // UI Components
    juce::ComboBox oscTypeCombo;
    juce::ToggleButton bandLimitedToggle{"PolyBLEP"};
    juce::Slider detuneSlider;
    juce::Slider octaveSlider;
    juce::Label detuneLabel;
    juce::Label octaveLabel;
    juce::ComboBox filterTypeCombo;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;

    juce::Label oscTypeLabel;
    juce::Label filterTypeLabel;
    juce::Label cutoffLabel;
    juce::Label resonanceLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;

    // Master volume
    juce::Slider masterVolumeSlider;
    juce::Label masterVolumeLabel;

    // Level meter
    LevelMeter levelMeter;

    // Virtual keyboard
    VirtualKeyboard virtualKeyboard;

    // Chain editor (toggleable)
    vizasynth::ChainEditor chainEditor;
    juce::TextButton chainEditorButton{"Chain Editor"};
    bool showChainEditor = false;

    // Probe selection debouncing to prevent flashing
    bool isUpdatingProbeSelection = false;
    juce::int64 lastProbeUpdateTime = 0;
    static constexpr int kProbeUpdateDebounceMs = 50;  // Minimum ms between updates

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oscTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandLimitedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthAudioProcessorEditor)
};
