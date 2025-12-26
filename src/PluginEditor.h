#pragma once

#include "PluginProcessor.h"
#include "Visualization/TimeDomain/Oscilloscope.h"
#include "Visualization/FrequencyDomain/SpectrumAnalyzer.h"
#include "Visualization/FrequencyDomain/HarmonicView.h"
#include "Visualization/FrequencyDomain/BodePlot.h"
#include "Visualization/ZDomain/PoleZeroPlot.h"
#include "Visualization/SingleCycleView.h"
#include "Visualization/EnvelopeVisualizer.h"
#include "UI/LevelMeter.h"
#include "UI/VirtualKeyboard.h"
#include "Core/Configuration.h"

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
    Bode
};

//==============================================================================
/**
 * Main editor UI for Viz-A-Synth
 */
class VizASynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer,
                                       public juce::ChangeListener
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
    void updateProbeButtons();
    void updateVisualizationMode();
    void setVisualizationMode(VisualizationMode mode);
    void applyThemeToComponents();

    VizASynthAudioProcessor& audioProcessor;

    // Track active note count for envelope visualization
    int lastActiveNoteCount = 0;

    // Visualization
    vizasynth::Oscilloscope oscilloscope;
    vizasynth::SpectrumAnalyzer spectrumAnalyzer;
    vizasynth::HarmonicView harmonicView;
    vizasynth::PoleZeroPlot poleZeroPlot;
    vizasynth::BodePlot bodePlot;
    vizasynth::SingleCycleView singleCycleView;
    vizasynth::EnvelopeVisualizer envelopeVisualizer;
    VisualizationMode currentVizMode = VisualizationMode::Oscilloscope;

    // Visualization mode selector
    juce::TextButton scopeButton{"Scope"};
    juce::TextButton spectrumButton{"Spectrum"};
    juce::TextButton harmonicsButton{"Harmonics"};
    juce::TextButton poleZeroButton{"P/Z"};
    juce::TextButton bodeButton{"Bode"};

    // Probe selector buttons
    juce::TextButton probeOscButton{"OSC"};
    juce::TextButton probeFilterButton{"FILT"};
    juce::TextButton probeOutputButton{"OUT"};
    juce::TextButton freezeButton{"Freeze"};
    juce::TextButton clearTraceButton{"Clear"};

    // Time window slider
    juce::Slider timeWindowSlider;
    juce::Label timeWindowLabel;

    // UI Components
    juce::ComboBox oscTypeCombo;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;

    juce::Label oscTypeLabel;
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

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oscTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthAudioProcessorEditor)
};
