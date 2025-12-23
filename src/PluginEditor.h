#pragma once

#include "PluginProcessor.h"

//==============================================================================
/**
 * Main editor UI for Viz-A-Synth
 */
class VizASynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    VizASynthAudioProcessorEditor(VizASynthAudioProcessor&);
    ~VizASynthAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VizASynthAudioProcessor& audioProcessor;

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

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oscTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthAudioProcessorEditor)
};
